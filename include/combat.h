#ifndef RYUNARIA_COMBAT_H
#define RYUNARIA_COMBAT_H

#include "ryunaria.h"

void combat_init(GameState *gs);
void combat_update(GameState *gs, const InputState *in, float dt);

void combat_damage_chests_in_arc(GameState *gs, float cx, float cy, float range);
int combat_use_tonic(GameState *gs);

const char *monster_kind_name(MonsterKind kind);
void monster_format_traits(unsigned int traits, char *buf, int buf_size);
const char *monster_trait_counter_hint(unsigned int traits);
unsigned int monster_primary_trait_color(unsigned int traits);

#endif
