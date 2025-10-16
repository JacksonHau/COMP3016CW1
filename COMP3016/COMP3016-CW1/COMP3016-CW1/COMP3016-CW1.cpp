#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_image/SDL_image.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <optional>
#include <random>
#include <sstream>
#include <string>
#include <vector>

struct SDLState {
    SDL_Window* window{};
    SDL_Renderer* renderer{};
};

static void cleanup(SDLState& s) {
    if (s.renderer) SDL_DestroyRenderer(s.renderer);
    if (s.window)   SDL_DestroyWindow(s.window);
    SDL_Quit(); 
}

// ---------- Math ----------
struct Vec2 {
    float x{ 0 }, y{ 0 };
    Vec2() = default;
    Vec2(float X, float Y) : x(X), y(Y) {}
    Vec2 operator+(const Vec2& o) const { return { x + o.x, y + o.y }; }
    Vec2 operator-(const Vec2& o) const { return { x - o.x, y - o.y }; }
    Vec2 operator*(float s) const { return { x * s, y * s }; }
    Vec2& operator+=(const Vec2& o) { x += o.x; y += o.y; return *this; }
    float len() const { return std::sqrt(x * x + y * y); }
    Vec2 normalized() const { float L = len(); return (L > 0.0001f) ? Vec2{ x / L,y / L } : Vec2{ 0,0 }; }
};

// ---------- Config loaded from file ----------
struct WaveConfig {
    int   maxZombies = 20;
    float zombieSpeed = 90.0f;   // px/s
    float spawnIntervalSec = 1.0f;    // seconds
};

static WaveConfig load_wave_config(const std::string& path) {
    WaveConfig cfg{};
    std::ifstream f(path);
    if (!f.good()) return cfg; 
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream iss(line);
        std::string k, eq;
        if (!(iss >> k >> eq)) continue;
        if (eq != "=") continue;
        if (k == "maxZombies")          iss >> cfg.maxZombies;
        else if (k == "zombieSpeed")    iss >> cfg.zombieSpeed;
        else if (k == "spawnInterval")  iss >> cfg.spawnIntervalSec;
    }
    return cfg;
}

// ---------- Entities ----------
struct Entity {
    Vec2 pos;
    Vec2 vel;
    float radius{ 12.f };
    bool alive{ true };
    virtual ~Entity() = default;
    virtual void update(float dt) { pos += vel * dt; }
    virtual void draw(SDL_Renderer* r) const = 0;
};

struct Bullet : public Entity {
    float lifetime{ 1.2f };
    float age{ 0.f };
    Bullet(const Vec2& p, const Vec2& v) { pos = p; vel = v; radius = 4.f; }
    void update(float dt) override {
        age += dt;
        if (age >= lifetime) alive = false;
        Entity::update(dt);
    }
    void draw(SDL_Renderer* r) const override {
        SDL_FRect rect{ pos.x - radius, pos.y - radius, radius * 2, radius * 2 };
        SDL_SetRenderDrawColor(r, 255, 230, 110, 255);
        SDL_RenderFillRect(r, &rect);
    }
};

struct Zombie : public Entity {
    float speed{ 80.f };
    Zombie(const Vec2& p, float s) { pos = p; speed = s; radius = 14.f; }
    void steer_to(const Vec2& target) {
        Vec2 dir = (target - pos).normalized();
        vel = dir * speed;
    }
    void draw(SDL_Renderer* r) const override {
        SDL_FRect rect{ pos.x - radius, pos.y - radius, radius * 2, radius * 2 };
        SDL_SetRenderDrawColor(r, 120, 255, 120, 255);
        SDL_RenderFillRect(r, &rect);
        SDL_FRect eye{ pos.x - 3, pos.y - radius / 2.f, 6, 6 };
        SDL_SetRenderDrawColor(r, 20, 40, 20, 255);
        SDL_RenderFillRect(r, &eye);
    }
};

struct Player : public Entity {
    float speed{ 220.f };
    int   hp{ 3 };
    float shootCooldown{ 0.15f };
    float shootTimer{ 0.f };
    Vec2  aimDir{ 1,0 };

    Player(const Vec2& p) { pos = p; radius = 14.f; }

    void update_input(float dt, const bool* kstate, float mx, float my) {
        Vec2 acc{ 0,0 };
        if (kstate[SDL_SCANCODE_W]) acc.y -= 1;
        if (kstate[SDL_SCANCODE_S]) acc.y += 1;
        if (kstate[SDL_SCANCODE_A]) acc.x -= 1;
        if (kstate[SDL_SCANCODE_D]) acc.x += 1;
        acc = acc.normalized() * speed;
        vel = acc;

        Vec2 mouse{ mx, my };
        aimDir = (mouse - pos).normalized();
        shootTimer = std::max(0.f, shootTimer - dt);
    }

    bool can_shoot() const { return shootTimer <= 0.f; }
    void did_shoot() { shootTimer = shootCooldown; }

    void draw(SDL_Renderer* r) const override {
        SDL_FRect body{ pos.x - radius, pos.y - radius, radius * 2, radius * 2 };
        SDL_SetRenderDrawColor(r, 120, 170, 255, 255);
        SDL_RenderFillRect(r, &body);
        SDL_SetRenderDrawColor(r, 230, 230, 230, 255);
        SDL_RenderLine(r, pos.x, pos.y, pos.x + aimDir.x * 22.f, pos.y + aimDir.y * 22.f);
    }
};

// ---------- Game ----------
class Game {
public:
    Game(SDL_Renderer* ren, int w, int h)
        : r(ren), width(w), height(h), rnd(std::random_device{}()),
        distX(20.f, w - 20.f), distY(20.f, h - 20.f) {

        player = std::make_unique<Player>(Vec2{ w * 0.5f, h * 0.5f });

        // Optional config
        cfg = load_wave_config("data/waves.txt");
        spawnInterval = cfg.spawnIntervalSec;
        zombieSpeed = cfg.zombieSpeed;
        maxZombies = cfg.maxZombies;

        // Background
        background = IMG_LoadTexture(r, "data/assets/map.png");
        if (!background) background = IMG_LoadTexture(r, "map.png");
    }

    ~Game() {
        if (background) SDL_DestroyTexture(background);
    }

    void handle_event(const SDL_Event& e) {
        if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT) {
            queuedShoot = true;
        }
    }

    void update(float dt, const bool* kstate, float mx, float my) {
        if (!running) return;

        player->update_input(dt, kstate, mx, my);

        if (queuedShoot && player->can_shoot()) {
            Vec2 dir = player->aimDir;
            bullets.emplace_back(player->pos + dir * 18.f, dir * 520.f);
            player->did_shoot();
        }
        queuedShoot = false;

        // spawning
        spawnTimer -= dt;
        if (spawnTimer <= 0.f && (int)zombies.size() < maxZombies) {
            spawnTimer = spawnInterval;
            spawn_zombie();
        }

        player->update(dt);
        clamp_to_arena(*player);

        for (auto& z : zombies) {
            z.steer_to(player->pos);
            z.update(dt);
            clamp_to_arena(z);
        }
        for (auto& b : bullets) b.update(dt);

        // collisions
        for (auto& z : zombies) {
            for (auto& b : bullets) {
                if (!z.alive || !b.alive) continue;
                if (circle_hit(z.pos, z.radius, b.pos, b.radius)) {
                    z.alive = false;
                    b.alive = false;
                    score += 10;
                }
            }
        }
        for (auto& z : zombies) {
            if (z.alive && circle_hit(z.pos, z.radius, player->pos, player->radius)) {
                z.alive = false;
                player->hp -= 1;
                if (player->hp <= 0) {
                    running = false;
                    gameOverAnim = 2.0f;
                }
            }
        }

        erase_dead(bullets);
        erase_dead(zombies);

        surviveTime += dt;
        if (!running) gameOverAnim = std::max(0.f, gameOverAnim - dt);
    }

    void draw() const {
        // Background
        if (background) {
            SDL_FRect dst{ 0, 0, (float)width, (float)height };
            SDL_RenderTexture(r, background, nullptr, &dst); 
        }
        else {
            SDL_SetRenderDrawColor(r, 18, 14, 22, 255);
            SDL_RenderClear(r);
        }

        // Arena border
        SDL_SetRenderDrawColor(r, 60, 50, 80, 255);
        SDL_FRect border{ 10, 10, (float)width - 20, (float)height - 20 };
        SDL_RenderRect(r, &border);

        // Entities
        player->draw(r);
        for (const auto& z : zombies) z.draw(r);
        for (const auto& b : bullets) b.draw(r);

        // HUD
        draw_hud();

        SDL_RenderPresent(r);
    }

    bool is_running() const { return true; }
    bool is_alive() const { return running; }

private:
    SDL_Renderer* r{};
    int width{}, height{};
    SDL_Texture* background{}; 

    std::unique_ptr<Player> player;
    std::vector<Zombie> zombies;
    std::vector<Bullet> bullets;

    bool  running{ true };
    float surviveTime{ 0.f };
    int   score{ 0 };

    WaveConfig cfg{};
    int   maxZombies{ 20 };
    float zombieSpeed{ 90.f };
    float spawnInterval{ 1.0f };
    float spawnTimer{ 0.2f };

    bool queuedShoot{ false };

    std::mt19937 rnd;
    std::uniform_real_distribution<float> distX;
    std::uniform_real_distribution<float> distY;

    mutable float gameOverAnim{ 0.f };

    static bool circle_hit(const Vec2& a, float ar, const Vec2& b, float br) {
        float dx = a.x - b.x;
        float dy = a.y - b.y;
        float rr = (ar + br); rr *= rr;
        return dx * dx + dy * dy <= rr;
    }

    template<typename T>
    static void erase_dead(std::vector<T>& v) {
        v.erase(std::remove_if(v.begin(), v.end(), [](const T& e) { return !e.alive; }), v.end());
    }

    void clamp_to_arena(Entity& e) const {
        float minX = 20.f, minY = 20.f;
        float maxX = (float)width - 20.f, maxY = (float)height - 20.f;
        e.pos.x = std::clamp(e.pos.x, minX, maxX);
        e.pos.y = std::clamp(e.pos.y, minY, maxY);
    }

    void spawn_zombie() {
        int side = std::uniform_int_distribution<int>(0, 3)(rnd);
        float x = 0, y = 0;
        if (side == 0) { x = distX(rnd); y = 18.f; }
        if (side == 1) { x = distX(rnd); y = height - 18.f; }
        if (side == 2) { x = 18.f; y = distY(rnd); }
        if (side == 3) { x = width - 18.f; y = distY(rnd); }
        zombies.emplace_back(Vec2{ x,y }, zombieSpeed);
    }

    void draw_hud() const {
        for (int i = 0; i < player->hp; i++) {
            SDL_FRect hp{ 16.f + i * 16.f, 16.f, 10.f, 10.f };
            SDL_SetRenderDrawColor(r, 255, 90, 90, 255);
            SDL_RenderFillRect(r, &hp);
        }
        if (!running && gameOverAnim > 0.f) {
            Uint8 alpha = (Uint8)std::clamp(gameOverAnim / 2.f * 200.f, 0.f, 200.f);
            SDL_SetRenderDrawColor(r, 220, 40, 40, alpha);
            SDL_FRect f{ 0,0,(float)width,(float)height };
            SDL_RenderFillRect(r, &f);
        }
    }
};

// ---------- Main ----------
int main(int argc, char* argv[])
{
    (void)argc; (void)argv;

    SDLState state{};
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Error initialising SDL3", nullptr);
        return 1;
    }

    const int width = 960, height = 540;
    state.window = SDL_CreateWindow("COMP3016 CW1 - Top-Down Zombies", width, height, 0);
    if (!state.window) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Error creating window", nullptr);
        cleanup(state);
        return 1;
    }

    state.renderer = SDL_CreateRenderer(state.window, nullptr);
    if (!state.renderer) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Error creating renderer", state.window);
        cleanup(state);
        return 1;
    }

    Game game(state.renderer, width, height);

    bool running = true;
    Uint64 perfFreq = SDL_GetPerformanceFrequency();
    Uint64 prev = SDL_GetPerformanceCounter();

    while (running) {
        Uint64 now = SDL_GetPerformanceCounter();
        float dt = float(now - prev) / float(perfFreq);
        prev = now;
        dt = std::min(dt, 0.033f); // cap big spikes

        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) { running = false; break; }
            if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE) { running = false; break; }
            game.handle_event(e);
        }

        float mx = 0.f, my = 0.f;
        (void)SDL_GetMouseState(&mx, &my);

        const bool* kstate = SDL_GetKeyboardState(nullptr);

        game.update(dt, kstate, mx, my);
        game.draw();

        SDL_Delay(1);
    }

    cleanup(state);
    return 0;
}
