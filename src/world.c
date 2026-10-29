#include <stdio.h>
#include <math.h>

#include "world.h"
#include "items.h"
#include "render.h"

ZoneId world_zone_at(float wx, float wy) {
    if (wx >= 15.0f && wx <= 25.0f && wy >= 15.0f && wy <= 25.0f) {
        return ZONE_SANCTUARY;
    }
    if (wy >= 23.0f) {
        return ZONE_ASHEN;
    }
    if (wx >= 20.0f) {
        return ZONE_CRYSTAL;
    }
    return ZONE_RUINS;
}

const char *world_zone_name(ZoneId z) {
    switch (z) {
        case ZONE_SANCTUARY: return "Sunlit Sanctuary";
        case ZONE_RUINS:     return "Verdant Ruins";
        case ZONE_CRYSTAL:   return "Crystal Hollow";
        case ZONE_ASHEN:     return "Ashen Wastes";
        default:             return "Unknown Frontier";
    }
}

unsigned int world_zone_color(ZoneId z) {
    switch (z) {
        case ZONE_SANCTUARY: return RGBA8(240, 214, 138, 255);
        case ZONE_RUINS:     return RGBA8(130, 226, 154, 255);
        case ZONE_CRYSTAL:   return RGBA8(120, 214, 255, 255);
        case ZONE_ASHEN:     return RGBA8(255, 136, 98, 255);
        default:             return RGBA8(220, 220, 220, 255);
    }
}

int world_tile_solid(TileType t) {
    return (t == TILE_WATER_CHASM || t == TILE_WALL_PILLAR || t == TILE_CRYSTAL_SPIRE);
}

static unsigned int tile_hash(int x, int y) {
    unsigned int h = (unsigned int)(x * 374761393u) ^ (unsigned int)(y * 668265263u);
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

void world_generate(GameState *gs) {
    for (int y = 0; y < MAP_SIZE; y++) {
        for (int x = 0; x < MAP_SIZE; x++) {
            /* Outer world border */
            if (x == 0 || y == 0 || x == MAP_SIZE - 1 || y == MAP_SIZE - 1) {
                gs->tiles[y][x] = (unsigned char)(((x + y) % 3 == 0) ? TILE_WALL_PILLAR : TILE_WATER_CHASM);
                continue;
            }

            ZoneId z = world_zone_at((float)x + 0.5f, (float)y + 0.5f);
            unsigned int h = tile_hash(x, y);

            /* Keep main roads from Sanctuary to all biomes unobstructed */
            int on_road = ((x >= 19 && x <= 21) && (y >= 4 && y <= MAP_SIZE - 5)) ||
                          ((y >= 19 && y <= 21) && (x >= 4 && x <= MAP_SIZE - 5));

            if (z == ZONE_SANCTUARY) {
                if (x >= 19 && x <= 21 && y >= 19 && y <= 21) {
                    gs->tiles[y][x] = TILE_SHRINE_PAD;
                } else if ((x == 16 || x == 24) && (y == 16 || y == 24)) {
                    gs->tiles[y][x] = TILE_WALL_PILLAR;
                } else if (x >= 16 && x <= 24 && y >= 16 && y <= 24) {
                    gs->tiles[y][x] = TILE_STONE_PLAZA;
                } else {
                    gs->tiles[y][x] = TILE_GRASS;
                }
            } else if (on_road) {
                gs->tiles[y][x] = TILE_STONE_PLAZA;
            } else if (z == ZONE_RUINS) {
                if ((h % 19u) == 0u) {
                    gs->tiles[y][x] = TILE_WALL_PILLAR;
                } else if ((h % 29u) == 0u) {
                    gs->tiles[y][x] = TILE_WATER_CHASM;
                } else if ((h % 3u) == 0u) {
                    gs->tiles[y][x] = TILE_MOSS_RUIN;
                } else {
                    gs->tiles[y][x] = TILE_GRASS;
                }
            } else if (z == ZONE_CRYSTAL) {
                if ((h % 16u) == 0u) {
                    gs->tiles[y][x] = TILE_CRYSTAL_SPIRE;
                } else if ((h % 27u) == 0u) {
                    gs->tiles[y][x] = TILE_WATER_CHASM;
                } else {
                    gs->tiles[y][x] = TILE_CRYSTAL_FLOOR;
                }
            } else {
                /* ZONE_ASHEN */
                if ((h % 18u) == 0u) {
                    gs->tiles[y][x] = TILE_WALL_PILLAR;
                } else if ((h % 14u) == 1u) {
                    gs->tiles[y][x] = TILE_EMBER_VENT;
                } else if ((h % 31u) == 2u) {
                    gs->tiles[y][x] = TILE_WATER_CHASM;
                } else {
                    gs->tiles[y][x] = TILE_ASH_BASALT;
                }
            }
        }
    }

    /* Curated chest coordinates across the three hunting biomes */
    static const int chest_coords[MAX_CHESTS][2] = {
        { 6,  6}, {11,  5}, { 5, 13}, {12, 12}, /* Verdant Ruins */
        {28,  5}, {34,  7}, {27, 13}, {35, 15}, {33, 21}, /* Crystal Hollow */
        { 7, 28}, {14, 34}, {20, 35}, {28, 33}, {35, 34}  /* Ashen Wastes */
    };

    for (int i = 0; i < MAX_CHESTS; i++) {
        int cx = chest_coords[i][0];
        int cy = chest_coords[i][1];

        /* Guarantee chest tile & immediate neighbors are walkable */
        for (int ny = cy - 1; ny <= cy + 1; ny++) {
            for (int nx = cx - 1; nx <= cx + 1; nx++) {
                if (nx > 1 && ny > 1 && nx < MAP_SIZE - 2 && ny < MAP_SIZE - 2) {
                    if (world_tile_solid((TileType)gs->tiles[ny][nx])) {
                        ZoneId nz = world_zone_at((float)nx + 0.5f, (float)ny + 0.5f);
                        gs->tiles[ny][nx] = (nz == ZONE_CRYSTAL) ? TILE_CRYSTAL_FLOOR
                                          : (nz == ZONE_ASHEN)   ? TILE_ASH_BASALT
                                                                 : TILE_MOSS_RUIN;
                    }
                }
            }
        }

        gs->chests[i].active = 1;
        gs->chests[i].opened = 0;
        gs->chests[i].respawn_timer = 0.0f;
        gs->chests[i].wx = (float)cx + 0.5f;
        gs->chests[i].wy = (float)cy + 0.5f;
        gs->chests[i].zone = world_zone_at(gs->chests[i].wx, gs->chests[i].wy);
        gs->chests[i].tier = (gs->chests[i].zone == ZONE_ASHEN) ? 3
                           : (gs->chests[i].zone == ZONE_CRYSTAL) ? 2 : 1;
    }
}

int world_point_blocked(const GameState *gs, float wx, float wy, float radius) {
    if (wx - radius < 1.05f || wy - radius < 1.05f ||
        wx + radius > (float)(MAP_SIZE - 1) - 0.05f ||
        wy + radius > (float)(MAP_SIZE - 1) - 0.05f) {
        return 1;
    }

    int min_x = (int)floorf(wx - radius);
    int max_x = (int)floorf(wx + radius);
    int min_y = (int)floorf(wy - radius);
    int max_y = (int)floorf(wy + radius);

    for (int ty = min_y; ty <= max_y; ty++) {
        for (int tx = min_x; tx <= max_x; tx++) {
            if (tx < 0 || ty < 0 || tx >= MAP_SIZE || ty >= MAP_SIZE) return 1;
            TileType t = (TileType)gs->tiles[ty][tx];
            if (world_tile_solid(t)) {
                return 1;
            }
            /* Center shrine obelisk column */
            if (tx == MAP_SIZE / 2 && ty == MAP_SIZE / 2) {
                float dx = wx - ((float)tx + 0.5f);
                float dy = wy - ((float)ty + 0.5f);
                if (dx * dx + dy * dy < (0.38f + radius) * (0.38f + radius)) {
                    return 1;
                }
            }
        }
    }
    return 0;
}

void world_move_slide(const GameState *gs, float *wx, float *wy, float dx, float dy, float radius) {
    float nx = *wx + dx;
    if (!world_point_blocked(gs, nx, *wy, radius)) {
        *wx = nx;
    }
    float ny = *wy + dy;
    if (!world_point_blocked(gs, *wx, ny, radius)) {
        *wy = ny;
    }
}

static void open_chest(GameState *gs, WorldChest *c) {
    c->opened = 1;
    c->respawn_timer = 75.0f;

    int gold_gain = 18 + c->tier * 16 + (int)(game_rand(gs) % 18u);
    gs->player.gold += gold_gain;

    int target_lv = gs->player.level + (c->tier - 1);
    if (target_lv < c->tier * 2 - 1) target_lv = c->tier * 2 - 1;

    Item drop = items_roll_equipment(gs, target_lv, c->tier >= 2 ? 1 : 0);
    items_spawn_ground_gear(gs, c->wx + 0.35f, c->wy + 0.35f, &drop);

    if ((game_rand(gs) % 100u) < 65u) {
        items_spawn_ground_tonic(gs, c->wx - 0.35f, c->wy + 0.25f, 8 + c->tier * 5);
    }

    char msg[48];
    snprintf(msg, sizeof(msg), "Chest Opened! +%dG", gold_gain);
    game_set_banner(gs, msg, RGBA8(255, 224, 118, 255), 2.2f);
    game_spawn_float_text(gs, c->wx, c->wy, 16.0f, "+LOOT!", RGBA8(255, 230, 120, 255));
}

void world_update_chests(GameState *gs, float dt) {
    for (int i = 0; i < MAX_CHESTS; i++) {
        WorldChest *c = &gs->chests[i];
        if (!c->active || !c->opened) continue;
        c->respawn_timer -= dt;
        if (c->respawn_timer <= 0.0f) {
            c->opened = 0;
            c->respawn_timer = 0.0f;
        }
    }
}

int world_try_interact(GameState *gs) {
    Player *p = &gs->player;

    /* 1. Check Sanctuary Shrine Altar */
    float sh_x = (float)(MAP_SIZE / 2) + 0.5f;
    float sh_y = (float)(MAP_SIZE / 2) + 0.5f;
    float sdx = p->wx - sh_x;
    float sdy = p->wy - sh_y;
    if (sdx * sdx + sdy * sdy <= 2.35f * 2.35f) {
        int bless_cost = 60 + p->shrine_rank * 55;
        p->hp = p->max_hp;
        p->mp = p->max_mp;
        p->stamina = p->max_stamina;
        p->tonics = p->max_tonics;

        if (p->gold >= bless_cost && p->shrine_rank < 10) {
            p->gold -= bless_cost;
            p->shrine_rank++;
            items_recalc_player_stats(p);
            p->hp = p->max_hp;
            p->mp = p->max_mp;
            char msg[52];
            snprintf(msg, sizeof(msg), "Shrine Blessing Lv.%d! Stats Up & Restored", p->shrine_rank);
            game_set_banner(gs, msg, RGBA8(255, 232, 136, 255), 2.8f);
            game_spawn_float_text(gs, p->wx, p->wy, 20.0f, "BLESSED!", RGBA8(255, 235, 130, 255));
        } else {
            char msg[52];
            snprintf(msg, sizeof(msg), "Sanctuary Restored! (Blessing: %dG)", bless_cost);
            game_set_banner(gs, msg, RGBA8(150, 245, 190, 255), 2.4f);
            game_spawn_float_text(gs, p->wx, p->wy, 18.0f, "FULL RESTORE", RGBA8(140, 250, 185, 255));
        }
        return 1;
    }

    /* 2. Check Unopened World Chests */
    for (int i = 0; i < MAX_CHESTS; i++) {
        WorldChest *c = &gs->chests[i];
        if (!c->active || c->opened) continue;
        float dx = p->wx - c->wx;
        float dy = p->wy - c->wy;
        if (dx * dx + dy * dy <= 1.65f * 1.65f) {
            open_chest(gs, c);
            return 1;
        }
    }

    return 0;
}
