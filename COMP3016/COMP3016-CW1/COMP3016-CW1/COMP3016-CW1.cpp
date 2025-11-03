#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3/SDL_audio.h>

#include <algorithm>
#include <cmath>
#include <cctype>
#include <memory>
#include <random>
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
    if (t) {
        SDL_SetTextureScaleMode(t, SDL_SCALEMODE_NEAREST);
        SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
    }
    return t;
}

// MUSIC (SDL3 stream API)
class MusicPlayer {
public:
    bool init_and_load()
    {
        if (!load_wav_any("data/sounds/Menu Music.wav", menu)) {
            SDL_Log("Menu music not found");
        }
        if (!load_wav_any("data/sounds/Background Music.wav", game)) {
            SDL_Log("Game music not found");
        }

        if (!menu.buf && !game.buf) return true; 

        const SDL_AudioSpec* want = menu.buf ? &menu.spec : &game.spec;
        stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, want,
            /*callback*/nullptr, /*userdata*/nullptr);
        if (!stream) {
            SDL_Log("SDL_OpenAudioDeviceStream failed: %s", SDL_GetError());
            return false;
        }
        SDL_ResumeAudioStreamDevice(stream);
        return true;
    }

    void shutdown()
    {
        if (stream) {
            SDL_DestroyAudioStream(stream);
            stream = nullptr;
        }
        if (menu.buf) SDL_free(menu.buf);
        if (game.buf) SDL_free(game.buf);
        menu = {}; game = {}; current = Which::None;
    }

    void play_menu() { switch_track(menu, Which::Menu); }
    void play_game() { switch_track(game, Which::Game); }
    void stop()
    {
        current = Which::None;
        if (stream) SDL_ClearAudioStream(stream);
    }

    void update()
    {
        if (!stream || current == Which::None) return;
        const Track& t = (current == Which::Menu ? menu : game);
        if (!t.buf || t.len == 0) return;

        const int avail = SDL_GetAudioStreamAvailable(stream);
        if (avail < int(t.len / 2)) {
            SDL_PutAudioStreamData(stream, t.buf, (int)t.len);
        }
    }

private:
    struct Track {
        SDL_AudioSpec spec{};
        Uint8* buf{ nullptr };
        Uint32 len{ 0 };
    };
    enum class Which { None, Menu, Game };

    SDL_AudioStream* stream{ nullptr };
    Track menu{}, game{};
    Which current{ Which::None };

    static bool load_wav_any(const char* path, Track& out)
    {
        if (!SDL_LoadWAV(path, &out.spec, &out.buf, &out.len)) return false;
        return true;
    }

    void switch_track(const Track& t, Which w)
    {
        if (!stream) return;
        SDL_ClearAudioStream(stream);
        if (t.buf && t.len) SDL_PutAudioStreamData(stream, t.buf, (int)t.len);
        current = w;
    }
};

// math, text, entities
struct Vec2 {
    float x{ 0 }, y{ 0 };
    Vec2() = default;
    Vec2(float X, float Y) :x(X), y(Y) {}
    Vec2 operator+(const Vec2& o) const { return { x + o.x,y + o.y }; }
    Vec2 operator-(const Vec2& o) const { return { x - o.x,y - o.y }; }
    Vec2 operator*(float s) const { return { x * s,y * s }; }
    Vec2& operator+=(const Vec2& o) { x += o.x; y += o.y; return *this; }
    float len() const { return std::sqrt(x * x + y * y); }
    Vec2 normalized() const { float L = len(); return L > 0.0001f ? Vec2{ x / L,y / L } : Vec2{ 0,0 }; }
};

struct Glyph5x7 { const char ch; const unsigned char rows[7]; };
static const Glyph5x7 FONT[] = {
    { 'A',{0b01110,0b10001,0b10001,0b11111,0b10001,0b10001,0b10001} },
    { 'C',{0b01110,0b10001,0b10000,0b10000,0b10000,0b10001,0b01110} },
    { 'D',{0b11110,0b10001,0b10001,0b10001,0b10001,0b10001,0b11110} },
    { 'E',{0b11111,0b10000,0b10000,0b11110,0b10000,0b10000,0b11111} },
    { 'F',{0b11111,0b10000,0b10000,0b11110,0b10000,0b10000,0b10000} },
    { 'G',{0b01110,0b10001,0b10000,0b10111,0b10001,0b10001,0b01110} },
    { 'H',{0b10001,0b10001,0b10001,0b11111,0b10001,0b10001,0b10001} },
    { 'I',{0b11111,0b00100,0b00100,0b00100,0b00100,0b00100,0b11111} },
    { 'L',{0b10000,0b10000,0b10000,0b10000,0b10000,0b10000,0b11111} },
    { 'M',{0b10001,0b11011,0b10101,0b10101,0b10001,0b10001,0b10001} },
    { 'N',{0b10001,0b11001,0b10101,0b10011,0b10001,0b10001,0b10001} },
    { 'O',{0b01110,0b10001,0b10001,0b10001,0b10001,0b10001,0b01110} },
    { 'P',{0b11110,0b10001,0b10001,0b11110,0b10000,0b10000,0b10000} },
    { 'Q',{0b01110,0b10001,0b10001,0b10001,0b10001,0b10101,0b01010} },
    { 'R',{0b11110,0b10001,0b10001,0b11110,0b10100,0b10010,0b10001} },
    { 'S',{0b01111,0b10000,0b10000,0b01110,0b00001,0b00001,0b11110} },
    { 'T',{0b11111,0b00100,0b00100,0b00100,0b00100,0b00100,0b00100} },
    { 'U',{0b10001,0b10001,0b10001,0b10001,0b10001,0b10001,0b01110} },
    { 'V',{0b10001,0b10001,0b10001,0b01010,0b01010,0b00100,0b00100} },
    { 'W',{0b10001,0b10001,0b10101,0b10101,0b10101,0b11011,0b10001} },
    { 'X',{0b10001,0b01010,0b00100,0b00100,0b00100,0b01010,0b10001} },
    { 'Y',{0b10001,0b10001,0b01010,0b00100,0b00100,0b00100,0b00100} },
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
    { '/', {0b00001,0b00001,0b00010,0b00100,0b01000,0b10000,0b10000} },
    { ' ',{0,0,0,0,0,0,0} },
    { '-',{0,0,0b11111,0,0,0,0} },
};

static const Glyph5x7* glyph_of(char c)
{
    c = (char)std::toupper((unsigned char)c);
    for (const auto& g : FONT) if (g.ch == c) return &g;
    for (const auto& g : FONT) if (g.ch == ' ') return &g;
    return &FONT[0];
}

static void draw_text(SDL_Renderer* r, float x, float y, const std::string& s,
    float scale = 2.0f, SDL_Color col = { 255,255,255,255 })
{
    SDL_SetRenderDrawColor(r, col.r, col.g, col.b, col.a);
    float cx = x;
    for (char c : s) {
        const Glyph5x7* g = glyph_of((char)std::toupper((unsigned char)c));
        for (int row = 0; row < 7; ++row) {
            unsigned char bits = g->rows[row];
            for (int colb = 0; colb < 5; ++colb) {
                if (bits & (1 << (4 - colb))) {
                    SDL_FRect px{ cx + colb * scale, y + row * scale, scale, scale };
                    SDL_RenderFillRect(r, &px);
                }
            }
        }
        cx += 6.0f * scale;
    }
}

struct Entity {
    Vec2 pos, vel;
    float radius{ 12.f };
    bool  alive{ true };
    virtual ~Entity() = default;
    virtual void update(float dt) { pos += vel * dt; }
    virtual void draw(SDL_Renderer* r) const = 0;
};

struct Bullet : public Entity {
    float lifetime{ 1.2f }, age{ 0.f };
    int dmg{ 10 };
    Bullet(const Vec2& p, const Vec2& v, float life = 1.2f, float rad = 4.f, int damage = 10) {
        pos = p; vel = v; lifetime = life; radius = rad; dmg = damage;
    }
    void update(float dt) override { age += dt; if (age >= lifetime) alive = false; Entity::update(dt); }
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
    int maxHP{ 30 }, hp{ 30 };

    Zombie(const Vec2& p, float s) { pos = p; speed = s; radius = 14.f; }

    bool load_textures(SDL_Renderer* r) {
        const char* pf[8][3] = {
            {"data/assets/Zombie Right.png"},
            {"data/assets/Zombie Down Right.png"},
            {"data/assets/Zombie Down.png"},
            {"data/assets/Zombie Down Left.png"},
            {"data/assets/Zombie Left.png"},
            {"data/assets/Zombie Up Left.png"},
            {"data/assets/Zombie Up.png"},
            {"data/assets/Zombie Up Right.png"}
        };
        bool ok = false;
        for (int i = 0; i < 8; i++) { tex[i] = load_any(r, pf[i][0], pf[i][1], pf[i][2]); if (tex[i]) ok = true; }
        return ok;
    }
    void steer_to(const Vec2& target) { Vec2 d = (target - pos).normalized(); vel = d * speed; if (d.len() > 0.0001f) faceDir = d; }
    SDL_Texture* pick_texture() const {
        float a = std::atan2(faceDir.y, faceDir.x); if (a < 0) a += PI * 2.f;
        int sector = int(std::floor((a + PI / 8.0f) / (PI / 4.0f))) & 7;
        return tex[sector];
    }
    void update(float dt) override { pos += vel * dt; }
    void draw(SDL_Renderer* r) const override {
        SDL_Texture* t = pick_texture();
        float tw = 0.f, th = 0.f; if (t) SDL_GetTextureSize(t, &tw, &th);
        if (t) {
            float s = spriteScale;
            SDL_FRect dst{ pos.x - (tw * s) / 2.f, pos.y - (th * s) / 2.f, tw * s, th * s };
            SDL_RenderTexture(r, t, nullptr, &dst);
        }
        else {
            SDL_FRect rect{ pos.x - radius, pos.y - radius, radius * 2, radius * 2 };
            SDL_SetRenderDrawColor(r, 120, 255, 120, 255); SDL_RenderFillRect(r, &rect);
        }
        if (hp < maxHP) {
            const float barW = 30.f, barH = 4.f;
            float yOff = (th * spriteScale) / 2.f + 8.f;
            SDL_FRect back{ pos.x - barW / 2.f, pos.y - yOff, barW, barH };
            SDL_SetRenderDrawColor(r, 40, 30, 30, 220); SDL_RenderFillRect(r, &back);
            float pct = std::max(0.f, std::min(1.f, hp / float(maxHP)));
            SDL_FRect fill{ back.x + 1, back.y + 1, (barW - 2) * pct, barH - 2 };
            SDL_SetRenderDrawColor(r, 220, 60, 60, 255); SDL_RenderFillRect(r, &fill);
            SDL_SetRenderDrawColor(r, 30, 30, 30, 255); SDL_RenderRect(r, &back);
        }
    }
};

struct AmmoCrate : public Entity {
    SDL_Texture* tex{};
    float spriteScale{ 0.05f };
    AmmoCrate(const Vec2& p) { pos = p; radius = 10.f; }
    bool load(SDL_Renderer* r) {
        tex = load_any(r, "data/assets/Ammo Crate.png");
        return tex != nullptr;
    }
    void draw(SDL_Renderer* r) const override {
        if (tex) {
            float tw = 0.f, th = 0.f; SDL_GetTextureSize(tex, &tw, &th);
            float s = spriteScale;
            SDL_FRect dst{ pos.x - (tw * s) / 2.f, pos.y - (th * s) / 2.f, tw * s, th * s };
            SDL_RenderTexture(r, tex, nullptr, &dst);
        }
        else {
            SDL_FRect rect{ pos.x - radius, pos.y - radius, radius * 2, radius * 2 };
            SDL_SetRenderDrawColor(r, 200, 150, 90, 255); SDL_RenderFillRect(r, &rect);
        }
    }
};

struct HealthBox : public Entity {
    SDL_Texture* tex{};
    float spriteScale{ 0.12f };
    HealthBox(const Vec2& p) { pos = p; radius = 12.f; }
    bool load(SDL_Renderer* r) {
        tex = load_any(r, "data/assets/Health Box.png");
        return tex != nullptr;
    }
    void draw(SDL_Renderer* r) const override {
        if (tex) {
            float tw = 0.f, th = 0.f; SDL_GetTextureSize(tex, &tw, &th);
            float s = spriteScale;
            SDL_FRect dst{ pos.x - (tw * s) / 2.f, pos.y - (th * s) / 2.f, tw * s, th * s };
            SDL_RenderTexture(r, tex, nullptr, &dst);
        }
        else {
            SDL_FRect rect{ pos.x - radius, pos.y - radius, radius * 2, radius * 2 };
            SDL_SetRenderDrawColor(r, 210, 70, 70, 255); SDL_RenderFillRect(r, &rect);
        }
    }
};

struct Weapon {
    std::string  name;
    SDL_Texture* sprite{};
    float fireRate{ 6.f };
    float bulletSpeed{ 520.f };
    float bulletLife{ 1.0f };
    float spreadDeg{ 0.f };
    int   pellets{ 1 };
    int   ammo{ -1 };
    int   damage{ 20 };
};

class Player : public Entity {
public:
    Player(const Vec2& p) { pos = p; radius = 14.f; }

    bool load_textures(SDL_Renderer* r) {
        const char* pf[8][3] = {
            {"data/assets/Player Right.png"},
            {"data/assets/Player Down Right.png"},
            {"data/assets/Player Down.png"},
            {"data/assets/Player Down Left.png"},
            {"data/assets/Player Left.png"},
            {"data/assets/Player Up Left.png"},
            {"data/assets/Player Up.png"},
            {"data/assets/Player Up Right.png"}
        };
        bool ok = false;
        for (int i = 0; i < 8; i++) { tex[i] = load_any(r, pf[i][0], pf[i][1], pf[i][2]); if (tex[i]) ok = true; }
        pistol.name = "PISTOL"; pistol.sprite = load_any(r, "data/assets/Pistol.png");
        shotgun.name = "SHOTGUN"; shotgun.sprite = load_any(r, "data/assets/Shotgun.png");
        rifle.name = "RIFLE"; rifle.sprite = load_any(r, "data/assets/Rifle.png");
        return ok;
    }
    void setup_weapons() {
        pistol.fireRate = 7.0f; pistol.bulletSpeed = 620.f; pistol.bulletLife = 0.9f; pistol.spreadDeg = 4.f;  pistol.pellets = 1; pistol.ammo = -1; pistol.damage = 22;
        shotgun.fireRate = 1.2f; shotgun.bulletSpeed = 520.f; shotgun.bulletLife = 0.7f; shotgun.spreadDeg = 22.f; shotgun.pellets = 6; shotgun.damage = 12;
        rifle.fireRate = 10.0f; rifle.bulletSpeed = 780.f; rifle.bulletLife = 1.0f; rifle.spreadDeg = 2.0f; rifle.pellets = 1; rifle.damage = 28;

        shotgunMagSize = 6;  shotgunMag = shotgunMagSize; shotgunReserve = 24;
        rifleMagSize = 12; rifleMag = rifleMagSize;   rifleReserve = 90;

        select = 0;
        hp = maxHP = 5;
    }

    void update_input(float dt, const bool* ks, float mx, float my) {
        Vec2 acc{ 0,0 };
        if (ks[SDL_SCANCODE_W]) acc.y -= 1;
        if (ks[SDL_SCANCODE_S]) acc.y += 1;
        if (ks[SDL_SCANCODE_A]) acc.x -= 1;
        if (ks[SDL_SCANCODE_D]) acc.x += 1;
        acc = acc.normalized() * speed;
        vel = acc;

        Vec2 mouse{ mx,my };
        aimDir = (mouse - pos).normalized();

        shootTimer = std::max(0.f, shootTimer - dt);

        if (reloading) {
            reloadTimer -= dt;
            if (reloadTimer <= 0.f) {
                reloading = false;
                if (select == 1) {
                    int need = shotgunMagSize - shotgunMag;
                    int take = std::min(need, shotgunReserve);
                    shotgunMag += take; shotgunReserve -= take;
                }
                else if (select == 2) {
                    int need = rifleMagSize - rifleMag;
                    int take = std::min(need, rifleReserve);
                    rifleMag += take; rifleReserve -= take;
                }
            }
        }
    }

    int try_shoot(std::vector<Bullet>& out, std::mt19937& rng) {
        if (reloading) return 0;
        const Weapon& w = current();
        if (shootTimer > 0.f) return 0;

        if (select == 1) { if (shotgunMag <= 0) { if (shotgunReserve > 0) start_reload(1.5f); return 0; } }
        if (select == 2) { if (rifleMag <= 0) { if (rifleReserve > 0) start_reload(2.0f); return 0; } }

        shootTimer = 1.0f / w.fireRate;

        if (select == 1) shotgunMag--;
        if (select == 2) rifleMag--;

        std::uniform_real_distribution<float> jitter(-w.spreadDeg, w.spreadDeg);
        for (int i = 0; i < w.pellets; i++) {
            float ang = std::atan2(aimDir.y, aimDir.x) + (jitter(rng) * (PI / 180.f));
            Vec2 dir{ std::cos(ang), std::sin(ang) };
            out.emplace_back(pos + dir * 18.f, dir * w.bulletSpeed, w.bulletLife, 4.f, w.damage);
        }
        return 1;
    }

    void set_weapon(int idx) { select = std::clamp(idx, 0, 2); }
    int  get_weapon_index() const { return select; }
    void manual_reload() {
        if (select == 1 && shotgunMag < shotgunMagSize && shotgunReserve > 0 && !reloading) start_reload(1.5f);
        if (select == 2 && rifleMag < rifleMagSize && rifleReserve   > 0 && !reloading) start_reload(2.0f);
    }
    void full_refill() {
        shotgunReserve = 24; rifleReserve = 90;
        shotgunMag = shotgunMagSize; rifleMag = rifleMagSize;
        reloading = false; reloadTimer = 0.f;
    }

    bool is_reloading() const { return reloading; }
    std::string ammo_text() const {
        if (select == 0) return "PISTOL INF";
        if (select == 1) return "SHOTGUN " + std::to_string(shotgunMag) + "/" + std::to_string(shotgunReserve);
        return "RIFLE " + std::to_string(rifleMag) + "/" + std::to_string(rifleReserve);
    }

    int maxHP{ 5 };
    int hp{ 5 };

    const Weapon& current() const {
        if (select == 1) return shotgun;
        if (select == 2) return rifle;
        return pistol;
    }

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
        SDL_Texture* gun = current().sprite;
        if (gun) {
            float gw = 0.f, gh = 0.f; SDL_GetTextureSize(gun, &gw, &gh);
            const float TARGET_H = 22.f; float s = TARGET_H / gh;
            SDL_FRect gd{ pos.x - (gw * s) / 2.f + aimDir.x * 8.f,
                          pos.y - (gh * s) / 2.f + aimDir.y * 8.f,
                          gw * s, gh * s };
            float angle = std::atan2(aimDir.y, aimDir.x) * 180.f / PI;
            SDL_FPoint center{ gd.w / 2.f, gd.h / 2.f };
            SDL_RenderTextureRotated(r, gun, nullptr, &gd, angle, &center, SDL_FLIP_NONE);
        }
    }

private:
    float speed{ 220.f };
    float shootTimer{ 0.f };
    Vec2  aimDir{ 1,0 };
    float spriteScale{ 0.06f };
    SDL_Texture* tex[8]{};

    Weapon pistol, shotgun, rifle;
    int select{ 0 };

    int shotgunMagSize{ 6 }, shotgunMag{ 6 }, shotgunReserve{ 24 };
    int rifleMagSize{ 12 }, rifleMag{ 12 }, rifleReserve{ 90 };

    float reloadTimer{ 0.f };
    bool  reloading{ false };
    void start_reload(float sec) { reloading = true; reloadTimer = sec; }

    SDL_Texture* pick_texture() const {
        float a = std::atan2(aimDir.y, aimDir.x); if (a < 0) a += PI * 2.f;
        int sector = int(std::floor((a + PI / 8.0f) / (PI / 4.0f))) & 7;
        return tex[sector];
    }
};

template<typename T>
static int count_alive(const std::vector<T>& v) {
    int n = 0; for (const auto& e : v) if (e.alive) ++n; return n;
}
static bool point_in_rect(float px, float py, const SDL_FRect& r) {
    return (px >= r.x && px <= r.x + r.w && py >= r.y && py <= r.y + r.h);
}

// GAME
class Game {
public:
    enum class State { Menu, Playing, Intermission, GameOver };

    Game(SDL_Renderer* ren, SDL_Window* win, int w, int h)
        : r(ren), window(win), width(w), height(h), rnd(std::random_device{}()),
        distX(30.f, w - 30.f), distY(30.f, h - 30.f)
    {
        background = load_any(r, "data/assets/map.png");
        menuBg = load_any(r, "data/assets/Game Menu.png");

        music.init_and_load();
        music.play_menu();

        state = State::Menu;
        prepare_menu_ui();
        if (window) SDL_SetWindowTitle(window, "CW1 – Top-Down Zombies | Menu");
    }

    ~Game() {
        if (background) SDL_DestroyTexture(background);
        if (menuBg)     SDL_DestroyTexture(menuBg);
        music.shutdown();
    }

    // event handling
    void handle_event(const SDL_Event& e) {
        // Menu
        if (state == State::Menu) {
            if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT) {
                float mx = 0.f, my = 0.f; SDL_GetMouseState(&mx, &my);
                if (point_in_rect(mx, my, btnPlay)) { reset_run(); return; }
                if (point_in_rect(mx, my, btnExit)) { wantQuit = true; return; }
            }
            if (e.type == SDL_EVENT_KEY_DOWN &&
                (e.key.key == SDLK_RETURN || e.key.key == SDLK_RETURN2 || e.key.key == SDLK_SPACE)) {
                reset_run(); return;
            }
            return;
        }

		// Game Over
        if (state == State::GameOver) {
            if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT) {
                float mx = 0.f, my = 0.f; SDL_GetMouseState(&mx, &my);
                if (point_in_rect(mx, my, btnRestart)) { reset_run(); }
                else if (point_in_rect(mx, my, btnMenu)) { go_to_menu(); }
                else if (point_in_rect(mx, my, btnQuit)) { wantQuit = true; }
            }
            return;
        }

        // Playing / Intermission input
        if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT) {
            fireHeld = true;
            if (player->get_weapon_index() == 0) pistolClickQueued = true; // pistol = semi-auto
        }
        if (e.type == SDL_EVENT_MOUSE_BUTTON_UP && e.button.button == SDL_BUTTON_LEFT) {
            fireHeld = false;
        }

        if (e.type == SDL_EVENT_KEY_DOWN) {
            if (e.key.key == SDLK_1) player->set_weapon(0);
            if (e.key.key == SDLK_2) player->set_weapon(1);
            if (e.key.key == SDLK_3) player->set_weapon(2);
            if (e.key.key == SDLK_R) player->manual_reload();
        }
        if (e.type == SDL_EVENT_MOUSE_WHEEL) {
            int change = (e.wheel.y > 0) ? -1 : ((e.wheel.y < 0) ? +1 : 0);
            if (change) {
                int cur = player->get_weapon_index();
                cur = (cur + change + 3) % 3;
                player->set_weapon(cur);
            }
        }
    }

	// core update
    void update(float dt, const bool* kstate, float mx, float my) {
        music.update();

        if (state == State::Menu || state == State::GameOver) return;

        damageCooldown = std::max(0.f, damageCooldown - dt);

        if (state == State::Intermission) {
            intermissionTimer -= dt;
            if (intermissionTimer <= 0.f) start_wave(currentWave + 1);
        }

        player->update_input(dt, kstate, mx, my);

        if (pistolClickQueued) { pistolClickQueued = false; (void)player->try_shoot(bullets, rnd); }
        if (fireHeld && player->get_weapon_index() != 0) (void)player->try_shoot(bullets, rnd);

		// spawn zombies
        spawnTimer -= dt;
        if (state == State::Playing && spawnedThisWave < totalThisWave && spawnTimer <= 0.f) {
            if (alive_zombies() < simultaneousCap) {
                spawn_zombie();
                spawnedThisWave++;
                spawnTimer = spawnInterval;
            }
            else {
                spawnTimer = 0.15f;
            }
        }

        // independent pickups
        ammoSpawnTimer -= dt;
        healthSpawnTimer -= dt;
        if (ammoSpawnTimer <= 0.f) {
            if (count_alive(ammoCrates) < maxAmmoAlive) spawn_ammo_crate();
            ammoSpawnTimer = next_interval(ammoBase);
        }
        if (healthSpawnTimer <= 0.f) {
            if (count_alive(healthBoxes) < maxHealthAlive) spawn_health_box();
            healthSpawnTimer = next_interval(healthBase);
        }

        player->update(dt); clamp_to_arena(*player);
        for (auto& z : zombies) { z.steer_to(player->pos); z.update(dt); clamp_to_arena(z); }
        for (auto& b : bullets) { b.update(dt); }
        for (auto& c : ammoCrates) { c.update(dt); }
        for (auto& h : healthBoxes) { h.update(dt); }

        // bullets -> zombies
        for (auto& z : zombies) {
            if (!z.alive) continue;
            for (auto& b : bullets) {
                if (!b.alive) continue;
                if (circle_hit(z.pos, z.radius, b.pos, b.radius)) {
                    z.hp -= b.dmg; b.alive = false;
                    if (z.hp <= 0) { z.alive = false; score += 10; killedThisWave++; }
                }
            }
        }

        // zombies -> player
        for (auto& z : zombies) {
            if (z.alive && circle_hit(z.pos, z.radius, player->pos, player->radius)) {
                if (damageCooldown <= 0.f) {
                    player->hp = std::max(0, player->hp - 1);
                    damageCooldown = 0.6f;
                    if (player->hp <= 0) {
                        state = State::GameOver;
                        gameOverAnim = 0.0f;
                        prepare_game_over_ui();
                        music.stop();   // stop in-game music on death
                    }
                }
                Vec2 away = (z.pos - player->pos).normalized();
                z.pos += away * 6.f;
            }
        }

        // player -> pickups
        for (auto& c : ammoCrates) {
            if (c.alive && circle_hit(c.pos, c.radius, player->pos, player->radius)) {
                player->full_refill(); c.alive = false;
            }
        }
        for (auto& h : healthBoxes) {
            if (h.alive && circle_hit(h.pos, h.radius, player->pos, player->radius)) {
                if (player->hp < player->maxHP) { player->hp = std::min(player->maxHP, player->hp + 1); h.alive = false; }
            }
        }

        erase_dead(bullets);
        erase_dead(ammoCrates);
        erase_dead(healthBoxes);
        erase_dead(zombies);

        // wave end -> intermission
        if (state == State::Playing &&
            spawnedThisWave >= totalThisWave &&
            killedThisWave >= totalThisWave &&
            alive_zombies() == 0)
        {
            state = State::Intermission; intermissionTimer = 3.0f;
        }

        surviveTime += dt;
    }

    void draw() const {
        // MENU
        if (state == State::Menu) {
            SDL_SetRenderDrawColor(r, 10, 15, 18, 255);
            SDL_RenderClear(r);

            if (menuBg) {
                float tw = 0, th = 0; SDL_GetTextureSize(menuBg, &tw, &th);
                float sx = (float)width / tw, sy = (float)height / th;
                float s = std::max(sx, sy);
                SDL_FRect dst{ (float)width / 2.f - tw * s / 2.f, (float)height / 2.f - th * s / 2.f, tw * s, th * s };
                SDL_RenderTexture(r, menuBg, nullptr, &dst);
            }

            float mx = 0, my = 0; SDL_GetMouseState(&mx, &my);
            auto draw_btn = [&](const SDL_FRect& rc, const char* label) {
                bool hov = point_in_rect(mx, my, rc);
                SDL_SetRenderDrawColor(r, hov ? 90 : 60, hov ? 120 : 80, 140, 255);
                SDL_RenderFillRect(r, &rc);
                SDL_SetRenderDrawColor(r, 20, 20, 30, 255);
                SDL_RenderRect(r, &rc);
                draw_text(r, rc.x + 26.f, rc.y + 10.f, label, 2.0f, SDL_Color{ 255,255,255,255 });
                };
            draw_btn(btnPlay, "PLAY");
            draw_btn(btnExit, "EXIT");

            SDL_RenderPresent(r);
            return;
        }

        // GAME / GAME OVER RENDER
        if (background) {
            SDL_FRect dst{ 0.f,0.f,(float)width,(float)height };
            SDL_RenderTexture(r, background, nullptr, &dst);
        }
        else {
            SDL_SetRenderDrawColor(r, 18, 14, 22, 255); SDL_RenderClear(r);
        }

        SDL_SetRenderDrawColor(r, 60, 50, 80, 255);
        SDL_FRect border{ 10.f,10.f,(float)width - 20.f,(float)height - 20.f }; SDL_RenderRect(r, &border);

        for (const auto& c : ammoCrates)  c.draw(r);
        for (const auto& h : healthBoxes) h.draw(r);
        player->draw(r);
        for (const auto& z : zombies) z.draw(r);
        for (const auto& b : bullets)  b.draw(r);

        draw_hud();

        if (state == State::GameOver) draw_game_over();

        SDL_RenderPresent(r);
    }

    bool wants_quit() const { return wantQuit; }

private:
    SDL_Renderer* r{};
    SDL_Window* window{};
    int width{}, height{};
    SDL_Texture* background{};
    SDL_Texture* menuBg{};

    MusicPlayer music;

    std::unique_ptr<Player> player;
    std::vector<Zombie> zombies;
    std::vector<Bullet> bullets;
    std::vector<AmmoCrate> ammoCrates;
    std::vector<HealthBox> healthBoxes;

    State state{ State::Menu };

    float surviveTime{ 0.f };
    int   score{ 0 };

    int   currentWave{ 1 };
    int   totalThisWave{ 0 };
    int   spawnedThisWave{ 0 };
    int   killedThisWave{ 0 };
    int   simultaneousCap{ 6 };
    float zombieSpeed{ 90.f };
    float spawnInterval{ 1.0f };
    float spawnTimer{ 0.f };
    float intermissionTimer{ 0.f };

    // independent pickup timers + caps
    float ammoBase{ 10.0f };
    float healthBase{ 10.0f };
    float ammoSpawnTimer{ 0.0f };
    float healthSpawnTimer{ 0.0f };
    int   maxAmmoAlive{ 2 };
    int   maxHealthAlive{ 2 };

    bool  fireHeld{ false };
    bool  pistolClickQueued{ false };

    std::mt19937 rnd;
    std::uniform_real_distribution<float> distX;
    std::uniform_real_distribution<float> distY;

    float damageCooldown{ 0.f };
    mutable float gameOverAnim{ 0.f };

    SDL_FRect btnRestart{}, btnQuit{};
    SDL_FRect btnPlay{}, btnExit{};
    SDL_FRect btnMenu{};
    bool      wantQuit{ false };

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

    Vec2 random_point_away_from_player(float dmin) {
        for (int tries = 0; tries < 20; ++tries) {
            Vec2 p{ distX(rnd), distY(rnd) };
            if (!player || (p - player->pos).len() >= dmin) return p;
        }
        return Vec2{ distX(rnd), distY(rnd) };
    }

    void spawn_zombie() {
        int side = std::uniform_int_distribution<int>(0, 3)(rnd);
        float x = 0, y = 0;
        if (side == 0) { x = distX(rnd); y = 18.f; }
        if (side == 1) { x = distX(rnd); y = height - 18.f; }
        if (side == 2) { x = 18.f; y = distY(rnd); }
        if (side == 3) { x = width - 18.f; y = distY(rnd); }
        zombies.emplace_back(Vec2{ x,y }, zombieSpeed);
        zombies.back().load_textures(r);
        zombies.back().spriteScale = 0.06f;

        int baseHP = 30 + int(6 * (currentWave - 1));
        baseHP = std::min(baseHP, 200);
        zombies.back().maxHP = zombies.back().hp = baseHP;
    }

    // independent pickup spawns
    void spawn_ammo_crate() {
        Vec2 p = random_point_away_from_player(80.f);
        ammoCrates.emplace_back(p);
        ammoCrates.back().load(r);
    }
    void spawn_health_box() {
        Vec2 p = random_point_away_from_player(80.f);
        healthBoxes.emplace_back(p);
        healthBoxes.back().load(r);
    }

    float next_interval(float base) {
        float waveScale = 1.0f + 0.35f * float(currentWave - 1);
        std::uniform_real_distribution<float> jitter(0.70f, 1.40f);
        return std::min(40.0f, base * waveScale * jitter(rnd));
    }

    void start_wave(int wave) {
        state = State::Playing;
        currentWave = wave;
        totalThisWave = 8 + (wave - 1) * 5;
        simultaneousCap = std::min(6 + (wave - 1) * 2, 40);
        spawnInterval = std::max(0.20f, 1.0f * std::pow(0.92f, float(wave - 1)));
        zombieSpeed = 90.f * (1.0f + 0.06f * float(wave - 1));

        spawnedThisWave = 0; 
        killedThisWave = 0; 
        spawnTimer = 0.25f;

        // pickup timers
        ammoSpawnTimer = next_interval(ammoBase);
        healthSpawnTimer = next_interval(healthBase);

        music.play_game();

        if (window) {
            std::string t = "CW1 – Top-Down Zombies  |  Wave " + std::to_string(currentWave);
            SDL_SetWindowTitle(window, t.c_str());
        }
    }

    void draw_hud() const {
        draw_text(r, 16.f, 10.f, "WAVE " + std::to_string(currentWave), 2.0f, SDL_Color{ 255,220,120,255 });
        for (int i = 0; i < player->maxHP; i++) {
            SDL_FRect heart{ 16.f + i * 16.f, 28.f, 10.f, 10.f };
            if (i < player->hp) {
                SDL_SetRenderDrawColor(r, 255, 90, 90, 255);
                SDL_RenderFillRect(r, &heart);
            }
            else {
                SDL_SetRenderDrawColor(r, 90, 70, 70, 255);
                SDL_RenderRect(r, &heart);
            }
        }
        std::string ammoText = player->is_reloading() ? "RELOADING..." : player->ammo_text();
        draw_text(r, 16.f, 48.f, ammoText, 2.0f, SDL_Color{ 190,240,255,255 });
    }

    void draw_game_over() const {
        SDL_SetRenderDrawColor(r, 0, 0, 0, 180);
        SDL_FRect full{ 0,0,(float)width,(float)height };
        SDL_RenderFillRect(r, &full);

        draw_text(r, width / 2.f - 120.f, height / 2.f - 90.f, "GAME OVER", 3.0f, SDL_Color{ 255,120,120,255 });
        draw_text(r, width / 2.f - 120.f, height / 2.f - 60.f,
            "WAVE " + std::to_string(currentWave) + "  SCORE " + std::to_string(score),
            2.0f, SDL_Color{ 230,210,180,255 });

		// buttons
        float mx = 0.f, my = 0.f; SDL_GetMouseState(&mx, &my);
        auto draw_button = [&](const SDL_FRect& rect, const char* label) {
            bool hover = point_in_rect(mx, my, rect);
            SDL_SetRenderDrawColor(r, hover ? 90 : 60, hover ? 120 : 80, 140, 255);
            SDL_RenderFillRect(r, &rect);
            SDL_SetRenderDrawColor(r, 20, 20, 30, 255);
            SDL_RenderRect(r, &rect);
            draw_text(r, rect.x + 22.f, rect.y + 10.f, label, 2.0f, SDL_Color{ 255,255,255,255 });
            };
        draw_button(btnRestart, "RESTART");
        draw_button(btnMenu, "MENU");
        draw_button(btnQuit, "EXIT");
    }

    void prepare_game_over_ui() {
        const float bw = 180.f, bh = 40.f;
        const float cx = width / 2.f - bw / 2.f;
        const float cy = height / 2.f - 5.f;
        btnRestart = SDL_FRect{ cx, cy, bw, bh };
        btnMenu = SDL_FRect{ cx, cy + bh + 10.f, bw, bh };
        btnQuit = SDL_FRect{ cx, cy + (bh + 10.f) * 2.f, bw, bh };
    }

    void prepare_menu_ui() {
        const float bw = 100.f;
        const float bh = 48.f;
        const float left = 210.f;
        const float top = height * 0.70f;

        btnPlay = SDL_FRect{ left, top, bw, bh };
        btnExit = SDL_FRect{ left, top + bh + 18.f, bw, bh };
    }

    void reset_run() {
        // clear world
        zombies.clear(); bullets.clear(); ammoCrates.clear(); healthBoxes.clear();
        score = 0; surviveTime = 0.f; damageCooldown = 0.f;

        // (re)create player
        player = std::make_unique<Player>(Vec2{ width * 0.5f, height * 0.5f });
        player->load_textures(r);
        player->setup_weapons();

        // start wave 1
        currentWave = 1;
        start_wave(currentWave);

        state = State::Playing;
    }

    void go_to_menu() {
        zombies.clear(); bullets.clear(); ammoCrates.clear(); healthBoxes.clear();
        player.reset();
        state = State::Menu;
        if (window)
            SDL_SetWindowTitle(window, "CW1 – Top-Down Zombies | Menu");
    }
};

// main
int main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Error initialising SDL3", nullptr);
        return 1;
    }
    const int width = 960, height = 540;
    SDL_Window* win = SDL_CreateWindow("CW1 – Top-Down Zombies", width, height, 0);
    if (!win) { SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Error creating window", nullptr); SDL_Quit(); return 1; }
    SDL_Renderer* ren = SDL_CreateRenderer(win, nullptr);
    if (!ren) { SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Error creating renderer", win); SDL_DestroyWindow(win); SDL_Quit(); return 1; }

    Game game(ren, win, width, height);

    bool running = true;
    Uint64 freq = SDL_GetPerformanceFrequency(), prev = SDL_GetPerformanceCounter();
    while (running) {
        Uint64 now = SDL_GetPerformanceCounter();
        float dt = float(now - prev) / float(freq); prev = now;
        dt = std::min(dt, 0.033f);

        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) { running = false; break; }
            if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE) { running = false; break; }
            game.handle_event(e);
        }
        if (!running) break;
        if (game.wants_quit()) break;

        float mx = 0.f, my = 0.f; SDL_GetMouseState(&mx, &my);
        const bool* kstate = SDL_GetKeyboardState(nullptr);

        game.update(dt, kstate, mx, my);
        game.draw();
        SDL_Delay(1);
    }

    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
