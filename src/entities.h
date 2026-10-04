#pragma once
#include <atomic>
#include <cstdint>

enum class Team : uint8_t { Red = 0, Blue = 1 };
enum class ShellType : uint8_t { AP, HE, HEAT };

struct Tank {
    int   id        = 0;
    Team  team      = Team::Red;
    bool  is_player = false;

    std::atomic<int>   hp{100};
    std::atomic<int>   max_hp{100};
    std::atomic<float> x{0}, y{0};
    std::atomic<float> aim_angle{0};
    std::atomic<float> reload{0};
    std::atomic<bool>  alive{true};

    std::atomic<float> intent_dx{0};
    std::atomic<float> intent_dy{0};
    std::atomic<float> intent_aim{0};
    std::atomic<bool>  intent_fire{false};

    std::atomic<float> player_dx{0};
    std::atomic<float> player_dy{0};
    std::atomic<float> player_aim{0};
    std::atomic<bool>  player_fire{false};

    float reload_time  = 1.5f;
    float shell_speed  = 20.0f;
    float move_speed   = 6.0f;
    ShellType shell_type = ShellType::AP;

    Tank() = default;
    Tank(const Tank&) = delete;
    Tank& operator=(const Tank&) = delete;
    Tank(Tank&& o) noexcept { copy_from(o); }
    Tank& operator=(Tank&& o) noexcept { copy_from(o); return *this; }
    void copy_from(const Tank& o) {
        id = o.id; team = o.team; is_player = o.is_player;
        hp.store(o.hp.load()); max_hp.store(o.max_hp.load());
        x.store(o.x.load()); y.store(o.y.load());
        aim_angle.store(o.aim_angle.load());
        reload.store(o.reload.load());
        alive.store(o.alive.load());
        intent_dx.store(o.intent_dx.load());
        intent_dy.store(o.intent_dy.load());
        intent_aim.store(o.intent_aim.load());
        intent_fire.store(o.intent_fire.load());
        player_dx.store(o.player_dx.load());
        player_dy.store(o.player_dy.load());
        player_aim.store(o.player_aim.load());
        player_fire.store(o.player_fire.load());
        reload_time = o.reload_time;
        shell_speed = o.shell_speed;
        move_speed  = o.move_speed;
        shell_type  = o.shell_type;
    }
};

struct Shell {
    int   id = 0;
    int   owner_id = 0;
    Team  team = Team::Red;
    ShellType type = ShellType::AP;
    std::atomic<float> x{0}, y{0};
    std::atomic<float> vx{0}, vy{0};
    std::atomic<float> life{0};
    std::atomic<bool>  active{false};
    float damage = 25.0f;

    Shell() = default;
    Shell(const Shell&) = delete;
    Shell& operator=(const Shell&) = delete;
    Shell(Shell&& o) noexcept { copy_from(o); }
    Shell& operator=(Shell&& o) noexcept { copy_from(o); return *this; }
    void copy_from(const Shell& o) {
        id = o.id; owner_id = o.owner_id; team = o.team; type = o.type;
        x.store(o.x.load()); y.store(o.y.load());
        vx.store(o.vx.load()); vy.store(o.vy.load());
        life.store(o.life.load());
        active.store(o.active.load());
        damage = o.damage;
    }
};

struct Building {
    int x = 0, y = 0, w = 1, h = 1;
    std::atomic<int> hp{100};
    std::atomic<bool> destroyed{false};

    Building() = default;
    Building(const Building&) = delete;
    Building& operator=(const Building&) = delete;
    Building(Building&& o) noexcept { copy_from(o); }
    Building& operator=(Building&& o) noexcept { copy_from(o); return *this; }
    void copy_from(const Building& o) {
        x = o.x; y = o.y; w = o.w; h = o.h;
        hp.store(o.hp.load());
        destroyed.store(o.destroyed.load());
    }
};

struct Explosion {
    float x, y, ttl, radius;
};

struct HitEvent {
    int   target_tank_id = -1;
    int   building_idx   = -1;
    int   shell_id       = -1;
    float x = 0, y = 0;
    float damage = 0;
    ShellType type = ShellType::AP;
};

struct ShellSpawnEvent {
    int   owner_id = 0;
    Team  team = Team::Red;
    float x = 0, y = 0;
    float vx = 0, vy = 0;
    ShellType type = ShellType::AP;
    float damage = 25.0f;
};
