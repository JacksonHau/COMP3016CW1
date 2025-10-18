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

constexpr float PI = 3.14159265358979323846f;

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

struct WaveConfig {
    int   maxZombies = 20;
    float zombieSpeed = 90.0f;
    float spawnIntervalSec = 1.0f;
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

// ---------- Wave count ----------
struct Glyph5x7 { const char ch; const unsigned char rows[7]; };

static const Glyph5x7 FONT[] = {
    { 'A', {0b01110,0b10001,0b10001,0b11111,0b10001,0b10001,0b10001} },
    { 'E', {0b11111,0b10000,0b10000,0b11110,0b10000,0b10000,0b11111} },
    { 'V', {0b10001,0b10001,0b10001,0b01010,0b01010,0b00100,0b00100} },
    { 'W', {0b10001,0b10001,0b10101,0b10101,0b10101,0b11011,0b10001} },
    { '0', {0b01110,0b10001,0b10011,0b10101,0b11001,0b10001,0b01110} },
    { '1', {0b00100,0b01100,0b00100,0b00100,0b00100,0b00100,0b01110} },
    { '2', {0b01110,0b10001,0b00001,0b00010,0b00100,0b01000,0b11111} },
    { '3', {0b11110,0b00001,0b00001,0b01110,0b00001,0b00001,0b11110} },
    { '4', {0b00010,0b00110,0b01010,0b10010,0b11111,0b00010,0b00010} },
    { '5', {0b11111,0b10000,0b11110,0b00001,0b00001,0b10001,0b01110} },
    { '6', {0b00110,0b01000,0b10000,0b11110,0b10001,0b10001,0b01110} },
    { '7', {0b11111,0b00001,0b00010,0b00100,0b01000,0b01000,0b01000} },
    { '8', {0b01110,0b10001,0b10001,0b01110,0b10001,0b10001,0b01110} },
    { '9', {0b01110,0b10001,0b10001,0b01111,0b00001,0b00010,0b01100} },
    { ' ', {0,0,0,0,0,0,0} },
};
static const Glyph5x7* find_glyph(char c) {
    for (auto& g : FONT) if (g.ch == c) return &g;
    return &FONT[sizeof(FONT) / sizeof(FONT[0]) - 1]; // space
}
static void draw_text(SDL_Renderer* r, float x, float y, const std::string& s, float scale = 2.0f,
    SDL_Color color = { 255,255,255,255 })
{
    SDL_SetRenderDrawColor(r, color.r, color.g, color.b, color.a);
    float cursor = x;
    for (char c : s) {
        const Glyph5x7* g = find_glyph((char)std::toupper((unsigned char)c));
        for (int row = 0; row < 7; ++row) {
            unsigned char bits = g->rows[row];
            for (int col = 0; col < 5; ++col) {
                if (bits & (1 << (4 - col))) {
                    SDL_FRect px{ cursor + col * scale, y + row * scale, scale, scale };
                    SDL_RenderFillRect(r, &px);
                }
            }
        }
        cursor += 6.0f * scale;
    }
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

// ---------- Player with texture ----------
class Player : public Entity {
public:
    Player(const Vec2& p) { pos = p; radius = 14.f; }

    bool load_textures(SDL_Renderer* r) {
        const char* files[8] = {
            "data/assets/Player Right.png",
            "data/assets/Player Down Right.png",
            "data/assets/Player Down.png",
            "data/assets/Player Down Left.png",
            "data/assets/Player Left.png",
            "data/assets/Player Up Left.png",
            "data/assets/Player Up.png",
            "data/assets/Player Up Right.png"
        };
        bool ok = false;
        for (int i = 0; i < 8; i++) {
            tex[i] = IMG_LoadTexture(r, files[i]);
            if (tex[i]) {
                SDL_SetTextureScaleMode(tex[i], SDL_SCALEMODE_NEAREST); // crisp pixels
                ok = true;
            }
        }
        return ok;
    }

    ~Player() override {
        for (auto& t : tex) if (t) SDL_DestroyTexture(t);
    }

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
        SDL_Texture* t = pick_texture();
        if (t) {
            float tw = 0.f, th = 0.f;
            SDL_GetTextureSize(t, &tw, &th);
            const float scale = spriteScale;
            SDL_FRect dst{
                pos.x - (tw * scale) / 2.f,
                pos.y - (th * scale) / 2.f,
                tw * scale,
                th * scale
            };
            SDL_RenderTexture(r, t, nullptr, &dst);
        }
        else {
            SDL_FRect body{ pos.x - radius, pos.y - radius, radius * 2, radius * 2 };
            SDL_SetRenderDrawColor(r, 120, 170, 255, 255);
            SDL_RenderFillRect(r, &body);
        }
    }

    int  hp{ 3 };

private:
    float speed{ 220.f };
    float shootCooldown{ 0.15f };
    float shootTimer{ 0.f };
    Vec2  aimDir{ 1,0 };
    float spriteScale{ 0.06f };
    SDL_Texture* tex[8]{};

    SDL_Texture* pick_texture() const {
        float a = std::atan2(aimDir.y, aimDir.x);
        if (a < 0) a += PI * 2.0f;
        int sector = int(std::floor((a + PI / 8.0f) / (PI / 4.0f))) & 7;
        return tex[sector];
    }
};

// ---------- Game (with Waves) ----------
class Game {
public:
    Game(SDL_Renderer* ren, SDL_Window* win, int w, int h)
        : r(ren), window(win), width(w), height(h), rnd(std::random_device{}()),
        distX(20.f, w - 20.f), distY(20.f, h - 20.f) {

        player = std::make_unique<Player>(Vec2{ w * 0.5f, h * 0.5f });

        cfg = load_wave_config("data/waves.txt");
        baseSpawnInterval = cfg.spawnIntervalSec;
        baseZombieSpeed = cfg.zombieSpeed;

        // Background
        background = IMG_LoadTexture(r, "data/assets/map.png");
        if (!background) background = IMG_LoadTexture(r, "map.png");
        if (background) SDL_SetTextureScaleMode(background, SDL_SCALEMODE_NEAREST);

        // Player textures
        player->load_textures(r);

        // Start wave 1
        start_wave(1);
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

        if (inIntermission) {
            intermissionTimer -= dt;
            if (intermissionTimer <= 0.f) {
                start_wave(currentWave + 1);
            }
        }

        player->update_input(dt, kstate, mx, my);

        // Shooting
        if (queuedShoot && player->can_shoot()) {
            Vec2 dir = (Vec2{ mx,my } - player->pos).normalized();
            bullets.emplace_back(player->pos + dir * 18.f, dir * 520.f);
            player->did_shoot();
        }
        queuedShoot = false;

        // Spawning for current wave
        spawnTimer -= dt;
        if (!inIntermission && pendingToSpawn > 0 && spawnTimer <= 0.f) {
            if ((int)alive_zombies() < simultaneousCap) {
                spawn_zombie();
                pendingToSpawn--;
                spawnedThisWave++;
                spawnTimer = spawnInterval;
            }
            else {
                spawnTimer = 0.15f;
            }
        }

        // Update entities
        player->update(dt);
        clamp_to_arena(*player);

        for (auto& z : zombies) {
            z.steer_to(player->pos);
            z.update(dt);
            clamp_to_arena(z);
        }
        for (auto& b : bullets) b.update(dt);

        // Collisions: bullets vs zombies
        for (auto& z : zombies) {
            for (auto& b : bullets) {
                if (!z.alive || !b.alive) continue;
                if (circle_hit(z.pos, z.radius, b.pos, b.radius)) {
                    z.alive = false;
                    b.alive = false;
                    score += 10;
                    killedThisWave++;
                }
            }
        }
        // Zombies vs player
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

        // Cleanup
        erase_dead(bullets);
        erase_dead(zombies);

        // Wave completion check (all spawned and all killed)
        if (!inIntermission &&
            spawnedThisWave >= totalThisWave &&
            killedThisWave >= totalThisWave &&
            alive_zombies() == 0)
        {
            inIntermission = true;
            intermissionTimer = 3.0f;
        }

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

        // HUD + Wave label
        draw_hud();

        SDL_RenderPresent(r);
    }

private:
    // --- SDL ---
    SDL_Renderer* r{};
    SDL_Window* window{};
    int width{}, height{};
    SDL_Texture* background{};

    // --- Entities ---
    std::unique_ptr<Player> player;
    std::vector<Zombie> zombies;
    std::vector<Bullet> bullets;

    // --- Game state ---
    bool  running{ true };
    float surviveTime{ 0.f };
    int   score{ 0 };

    WaveConfig cfg{};
    float baseZombieSpeed{ 90.f };
    float baseSpawnInterval{ 1.0f };

    // --- Wave system ---
    int   currentWave{ 1 };
    int   totalThisWave{ 0 };
    int   spawnedThisWave{ 0 };
    int   killedThisWave{ 0 };
    int   simultaneousCap{ 6 };
    int   pendingToSpawn{ 0 };
    float zombieSpeed{ 90.f };
    float spawnInterval{ 1.0f };
    float spawnTimer{ 0.f };
    bool  inIntermission{ false };
    float intermissionTimer{ 0.f };

    // --- Input ---
    bool queuedShoot{ false };

    // --- RNG ---
    std::mt19937 rnd;
    std::uniform_real_distribution<float> distX;
    std::uniform_real_distribution<float> distY;

    // --- FX ---
    mutable float gameOverAnim{ 0.f };

    // --- Helpers ---
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

    int alive_zombies() const {
        int n = 0;
        for (auto& z : zombies) if (z.alive) ++n;
        return n;
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

    void start_wave(int wave) {
        currentWave = wave;
        totalThisWave = 8 + (wave - 1) * 5;
        simultaneousCap = 6 + (wave - 1) * 2;
        simultaneousCap = std::min(simultaneousCap, 40);

        spawnInterval = std::max(0.20f, baseSpawnInterval * std::pow(0.92f, (float)(wave - 1)));
        zombieSpeed = baseZombieSpeed * (1.0f + 0.06f * (wave - 1));

        spawnedThisWave = 0;
        killedThisWave = 0;
        pendingToSpawn = totalThisWave;
        spawnTimer = 0.25f;
        inIntermission = false;

        if (window) {
            std::string title = "COMP3016 CW1 - Top-Down Zombies  |  Wave " + std::to_string(currentWave);
            SDL_SetWindowTitle(window, title.c_str());
        }
    }

    void draw_hud() const {
        // Wave label (top-left)
        std::string waveText = "WAVE " + std::to_string(currentWave);
        draw_text(r, 16.f, 10.f, waveText, 2.0f, SDL_Color{ 255, 220, 120, 255 });

        // Health pips just below the label
        for (int i = 0; i < player->hp; i++) {
            SDL_FRect hp{ 16.f + i * 16.f, 28.f, 10.f, 10.f };
            SDL_SetRenderDrawColor(r, 255, 90, 90, 255);
            SDL_RenderFillRect(r, &hp);
        }

        // Wave progress bar (bottom)
        float pct = (totalThisWave > 0) ? (float)killedThisWave / (float)totalThisWave : 0.f;
        float barW = (float)width - 40.f;
        SDL_FRect bg{ 20.f, (float)height - 18.f, barW, 6.f };
        SDL_SetRenderDrawColor(r, 40, 40, 60, 180);
        SDL_RenderFillRect(r, &bg);
        SDL_FRect fg{ 20.f, (float)height - 18.f, barW * std::clamp(pct, 0.f, 1.f), 6.f };
        SDL_SetRenderDrawColor(r, 120, 230, 120, 255);
        SDL_RenderFillRect(r, &fg);

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

    Game game(state.renderer, state.window, width, height);

    bool running = true;
    Uint64 perfFreq = SDL_GetPerformanceFrequency();
    Uint64 prev = SDL_GetPerformanceCounter();

    while (running) {
        Uint64 now = SDL_GetPerformanceCounter();
        float dt = float(now - prev) / float(perfFreq);
        prev = now;
        dt = std::min(dt, 0.033f);

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
