#include "game.h"
#include <algorithm>
#include <cmath>
#include <chrono>
#include <QDebug>

using Clock = std::chrono::steady_clock;

static float randf(std::mt19937& rng, float lo, float hi) {
    std::uniform_real_distribution<float> d(lo, hi);
    return d(rng);
}
static int randi(std::mt19937& rng, int lo, int hi) {
    std::uniform_int_distribution<int> d(lo, hi);
    return d(rng);
}

Game::Game() : rng_(std::random_device{}()) {}
Game::~Game() { stop(); }

void Game::setup() {
    tanks_.reserve(cfg::ENEMIES + 1);
    shells_.resize(512);
    buildings_.reserve(64);

    Tank player;
    player.id = 0;
    player.team = Team::Red;
    player.is_player = true;
    player.x.store(5.0f);
    player.y.store(float(cfg::MAP_H) / 2);
    player.hp.store(150);
    player.max_hp.store(150);
    player.reload_time = 0.7f;
    player.shell_speed = 24.0f;
    player.move_speed  = 8.0f;
    player.aim_angle.store(0.0f);
    player.shell_type = ShellType::AP;
    tanks_.push_back(std::move(player));

    for (int i = 0; i < cfg::ENEMIES; ++i) {
        Tank e;
        e.id = 1 + i;
        e.team = Team::Blue;
        e.x.store(float(cfg::MAP_W - 5 - randi(rng_, 0, 5)));
        e.y.store(float(3 + i * 4));
        int hp = 60 + randi(rng_, 0, 60);
        e.hp.store(hp);
        e.max_hp.store(hp);
        e.aim_angle.store(3.14159f);
        e.reload_time = randf(rng_, 1.5f, 3.0f);
        e.shell_speed = randf(rng_, 14.0f, 20.0f);
        e.move_speed  = randf(rng_, 2.0f, 4.0f);
        e.shell_type  = static_cast<ShellType>(randi(rng_, 0, 2));
        tanks_.push_back(std::move(e));
    }

    for (int i = 0; i < 14; ++i) {
        Building b;
        b.x = randi(rng_, 8, cfg::MAP_W - 12);
        b.y = randi(rng_, 3, cfg::MAP_H - 6);
        b.w = randi(rng_, 2, 4);
        b.h = randi(rng_, 2, 4);
        b.hp.store(80 + randi(rng_, 0, 80));
        buildings_.push_back(std::move(b));
    }

    wind_x_.store(randf(rng_, -cfg::WIND_MAX, cfg::WIND_MAX));
}

void Game::start() {
    stop_flag_.store(false);

    int num_threads = 1 + int(tanks_.size())
                    + cfg::SHELL_POOL_THREADS
                    + 2;                     // destruction
    qDebug() << "[Game::start] num_threads =" << num_threads
             << " tanks =" << tanks_.size()
             << " shell_pool =" << cfg::SHELL_POOL_THREADS;

    auto noop = []{};
    start_barrier_ = std::make_unique<
        std::barrier<std::function<void()>>>(num_threads, noop);
    end_barrier_   = std::make_unique<
        std::barrier<std::function<void()>>>(num_threads, noop);

    for (size_t i = 0; i < tanks_.size(); ++i) {
        Tank* t = &tanks_[i];
        threads_.emplace_back([this, t] { ai_loop(t); });
    }
    for (int i = 0; i < cfg::SHELL_POOL_THREADS; ++i) {
        threads_.emplace_back([this] { shell_pool_loop(); });
    }
    threads_.emplace_back([this] { collision_loop(); });
    threads_.emplace_back([this] { destruction_loop(); });
    threads_.emplace_back([this] { sim_loop(); });

    qDebug() << "[Game::start] spawned" << threads_.size() << "worker threads";
}

void Game::stop() {
    stop_flag_.store(true);
    // Разбудить все потоки, пришедшие на barrier
    if (start_barrier_ && end_barrier_) {
        // Каждый поток должен дойти до barrier, но они проверяют stop_flag_
        // в начале цикла. Просто даём им время завершиться.
    }
    for (auto& t : threads_) if (t.joinable()) t.join();
    threads_.clear();
}

void Game::set_player_input(float dx, float dy, float aim, bool fire) {
    if (tanks_.empty()) return;
    Tank& p = tanks_[0];
    p.player_dx.store(dx);
    p.player_dy.store(dy);
    p.player_aim.store(aim);
    p.player_fire.store(fire);
}

// ============================================================ AI thread
void Game::ai_loop(Tank* tank) {
    qDebug() << "[ai] thread started for tank" << tank->id;

    while (!stop_flag_.load()) {
        start_barrier_->arrive_and_wait();
        if (stop_flag_.load()) break;

        if (tank->is_player) {
            // Игрок — копируем ввод в intent
            tank->intent_dx.store(tank->player_dx.load());
            tank->intent_dy.store(tank->player_dy.load());
            tank->intent_aim.store(tank->player_aim.load());
            tank->intent_fire.store(tank->player_fire.load());
            end_barrier_->arrive_and_wait();
            continue;
        }

        if (tank->alive.load()) {
            // Ищем ближайшую цель (игрока или другую команду)
            Tank* target = nullptr;
            float best = 1e9f;
            for (auto& other : tanks_) {
                if (other.team == tank->team) continue;
                if (!other.alive.load()) continue;
                float dx = other.x.load() - tank->x.load();
                float dy = other.y.load() - tank->y.load();
                float d = dx*dx + dy*dy;
                if (d < best) { best = d; target = &other; }
            }
            if (target) {
                float dx = target->x.load() - tank->x.load();
                float dy = target->y.load() - tank->y.load();
                float dist = std::sqrt(dx*dx + dy*dy);
                if (dist > 0.01f) { dx /= dist; dy /= dist; }
                float aim = std::atan2(dy, dx);
                float move = (dist > 18.0f) ? 1.0f
                           : (dist < 8.0f)  ? -0.5f
                           : 0.3f * std::sin(float(tank->id) * 1.7f);
                tank->intent_dx.store(dx * move);
                tank->intent_dy.store(dy * move);
                tank->intent_aim.store(aim);
                float adiff = std::abs(normalize_angle(aim - tank->aim_angle.load()));
                bool fire = tank->reload.load() <= 0.0f
                         && adiff < 0.25f
                         && dist < 35.0f;
                tank->intent_fire.store(fire);
            } else {
                tank->intent_dx.store(0);
                tank->intent_dy.store(0);
                tank->intent_fire.store(false);
            }
        }
        end_barrier_->arrive_and_wait();
    }
}

// ============================================================ sim tick
void Game::sim_loop() {
    qDebug() << "[sim] thread started";

    using namespace std::chrono;
    const auto tick_dur = microseconds(1'000'000 / cfg::TICK_HZ);
    auto next = Clock::now();
    const float dt = 1.0f / cfg::TICK_HZ;

    while (!stop_flag_.load()) {
        next += tick_dur;
        start_barrier_->arrive_and_wait();
        if (stop_flag_.load()) break;

        integrate_tanks(dt);

        float w = wind_x_.load() + randf(rng_, -0.03f, 0.03f);
        w = std::max(-cfg::WIND_MAX, std::min(cfg::WIND_MAX, w));
        wind_x_.store(w);

        cleanup_explosions(dt);
        sim_tick_.fetch_add(1);

        end_barrier_->arrive_and_wait();

        auto now = Clock::now();
        if (now < next) std::this_thread::sleep_until(next);
        else next = now;
    }
}

void Game::integrate_tanks(float dt) {
    for (auto& t : tanks_) {
        if (!t.alive.load()) continue;
        float dx = t.intent_dx.load();
        float dy = t.intent_dy.load();
        float len = std::sqrt(dx*dx + dy*dy);
        if (len > 1.0f) { dx /= len; dy /= len; }
        float nx = t.x.load() + dx * t.move_speed * dt;
        float ny = t.y.load() + dy * t.move_speed * dt;
        nx = std::clamp(nx, 0.5f, float(cfg::MAP_W) - 0.5f);
        ny = std::clamp(ny, 0.5f, float(cfg::MAP_H) - 0.5f);

        bool blocked = false;
        for (auto& b : buildings_) {
            if (b.destroyed.load()) continue;
            if (nx >= b.x && nx < b.x + b.w &&
                ny >= b.y && ny < b.y + b.h) { blocked = true; break; }
        }
        if (!blocked) { t.x.store(nx); t.y.store(ny); }

        t.aim_angle.store(t.intent_aim.load());

        float r = t.reload.load() - dt;
        if (r < 0) r = 0;
        t.reload.store(r);

        if (t.intent_fire.load() && r <= 0.0f) {
            spawn_shell(t);
            t.reload.store(t.reload_time);
        }
    }
}

void Game::spawn_shell(Tank& tank) {
    float angle = tank.aim_angle.load();
    ShellSpawnEvent ev;
    ev.owner_id = tank.id;
    ev.team     = tank.team;
    ev.x        = tank.x.load() + std::cos(angle) * 0.8f;
    ev.y        = tank.y.load() + std::sin(angle) * 0.8f;
    ev.vx       = std::cos(angle) * tank.shell_speed;
    ev.vy       = std::sin(angle) * tank.shell_speed;
    ev.type     = tank.shell_type;
    ev.damage   = (ev.type == ShellType::HE)   ? 35.0f
                : (ev.type == ShellType::HEAT) ? 45.0f
                : 25.0f;
    spawn_queue_.push(ev);
}

void Game::add_explosion(float x, float y, float radius, float ttl) {
    std::lock_guard<std::mutex> lk(expl_mtx_);
    explosions_.push_back({x, y, ttl, radius});
}

void Game::cleanup_explosions(float dt) {
    std::lock_guard<std::mutex> lk(expl_mtx_);
    for (auto& e : explosions_) e.ttl -= dt;
    while (!explosions_.empty() && explosions_.front().ttl <= 0) {
        explosions_.pop_front();
    }
}

// ============================================================ shell pool
void Game::shell_pool_loop() {
    qDebug() << "[shell_pool] thread started";

    const float dt = 1.0f / cfg::TICK_HZ;
    while (!stop_flag_.load()) {
        start_barrier_->arrive_and_wait();
        if (stop_flag_.load()) break;

        ShellSpawnEvent sev;
        while (spawn_queue_.pop(sev)) {
            for (auto& s : shells_) {
                if (!s.active.load()) {
                    s.id       = shell_next_id_.fetch_add(1);
                    s.owner_id = sev.owner_id;
                    s.team     = sev.team;
                    s.type     = sev.type;
                    s.x.store(sev.x);
                    s.y.store(sev.y);
                    s.vx.store(sev.vx);
                    s.vy.store(sev.vy);
                    s.life.store(cfg::SHELL_LIFETIME);
                    s.damage   = sev.damage;
                    s.active.store(true);
                    break;
                }
            }
        }

        int worker_id = shell_worker_counter_.fetch_add(1) % cfg::SHELL_POOL_THREADS;
        int idx = 0;
        for (auto& s : shells_) {
            if (!s.active.load()) { ++idx; continue; }
            if ((idx % cfg::SHELL_POOL_THREADS) != worker_id) { ++idx; continue; }
            ++idx;

            float nx = s.x.load() + s.vx.load() * dt;
            float ny = s.y.load() + s.vy.load() * dt;
            // float nvx = s.vx.load() + wind_x_.load() * dt;
            // float nvy = s.vy.load() + cfg::GRAVITY * dt;
            float nvx = s.vx.load();   // скорость не меняется — прямое движение
            float nvy = s.vy.load();
            s.x.store(nx); s.y.store(ny);
            s.vx.store(nvx); s.vy.store(nvy);
            float l = s.life.load() - dt;
            s.life.store(l);
            if (l <= 0) { s.active.store(false); continue; }
            if (nx < 0 || nx >= cfg::MAP_W || ny < 0 || ny >= cfg::MAP_H) {
                HitEvent he;
                he.shell_id = s.id;
                he.x = nx; he.y = ny;
                he.damage = 0;
                hit_queue_.push(he);
                s.active.store(false);
            }
        }

        end_barrier_->arrive_and_wait();
    }
}

// ============================================================ collision
void Game::collision_loop() {
    while (!stop_flag_.load()) {
        start_barrier_->arrive_and_wait();
        if (stop_flag_.load()) break;

        for (auto& s : shells_) {
            if (!s.active.load()) continue;
            float sx = s.x.load(), sy = s.y.load();

            for (auto& t : tanks_) {
                if (!t.alive.load()) continue;
                if (t.id == s.owner_id) continue;
                if (t.team == s.team) continue;
                float dx = t.x.load() - sx;
                float dy = t.y.load() - sy;
                if (dx*dx + dy*dy < 0.8f * 0.8f) {
                    HitEvent he;
                    he.target_tank_id = t.id;
                    he.shell_id = s.id;
                    he.x = sx; he.y = sy;
                    he.damage = s.damage;
                    he.type = s.type;
                    hit_queue_.push(he);
                    s.active.store(false);
                    break;
                }
            }
            if (!s.active.load()) continue;

            for (size_t bi = 0; bi < buildings_.size(); ++bi) {
                auto& b = buildings_[bi];
                if (b.destroyed.load()) continue;
                if (sx >= b.x && sx < b.x + b.w &&
                    sy >= b.y && sy < b.y + b.h) {
                    HitEvent he;
                    he.building_idx = int(bi);
                    he.shell_id = s.id;
                    he.x = sx; he.y = sy;
                    he.damage = s.damage;
                    he.type = s.type;
                    hit_queue_.push(he);
                    s.active.store(false);
                    break;
                }
            }
        }

        end_barrier_->arrive_and_wait();
    }
}

// ============================================================ destruction
void Game::destruction_loop() {
    while (!stop_flag_.load()) {
        start_barrier_->arrive_and_wait();
        if (stop_flag_.load()) break;

        HitEvent he;
        while (hit_queue_.pop(he)) {
            if (he.target_tank_id >= 0 && he.target_tank_id < int(tanks_.size())) {
                Tank& t = tanks_[he.target_tank_id];
                int old = t.hp.fetch_sub(int(he.damage),
                                         std::memory_order_relaxed);
                if (old - int(he.damage) <= 0 && old > 0) {
                    t.alive.store(false);
                    add_explosion(t.x.load(), t.y.load(), 1.5f, 0.6f);
                }
            } else if (he.building_idx >= 0) {
                Building& b = buildings_[he.building_idx];
                int old = b.hp.fetch_sub(int(he.damage),
                                         std::memory_order_relaxed);
                if (old - int(he.damage) <= 0 && old > 0) {
                    b.destroyed.store(true);
                    add_explosion(float(b.x) + b.w / 2.0f,
                                  float(b.y) + b.h / 2.0f, 2.0f, 0.8f);
                }
            }
        }

        end_barrier_->arrive_and_wait();
    }
}

// ============================================================ snapshot
Game::Snapshot Game::snapshot() const {
    Snapshot s;
    s.wind_x = wind_x_.load();

    s.tanks.reserve(tanks_.size());
    for (auto& t : tanks_) {
        Snapshot::TankS ts;
        ts.id = t.id;
        ts.team = t.team;
        ts.is_player = t.is_player;
        ts.x = t.x.load();
        ts.y = t.y.load();
        ts.angle = t.aim_angle.load();
        ts.hp = t.hp.load();
        ts.max_hp = t.max_hp.load();
        ts.alive = t.alive.load();
        s.tanks.push_back(ts);
        if (ts.alive) {
            if (ts.team == Team::Red) ++s.red_alive;
            else ++s.blue_alive;
        }
    }
    s.shells.reserve(shells_.size());
    for (auto& sh : shells_) {
        if (!sh.active.load()) continue;
        Snapshot::ShellS ss;
        ss.x = sh.x.load();
        ss.y = sh.y.load();
        ss.type = sh.type;
        s.shells.push_back(ss);
    }
    s.buildings.reserve(buildings_.size());
    for (auto& b : buildings_) {
        Snapshot::BuildS bs;
        bs.x = b.x; bs.y = b.y; bs.w = b.w; bs.h = b.h;
        bs.destroyed = b.destroyed.load();
        s.buildings.push_back(bs);
    }
    {
        std::lock_guard<std::mutex> lk(expl_mtx_);
        s.explosions.reserve(explosions_.size());
        for (auto& e : explosions_) {
            s.explosions.push_back({e.x, e.y, e.ttl, e.radius});
        }
    }
    s.game_over = (s.red_alive == 0 || s.blue_alive == 0);
    s.player_won = s.game_over && (s.blue_alive == 0) && (s.red_alive > 0);
    return s;
}
