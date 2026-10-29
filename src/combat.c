#include <stdio.h>
#include <string.h>
#include <math.h>

#include "combat.h"
#include "world.h"
#include "items.h"
#include "render.h"

const char *monster_kind_name(MonsterKind kind) {
    switch (kind) {
        case MON_STALKER: return "Mire Stalker";
        case MON_GOLEM:   return "Carapace Golem";
        case MON_WEAVER:  return "Wisp Weaver";
        case MON_WARDEN:  return "Rift Warden";
        default:          return "Fiend";
    }
}

unsigned int monster_primary_trait_color(unsigned int traits) {
    if (traits & ADAPT_IRONHIDE)  return RGBA8(190, 214, 240, 220);
    if (traits & ADAPT_SPELLWARD) return RGBA8(96, 232, 255, 220);
    if (traits & ADAPT_PURSUER)   return RGBA8(255, 214, 92, 220);
    if (traits & ADAPT_FRENZY)    return RGBA8(255, 92, 92, 220);
    if (traits & ADAPT_THORNS)    return RGBA8(206, 124, 255, 220);
    if (traits & ADAPT_LEECHING)  return RGBA8(102, 240, 152, 220);
    return RGBA8(180, 180, 180, 160);
}

void monster_format_traits(unsigned int traits, char *buf, int buf_size) {
    if (!buf || buf_size <= 0) return;
    if (traits == ADAPT_NONE) {
        snprintf(buf, (size_t)buf_size, "Unadapted");
        return;
    }

    buf[0] = '\0';
    int first = 1;
    struct {
        unsigned int bit;
        const char *label;
    } table[6] = {
        {ADAPT_IRONHIDE,  "IRONHIDE"},
        {ADAPT_SPELLWARD, "SPELLWARD"},
        {ADAPT_PURSUER,   "PURSUER"},
        {ADAPT_FRENZY,    "FRENZY"},
        {ADAPT_THORNS,    "THORNS"},
        {ADAPT_LEECHING,  "LEECH"}
    };

    for (int i = 0; i < 6; i++) {
        if (traits & table[i].bit) {
            int used = (int)strlen(buf);
            if (used + 13 >= buf_size) break;
            snprintf(buf + used, (size_t)(buf_size - used), "%s%s", first ? "" : "+", table[i].label);
            first = 0;
        }
    }
}

const char *monster_trait_counter_hint(unsigned int traits) {
    if (traits & ADAPT_IRONHIDE)  return "Weak: [Square] Arcane";
    if (traits & ADAPT_SPELLWARD) return "Crack: [Cross] Melee";
    if (traits & ADAPT_THORNS)    return "Counter: Space / Bolt";
    if (traits & ADAPT_PURSUER)   return "Counter: Side-Step";
    if (traits & ADAPT_FRENZY)    return "High Risk / +30% XP";
    if (traits & ADAPT_LEECHING)  return "Dodge Red Telegraph";
    return "Standard Threat";
}

static void spawn_projectile(GameState *gs, int from_player, float wx, float wy,
                             float dx, float dy, float speed, float life,
                             int damage, unsigned int color, float radius, int pierce) {
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (!gs->projectiles[i].active) {
            Projectile *pr = &gs->projectiles[i];
            pr->active = 1;
            pr->from_player = from_player;
            pr->wx = wx;
            pr->wy = wy;
            pr->vx = dx * speed;
            pr->vy = dy * speed;
            pr->life = life;
            pr->damage = damage;
            pr->color = color;
            pr->radius = radius;
            pr->pierce = pierce;
            return;
        }
    }
}

static unsigned int roll_spawn_adaptations(GameState *gs, MonsterKind kind, ZoneId zone, int is_elite) {
    unsigned int traits = ADAPT_NONE;
    const AdaptationProfile *ap = &gs->adapt;

    /* Base chance rises in higher zones and for elites */
    int base_chance = (zone == ZONE_RUINS) ? 24 : (zone == ZONE_CRYSTAL ? 46 : 65);
    if (is_elite) base_chance += 35;

    if ((int)(game_rand(gs) % 100u) >= base_chance && !is_elite) {
        return ADAPT_NONE;
    }

    /* Bias traits directly from observed player combat habits */
    if (ap->melee_pressure > ap->arcane_pressure + 12.0f) {
        traits |= ((kind == MON_GOLEM) && (game_rand(gs) % 2u)) ? ADAPT_THORNS : ADAPT_IRONHIDE;
    } else if (ap->arcane_pressure > ap->melee_pressure + 10.0f) {
        traits |= ADAPT_SPELLWARD;
    } else if (ap->dash_frequency > 18.0f) {
        traits |= ADAPT_PURSUER;
    } else {
        unsigned int pool[4] = {ADAPT_IRONHIDE, ADAPT_SPELLWARD, ADAPT_PURSUER, ADAPT_LEECHING};
        traits |= pool[game_rand(gs) % 4u];
    }

    if (is_elite || gs->player.chain_count >= 5 || ap->aggression_score > 28.0f) {
        if ((game_rand(gs) % 100u) < (is_elite ? 75u : 35u)) {
            traits |= (game_rand(gs) % 2u) ? ADAPT_FRENZY : ADAPT_LEECHING;
        }
    }

    return traits;
}

static int find_walkable_spawn_in_zone(GameState *gs, ZoneId target_zone, float *out_wx, float *out_wy) {
    for (int attempt = 0; attempt < 40; attempt++) {
        int tx = 2 + (int)(game_rand(gs) % (unsigned int)(MAP_SIZE - 4));
        int ty = 2 + (int)(game_rand(gs) % (unsigned int)(MAP_SIZE - 4));
        float wx = (float)tx + 0.5f;
        float wy = (float)ty + 0.5f;

        if (world_zone_at(wx, wy) != target_zone) continue;
        if (world_point_blocked(gs, wx, wy, 0.42f)) continue;

        /* Don't spawn right on top of Ryuna */
        float dx = wx - gs->player.wx;
        float dy = wy - gs->player.wy;
        if (dx * dx + dy * dy < 6.5f * 6.5f) continue;

        *out_wx = wx;
        *out_wy = wy;
        return 1;
    }
    return 0;
}

static void spawn_monster_slot(GameState *gs, int slot, ZoneId zone, int force_elite) {
    float wx = 8.5f, wy = 8.5f;
    if (!find_walkable_spawn_in_zone(gs, zone, &wx, &wy)) {
        return;
    }

    Monster *m = &gs->monsters[slot];
    memset(m, 0, sizeof(*m));
    m->active = 1;
    m->home_zone = zone;
    m->wx = wx;
    m->wy = wy;
    m->spawn_wx = wx;
    m->spawn_wy = wy;

    int zone_base_lv = (zone == ZONE_RUINS) ? 1 : (zone == ZONE_CRYSTAL ? 3 : 5);
    int scaled_lv = zone_base_lv + (gs->player.level - 1) / 2 + (int)(game_rand(gs) % 2u);
    if (scaled_lv < zone_base_lv) scaled_lv = zone_base_lv;

    m->is_elite = force_elite || ((zone == ZONE_ASHEN) && ((game_rand(gs) % 100u) < 22u));
    if (m->is_elite) {
        scaled_lv += 1;
    }
    m->level = scaled_lv;

    if (m->is_elite && zone == ZONE_ASHEN) {
        m->kind = MON_WARDEN;
    } else if (zone == ZONE_RUINS) {
        m->kind = (game_rand(gs) % 3u == 0u) ? MON_GOLEM : MON_STALKER;
    } else if (zone == ZONE_CRYSTAL) {
        unsigned int r = game_rand(gs) % 3u;
        m->kind = (r == 0u) ? MON_WEAVER : (r == 1u ? MON_STALKER : MON_GOLEM);
    } else {
        unsigned int r = game_rand(gs) % 4u;
        m->kind = (r == 0u) ? MON_WARDEN : (r == 1u ? MON_GOLEM : (r == 2u ? MON_WEAVER : MON_STALKER));
    }

    float hp_mult = m->is_elite ? 2.15f : 1.0f;
    switch (m->kind) {
        case MON_STALKER:
            m->max_hp = (36.0f + (float)scaled_lv * 16.0f) * hp_mult;
            m->atk = 8 + scaled_lv * 3 + (m->is_elite ? 4 : 0);
            m->def = 2 + scaled_lv;
            m->move_speed = 2.65f;
            break;
        case MON_GOLEM:
            m->max_hp = (58.0f + (float)scaled_lv * 24.0f) * hp_mult;
            m->atk = 11 + scaled_lv * 4 + (m->is_elite ? 5 : 0);
            m->def = 5 + scaled_lv * 2;
            m->move_speed = 1.75f;
            break;
        case MON_WEAVER:
            m->max_hp = (32.0f + (float)scaled_lv * 14.0f) * hp_mult;
            m->atk = 9 + scaled_lv * 3 + (m->is_elite ? 4 : 0);
            m->def = 2 + scaled_lv;
            m->move_speed = 2.25f;
            break;
        case MON_WARDEN:
        default:
            m->is_elite = 1;
            m->max_hp = 125.0f + (float)scaled_lv * 36.0f;
            m->atk = 15 + scaled_lv * 4;
            m->def = 6 + scaled_lv * 2;
            m->move_speed = 2.20f;
            break;
    }

    m->traits = roll_spawn_adaptations(gs, m->kind, zone, m->is_elite);
    if (m->traits & ADAPT_PURSUER) {
        m->move_speed *= 1.32f;
    }
    if (m->traits & ADAPT_FRENZY) {
        m->atk = (m->atk * 5) / 4;
    }

    m->hp = m->max_hp;
    m->attack_cd = 0.6f + game_randf(gs) * 0.8f;
}

void combat_init(GameState *gs) {
    memset(gs->monsters, 0, sizeof(gs->monsters));
    memset(gs->projectiles, 0, sizeof(gs->projectiles));
    memset(&gs->adapt, 0, sizeof(gs->adapt));

    for (int i = 0; i < MAX_MONSTERS; i++) {
        ZoneId z = (i < 7) ? ZONE_RUINS : (i < 14 ? ZONE_CRYSTAL : ZONE_ASHEN);
        int elite = (i == MAX_MONSTERS - 1);
        spawn_monster_slot(gs, i, z, elite);
    }
}

int combat_use_tonic(GameState *gs) {
    Player *p = &gs->player;
    if (p->tonics <= 0) {
        game_spawn_float_text(gs, p->wx, p->wy, 18.0f, "NO TONICS!", RGBA8(255, 110, 110, 255));
        return 0;
    }
    if (p->hp >= p->max_hp - 0.5f && p->mp >= p->max_mp - 0.5f) {
        return 0;
    }

    p->tonics--;
    p->hp += p->max_hp * 0.48f;
    if (p->hp > p->max_hp) p->hp = p->max_hp;
    p->mp += p->max_mp * 0.38f;
    if (p->mp > p->max_mp) p->mp = p->max_mp;
    p->stamina += 40.0f;
    if (p->stamina > p->max_stamina) p->stamina = p->max_stamina;

    game_spawn_float_text(gs, p->wx, p->wy, 18.0f, "+TONIC HEAL", RGBA8(115, 250, 165, 255));
    return 1;
}

void combat_damage_chests_in_arc(GameState *gs, float cx, float cy, float range) {
    for (int i = 0; i < MAX_CHESTS; i++) {
        WorldChest *c = &gs->chests[i];
        if (!c->active || c->opened) continue;
        float dx = c->wx - cx;
        float dy = c->wy - cy;
        if (dx * dx + dy * dy <= range * range) {
            /* Open chest via combat swing too! */
            float save_x = gs->player.wx;
            float save_y = gs->player.wy;
            gs->player.wx = c->wx;
            gs->player.wy = c->wy;
            world_try_interact(gs);
            gs->player.wx = save_x;
            gs->player.wy = save_y;
        }
    }
}

static void award_kill_rewards(GameState *gs, const Monster *m) {
    Player *p = &gs->player;
    p->total_kills++;
    if (m->is_elite) p->boss_kills++;

    p->chain_count++;
    p->chain_timer = 7.5f;
    gs->adapt.aggression_score += 5.5f;
    if (gs->adapt.aggression_score > 60.0f) gs->adapt.aggression_score = 60.0f;

    float chain_mult = 1.0f + (float)(p->chain_count - 1) * 0.12f;
    if (chain_mult > 2.5f) chain_mult = 2.5f;
    if (m->traits & ADAPT_FRENZY) chain_mult += 0.30f;

    int xp_gain = (int)((float)(14 + m->level * 9 + (m->is_elite ? 32 : 0)) * chain_mult);
    int gold_gain = (int)((float)(5 + m->level * 4 + (m->is_elite ? 18 : 0)) * chain_mult);

    p->xp += xp_gain;
    p->gold += gold_gain;

    char pop[24];
    snprintf(pop, sizeof(pop), "+%d XP", xp_gain);
    game_spawn_float_text(gs, m->wx, m->wy, 16.0f, pop, RGBA8(165, 225, 255, 255));

    /* Drop loot rolls */
    int gear_chance = 26 + p->chain_count * 3 + (m->is_elite ? 75 : 0);
    if ((int)(game_rand(gs) % 100u) < gear_chance) {
        Item drop = items_roll_equipment(gs, m->level, m->is_elite ? 1 : 0);
        items_spawn_ground_gear(gs, m->wx, m->wy, &drop);
    } else if ((game_rand(gs) % 100u) < 34u) {
        items_spawn_ground_tonic(gs, m->wx, m->wy, 4 + m->level * 2);
    }

    /* Check Level Up */
    while (p->xp >= p->xp_next) {
        p->xp -= p->xp_next;
        p->level++;
        p->xp_next = 45 + (p->level - 1) * 35 + (p->level - 1) * (p->level - 1) * 8;
        items_recalc_player_stats(p);
        p->hp = p->max_hp;
        p->mp = p->max_mp;
        p->stamina = p->max_stamina;

        char lv_msg[52];
        snprintf(lv_msg, sizeof(lv_msg), "LEVEL UP! Ryuna reached Lv.%d!", p->level);
        game_set_banner(gs, lv_msg, RGBA8(255, 232, 124, 255), 3.0f);
        game_spawn_float_text(gs, p->wx, p->wy, 24.0f, "LEVEL UP!", RGBA8(255, 236, 120, 255));
    }
}

static void monster_try_midfight_adapt(GameState *gs, Monster *m) {
    if (m->adapted_midfight || m->hp <= 0.0f || m->hp > m->max_hp * 0.62f) {
        return;
    }

    if (m->melee_hits_taken >= 3 && !(m->traits & ADAPT_IRONHIDE)) {
        m->traits |= ADAPT_IRONHIDE;
        m->adapted_midfight = 1;
        m->adapt_flash = 0.45f;
        gs->adapt.adaptations_triggered++;
        game_spawn_float_text(gs, m->wx, m->wy, 20.0f, "ADAPT: IRONHIDE!", RGBA8(190, 228, 255, 255));
    } else if (m->arcane_hits_taken >= 2 && !(m->traits & ADAPT_SPELLWARD)) {
        m->traits |= ADAPT_SPELLWARD;
        m->adapted_midfight = 1;
        m->adapt_flash = 0.45f;
        gs->adapt.adaptations_triggered++;
        game_spawn_float_text(gs, m->wx, m->wy, 20.0f, "ADAPT: SPELLWARD!", RGBA8(110, 240, 255, 255));
    }
}

static void apply_damage_to_monster(GameState *gs, int mon_idx, int raw_damage, int is_arcane, float kb_x, float kb_y) {
    Monster *m = &gs->monsters[mon_idx];
    if (!m->active) return;

    Player *p = &gs->player;
    float mult = 1.0f;
    int ward_cracked = 0;
    int armor_pierced = 0;

    if (!is_arcane) {
        m->melee_hits_taken++;
        gs->adapt.melee_pressure += 6.0f;
        if (gs->adapt.melee_pressure > 70.0f) gs->adapt.melee_pressure = 70.0f;

        if (m->traits & ADAPT_SPELLWARD) {
            /* Counterplay: Melee shatters Spellward! */
            m->traits &= ~ADAPT_SPELLWARD;
            mult *= 1.30f;
            ward_cracked = 1;
        }
        if (m->traits & ADAPT_IRONHIDE) {
            mult *= 0.60f;
        }
        if (m->traits & ADAPT_THORNS) {
            if (p->iframe_timer <= 0.0f) {
                float thorn_dmg = 4.0f + (float)m->level * 1.4f;
                p->hp -= thorn_dmg;
                p->hurt_flash = 0.14f;
                game_spawn_float_text(gs, p->wx, p->wy, 16.0f, "THORNS!", RGBA8(215, 130, 255, 255));
            }
        }
    } else {
        m->arcane_hits_taken++;
        gs->adapt.arcane_pressure += 6.5f;
        if (gs->adapt.arcane_pressure > 70.0f) gs->adapt.arcane_pressure = 70.0f;

        if (m->traits & ADAPT_IRONHIDE) {
            /* Counterplay: Arcane pierces Ironhide! */
            mult *= 1.32f;
            armor_pierced = 1;
        }
        if (m->traits & ADAPT_SPELLWARD) {
            mult *= 0.52f;
        }
    }

    int is_crit = ((int)(game_rand(gs) % 100u) < p->crit_pct);
    if (is_crit) {
        mult *= 1.55f;
    }

    int mitigated = raw_damage - (is_arcane ? (m->def / 3) : (m->def / 2));
    if (mitigated < 2) mitigated = 2;
    int final_dmg = (int)((float)mitigated * mult);
    if (final_dmg < 1) final_dmg = 1;

    m->hp -= (float)final_dmg;
    m->hurt_flash = 0.14f;
    gs->locked_monster = mon_idx;

    float kb_scale = (m->kind == MON_GOLEM || m->is_elite) ? 0.35f : 0.72f;
    world_move_slide(gs, &m->wx, &m->wy, kb_x * kb_scale, kb_y * kb_scale, 0.38f);

    char num[24];
    if (ward_cracked) {
        snprintf(num, sizeof(num), "%d CRACK!", final_dmg);
        game_spawn_float_text(gs, m->wx, m->wy, 16.0f, num, RGBA8(130, 245, 255, 255));
    } else if (armor_pierced) {
        snprintf(num, sizeof(num), "%d PIERCE!", final_dmg);
        game_spawn_float_text(gs, m->wx, m->wy, 16.0f, num, RGBA8(180, 225, 255, 255));
    } else if (is_crit) {
        snprintf(num, sizeof(num), "%d!", final_dmg);
        game_spawn_float_text(gs, m->wx, m->wy, 16.0f, num, RGBA8(255, 226, 96, 255));
    } else {
        unsigned int col = (mult < 0.8f) ? RGBA8(170, 178, 195, 255) : RGBA8(255, 245, 240, 255);
        snprintf(num, sizeof(num), "%d", final_dmg);
        game_spawn_float_text(gs, m->wx, m->wy, 14.0f, num, col);
    }

    if (m->hp <= 0.0f) {
        m->active = 0;
        if (gs->locked_monster == mon_idx) {
            gs->locked_monster = -1;
        }
        gs->shake_timer = m->is_elite ? 0.22f : 0.08f;
        gs->shake_mag = m->is_elite ? 0.14f : 0.06f;
        award_kill_rewards(gs, m);
    } else {
        monster_try_midfight_adapt(gs, m);
    }
}

static void damage_player(GameState *gs, Monster *source, int raw_atk) {
    Player *p = &gs->player;
    if (p->iframe_timer > 0.0f) return;

    int dmg = raw_atk - (p->def * 2) / 3;
    int min_dmg = 2 + raw_atk / 6;
    if (dmg < min_dmg) dmg = min_dmg;

    p->hp -= (float)dmg;
    p->iframe_timer = 0.45f;
    p->hurt_flash = 0.22f;
    gs->shake_timer = 0.14f;
    gs->shake_mag = 0.10f;

    char pop[24];
    snprintf(pop, sizeof(pop), "-%d", dmg);
    game_spawn_float_text(gs, p->wx, p->wy, 18.0f, pop, RGBA8(255, 92, 92, 255));

    if (source && (source->traits & ADAPT_LEECHING) && source->active) {
        float heal = (float)dmg * 0.65f;
        source->hp += heal;
        if (source->hp > source->max_hp) source->hp = source->max_hp;
        game_spawn_float_text(gs, source->wx, source->wy, 16.0f, "+LEECH", RGBA8(110, 245, 155, 255));
    }

    if (p->hp <= 0.0f) {
        /* Respawn at Sunlit Sanctuary keeping Level, XP & Equipment */
        int lost_gold = p->gold / 6;
        p->gold -= lost_gold;
        p->chain_count = 0;
        p->chain_timer = 0.0f;
        p->wx = (float)(MAP_SIZE / 2) + 0.5f;
        p->wy = (float)(MAP_SIZE / 2) + 2.2f;
        p->hp = p->max_hp;
        p->mp = p->max_mp;
        p->stamina = p->max_stamina;
        if (p->tonics < 2) p->tonics = 2;
        p->iframe_timer = 1.5f;
        gs->locked_monster = -1;

        char msg[52];
        snprintf(msg, sizeof(msg), "Returned to Sanctuary (-%dG, Gear Kept)", lost_gold);
        game_set_banner(gs, msg, RGBA8(255, 168, 120, 255), 3.0f);
    }
}

static void lock_nearest_monster(GameState *gs) {
    float best_d2 = 11.5f * 11.5f;
    int best = -1;
    for (int i = 0; i < MAX_MONSTERS; i++) {
        if (!gs->monsters[i].active) continue;
        float dx = gs->monsters[i].wx - gs->player.wx;
        float dy = gs->monsters[i].wy - gs->player.wy;
        float d2 = dx * dx + dy * dy;
        if (d2 < best_d2) {
            best_d2 = d2;
            best = i;
        }
    }
    gs->locked_monster = best;
}

void combat_update(GameState *gs, const InputState *in, float dt) {
    Player *p = &gs->player;

    /* Natural decay of Ecosystem Adaptation Telemetry */
    gs->adapt.melee_pressure   *= (1.0f - 0.06f * dt);
    gs->adapt.arcane_pressure  *= (1.0f - 0.06f * dt);
    gs->adapt.dash_frequency   *= (1.0f - 0.08f * dt);
    gs->adapt.aggression_score *= (1.0f - 0.07f * dt);

    /* Cooldowns & Resource Regen */
    if (p->attack_timer > 0.0f)    p->attack_timer -= dt;
    if (p->attack_cooldown > 0.0f) p->attack_cooldown -= dt;
    if (p->combo_window > 0.0f)    p->combo_window -= dt;
    if (p->cast_cooldown > 0.0f)   p->cast_cooldown -= dt;
    if (p->nova_flash_timer > 0.0f) p->nova_flash_timer -= dt;
    if (p->dash_timer > 0.0f)      p->dash_timer -= dt;
    if (p->dash_cooldown > 0.0f)   p->dash_cooldown -= dt;
    if (p->iframe_timer > 0.0f)    p->iframe_timer -= dt;
    if (p->hurt_flash > 0.0f)      p->hurt_flash -= dt;

    if (p->chain_timer > 0.0f) {
        p->chain_timer -= dt;
        if (p->chain_timer <= 0.0f) {
            p->chain_count = 0;
        }
    }

    ZoneId cur_zone = world_zone_at(p->wx, p->wy);
    float mp_regen = (cur_zone == ZONE_SANCTUARY) ? 9.0f : 3.2f;
    float hp_regen = (cur_zone == ZONE_SANCTUARY) ? 14.0f : 0.45f;
    p->mp += mp_regen * dt;
    if (p->mp > p->max_mp) p->mp = p->max_mp;
    p->hp += hp_regen * dt;
    if (p->hp > p->max_hp) p->hp = p->max_hp;
    if (p->dash_timer <= 0.0f) {
        p->stamina += 26.0f * dt;
        if (p->stamina > p->max_stamina) p->stamina = p->max_stamina;
    }

    /* Validate current locked target */
    if (gs->locked_monster >= 0) {
        Monster *lm = &gs->monsters[gs->locked_monster];
        float dx = lm->wx - p->wx;
        float dy = lm->wy - p->wy;
        if (!lm->active || (dx * dx + dy * dy > 13.5f * 13.5f)) {
            gs->locked_monster = -1;
        }
    }

    /* L Trigger: Lock-on nearest monster */
    if ((in->pressed & PSP_CTRL_LTRIGGER) && !(in->held & PSP_CTRL_SQUARE)) {
        lock_nearest_monster(gs);
    }

    /* Convert screen-space stick/D-pad input into isometric world vector */
    float move_wx = (in->move_x + in->move_y) * 0.70710678f;
    float move_wy = (-in->move_x + in->move_y) * 0.70710678f;
    float mlen = sqrtf(move_wx * move_wx + move_wy * move_wy);
    if (mlen > 1.0f) {
        move_wx /= mlen;
        move_wy /= mlen;
    }

    if (mlen > 0.12f && p->attack_timer <= 0.0f) {
        p->facing_x = move_wx / mlen;
        p->facing_y = move_wy / mlen;
    } else if (gs->locked_monster >= 0) {
        Monster *lm = &gs->monsters[gs->locked_monster];
        float dx = lm->wx - p->wx;
        float dy = lm->wy - p->wy;
        float d = sqrtf(dx * dx + dy * dy);
        if (d > 0.05f) {
            p->facing_x = dx / d;
            p->facing_y = dy / d;
        }
    }

    /* Circle: Shadow Dash (with i-frames) */
    if ((in->pressed & PSP_CTRL_CIRCLE) && p->dash_cooldown <= 0.0f && p->stamina >= 22.0f) {
        p->stamina -= 22.0f;
        p->dash_timer = 0.17f;
        p->dash_cooldown = 0.46f;
        p->iframe_timer = 0.26f;
        float dx = (mlen > 0.15f) ? (move_wx / mlen) : p->facing_x;
        float dy = (mlen > 0.15f) ? (move_wy / mlen) : p->facing_y;
        p->vx = dx * 11.4f;
        p->vy = dy * 11.4f;
        gs->adapt.dash_frequency += 5.0f;
        if (gs->adapt.dash_frequency > 60.0f) gs->adapt.dash_frequency = 60.0f;
    }

    /* Cross: 3-Hit Melee Combo */
    if ((in->pressed & PSP_CTRL_CROSS) && p->attack_cooldown <= 0.0f && p->dash_timer <= 0.0f) {
        if (p->combo_window > 0.0f) {
            p->combo_step = (p->combo_step + 1) % 3;
        } else {
            p->combo_step = 0;
        }

        if (gs->locked_monster < 0) {
            lock_nearest_monster(gs);
            if (gs->locked_monster >= 0) {
                Monster *lm = &gs->monsters[gs->locked_monster];
                float dx = lm->wx - p->wx;
                float dy = lm->wy - p->wy;
                float d = sqrtf(dx * dx + dy * dy);
                if (d > 0.05f && d < 3.0f) {
                    p->facing_x = dx / d;
                    p->facing_y = dy / d;
                }
            }
        }

        p->swing_angle = atan2f(p->facing_y, p->facing_x);
        p->swing_hit_mask = 0u;
        p->attack_timer = (p->combo_step == 2) ? 0.22f : 0.16f;
        p->attack_cooldown = (p->combo_step == 2) ? 0.34f : 0.21f;
        p->combo_window = 0.62f;

        /* Slight forward combat step */
        world_move_slide(gs, &p->wx, &p->wy, p->facing_x * 0.26f, p->facing_y * 0.26f, 0.34f);
        combat_damage_chests_in_arc(gs, p->wx + p->facing_x * 0.8f, p->wy + p->facing_y * 0.8f, 1.35f);
    }

    /* Resolve active melee slash hits */
    if (p->attack_timer > 0.0f) {
        float reach = (p->combo_step == 2) ? 2.15f : 1.75f;
        float cos_limit = (p->combo_step == 2) ? -0.35f : 0.20f;
        float mult = (p->combo_step == 0) ? 1.0f : (p->combo_step == 1 ? 1.24f : 1.68f);
        int raw = (int)((float)p->atk * mult);

        for (int i = 0; i < MAX_MONSTERS; i++) {
            if (!gs->monsters[i].active) continue;
            if (p->swing_hit_mask & (1u << i)) continue;

            float dx = gs->monsters[i].wx - p->wx;
            float dy = gs->monsters[i].wy - p->wy;
            float d2 = dx * dx + dy * dy;
            if (d2 > reach * reach) continue;

            float d = sqrtf(d2);
            float dot = (d > 0.05f) ? ((dx * cosf(p->swing_angle) + dy * sinf(p->swing_angle)) / d) : 1.0f;
            if (dot >= cos_limit || d < 0.72f) {
                p->swing_hit_mask |= (1u << i);
                float kbx = (d > 0.05f) ? (dx / d) : cosf(p->swing_angle);
                float kby = (d > 0.05f) ? (dy / d) : sinf(p->swing_angle);
                float kb_force = (p->combo_step == 2) ? 0.85f : 0.38f;
                apply_damage_to_monster(gs, i, raw, 0, kbx * kb_force, kby * kb_force);
            }
        }
    }

    /* Square: Arcane Bolt OR (L + Square) Arcane Nova Burst */
    if ((in->pressed & PSP_CTRL_SQUARE) && p->cast_cooldown <= 0.0f) {
        if (in->held & PSP_CTRL_LTRIGGER) {
            /* Arcane Nova radial AoE */
            if (p->mp >= 20.0f) {
                p->mp -= 20.0f;
                p->cast_cooldown = 0.55f;
                p->nova_flash_timer = 0.35f;
                gs->shake_timer = 0.12f;
                gs->shake_mag = 0.08f;

                int raw = (int)((float)p->arc * 1.65f + (float)p->atk * 0.35f);
                for (int i = 0; i < MAX_MONSTERS; i++) {
                    if (!gs->monsters[i].active) continue;
                    float dx = gs->monsters[i].wx - p->wx;
                    float dy = gs->monsters[i].wy - p->wy;
                    float d2 = dx * dx + dy * dy;
                    if (d2 <= 3.1f * 3.1f) {
                        float d = sqrtf(d2) + 0.01f;
                        apply_damage_to_monster(gs, i, raw, 1, (dx / d) * 0.9f, (dy / d) * 0.9f);
                    }
                }
            } else {
                game_spawn_float_text(gs, p->wx, p->wy, 18.0f, "LOW MP", RGBA8(130, 175, 255, 255));
            }
        } else {
            /* Directed Arcane Bolt */
            if (p->mp >= 10.0f) {
                p->mp -= 10.0f;
                p->cast_cooldown = 0.28f;
                if (gs->locked_monster >= 0 && gs->monsters[gs->locked_monster].active) {
                    Monster *lm = &gs->monsters[gs->locked_monster];
                    float dx = lm->wx - p->wx;
                    float dy = lm->wy - p->wy;
                    float d = sqrtf(dx * dx + dy * dy);
                    if (d > 0.05f) {
                        p->facing_x = dx / d;
                        p->facing_y = dy / d;
                    }
                }
                int dmg = (int)((float)p->arc * 1.38f + 4.0f);
                int pierce = (p->arc >= 22) ? 1 : 0;
                spawn_projectile(gs, 1, p->wx + p->facing_x * 0.45f, p->wy + p->facing_y * 0.45f,
                                 p->facing_x, p->facing_y, 10.2f, 0.85f,
                                 dmg, RGBA8(108, 224, 255, 255), 0.42f, pierce);
            } else {
                game_spawn_float_text(gs, p->wx, p->wy, 18.0f, "LOW MP", RGBA8(130, 175, 255, 255));
            }
        }
    }

    /* Triangle: Context Interact (Shrine/Chest), else Drink Tonic; R Trigger: Always Tonic */
    if (in->pressed & PSP_CTRL_TRIANGLE) {
        if (!world_try_interact(gs)) {
            combat_use_tonic(gs);
        }
    } else if (in->pressed & PSP_CTRL_RTRIGGER) {
        combat_use_tonic(gs);
    }

    /* Apply Player Movement */
    if (p->dash_timer <= 0.0f) {
        float spd = (p->attack_timer > 0.0f) ? (p->move_speed * 0.45f) : p->move_speed;
        p->vx = move_wx * spd;
        p->vy = move_wy * spd;
    }
    world_move_slide(gs, &p->wx, &p->wy, p->vx * dt, p->vy * dt, 0.34f);
    if (fabsf(p->vx) + fabsf(p->vy) > 0.2f) {
        p->walk_anim += dt;
    }

    /* Ember Vent environmental hazard tick in Ashen Wastes */
    {
        int ptx = (int)floorf(p->wx);
        int pty = (int)floorf(p->wy);
        if (ptx >= 0 && pty >= 0 && ptx < MAP_SIZE && pty < MAP_SIZE &&
            gs->tiles[pty][ptx] == TILE_EMBER_VENT && p->iframe_timer <= 0.0f && p->dash_timer <= 0.0f) {
            damage_player(gs, NULL, 9);
        }
    }

    /* Update Projectiles */
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        Projectile *pr = &gs->projectiles[i];
        if (!pr->active) continue;

        pr->wx += pr->vx * dt;
        pr->wy += pr->vy * dt;
        pr->life -= dt;

        if (pr->life <= 0.0f || world_point_blocked(gs, pr->wx, pr->wy, 0.15f)) {
            pr->active = 0;
            continue;
        }

        if (pr->from_player) {
            for (int m_i = 0; m_i < MAX_MONSTERS; m_i++) {
                Monster *m = &gs->monsters[m_i];
                if (!m->active) continue;
                float dx = m->wx - pr->wx;
                float dy = m->wy - pr->wy;
                float hit_r = pr->radius + (m->is_elite ? 0.55f : 0.40f);
                if (dx * dx + dy * dy <= hit_r * hit_r) {
                    float vlen = sqrtf(pr->vx * pr->vx + pr->vy * pr->vy) + 0.01f;
                    apply_damage_to_monster(gs, m_i, pr->damage, 1, (pr->vx / vlen) * 0.35f, (pr->vy / vlen) * 0.35f);
                    if (pr->pierce > 0) {
                        pr->pierce--;
                    } else {
                        pr->active = 0;
                    }
                    break;
                }
            }
        } else {
            /* Monster projectile hitting Ryuna */
            if (world_zone_at(pr->wx, pr->wy) == ZONE_SANCTUARY) {
                pr->active = 0;
                continue;
            }
            float dx = p->wx - pr->wx;
            float dy = p->wy - pr->wy;
            if (dx * dx + dy * dy <= (pr->radius + 0.36f) * (pr->radius + 0.36f)) {
                damage_player(gs, NULL, pr->damage);
                pr->active = 0;
            }
        }
    }

    /* Update Adaptive Monsters & Respawns */
    int active_count = 0;
    for (int i = 0; i < MAX_MONSTERS; i++) {
        Monster *m = &gs->monsters[i];
        if (!m->active) continue;
        active_count++;

        if (m->hurt_flash > 0.0f)  m->hurt_flash -= dt;
        if (m->adapt_flash > 0.0f) m->adapt_flash -= dt;
        if (m->attack_cd > 0.0f)   m->attack_cd -= dt;

        /* Monsters respect the Sunlit Sanctuary ward and retreat to spawn */
        int player_safe = (cur_zone == ZONE_SANCTUARY);
        float dx = p->wx - m->wx;
        float dy = p->wy - m->wy;
        float dist = sqrtf(dx * dx + dy * dy);

        if (player_safe || dist > 10.5f) {
            m->windup_timer = 0.0f;
            float hdx = m->spawn_wx - m->wx;
            float hdy = m->spawn_wy - m->wy;
            float hd = sqrtf(hdx * hdx + hdy * hdy);
            if (hd > 0.35f) {
                world_move_slide(gs, &m->wx, &m->wy,
                                 (hdx / hd) * m->move_speed * 0.8f * dt,
                                 (hdy / hd) * m->move_speed * 0.8f * dt, 0.38f);
            }
            continue;
        }

        /* Handle active attack windup & execution */
        if (m->windup_timer > 0.0f) {
            m->windup_timer -= dt;
            if (m->windup_timer <= 0.0f) {
                m->windup_timer = 0.0f;
                m->attack_cd = (m->traits & ADAPT_FRENZY) ? 0.85f : 1.25f;

                if (m->kind == MON_WEAVER) {
                    if (dist > 0.05f) {
                        spawn_projectile(gs, 0, m->wx, m->wy, dx / dist, dy / dist,
                                         6.2f, 1.6f, m->atk, RGBA8(214, 110, 255, 255), 0.36f, 0);
                    }
                } else if (m->kind == MON_WARDEN) {
                    /* Boss radial 4-orb volley + heavy shockwave */
                    for (int k = 0; k < 4; k++) {
                        float ang = atan2f(dy, dx) + (float)k * 1.5707963f;
                        spawn_projectile(gs, 0, m->wx, m->wy, cosf(ang), sinf(ang),
                                         5.6f, 1.7f, m->atk, RGBA8(255, 130, 80, 255), 0.38f, 0);
                    }
                    if (dist <= 2.35f) {
                        damage_player(gs, m, m->atk + 4);
                    }
                } else if (m->kind == MON_GOLEM) {
                    if (dist <= 2.25f) {
                        damage_player(gs, m, m->atk + 3);
                    }
                } else {
                    /* Stalker lunge strike */
                    float lunge_step = (m->traits & ADAPT_PURSUER) ? 1.15f : 0.78f;
                    world_move_slide(gs, &m->wx, &m->wy, m->lunge_vx * lunge_step, m->lunge_vy * lunge_step, 0.36f);
                    float ndx = p->wx - m->wx;
                    float ndy = p->wy - m->wy;
                    if (ndx * ndx + ndy * ndy <= 1.45f * 1.45f) {
                        damage_player(gs, m, m->atk);
                    }
                }
            }
            continue;
        }

        /* Chase / Kite AI & Initiate Telegraph */
        float trig_range = (m->kind == MON_WEAVER) ? 5.8f
                         : (m->kind == MON_WARDEN) ? 3.8f
                         : (m->kind == MON_GOLEM)  ? 1.85f
                         : ((m->traits & ADAPT_PURSUER) ? 2.35f : 1.75f);

        if (m->kind == MON_WEAVER && dist < 2.6f && dist > 0.05f) {
            /* Weaver kites away from Ryuna */
            world_move_slide(gs, &m->wx, &m->wy,
                             -(dx / dist) * m->move_speed * dt,
                             -(dy / dist) * m->move_speed * dt, 0.36f);
        } else if (dist > trig_range * 0.82f && dist > 0.05f) {
            float nx = (dx / dist) * m->move_speed * dt;
            float ny = (dy / dist) * m->move_speed * dt;
            /* Prevent stepping into Sanctuary plaza */
            if (world_zone_at(m->wx + nx, m->wy + ny) != ZONE_SANCTUARY) {
                world_move_slide(gs, &m->wx, &m->wy, nx, ny, 0.38f);
            }
        }

        if (dist <= trig_range && m->attack_cd <= 0.0f) {
            float base_windup = (m->kind == MON_GOLEM) ? 0.66f
                              : (m->kind == MON_WARDEN) ? 0.62f
                              : (m->kind == MON_WEAVER) ? 0.50f : 0.42f;
            if (m->traits & ADAPT_FRENZY) {
                base_windup *= 0.78f;
            }
            m->windup_total = base_windup;
            m->windup_timer = base_windup;
            if (dist > 0.05f) {
                m->lunge_vx = dx / dist;
                m->lunge_vy = dy / dist;
            }
        }
    }

    /* Gradual Monster Respawn Loop for Grinding */
    gs->respawn_clock += dt;
    if (gs->respawn_clock >= 3.2f && active_count < MAX_MONSTERS) {
        gs->respawn_clock = 0.0f;
        for (int i = 0; i < MAX_MONSTERS; i++) {
            if (!gs->monsters[i].active) {
                ZoneId z = (i < 7) ? ZONE_RUINS : (i < 14 ? ZONE_CRYSTAL : ZONE_ASHEN);
                int apex = ((p->total_kills > 0) && ((p->total_kills % 8) == 0)) || (i == MAX_MONSTERS - 1);
                spawn_monster_slot(gs, i, z, apex);
                break;
            }
        }
    }
}
