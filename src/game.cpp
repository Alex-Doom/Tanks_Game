#include "game.h"
#include <algorithm>
#include <cmath>
#include <chrono>
#include <QDebug>

using Clock = std::chrono::steady_clock;

static float normalize_angle(float angle) {
    while (angle > 3.14159265f) angle -= 2.0f * 3.14159265f;
    while (angle < -3.14159265f) angle += 2.0f * 3.14159265f;
    return angle;
}

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

void Game::setup(bool multiplayer) {
    is_multiplayer_ = multiplayer;
    tanks_.clear();
    tanks_.reserve(multiplayer ? 2 : (1 + cfg::ENEMIES));
    shells_.resize(512);
    buildings_.reserve(64);

    Tank p1;
    p1.id = 0;
    p1.team = Team::Red;
    p1.is_player = true;
    p1.x.store(5.0f);
    p1.y.store(float(cfg::MAP_H) / 2);
    p1.hp.store(150);
    p1.max_hp.store(150);
    p1.reload_time = 0.7f;
    p1.shell_speed = 24.0f;
    p1.move_speed  = 8.0f;
    p1.aim_angle.store(0.0f);
    p1.shell_type = ShellType::AP;
    tanks_.push_back(std::move(p1));

    if (multiplayer) {
        Tank p2;
        p2.id = 1;
        p2.team = Team::Blue;
        p2.is_player = true;
        p2.x.store(float(cfg::MAP_W) - 5.0f);
        p2.y.store(float(cfg::MAP_H) / 2);
        p2.hp.store(150);
        p2.max_hp.store(150);
        p2.reload_time = 0.7f;
        p2.shell_speed = 24.0f;
        p2.move_speed  = 8.0f;
        p2.aim_angle.store(3.14159f);
        p2.shell_type = ShellType::AP;
        tanks_.push_back(std::move(p2));
    } else {
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
    paused_.store(false);

    int num_threads = 1 + int(tanks_.size()) + cfg::SHELL_POOL_THREADS + 2;
    start_sync_ = std::make_unique<FrameSync>(num_threads);
    end_sync_   = std::make_unique<FrameSync>(num_threads);

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
}

void Game::stop() {
    stop_flag_.store(true);
    if (start_sync_) start_sync_->stop();
    if (end_sync_) end_sync_->stop();

    for (auto& t : threads_) {
        if (t.joinable()) t.join();
    }
    threads_.clear();
}

void Game::set_paused(bool p) {
    paused_.store(p);
}

void Game::set_player_input(int player_idx, float dx, float dy, float aim, bool fire) {
    if (player_idx >= 0 && player_idx < 2) {
        player_inputs_[player_idx].dx = dx;
        player_inputs_[player_idx].dy = dy;
        player_inputs_[player_idx].aim = aim;
        player_inputs_[player_idx].fire = fire;
    }
}

void Game::ai_loop(Tank* tank) {
    while (true) {
        if (!start_sync_->wait()) break; // Корректный выход при stop()

        if (paused_.load()) {
            end_sync_->wait(); // Важно: продолжаем синхронизацию, чтобы не сломать счетчик потоков
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        if (tank->is_player) {
            int p_idx = (tank->id == 0) ? 0 : 1;
            tank->intent_dx.store(player_inputs_[p_idx].dx);
            tank->intent_dy.store(player_inputs_[p_idx].dy);
            tank->intent_aim.store(player_inputs_[p_idx].aim);
            tank->intent_fire.store(player_inputs_[p_idx].fire);
            end_sync_->wait();
            continue;
        }

        if (tank->alive.load()) {
            Tank* target = nullptr;
            float best = 1e9f;
            for (auto& other : tanks_) {
                if (other.team == tank->team || !other.alive.load()) continue;
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
                float move = (dist > 18.0f) ? 1.0f : (dist < 8.0f) ? -0.5f : 0.3f * std::sin(float(tank->id) * 1.7f);
                tank->intent_dx.store(dx * move);
                tank->intent_dy.store(dy * move);
                tank->intent_aim.store(aim);
                float adiff = std::abs(normalize_angle(aim - tank->aim_angle.load()));
                bool fire = tank->reload.load() <= 0.0f && adiff < 0.25f && dist < 35.0f;
                tank->intent_fire.store(fire);
            } else {
                tank->intent_dx.store(0);
                tank->intent_dy.store(0);
                tank->intent_fire.store(false);
            }
        }
        end_sync_->wait();
    }
}

void Game::sim_loop() {
    using namespace std::chrono;
    const auto tick_dur = microseconds(1'000'000 / cfg::TICK_HZ);
    auto next = Clock::now();
    const float dt = 1.0f / cfg::TICK_HZ;

    while (true) {
        next += tick_dur;
        if (!start_sync_->wait()) break;

        if (paused_.load()) {
            end_sync_->wait();
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        integrate_tanks(dt);
        float w = wind_x_.load() + randf(rng_, -0.03f, 0.03f);
        w = std::max(-cfg::WIND_MAX, std::min(cfg::WIND_MAX, w));
        wind_x_.store(w);
        cleanup_explosions(dt);
        sim_tick_.fetch_add(1);
        end_sync_->wait();

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
            if (nx >= b.x && nx < b.x + b.w && ny >= b.y && ny < b.y + b.h) {
                blocked = true; break;
            }
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

void Game::shell_pool_loop() {
    const float dt = 1.0f / cfg::TICK_HZ;
    while (true) {
        if (!start_sync_->wait()) break;
        if (paused_.load()) { end_sync_->wait(); std::this_thread::sleep_for(std::chrono::milliseconds(50)); continue; }

        ShellSpawnEvent sev;
        while (spawn_queue_.pop(sev)) {
            for (auto& s : shells_) {
                if (!s.active.load()) {
                    s.id = shell_next_id_.fetch_add(1);
                    s.owner_id = sev.owner_id;
                    s.team = sev.team;
                    s.type = sev.type;
                    s.x.store(sev.x); s.y.store(sev.y);
                    s.vx.store(sev.vx); s.vy.store(sev.vy);
                    s.life.store(cfg::SHELL_LIFETIME);
                    s.damage = sev.damage;
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
            s.x.store(nx); s.y.store(ny);
            float l = s.life.load() - dt;
            s.life.store(l);
            if (l <= 0) { s.active.store(false); continue; }
            if (nx < 0 || nx >= cfg::MAP_W || ny < 0 || ny >= cfg::MAP_H) {
                HitEvent he;
                he.shell_id = s.id; he.x = nx; he.y = ny; he.damage = 0;
                hit_queue_.push(he);
                s.active.store(false);
            }
        }
        end_sync_->wait();
    }
}

void Game::collision_loop() {
    while (true) {
        if (!start_sync_->wait()) break;
        if (paused_.load()) { end_sync_->wait(); std::this_thread::sleep_for(std::chrono::milliseconds(50)); continue; }

        for (auto& s : shells_) {
            if (!s.active.load()) continue;
            float sx = s.x.load(), sy = s.y.load();

            for (auto& t : tanks_) {
                if (!t.alive.load() || t.id == s.owner_id || t.team == s.team) continue;
                float dx = t.x.load() - sx;
                float dy = t.y.load() - sy;
                if (dx*dx + dy*dy < 0.8f * 0.8f) {
                    HitEvent he;
                    he.target_tank_id = t.id; he.shell_id = s.id;
                    he.x = sx; he.y = sy; he.damage = s.damage; he.type = s.type;
                    hit_queue_.push(he);
                    s.active.store(false);
                    break;
                }
            }
            if (!s.active.load()) continue;

            for (size_t bi = 0; bi < buildings_.size(); ++bi) {
                auto& b = buildings_[bi];
                if (b.destroyed.load()) continue;
                if (sx >= b.x && sx < b.x + b.w && sy >= b.y && sy < b.y + b.h) {
                    HitEvent he;
                    he.building_idx = int(bi); he.shell_id = s.id;
                    he.x = sx; he.y = sy; he.damage = s.damage; he.type = s.type;
                    hit_queue_.push(he);
                    s.active.store(false);
                    break;
                }
            }
        }
        end_sync_->wait();
    }
}

void Game::destruction_loop() {
    while (true) {
        if (!start_sync_->wait()) break;
        if (paused_.load()) { end_sync_->wait(); std::this_thread::sleep_for(std::chrono::milliseconds(50)); continue; }

        HitEvent he;
        while (hit_queue_.pop(he)) {
            if (he.target_tank_id >= 0 && he.target_tank_id < int(tanks_.size())) {
                Tank& t = tanks_[he.target_tank_id];
                int old = t.hp.fetch_sub(int(he.damage), std::memory_order_relaxed);
                if (old - int(he.damage) <= 0 && old > 0) {
                    t.alive.store(false);
                    add_explosion(t.x.load(), t.y.load(), 1.5f, 0.6f);
                }
            } else if (he.building_idx >= 0) {
                Building& b = buildings_[he.building_idx];
                int old = b.hp.fetch_sub(int(he.damage), std::memory_order_relaxed);
                if (old - int(he.damage) <= 0 && old > 0) {
                    b.destroyed.store(true);
                    add_explosion(float(b.x) + b.w / 2.0f, float(b.y) + b.h / 2.0f, 2.0f, 0.8f);
                }
            }
        }
        end_sync_->wait();
    }
}

Game::Snapshot Game::snapshot() const {
    Snapshot s;
    s.wind_x = wind_x_.load();
    s.tanks.reserve(tanks_.size());
    for (auto& t : tanks_) {
        Snapshot::TankS ts;
        ts.id = t.id; ts.team = t.team; ts.is_player = t.is_player;
        ts.x = t.x.load(); ts.y = t.y.load(); ts.angle = t.aim_angle.load();
        ts.hp = t.hp.load(); ts.max_hp = t.max_hp.load(); ts.alive = t.alive.load();
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
        ss.x = sh.x.load(); ss.y = sh.y.load(); ss.type = sh.type;
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
        for (auto& e : explosions_) s.explosions.push_back({e.x, e.y, e.ttl, e.radius});
    }

    s.game_over = (s.red_alive == 0 || s.blue_alive == 0);
    if (is_multiplayer_) {
        s.player_won = (s.red_alive > 0);
    } else {
        s.player_won = s.game_over && (s.blue_alive == 0) && (s.red_alive > 0);
    }

    return s;
}
