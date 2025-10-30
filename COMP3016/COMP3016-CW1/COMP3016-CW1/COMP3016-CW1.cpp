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

static SDL_Texture* load_any(SDL_Renderer* r,
    const char* p1,
    const char* p2 = nullptr,
    const char* p3 = nullptr)
{
    SDL_Texture* t = nullptr;
    if (!t && p1) t = IMG_LoadTexture(r, p1);
    if (!t && p2) t = IMG_LoadTexture(r, p2);
    if (!t && p3) t = IMG_LoadTexture(r, p3);
    if (t) SDL_SetTextureScaleMode(t, SDL_SCALEMODE_NEAREST); 
    return t;
}

// SDL helpers
struct SDLState {
    SDL_Window* window{};
    SDL_Renderer* renderer{};
};

static void cleanup(SDLState& s) {
    if (s.renderer) SDL_DestroyRenderer(s.renderer);
    if (s.window)   SDL_DestroyWindow(s.window);
    SDL_Quit();
}

// math
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

struct Glyph5x7 { const char ch; const unsigned char rows[7]; };
static const Glyph5x7 FONT[] = {
    { 'A',{0b01110,0b10001,0b10001,0b11111,0b10001,0b10001,0b10001} },
    { 'E',{0b11111,0b10000,0b10000,0b11110,0b10000,0b10000,0b11111} },
    { 'F',{0b11111,0b10000,0b10000,0b11110,0b10000,0b10000,0b10000} },
    { 'I',{0b11111,0b00100,0b00100,0b00100,0b00100,0b00100,0b11111} },
    { 'L',{0b10000,0b10000,0b10000,0b10000,0b10000,0b10000,0b11111} },
    { 'N',{0b10001,0b11001,0b10101,0b10011,0b10001,0b10001,0b10001} },
    { 'O',{0b01110,0b10001,0b10001,0b10001,0b10001,0b10001,0b01110} },
    { 'P',{0b11110,0b10001,0b10001,0b11110,0b10000,0b10000,0b10000} },
    { 'R',{0b11110,0b10001,0b10001,0b11110,0b10100,0b10010,0b10001} },
    { 'S',{0b01111,0b10000,0b10000,0b01110,0b00001,0b00001,0b11110} },
    { 'T',{0b11111,0b00100,0b00100,0b00100,0b00100,0b00100,0b00100} },
    { 'U',{0b10001,0b10001,0b10001,0b10001,0b10001,0b10001,0b01110} },
    { 'V',{0b10001,0b10001,0b10001,0b01010,0b01010,0b00100,0b00100} },
    { 'W',{0b10001,0b10001,0b10101,0b10101,0b10101,0b11011,0b10001} },
    { '0',{0b01110,0b10001,0b10011,0b10101,0b11001,0b10001,0b01110} },
    { '1',{0b00100,0b01100,0b00100,0b00100,0b00100,0b00100,0b01110} },
    { '2',{0b01110,0b10001,0b00001,0b00010,0b00100,0b01000,0b11111} },
    { '3',{0b11110,0b00001,0b00001,0b01110,0b00001,0b00001,0b11110} },
    { '4',{0b00010,0b00110,0b01010,0b10010,0b11111,0b00010,0b00010} },
    { '5',{0b11111,0b10000,0b11110,0b00001,0b00001,0b10001,0b01110} },
    { '6',{0b00110,0b01000,0b10000,0b11110,0b10001,0b10001,0b01110} },
    { '7',{0b11111,0b00001,0b00010,0b00100,0b01000,0b01000,0b01000} },
    { '8',{0b01110,0b10001,0b10001,0b01110,0b10001,0b10001,0b01110} },
    { '9',{0b01110,0b10001,0b10001,0b01111,0b00001,0b00010,0b01100} },
    { ' ',{0,0,0,0,0,0,0} },
    { '-',{0,0,0b11111,0,0,0,0} },
};
static const Glyph5x7* find_glyph(char c) {
    for (auto& g : FONT) if (g.ch == c) return &g;
    return &FONT[sizeof(FONT) / sizeof(FONT[0]) - 2]; 
}
static void draw_text(SDL_Renderer* r, float x, float y, const std::string& s,
    float scale = 2.0f, SDL_Color color = { 255,255,255,255 })
{
    SDL_SetRenderDrawColor(r, color.r, color.g, color.b, color.a);
    float cx = x;
    for (char c : s) {
        const Glyph5x7* g = find_glyph((char)std::toupper((unsigned char)c));
        for (int row = 0; row < 7; ++row) {
            unsigned char bits = g->rows[row];
            for (int col = 0; col < 5; ++col) {
                if (bits & (1 << (4 - col))) {
                    SDL_FRect px{ cx + col * scale, y + row * scale, scale, scale };
                    SDL_RenderFillRect(r, &px);
                }
            }
        }
        cx += 6.0f * scale;
    }
}

// entities
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
    Bullet(const Vec2& p, const Vec2& v, float life = 1.2f, float rad = 4.f) {
        pos = p; vel = v; radius = rad; lifetime = life;
    }
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

    SDL_Texture* tex[8]{};
    float spriteScale{ 0.06f };
    Vec2 faceDir{ 1,0 };

    Zombie(const Vec2& p, float s) { pos = p; speed = s; radius = 14.f; }

    bool load_textures(SDL_Renderer* r) {
        const char* pf[8][3] = {
            {"data/Zombie Right.png",      "data/assets/Zombie Right.png",      "Zombie Right.png"},
            {"data/Zombie Down Right.png", "data/assets/Zombie Down Right.png", "Zombie Down Right.png"},
            {"data/Zombie Down.png",       "data/assets/Zombie Down.png",       "Zombie Down.png"},
            {"data/Zombie Down Left.png",  "data/assets/Zombie Down Left.png",  "Zombie Down Left.png"},
            {"data/Zombie Left.png",       "data/assets/Zombie Left.png",       "Zombie Left.png"},
            {"data/Zombie Up Left.png",    "data/assets/Zombie Up Left.png",    "Zombie Up Left.png"},
            {"data/Zombie Up.png",         "data/assets/Zombie Up.png",         "Zombie Up.png"},
            {"data/Zombie Up Right.png",   "data/assets/Zombie Up Right.png",   "Zombie Up Right.png"}
        };
        bool ok = false;
        for (int i = 0; i < 8; i++) {
            tex[i] = load_any(r, pf[i][0], pf[i][1], pf[i][2]);
            if (tex[i]) { SDL_SetTextureScaleMode(tex[i], SDL_SCALEMODE_NEAREST); ok = true; }
        }
        return ok;
    }

    void steer_to(const Vec2& target) {
        Vec2 dir = (target - pos).normalized();
        vel = dir * speed;
        if (dir.len() > 0.0001f) faceDir = dir;
    }

    SDL_Texture* pick_texture() const {
        float a = std::atan2(faceDir.y, faceDir.x); if (a < 0) a += PI * 2.f;
        int sector = int(std::floor((a + PI / 8.0f) / (PI / 4.0f))) & 7;
        return tex[sector];
    }

    void update(float dt) override {
        pos += vel * dt;
    }

    void draw(SDL_Renderer* r) const override {
        if (SDL_Texture* t = pick_texture()) {
            float tw = 0.f, th = 0.f; SDL_GetTextureSize(t, &tw, &th);
            float s = spriteScale;
            SDL_FRect dst{ pos.x - (tw * s) / 2.f, pos.y - (th * s) / 2.f, tw * s, th * s };
            SDL_RenderTexture(r, t, nullptr, &dst);
        }
        else {
            SDL_FRect rect{ pos.x - radius, pos.y - radius, radius * 2, radius * 2 };
            SDL_SetRenderDrawColor(r, 120, 255, 120, 255);
            SDL_RenderFillRect(r, &rect);
        }
    }
};

// Player + Weapons
struct Weapon {
    std::string name;
    SDL_Texture* sprite{};
    float fireRate{ 6.f };
    float bulletSpeed{ 520.f };
    float bulletLife{ 1.0f };
    float spreadDeg{ 0.f };
    int   pellets{ 1 };
    int   ammo{ -1 };
};

class Player : public Entity {
public:
    Player(const Vec2& p) { pos = p; radius = 14.f; }

    bool load_textures(SDL_Renderer* r) {
        const char* pf[8][3] = {
            {"data/Player Right.png",      "data/assets/Player Right.png",      "Player Right.png"},
            {"data/Player Down Right.png", "data/assets/Player Down Right.png", "Player Down Right.png"},
            {"data/Player Down.png",       "data/assets/Player Down.png",       "Player Down.png"},
            {"data/Player Down Left.png",  "data/assets/Player Down Left.png",  "Player Down Left.png"},
            {"data/Player Left.png",       "data/assets/Player Left.png",       "Player Left.png"},
            {"data/Player Up Left.png",    "data/assets/Player Up Left.png",    "Player Up Left.png"},
            {"data/Player Up.png",         "data/assets/Player Up.png",         "Player Up.png"},
            {"data/Player Up Right.png",   "data/assets/Player Up Right.png",   "Player Up Right.png"}
        };
        bool ok = false;
        for (int i = 0; i < 8; i++) {
            tex[i] = load_any(r, pf[i][0], pf[i][1], pf[i][2]);
            if (tex[i]) ok = true;
        }

        pistol.name = "PST"; pistol.sprite = load_any(r, "data/Pistol.png", "data/assets/Pistol.png", "Pistol.png");
        shotgun.name = "SG";  shotgun.sprite = load_any(r, "data/Shotgun.png", "data/assets/Shotgun.png", "Shotgun.png");
        rifle.name = "RF";  rifle.sprite = load_any(r, "data/Rifle.png", "data/assets/Rifle.png", "Rifle.png");
        return ok;
    }

    void setup_weapons() {
        pistol.fireRate = 7.0f; pistol.bulletSpeed = 620.f; pistol.bulletLife = 0.9f; pistol.spreadDeg = 4.f;  pistol.pellets = 1; pistol.ammo = -1; // ∞
        shotgun.fireRate = 1.2f; shotgun.bulletSpeed = 520.f; shotgun.bulletLife = 0.7f; shotgun.spreadDeg = 22.f; shotgun.pellets = 6; shotgun.ammo = 24;
        rifle.fireRate = 10.0f; rifle.bulletSpeed = 780.f; rifle.bulletLife = 1.0f; rifle.spreadDeg = 2.0f;  rifle.pellets = 1; rifle.ammo = 90;
        select = 0;
    }

    void update_input(float dt, const bool* kstate, float mx, float my) {
        Vec2 acc{ 0,0 };
        if (kstate[SDL_SCANCODE_W]) acc.y -= 1;
        if (kstate[SDL_SCANCODE_S]) acc.y += 1;
        if (kstate[SDL_SCANCODE_A]) acc.x -= 1;
        if (kstate[SDL_SCANCODE_D]) acc.x += 1;
        acc = acc.normalized() * speed;
        vel = acc;

        Vec2 mouse{ mx,my };
        aimDir = (mouse - pos).normalized();
        shootTimer = std::max(0.f, shootTimer - dt);
    }

    int try_shoot(std::vector<Bullet>& out, std::mt19937& rng) {
        const Weapon& w = current();
        if (shootTimer > 0.f) return 0;
        if (w.ammo == 0) return 0;

        shootTimer = 1.0f / w.fireRate;

        if (select == 1 && shotgunAmmo > 0) shotgunAmmo--;
        if (select == 2 && rifleAmmo > 0) rifleAmmo--;

        std::uniform_real_distribution<float> jitter(-w.spreadDeg, w.spreadDeg);
        int emitted = 0;
        for (int i = 0; i < w.pellets; i++) {
            float ang = std::atan2(aimDir.y, aimDir.x) + (jitter(rng) * (PI / 180.f));
            Vec2 dir{ std::cos(ang), std::sin(ang) };
            out.emplace_back(pos + dir * 18.f, dir * w.bulletSpeed, w.bulletLife, 4.f);
            emitted++;
        }
        return emitted;
    }

    // weapon switching 
    void set_weapon(int idx) { select = std::clamp(idx, 0, 2); }

    // ammo counts for HUD
    int pistolAmmo{ -1 }; 
    int shotgunAmmo{ 24 };
    int rifleAmmo{ 90 };

    int current_ammo() const {
        if (select == 1) return shotgunAmmo;
        if (select == 2) return rifleAmmo;
        return pistolAmmo;
    }

    const Weapon& current() const {
        if (select == 1) return shotgun;
        if (select == 2) return rifle;
        return pistol;
    }

    // draw player + gun
    void draw(SDL_Renderer* r) const override {
        SDL_Texture* t = pick_texture();
        if (t) {
            float tw = 0.f, th = 0.f; SDL_GetTextureSize(t, &tw, &th);
            float s = spriteScale;
            SDL_FRect dst{ pos.x - (tw * s) / 2.f, pos.y - (th * s) / 2.f, tw * s, th * s };
            SDL_RenderTexture(r, t, nullptr, &dst);
        }
        else {
            SDL_FRect rect{ pos.x - radius, pos.y - radius, radius * 2, radius * 2 };
            SDL_SetRenderDrawColor(r, 120, 170, 255, 255); SDL_RenderFillRect(r, &rect);
        }

        // gun
        SDL_Texture* gun = current().sprite;
        if (gun) {
            float gw = 0.f, gh = 0.f; SDL_GetTextureSize(gun, &gw, &gh);
            const float TARGET_H = 22.f;
            float s = TARGET_H / gh;

            SDL_FRect gd{
                pos.x - (gw * s) / 2.f + aimDir.x * 8.f,
                pos.y - (gh * s) / 2.f + aimDir.y * 8.f,
                gw * s, gh * s
            };

            float angle = std::atan2(aimDir.y, aimDir.x) * 180.0f / PI;
            SDL_FPoint center{ gd.w / 2.f, gd.h / 2.f };
            SDL_RenderTextureRotated(r, gun, nullptr, &gd, angle, &center, SDL_FLIP_NONE);
        }
    }

    int  hp{ 3 };

private:
    float speed{ 220.f };
    float shootTimer{ 0.f };
    Vec2  aimDir{ 1,0 };
    float spriteScale{ 0.06f };
    SDL_Texture* tex[8]{};

    // weapons
    Weapon pistol, shotgun, rifle;
    int select{ 0 }; // active weapon

    SDL_Texture* pick_texture() const {
        float a = std::atan2(aimDir.y, aimDir.x); if (a < 0) a += PI * 2.f;
        int sector = int(std::floor((a + PI / 8.0f) / (PI / 4.0f))) & 7;
        return tex[sector];
    }
};

// Game (waves + weapons)
class Game {
public:
    Game(SDL_Renderer* ren, SDL_Window* win, int w, int h)
        : r(ren), window(win), width(w), height(h), rnd(std::random_device{}()),
        distX(20.f, w - 20.f), distY(20.f, h - 20.f)
    {
        player = std::make_unique<Player>(Vec2{ w * 0.5f, h * 0.5f });

        cfg = load_wave_config("data/waves.txt");
        baseSpawnInterval = cfg.spawnIntervalSec;
        baseZombieSpeed = cfg.zombieSpeed;

        background = load_any(r, "data/map.png", "data/assets/map.png", "map.png");

        player->load_textures(r);
        player->setup_weapons();

        start_wave(1);
    }

    ~Game() { if (background) SDL_DestroyTexture(background); }

    void handle_event(const SDL_Event& e) {
        if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT) queuedShoot = true;
        if (e.type == SDL_EVENT_KEY_DOWN) {
            if (e.key.key == SDLK_1) player->set_weapon(0);
            if (e.key.key == SDLK_2) player->set_weapon(1);
            if (e.key.key == SDLK_3) player->set_weapon(2);
        }
    }

    void update(float dt, const bool* kstate, float mx, float my) {
        if (!running) return;

        damageCooldown = std::max(0.f, damageCooldown - dt);

        if (inIntermission) {
            intermissionTimer -= dt;
            if (intermissionTimer <= 0.f) start_wave(currentWave + 1);
        }

        player->update_input(dt, kstate, mx, my);

        if (queuedShoot) {
            queuedShoot = false;
            (void)player->try_shoot(bullets, rnd);
        }

        spawnTimer -= dt;
        if (!inIntermission && pendingToSpawn > 0 && spawnTimer <= 0.f) {
            if (alive_zombies() < simultaneousCap) {
                spawn_zombie(); pendingToSpawn--; spawnedThisWave++;
                spawnTimer = spawnInterval;
            }
            else spawnTimer = 0.15f;
        }

        player->update(dt);
        clamp_to_arena(*player);

        for (auto& z : zombies) { z.steer_to(player->pos); z.update(dt); clamp_to_arena(z); }
        for (auto& b : bullets) { b.update(dt); }

        for (auto& z : zombies) {
            for (auto& b : bullets) {
                if (!z.alive || !b.alive) continue;
                if (circle_hit(z.pos, z.radius, b.pos, b.radius)) {
                    z.alive = false; b.alive = false; score += 10; killedThisWave++;
                }
            }
        }

        for (auto& z : zombies) {
            if (z.alive && circle_hit(z.pos, z.radius, player->pos, player->radius)) {
                if (damageCooldown <= 0.f) {
                    player->hp -= 1;
                    damageCooldown = 0.6f; // 600 ms i-frames
                    if (player->hp <= 0) { running = false; gameOverAnim = 2.0f; }
                }
                Vec2 away = (z.pos - player->pos).normalized();
                z.pos += away * 6.f;
            }
        }

        erase_dead(bullets);
        erase_dead(zombies);

        if (!inIntermission &&
            spawnedThisWave >= totalThisWave &&
            killedThisWave >= totalThisWave &&
            alive_zombies() == 0)
        {
            inIntermission = true; intermissionTimer = 3.0f;
        }

        surviveTime += dt;
        if (!running) gameOverAnim = std::max(0.f, gameOverAnim - dt);
    }

    void draw() const {
        if (background) {
            SDL_FRect dst{ 0,0,(float)width,(float)height };
            SDL_RenderTexture(r, background, nullptr, &dst);
        }
        else {
            SDL_SetRenderDrawColor(r, 18, 14, 22, 255); SDL_RenderClear(r);
        }

        SDL_SetRenderDrawColor(r, 60, 50, 80, 255);
        SDL_FRect border{ 10,10,(float)width - 20,(float)height - 20 }; SDL_RenderRect(r, &border);

        player->draw(r);
        for (const auto& z : zombies) z.draw(r);
        for (const auto& b : bullets) b.draw(r);

        draw_hud();

        SDL_RenderPresent(r);
    }

private:
    // SDL
    SDL_Renderer* r{};
    SDL_Window* window{};
    int width{}, height{};
    SDL_Texture* background{};

    // entities
    std::unique_ptr<Player> player;
    std::vector<Zombie> zombies;
    std::vector<Bullet> bullets;

    // state
    bool  running{ true };
    float surviveTime{ 0.f };
    int   score{ 0 };

    // base tuning
    WaveConfig cfg{};
    float baseZombieSpeed{ 90.f };
    float baseSpawnInterval{ 1.0f };

    // waves
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

    // input
    bool queuedShoot{ false };

    // rng
    std::mt19937 rnd;
    std::uniform_real_distribution<float> distX;
    std::uniform_real_distribution<float> distY;

    // player hit cooldown
    float damageCooldown{ 0.f };

    // fx
    mutable float gameOverAnim{ 0.f };

    // helpers
    static bool circle_hit(const Vec2& a, float ar, const Vec2& b, float br) {
        float dx = a.x - b.x, dy = a.y - b.y; float rr = (ar + br); rr *= rr;
        return dx * dx + dy * dy <= rr;
    }
    template<typename T>
    static void erase_dead(std::vector<T>& v) {
        v.erase(std::remove_if(v.begin(), v.end(), [](const T& e) { return !e.alive; }), v.end());
    }
    int alive_zombies() const { int n = 0; for (auto& z : zombies) if (z.alive) ++n; return n; }

    void clamp_to_arena(Entity& e) const {
        float minX = 20.f, minY = 20.f, maxX = (float)width - 20.f, maxY = (float)height - 20.f;
        e.pos.x = std::clamp(e.pos.x, minX, maxX);
        e.pos.y = std::clamp(e.pos.y, minY, maxY);
    }

    void spawn_zombie() 
    {
        int side = std::uniform_int_distribution<int>(0, 3)(rnd);
        float x = 0, y = 0;
        if (side == 0) { x = distX(rnd); y = 18.f; }
        if (side == 1) { x = distX(rnd); y = height - 18.f; }
        if (side == 2) { x = 18.f;       y = distY(rnd); }
        if (side == 3) { x = width - 18.f; y = distY(rnd); }
        zombies.emplace_back(Vec2{ x,y }, zombieSpeed);

        zombies.back().load_textures(r);

        zombies.back().spriteScale = 0.06f;
    }


    void start_wave(int wave) {
        currentWave = wave;
        totalThisWave = 8 + (wave - 1) * 5;
        simultaneousCap = std::min(6 + (wave - 1) * 2, 40);
        spawnInterval = std::max(0.20f, baseSpawnInterval * std::pow(0.92f, float(wave - 1)));
        zombieSpeed = baseZombieSpeed * (1.0f + 0.06f * float(wave - 1));

        spawnedThisWave = 0;
        killedThisWave = 0;
        pendingToSpawn = totalThisWave;
        spawnTimer = 0.25f;
        inIntermission = false;

        if (window) {
            std::string t = "COMP3016 CW1 - Top-Down Zombies  |  Wave " + std::to_string(currentWave);
            SDL_SetWindowTitle(window, t.c_str());
        }
    }

    void draw_hud() const {
        // Wave
        draw_text(r, 16.f, 10.f, "WAVE " + std::to_string(currentWave), 2.0f, SDL_Color{ 255,220,120,255 });

        // Health
        for (int i = 0; i < player->hp; i++) {
            SDL_FRect hp{ 16.f + i * 16.f, 28.f, 10.f, 10.f };
            SDL_SetRenderDrawColor(r, 255, 90, 90, 255);
            SDL_RenderFillRect(r, &hp);
        }

        // Ammo (current weapon)
        const Weapon& w = player->current();
        int ammo = player->current_ammo();
        std::string ammoText = w.name + std::string(" ") + (ammo < 0 ? "INF" : std::to_string(ammo));
        draw_text(r, 16.f, 44.f, ammoText, 2.0f, SDL_Color{ 190,240,255,255 });

        // Wave progress bar (bottom)
        float pct = (totalThisWave > 0) ? (float)killedThisWave / (float)totalThisWave : 0.f;
        float barW = (float)width - 40.f;
        SDL_FRect bg{ 20.f, (float)height - 18.f, barW, 6.f };
        SDL_SetRenderDrawColor(r, 40, 40, 60, 180); SDL_RenderFillRect(r, &bg);
        SDL_FRect fg{ 20.f, (float)height - 18.f, barW * std::clamp(pct,0.f,1.f), 6.f };
        SDL_SetRenderDrawColor(r, 120, 230, 120, 255); SDL_RenderFillRect(r, &fg);

        if (!running && gameOverAnim > 0.f) {
            Uint8 a = (Uint8)std::clamp(gameOverAnim / 2.f * 200.f, 0.f, 200.f);
            SDL_SetRenderDrawColor(r, 220, 40, 40, a);
            SDL_FRect f{ 0,0,(float)width,(float)height }; SDL_RenderFillRect(r, &f);
        }
    }
};

// main
int main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    SDLState state{};
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Error initialising SDL3", nullptr);
        return 1;
    }

    const int width = 960, height = 540;
    state.window = SDL_CreateWindow("COMP3016 CW1 - Top-Down Zombies", width, height, 0);
    if (!state.window) { SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Error creating window", nullptr); cleanup(state); return 1; }
    state.renderer = SDL_CreateRenderer(state.window, nullptr);
    if (!state.renderer) { SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Error creating renderer", state.window); cleanup(state); return 1; }

    Game game(state.renderer, state.window, width, height);

    bool running = true;
    Uint64 freq = SDL_GetPerformanceFrequency(), prev = SDL_GetPerformanceCounter();
    while (running) {
        Uint64 now = SDL_GetPerformanceCounter();
        float dt = float(now - prev) / float(freq);
        prev = now;
        dt = std::min(dt, 0.033f);

        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) { running = false; break; }
            if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE) { running = false; break; }
            game.handle_event(e);
        }

        float mx = 0.f, my = 0.f; SDL_GetMouseState(&mx, &my);
        const bool* kstate = SDL_GetKeyboardState(nullptr);

        game.update(dt, kstate, mx, my);
        game.draw();
        SDL_Delay(1);
    }

    cleanup(state);
    return 0;
}
