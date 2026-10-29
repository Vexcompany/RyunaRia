#ifndef RYUNARIA_WORLD_H
#define RYUNARIA_WORLD_H

#include "ryunaria.h"

void world_generate(GameState *gs);
ZoneId world_zone_at(float wx, float wy);
const char *world_zone_name(ZoneId z);
unsigned int world_zone_color(ZoneId z);

int world_tile_solid(TileType t);
int world_point_blocked(const GameState *gs, float wx, float wy, float radius);
void world_move_slide(const GameState *gs, float *wx, float *wy, float dx, float dy, float radius);

void world_update_chests(GameState *gs, float dt);
int world_try_interact(GameState *gs);

#endif
