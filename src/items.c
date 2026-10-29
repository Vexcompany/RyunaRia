#include <stdio.h>
#include <string.h>

#include "items.h"
#include "render.h"

static const char *weapon_bases[5] = {
    "Bronze Blade", "Seeker Glaive", "Runeblade", "Astral Fang", "Rift Cleaver"
};

static const char *armor_bases[5] = {
    "Scout Tunic", "Brigandine", "Warden Mail", "Aegis Coat", "Eclipse Plate"
};

static const char *relic_bases[5] = {
    "Amber Charm", "Wind Signet", "Prism Core", "Solar Sigil", "Void Heart"
};

static const char *rarity_prefixes[4] = {
    "", "Fine ", "Runic ", "Mythic "
};

const char *items_slot_name(ItemSlot slot) {
    switch (slot) {
        case SLOT_WEAPON: return "Weapon";
        case SLOT_ARMOR:  return "Armor";
        case SLOT_RELIC:  return "Relic";
        default:          return "Gear";
    }
}

const char *items_rarity_name(ItemRarity rarity) {
    switch (rarity) {
        case RARITY_COMMON:   return "Common";
        case RARITY_UNCOMMON: return "Uncommon";
        case RARITY_RARE:     return "Rare";
        case RARITY_MYTHIC:   return "Mythic";
        default:              return "Common";
    }
}

unsigned int items_rarity_color(ItemRarity rarity) {
    switch (rarity) {
        case RARITY_COMMON:   return RGBA8(214, 218, 228, 255);
        case RARITY_UNCOMMON: return RGBA8(102, 232, 148, 255);
        case RARITY_RARE:     return RGBA8(104, 188, 255, 255);
        case RARITY_MYTHIC:   return RGBA8(255, 204, 88, 255);
        default:              return RGBA8(214, 218, 228, 255);
    }
}

void items_recalc_player_stats(Player *p) {
    float old_max_hp = p->max_hp > 1.0f ? p->max_hp : 100.0f;
    float old_max_mp = p->max_mp > 1.0f ? p->max_mp : 50.0f;
    float hp_pct = p->hp / old_max_hp;
    float mp_pct = p->mp / old_max_mp;
    if (hp_pct > 1.0f) hp_pct = 1.0f;
    if (mp_pct > 1.0f) mp_pct = 1.0f;

    int base_hp   = 95 + (p->level - 1) * 18 + p->shrine_rank * 15;
    int base_mp   = 48 + (p->level - 1) * 7  + p->shrine_rank * 5;
    int base_atk  = 11 + (p->level - 1) * 3  + p->shrine_rank * 3;
    int base_def  = 4  + (p->level - 1) * 2  + p->shrine_rank * 2;
    int base_arc  = 10 + (p->level - 1) * 3  + p->shrine_rank * 3;
    int base_crit = 6  + (p->level / 3);
    float base_spd = 4.15f;

    for (int s = 0; s < SLOT_COUNT; s++) {
        const Item *it = &p->equipped[s];
        if (!it->active) continue;
        base_hp   += it->hp_bonus;
        base_atk  += it->atk_bonus;
        base_def  += it->def_bonus;
        base_arc  += it->arc_bonus;
        base_crit += it->crit_bonus;
        base_spd  += it->spd_bonus;
    }

    if (base_crit > 65) base_crit = 65;
    if (base_spd > 6.2f) base_spd = 6.2f;

    p->max_hp = (float)base_hp;
    p->max_mp = (float)base_mp;
    p->max_stamina = 100.0f;
    p->atk = base_atk;
    p->def = base_def;
    p->arc = base_arc;
    p->crit_pct = base_crit;
    p->move_speed = base_spd;

    p->hp = p->max_hp * hp_pct;
    p->mp = p->max_mp * mp_pct;
    if (p->hp < 1.0f) p->hp = 1.0f;
}

void items_init_starter_gear(GameState *gs) {
    Player *p = &gs->player;
    memset(p->equipped, 0, sizeof(p->equipped));
    memset(p->bag, 0, sizeof(p->bag));

    /* Starter Weapon */
    p->equipped[SLOT_WEAPON].active = 1;
    p->equipped[SLOT_WEAPON].slot = SLOT_WEAPON;
    p->equipped[SLOT_WEAPON].rarity = RARITY_COMMON;
    p->equipped[SLOT_WEAPON].level = 1;
    snprintf(p->equipped[SLOT_WEAPON].name, sizeof(p->equipped[SLOT_WEAPON].name), "Wayfarer Blade");
    p->equipped[SLOT_WEAPON].atk_bonus = 5;
    p->equipped[SLOT_WEAPON].arc_bonus = 2;
    p->equipped[SLOT_WEAPON].crit_bonus = 2;
    p->equipped[SLOT_WEAPON].value = 10;

    /* Starter Armor */
    p->equipped[SLOT_ARMOR].active = 1;
    p->equipped[SLOT_ARMOR].slot = SLOT_ARMOR;
    p->equipped[SLOT_ARMOR].rarity = RARITY_COMMON;
    p->equipped[SLOT_ARMOR].level = 1;
    snprintf(p->equipped[SLOT_ARMOR].name, sizeof(p->equipped[SLOT_ARMOR].name), "Linen Coat");
    p->equipped[SLOT_ARMOR].def_bonus = 3;
    p->equipped[SLOT_ARMOR].hp_bonus = 15;
    p->equipped[SLOT_ARMOR].value = 10;

    /* Starter Relic */
    p->equipped[SLOT_RELIC].active = 1;
    p->equipped[SLOT_RELIC].slot = SLOT_RELIC;
    p->equipped[SLOT_RELIC].rarity = RARITY_UNCOMMON;
    p->equipped[SLOT_RELIC].level = 1;
    snprintf(p->equipped[SLOT_RELIC].name, sizeof(p->equipped[SLOT_RELIC].name), "Ryuna Crest");
    p->equipped[SLOT_RELIC].arc_bonus = 4;
    p->equipped[SLOT_RELIC].crit_bonus = 3;
    p->equipped[SLOT_RELIC].spd_bonus = 0.20f;
    p->equipped[SLOT_RELIC].value = 18;

    /* Put one spare arcane-focused weapon in backpack so player can test swapping right away */
    Item spare;
    memset(&spare, 0, sizeof(spare));
    spare.active = 1;
    spare.slot = SLOT_WEAPON;
    spare.rarity = RARITY_UNCOMMON;
    spare.level = 1;
    snprintf(spare.name, sizeof(spare.name), "Fine Runeblade");
    spare.atk_bonus = 4;
    spare.arc_bonus = 7;
    spare.crit_bonus = 3;
    spare.value = 22;
    p->bag[0] = spare;

    p->max_hp = 110.0f;
    p->hp = 110.0f;
    p->max_mp = 48.0f;
    p->mp = 48.0f;
    items_recalc_player_stats(p);
    p->hp = p->max_hp;
    p->mp = p->max_mp;
    p->stamina = p->max_stamina;
}

Item items_roll_equipment(GameState *gs, int target_level, int min_rarity_boost) {
    if (target_level < 1) target_level = 1;
    Item it;
    memset(&it, 0, sizeof(it));
    it.active = 1;
    it.level = target_level;
    it.slot = (ItemSlot)(game_rand(gs) % 3u);

    int roll = (int)(game_rand(gs) % 100u) + gs->player.chain_count * 2 + min_rarity_boost * 18;
    if (roll >= 93) {
        it.rarity = RARITY_MYTHIC;
    } else if (roll >= 72) {
        it.rarity = RARITY_RARE;
    } else if (roll >= 38) {
        it.rarity = RARITY_UNCOMMON;
    } else {
        it.rarity = RARITY_COMMON;
    }

    int tier_idx = (target_level - 1) / 2;
    if (tier_idx < 0) tier_idx = 0;
    if (tier_idx > 4) tier_idx = 4;

    const char *base_name = (it.slot == SLOT_WEAPON) ? weapon_bases[tier_idx]
                          : (it.slot == SLOT_ARMOR)  ? armor_bases[tier_idx]
                                                     : relic_bases[tier_idx];
    snprintf(it.name, sizeof(it.name), "%s%s", rarity_prefixes[it.rarity], base_name);

    float mult = 1.0f + 0.32f * (float)it.rarity;
    int lv = target_level;

    if (it.slot == SLOT_WEAPON) {
        int arcane_lean = (int)(game_rand(gs) % 2u);
        it.atk_bonus = (int)((float)(4 + lv * 3 + (int)(game_rand(gs) % 3u)) * mult);
        it.arc_bonus = (int)((float)((arcane_lean ? 3 : 1) + lv * 2) * mult);
        it.crit_bonus = 1 + (int)it.rarity * 2 + (int)(game_rand(gs) % 3u);
    } else if (it.slot == SLOT_ARMOR) {
        it.def_bonus = (int)((float)(2 + lv * 2 + (int)(game_rand(gs) % 2u)) * mult);
        it.hp_bonus  = (int)((float)(12 + lv * 9) * mult);
        if (it.rarity >= RARITY_RARE) {
            it.arc_bonus = 1 + lv;
        }
    } else {
        it.arc_bonus  = (int)((float)(3 + lv * 2 + (int)(game_rand(gs) % 3u)) * mult);
        it.crit_bonus = 2 + (int)it.rarity * 2 + (int)(game_rand(gs) % 4u);
        it.spd_bonus  = 0.12f + 0.11f * (float)it.rarity;
        if (it.rarity >= RARITY_UNCOMMON) {
            it.hp_bonus = 8 + lv * 4;
        }
    }

    it.value = (8 + lv * 6) * (1 + (int)it.rarity * 2);
    return it;
}

int items_add_to_bag(Player *p, const Item *it) {
    for (int i = 0; i < MAX_INVENTORY; i++) {
        if (!p->bag[i].active) {
            p->bag[i] = *it;
            p->bag[i].active = 1;
            return i;
        }
    }
    return -1;
}

void items_equip_from_bag(GameState *gs, int bag_index) {
    if (bag_index < 0 || bag_index >= MAX_INVENTORY) return;
    Player *p = &gs->player;
    Item *cand = &p->bag[bag_index];
    if (!cand->active) return;

    ItemSlot slot = cand->slot;
    Item prev = p->equipped[slot];
    p->equipped[slot] = *cand;
    if (prev.active) {
        *cand = prev;
    } else {
        memset(cand, 0, sizeof(*cand));
    }

    items_recalc_player_stats(p);
    char msg[52];
    snprintf(msg, sizeof(msg), "Equipped %s", p->equipped[slot].name);
    game_set_banner(gs, msg, items_rarity_color(p->equipped[slot].rarity), 2.0f);
}

void items_salvage_from_bag(GameState *gs, int bag_index) {
    if (bag_index < 0 || bag_index >= MAX_INVENTORY) return;
    Player *p = &gs->player;
    Item *cand = &p->bag[bag_index];
    if (!cand->active) return;

    int gold = cand->value;
    p->gold += gold;
    char msg[52];
    snprintf(msg, sizeof(msg), "Salvaged %s (+%dG)", cand->name, gold);
    memset(cand, 0, sizeof(*cand));
    game_set_banner(gs, msg, RGBA8(255, 224, 118, 255), 1.8f);
}

void items_spawn_ground_gear(GameState *gs, float wx, float wy, const Item *it) {
    for (int i = 0; i < MAX_GROUND_LOOT; i++) {
        if (!gs->loot[i].active) {
            gs->loot[i].active = 1;
            gs->loot[i].wx = wx;
            gs->loot[i].wy = wy;
            gs->loot[i].bob_phase = game_randf(gs) * 6.28f;
            gs->loot[i].is_tonic = 0;
            gs->loot[i].gold_amount = 0;
            gs->loot[i].item = *it;
            return;
        }
    }
}

void items_spawn_ground_tonic(GameState *gs, float wx, float wy, int gold_bonus) {
    for (int i = 0; i < MAX_GROUND_LOOT; i++) {
        if (!gs->loot[i].active) {
            gs->loot[i].active = 1;
            gs->loot[i].wx = wx;
            gs->loot[i].wy = wy;
            gs->loot[i].bob_phase = game_randf(gs) * 6.28f;
            gs->loot[i].is_tonic = 1;
            gs->loot[i].gold_amount = gold_bonus;
            memset(&gs->loot[i].item, 0, sizeof(Item));
            return;
        }
    }
}

void items_update_ground_loot(GameState *gs, float dt) {
    (void)dt;
    Player *p = &gs->player;

    for (int i = 0; i < MAX_GROUND_LOOT; i++) {
        GroundLoot *l = &gs->loot[i];
        if (!l->active) continue;

        float dx = p->wx - l->wx;
        float dy = p->wy - l->wy;
        if (dx * dx + dy * dy <= 0.92f * 0.92f) {
            if (l->is_tonic) {
                p->gold += l->gold_amount;
                if (p->tonics < p->max_tonics) {
                    p->tonics++;
                    game_spawn_float_text(gs, p->wx, p->wy, 16.0f, "+1 TONIC", RGBA8(115, 250, 165, 255));
                } else {
                    p->hp += p->max_hp * 0.18f;
                    if (p->hp > p->max_hp) p->hp = p->max_hp;
                    p->mp += p->max_mp * 0.20f;
                    if (p->mp > p->max_mp) p->mp = p->max_mp;
                    game_spawn_float_text(gs, p->wx, p->wy, 16.0f, "+HEAL/MP", RGBA8(115, 250, 165, 255));
                }
                l->active = 0;
            } else {
                int idx = items_add_to_bag(p, &l->item);
                if (idx >= 0) {
                    char msg[52];
                    snprintf(msg, sizeof(msg), "Looted [%s]", l->item.name);
                    game_set_banner(gs, msg, items_rarity_color(l->item.rarity), 2.2f);
                    game_spawn_float_text(gs, l->wx, l->wy, 16.0f, "+GEAR", items_rarity_color(l->item.rarity));
                    l->active = 0;
                } else {
                    /* Bag full: auto-salvage common/uncommon, or replace weakest common for rare+ */
                    int weakest = -1;
                    for (int b = 0; b < MAX_INVENTORY; b++) {
                        if (p->bag[b].active && p->bag[b].rarity < l->item.rarity) {
                            weakest = b;
                            break;
                        }
                    }
                    if (weakest >= 0) {
                        p->gold += p->bag[weakest].value;
                        p->bag[weakest] = l->item;
                        char msg[52];
                        snprintf(msg, sizeof(msg), "Looted [%s] (Auto-Salvaged old)", l->item.name);
                        game_set_banner(gs, msg, items_rarity_color(l->item.rarity), 2.3f);
                    } else {
                        p->gold += l->item.value;
                        char pop[24];
                        snprintf(pop, sizeof(pop), "+%dG SALVAGE", l->item.value);
                        game_spawn_float_text(gs, l->wx, l->wy, 16.0f, pop, RGBA8(255, 224, 118, 255));
                    }
                    l->active = 0;
                }
            }
        }
    }
}
