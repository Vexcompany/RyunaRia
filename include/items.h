#ifndef RYUNARIA_ITEMS_H
#define RYUNARIA_ITEMS_H

#include "ryunaria.h"

void items_init_starter_gear(GameState *gs);
void items_recalc_player_stats(Player *p);

const char *items_slot_name(ItemSlot slot);
const char *items_rarity_name(ItemRarity rarity);
unsigned int items_rarity_color(ItemRarity rarity);

Item items_roll_equipment(GameState *gs, int target_level, int min_rarity_boost);
int items_add_to_bag(Player *p, const Item *it);
void items_equip_from_bag(GameState *gs, int bag_index);
void items_salvage_from_bag(GameState *gs, int bag_index);

void items_spawn_ground_gear(GameState *gs, float wx, float wy, const Item *it);
void items_spawn_ground_tonic(GameState *gs, float wx, float wy, int gold_bonus);
void items_update_ground_loot(GameState *gs, float dt);

#endif
