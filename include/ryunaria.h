#ifndef RYUNARIA_H
#define RYUNARIA_H

#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>

#define RYUNA_RIA_TITLE   "Ryuna Ria"
#define RYUNA_RIA_VERSION "0.2.0"

#define SCREEN_WIDTH  480
#define SCREEN_HEIGHT 272
#define BUF_WIDTH     512

#define MAP_SIZE        40
#define ISO_TILE_HALF_W 22.0f
#define ISO_TILE_HALF_H 11.0f

#define MAX_INVENTORY   12
#define MAX_GROUND_LOOT 24
#define MAX_CHESTS      14
#define MAX_MONSTERS    22
#define MAX_PROJECTILES 32
#define MAX_FLOAT_TEXTS 24

typedef enum {
    MODE_TITLE = 0,
    MODE_PLAYING = 1,
    MODE_INVENTORY = 2,
    MODE_PAUSE = 3
} GameMode;

typedef enum {
    ZONE_SANCTUARY = 0,
    ZONE_RUINS = 1,
    ZONE_CRYSTAL = 2,
    ZONE_ASHEN = 3
} ZoneId;

typedef enum {
    TILE_GRASS = 0,
    TILE_STONE_PLAZA = 1,
    TILE_MOSS_RUIN = 2,
    TILE_CRYSTAL_FLOOR = 3,
    TILE_ASH_BASALT = 4,
    TILE_WATER_CHASM = 5,
    TILE_WALL_PILLAR = 6,
    TILE_CRYSTAL_SPIRE = 7,
    TILE_EMBER_VENT = 8,
    TILE_SHRINE_PAD = 9
} TileType;

typedef enum {
    SLOT_WEAPON = 0,
    SLOT_ARMOR = 1,
    SLOT_RELIC = 2,
    SLOT_COUNT = 3
} ItemSlot;

typedef enum {
    RARITY_COMMON = 0,
    RARITY_UNCOMMON = 1,
    RARITY_RARE = 2,
    RARITY_MYTHIC = 3
} ItemRarity;

typedef enum {
    MON_STALKER = 0,
    MON_GOLEM = 1,
    MON_WEAVER = 2,
    MON_WARDEN = 3
} MonsterKind;

typedef enum {
    ADAPT_NONE      = 0,
    ADAPT_IRONHIDE  = (1u << 0),
    ADAPT_SPELLWARD = (1u << 1),
    ADAPT_PURSUER   = (1u << 2),
    ADAPT_FRENZY    = (1u << 3),
    ADAPT_THORNS    = (1u << 4),
    ADAPT_LEECHING  = (1u << 5)
} AdaptTrait;

typedef struct {
    unsigned int held;
    unsigned int pressed;
    float move_x;
    float move_y;
    int menu_dx;
    int menu_dy;
} InputState;

typedef struct {
    int active;
    ItemSlot slot;
    ItemRarity rarity;
    int level;
    char name[28];
    int atk_bonus;
    int def_bonus;
    int arc_bonus;
    int hp_bonus;
    int crit_bonus;
    float spd_bonus;
    int value;
} Item;

typedef struct {
    int active;
    float wx;
    float wy;
    float bob_phase;
    int is_tonic;
    int gold_amount;
    Item item;
} GroundLoot;

typedef struct {
    int active;
    int opened;
    float respawn_timer;
    float wx;
    float wy;
    ZoneId zone;
    int tier;
} WorldChest;

typedef struct {
    float wx;
    float wy;
    float vx;
    float vy;
    float facing_x;
    float facing_y;
    float walk_anim;

    int level;
    int xp;
    int xp_next;
    int gold;
    int tonics;
    int max_tonics;
    int shrine_rank;

    float hp;
    float max_hp;
    float mp;
    float max_mp;
    float stamina;
    float max_stamina;

    int atk;
    int def;
    int arc;
    int crit_pct;
    float move_speed;

    int combo_step;
    float attack_timer;
    float attack_cooldown;
    float combo_window;
    float swing_angle;
    unsigned int swing_hit_mask;

    float cast_cooldown;
    float nova_flash_timer;
    float dash_timer;
    float dash_cooldown;
    float iframe_timer;
    float hurt_flash;

    int chain_count;
    float chain_timer;
    int total_kills;
    int boss_kills;

    Item equipped[SLOT_COUNT];
    Item bag[MAX_INVENTORY];
} Player;

typedef struct {
    float melee_pressure;
    float arcane_pressure;
    float dash_frequency;
    float aggression_score;
    int adaptations_triggered;
} AdaptationProfile;

typedef struct {
    int active;
    MonsterKind kind;
    ZoneId home_zone;
    int level;
    int is_elite;
    unsigned int traits;

    float wx;
    float wy;
    float vx;
    float vy;
    float spawn_wx;
    float spawn_wy;

    float hp;
    float max_hp;
    int atk;
    int def;
    float move_speed;

    float ai_timer;
    float windup_timer;
    float windup_total;
    float attack_cd;
    float lunge_vx;
    float lunge_vy;
    float hurt_flash;
    float adapt_flash;

    int melee_hits_taken;
    int arcane_hits_taken;
    int adapted_midfight;
} Monster;

typedef struct {
    int active;
    int from_player;
    float wx;
    float wy;
    float vx;
    float vy;
    float life;
    int damage;
    unsigned int color;
    float radius;
    int pierce;
} Projectile;

typedef struct {
    int active;
    float wx;
    float wy;
    float wz;
    float timer;
    float duration;
    unsigned int color;
    char text[24];
} FloatingText;

typedef struct {
    GameMode mode;
    Player player;
    AdaptationProfile adapt;

    unsigned char tiles[MAP_SIZE][MAP_SIZE];
    WorldChest chests[MAX_CHESTS];
    Monster monsters[MAX_MONSTERS];
    Projectile projectiles[MAX_PROJECTILES];
    GroundLoot loot[MAX_GROUND_LOOT];
    FloatingText floats[MAX_FLOAT_TEXTS];

    float cam_wx;
    float cam_wy;
    float time_sec;
    float shake_timer;
    float shake_mag;

    int locked_monster;
    int inv_cursor;
    int pause_cursor;
    float respawn_clock;

    unsigned int rng_state;
    char banner_msg[52];
    float banner_timer;
    unsigned int banner_color;

    int request_exit;
} GameState;

unsigned int game_rand(GameState *gs);
float game_randf(GameState *gs);
void game_set_banner(GameState *gs, const char *msg, unsigned int color, float duration);
void game_spawn_float_text(GameState *gs, float wx, float wy, float wz, const char *text, unsigned int color);

int game_init(GameState *gs);
void game_update(GameState *gs, const InputState *in, float dt);
void game_draw(const GameState *gs);
void game_shutdown(void);

void ui_draw(const GameState *gs);

#endif
