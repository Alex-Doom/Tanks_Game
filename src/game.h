#pragma once
#include <atomic>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <random>
#include <deque>
#include <array>
#include <functional>
#include "entities.h"
#include "ring_queue.h"

namespace cfg {
constexpr int MAP_W = 60;
constexpr int MAP_H = 30;
constexpr int ENEMIES = 6;
constexpr int SHELL_POOL_THREADS = 4;
constexpr int TICK_HZ = 60;
constexpr float WIND_MAX = 0.5f;
constexpr float SHELL_LIFETIME = 5.0f;
constexpr float GRAVITY = 9.8f;
constexpr int TILE_SIZE = 20;
}

// Безопасный синхронизатор кадров, который можно прервать без deadlock
class FrameSync {
public:
    explicit FrameSync(int count) : count_(count), expected_(count), generation_(0), stop_(false) {}

    void stop() {
        std::lock_guard<std::mutex> lk(mtx_);
        stop_ = true;
        cv_.notify_all(); // Будим все потоки, чтобы они могли выйти
    }

    bool wait() {
        std::unique_lock<std::mutex> lk(mtx_);
        if (stop_) return false;

        if (--expected_ == 0) {
            expected_ = count_;
            generation_++;
            cv_.notify_all();
            return true;
        }

        int gen = generation_;
        cv_.wait(lk, [this, gen] { return stop_ || generation_ > gen; });
        return !stop_;
    }

private:
    std::mutex mtx_;
    std::condition_variable cv_;
    int count_;
    int expected_;
    int generation_;
    bool stop_;
};

class Game {
public:
    struct PlayerInput {
        float dx = 0;
        float dy = 0;
        float aim = 0;
        bool fire = false;
    };

    struct Snapshot {
        struct TankS {
            int id;
            Team team;
            bool is_player;
            float x, y;
            float angle;
            int hp, max_hp;
            bool alive;
        };
        struct ShellS {
            float x, y;
            ShellType type;
        };
        struct BuildS {
            int x, y, w, h;
            bool destroyed;
        };
        float wind_x = 0;
        std::vector<TankS> tanks;
        std::vector<ShellS> shells;
        std::vector<BuildS> buildings;
        std::vector<Explosion> explosions;
        int red_alive = 0;
        int blue_alive = 0;
        bool game_over = false;
        bool player_won = false;
    };

    Game();
    ~Game();

    void setup(bool multiplayer);
    void start();
    void stop();
    void set_paused(bool p);
    void set_player_input(int player_idx, float dx, float dy, float aim, bool fire);
    Snapshot snapshot() const;

private:
    void ai_loop(Tank* tank);
    void sim_loop();
    void integrate_tanks(float dt);
    void spawn_shell(Tank& tank);
    void add_explosion(float x, float y, float radius, float ttl);
    void cleanup_explosions(float dt);
    void shell_pool_loop();
    void collision_loop();
    void destruction_loop();

    std::vector<Tank> tanks_;
    std::vector<Shell> shells_;
    std::vector<Building> buildings_;
    std::deque<Explosion> explosions_;
    mutable std::mutex expl_mtx_;

    std::atomic<float> wind_x_{0};
    std::atomic<int> sim_tick_{0};
    std::atomic<bool> stop_flag_{false};
    std::atomic<bool> paused_{false};
    bool is_multiplayer_ = false;

    std::unique_ptr<FrameSync> start_sync_;
    std::unique_ptr<FrameSync> end_sync_;
    std::vector<std::thread> threads_;

    std::mt19937 rng_;
    std::atomic<int> shell_next_id_{0};
    std::atomic<int> shell_worker_counter_{0};

    RingQueue<ShellSpawnEvent, 256> spawn_queue_;
    RingQueue<HitEvent, 256> hit_queue_;

    std::array<PlayerInput, 2> player_inputs_;
};
