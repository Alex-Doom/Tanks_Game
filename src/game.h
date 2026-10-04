#pragma once
#include "entities.h"
#include "ring_queue.h"
#include <atomic>
#include <barrier>
#include <functional>
#include <memory>
#include <mutex>
#include <random>
#include <thread>
#include <vector>
#include <deque>

namespace cfg {
    constexpr int   MAP_W              = 60;
    constexpr int   MAP_H              = 30;
    constexpr int   ENEMIES            = 6;
    constexpr int   SHELL_POOL_THREADS = 4;
    constexpr int   TICK_HZ            = 30;
    constexpr float GRAVITY            = 9.81f;
    constexpr float WIND_MAX           = 3.0f;
    constexpr float SHELL_LIFETIME     = 6.0f;
    constexpr float TILE_SIZE          = 24.0f;
}

class Game {
public:
    Game();
    ~Game();

    void setup();
    void start();
    void stop();
    bool is_running() const { return !stop_flag_.load(); }

    void set_player_input(float dx, float dy, float aim, bool fire);

    struct Snapshot {
        struct TankS { int id; Team team; bool is_player;
                       float x, y, angle; int hp, max_hp; bool alive; };
        struct ShellS { float x, y; ShellType type; };
        struct BuildS { int x, y, w, h; bool destroyed; };
        struct ExplS  { float x, y, ttl, radius; };
        std::vector<TankS> tanks;
        std::vector<ShellS> shells;
        std::vector<BuildS> buildings;
        std::vector<ExplS>  explosions;
        float wind_x = 0;
        int   red_alive = 0, blue_alive = 0;
        bool  game_over = false;
        bool  player_won = false;
    };
    Snapshot snapshot() const;

private:
    std::vector<Tank>     tanks_;
    std::vector<Shell>    shells_;
    std::vector<Building> buildings_;
    std::atomic<float>    wind_x_{0};
    std::atomic<int>      sim_tick_{0};

    mutable std::mutex     expl_mtx_;
    std::deque<Explosion>  explosions_;

    std::vector<std::thread> threads_;
    std::atomic<bool>        stop_flag_{false};
    std::unique_ptr<std::barrier<std::function<void()>>> start_barrier_;
    std::unique_ptr<std::barrier<std::function<void()>>> end_barrier_;

    RingQueue<HitEvent, 1024>       hit_queue_;
    RingQueue<ShellSpawnEvent, 256> spawn_queue_;

    std::mt19937      rng_;
    std::atomic<int>  shell_next_id_{1};
    std::atomic<int>  shell_worker_counter_{0};

    void ai_loop(Tank* tank);
    void shell_pool_loop();
    void collision_loop();
    void destruction_loop();
    void sim_loop();

    void spawn_shell(Tank& tank);
    void integrate_tanks(float dt);
    void add_explosion(float x, float y, float radius, float ttl);
    void cleanup_explosions(float dt);

    static float normalize_angle(float a) {
        while (a >  3.14159f) a -= 2 * 3.14159f;
        while (a < -3.14159f) a += 2 * 3.14159f;
        return a;
    }
};
