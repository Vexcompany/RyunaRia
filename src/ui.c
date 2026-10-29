#include <stdio.h>
#include <string.h>
#include <math.h>

#include "ryunaria.h"
#include "render.h"
#include "world.h"
#include "items.h"
#include "combat.h"

static void draw_bar(float x, float y, float w, float h, float val, float max_val,
                     unsigned int fill_col, unsigned int bg_col) {
    float ratio = (max_val > 0.001f) ? (val / max_val) : 0.0f;
    if (ratio < 0.0f) ratio = 0.0f;
    if (ratio > 1.0f) ratio = 1.0f;
    render_rect(x, y, w, h, bg_col);
    render_rect(x, y, w * ratio, h, fill_col);
    render_rect_outline(x, y, w, h, 1.0f, RGBA8(255, 255, 255, 42));
}

static void draw_minimap(const GameState *gs, float x, float y, float size) {
    render_panel(x, y, size, size, RGBA8(14, 16, 26, 205), RGBA8(130, 140, 175, 160));

    float scale = (size - 4.0f) / (float)MAP_SIZE;
    float ox = x + 2.0f;
    float oy = y + 2.0f;

    /* Sanctuary central square */
    render_rect(ox + 15.0f * scale, oy + 15.0f * scale,
                10.0f * scale, 10.0f * scale, RGBA8(236, 210, 128, 70));

    /* Unopened chests */
    for (int i = 0; i < MAX_CHESTS; i++) {
        if (!gs->chests[i].active || gs->chests[i].opened) continue;
        render_rect(ox + gs->chests[i].wx * scale - 1.0f,
                    oy + gs->chests[i].wy * scale - 1.0f,
                    2.0f, 2.0f, RGBA8(255, 216, 92, 230));
    }

    /* Active monsters */
    for (int i = 0; i < MAX_MONSTERS; i++) {
        const Monster *m = &gs->monsters[i];
        if (!m->active) continue;
        unsigned int mc = m->is_elite ? RGBA8(255, 155, 60, 255) : RGBA8(238, 78, 88, 225);
        float ms = m->is_elite ? 3.0f : 2.0f;
        render_rect(ox + m->wx * scale - 1.0f, oy + m->wy * scale - 1.0f, ms, ms, mc);
    }

    /* Player dot */
    render_rect(ox + gs->player.wx * scale - 1.5f,
                oy + gs->player.wy * scale - 1.5f,
                3.0f, 3.0f, RGBA8(130, 245, 255, 255));
}

static int find_nearby_interact_kind(const GameState *gs) {
    const Player *p = &gs->player;
    float sh_x = (float)(MAP_SIZE / 2) + 0.5f;
    float sh_y = (float)(MAP_SIZE / 2) + 0.5f;
    float sdx = p->wx - sh_x;
    float sdy = p->wy - sh_y;
    if (sdx * sdx + sdy * sdy <= 2.35f * 2.35f) return 1; /* Shrine */

    for (int i = 0; i < MAX_CHESTS; i++) {
        if (!gs->chests[i].active || gs->chests[i].opened) continue;
        float dx = p->wx - gs->chests[i].wx;
        float dy = p->wy - gs->chests[i].wy;
        if (dx * dx + dy * dy <= 1.65f * 1.65f) return 2; /* Chest */
    }
    return 0;
}

static void draw_playing_hud(const GameState *gs) {
    const Player *p = &gs->player;

    /* 1. Top-Left Player Status Panel */
    render_panel(6.0f, 6.0f, 160.0f, 50.0f, RGBA8(16, 18, 30, 196), RGBA8(110, 120, 156, 150));
    render_printf(11.0f, 10.0f, RGBA8(244, 224, 152, 255),
                  "Lv.%d  Tonic:%d/%d  %dG", p->level, p->tonics, p->max_tonics, p->gold);

    draw_bar(11.0f, 21.0f, 150.0f, 8.0f, p->hp, p->max_hp,
             RGBA8(226, 72, 86, 255), RGBA8(42, 20, 28, 220));
    render_printf(14.0f, 21.0f, RGBA8(255, 255, 255, 245),
                  "HP %d/%d", (int)ceilf(p->hp), (int)p->max_hp);

    draw_bar(11.0f, 32.0f, 150.0f, 6.0f, p->mp, p->max_mp,
             RGBA8(72, 178, 248, 255), RGBA8(18, 32, 54, 220));

    /* Stamina (green) & XP (gold) compact dual bars */
    draw_bar(11.0f, 41.0f, 72.0f, 4.0f, p->stamina, p->max_stamina,
             RGBA8(102, 232, 152, 255), RGBA8(18, 38, 28, 220));
    draw_bar(89.0f, 41.0f, 72.0f, 4.0f, (float)p->xp, (float)p->xp_next,
             RGBA8(245, 206, 96, 255), RGBA8(42, 34, 16, 220));

    /* 2. Top-Center Target Adaptive Inspector OR Zone + Streak Header */
    if (gs->locked_monster >= 0 && gs->monsters[gs->locked_monster].active) {
        const Monster *m = &gs->monsters[gs->locked_monster];
        render_panel(172.0f, 6.0f, 242.0f, 46.0f, RGBA8(20, 16, 30, 205),
                     m->is_elite ? RGBA8(255, 190, 92, 210) : RGBA8(150, 110, 140, 170));

        render_printf(178.0f, 10.0f,
                      m->is_elite ? RGBA8(255, 214, 108, 255) : RGBA8(255, 235, 235, 255),
                      "%s%s Lv.%d", m->is_elite ? "[APEX] " : "", monster_kind_name(m->kind), m->level);

        draw_bar(178.0f, 20.0f, 230.0f, 6.0f, m->hp, m->max_hp,
                 m->is_elite ? RGBA8(255, 164, 62, 255) : RGBA8(232, 74, 86, 255),
                 RGBA8(42, 18, 26, 220));

        char trait_buf[40];
        monster_format_traits(m->traits, trait_buf, (int)sizeof(trait_buf));
        unsigned int tc = monster_primary_trait_color(m->traits);
        render_printf(178.0f, 29.0f, tc, "Adapt: %s", trait_buf);
        render_printf(178.0f, 39.0f, RGBA8(185, 215, 240, 245), "%s", monster_trait_counter_hint(m->traits));
    } else {
        ZoneId z = world_zone_at(p->wx, p->wy);
        render_panel(176.0f, 6.0f, 234.0f, 30.0f, RGBA8(16, 18, 30, 185), RGBA8(100, 112, 146, 140));
        render_printf(184.0f, 10.0f, world_zone_color(z), "%s", world_zone_name(z));
        if (p->chain_count > 1) {
            int bonus_pct = (p->chain_count - 1) * 12;
            if (bonus_pct > 150) bonus_pct = 150;
            render_printf(184.0f, 21.0f, RGBA8(255, 216, 102, 255),
                          "HUNT CHAIN x%d (+%d%% XP/Loot)", p->chain_count, bonus_pct);
        } else {
            render_printf(184.0f, 21.0f, RGBA8(176, 186, 210, 230),
                          "Kills: %d   Adaptations: %d", p->total_kills, gs->adapt.adaptations_triggered);
        }
    }

    /* 3. Top-Right Radar Minimap */
    draw_minimap(gs, 420.0f, 6.0f, 54.0f);

    /* 4. Toast Banner */
    if (gs->banner_timer > 0.0f && gs->banner_msg[0]) {
        int tw = render_text_width(gs->banner_msg, 1.0f);
        float bx = (SCREEN_WIDTH - (float)tw) * 0.5f - 8.0f;
        render_panel(bx, 58.0f, (float)tw + 16.0f, 16.0f, RGBA8(14, 16, 28, 215), gs->banner_color);
        render_text_shadow(bx + 8.0f, 62.0f, gs->banner_color, gs->banner_msg);
    }

    /* 5. Bottom Control Strip */
    render_rect(0.0f, SCREEN_HEIGHT - 16.0f, (float)SCREEN_WIDTH, 16.0f, RGBA8(10, 12, 22, 188));
    int interact = find_nearby_interact_kind(gs);
    if (interact == 1) {
        int cost = 60 + p->shrine_rank * 55;
        render_printf(8.0f, SCREEN_HEIGHT - 12.0f, RGBA8(255, 232, 132, 255),
                      "[/\\] Shrine Heal & Bless (%dG)  [SELECT] Gear  [START] Codex", cost);
    } else if (interact == 2) {
        render_printf(8.0f, SCREEN_HEIGHT - 12.0f, RGBA8(255, 224, 118, 255),
                      "[/\\] Open Treasure Chest!  [X] Slash  [[]] Bolt  [O] Dash");
    } else {
        render_printf(8.0f, SCREEN_HEIGHT - 12.0f, RGBA8(205, 214, 235, 240),
                      "[X]Combo [[]]Bolt(L+[]Nova) [O]Dash [/\\]Tonic [L]Lock [SEL]Gear");
    }
}

static void format_item_stats(const Item *it, char *buf, int buf_size) {
    if (!it->active) {
        snprintf(buf, (size_t)buf_size, "Empty");
        return;
    }
    snprintf(buf, (size_t)buf_size, "Lv%d %s | ATK+%d ARC+%d DEF+%d HP+%d CRT+%d%%",
             it->level, items_slot_name(it->slot),
             it->atk_bonus, it->arc_bonus, it->def_bonus, it->hp_bonus, it->crit_bonus);
}

static void draw_inventory_overlay(const GameState *gs) {
    const Player *p = &gs->player;
    render_rect(0.0f, 0.0f, (float)SCREEN_WIDTH, (float)SCREEN_HEIGHT, RGBA8(6, 8, 16, 175));
    render_panel(14.0f, 12.0f, 452.0f, 248.0f, RGBA8(18, 21, 36, 242), RGBA8(236, 210, 134, 220));

    render_printf(24.0f, 19.0f, RGBA8(245, 222, 142, 255),
                  "RYUNA EQUIPMENT & INVENTORY");
    render_printf(318.0f, 19.0f, RGBA8(255, 228, 126, 255),
                  "Gold: %dG  Tonics: %d", p->gold, p->tonics);
    render_line(22.0f, 31.0f, 458.0f, 31.0f, RGBA8(120, 132, 170, 140));

    /* Left Column: Ryuna Stats + Equipped Loadout */
    render_printf(24.0f, 37.0f, RGBA8(165, 225, 255, 255),
                  "Ryuna Lv.%d  (XP %d/%d)", p->level, p->xp, p->xp_next);
    render_printf(24.0f, 49.0f, RGBA8(230, 234, 245, 255),
                  "HP: %d/%d   MP: %d/%d", (int)p->hp, (int)p->max_hp, (int)p->mp, (int)p->max_mp);
    render_printf(24.0f, 60.0f, RGBA8(230, 234, 245, 255),
                  "ATK: %-3d  ARC: %-3d  DEF: %-3d", p->atk, p->arc, p->def);
    render_printf(24.0f, 71.0f, RGBA8(230, 234, 245, 255),
                  "CRIT: %d%%  SPD: %.1f  Bless: +%d", p->crit_pct, p->move_speed, p->shrine_rank);

    render_printf(24.0f, 88.0f, RGBA8(240, 214, 138, 255), "EQUIPPED GEAR:");
    for (int s = 0; s < SLOT_COUNT; s++) {
        const Item *eq = &p->equipped[s];
        float ey = 100.0f + (float)s * 28.0f;
        render_panel(22.0f, ey, 184.0f, 24.0f, RGBA8(26, 30, 48, 230), RGBA8(90, 102, 138, 150));
        render_printf(26.0f, ey + 3.0f, items_rarity_color(eq->rarity),
                      "[%s] %s", items_slot_name((ItemSlot)s), eq->active ? eq->name : "None");
        render_printf(26.0f, ey + 13.0f, RGBA8(178, 188, 212, 240),
                      "ATK+%d ARC+%d DEF+%d HP+%d",
                      eq->atk_bonus, eq->arc_bonus, eq->def_bonus, eq->hp_bonus);
    }

    /* Right Column: 12-Slot Backpack */
    render_printf(216.0f, 37.0f, RGBA8(240, 214, 138, 255), "BACKPACK (12 SLOTS):");
    for (int i = 0; i < MAX_INVENTORY; i++) {
        float iy = 49.0f + (float)i * 12.0f;
        int sel = (i == gs->inv_cursor);
        if (sel) {
            render_rect(214.0f, iy - 1.0f, 242.0f, 11.0f, RGBA8(74, 92, 142, 180));
        }
        const Item *it = &p->bag[i];
        if (it->active) {
            render_printf(218.0f, iy, items_rarity_color(it->rarity),
                          "%s%-18s Lv%-2d %3dG", sel ? ">" : " ", it->name, it->level, it->value);
        } else {
            render_printf(218.0f, iy, RGBA8(108, 116, 138, 200),
                          "%s-- Empty Slot --", sel ? ">" : " ");
        }
    }

    /* Comparison / Selected Item Details Footer */
    render_panel(22.0f, 196.0f, 434.0f, 42.0f, RGBA8(12, 15, 26, 235), RGBA8(118, 132, 175, 170));
    const Item *sel_item = &p->bag[gs->inv_cursor];
    if (sel_item->active) {
        char line1[96];
        format_item_stats(sel_item, line1, (int)sizeof(line1));
        render_printf(28.0f, 201.0f, items_rarity_color(sel_item->rarity), "%s", line1);

        const Item *cur_eq = &p->equipped[sel_item->slot];
        int d_atk = sel_item->atk_bonus - cur_eq->atk_bonus;
        int d_arc = sel_item->arc_bonus - cur_eq->arc_bonus;
        int d_def = sel_item->def_bonus - cur_eq->def_bonus;
        int d_hp  = sel_item->hp_bonus  - cur_eq->hp_bonus;
        render_printf(28.0f, 213.0f, RGBA8(205, 224, 245, 255),
                      "Vs Equipped %s: ATK %+d  ARC %+d  DEF %+d  HP %+d",
                      items_slot_name(sel_item->slot), d_atk, d_arc, d_def, d_hp);
        render_printf(28.0f, 225.0f, RGBA8(180, 195, 220, 240),
                      "[X] Equip/Swap   [[]] Salvage (+%dG)   [O / SELECT] Close", sel_item->value);
    } else {
        render_printf(28.0f, 208.0f, RGBA8(165, 175, 198, 235),
                      "Select a looted item to compare stats, equip, or salvage into Gold.");
        render_printf(28.0f, 222.0f, RGBA8(180, 195, 220, 240),
                      "[Up/Down] Navigate   [O / SELECT] Return to Frontier");
    }
}

static void draw_pause_overlay(const GameState *gs) {
    render_rect(0.0f, 0.0f, (float)SCREEN_WIDTH, (float)SCREEN_HEIGHT, RGBA8(6, 8, 16, 180));
    render_panel(38.0f, 18.0f, 404.0f, 236.0f, RGBA8(18, 22, 38, 244), RGBA8(236, 210, 134, 220));

    render_printf(50.0f, 26.0f, RGBA8(245, 222, 142, 255),
                  "TACTICAL PAUSE & ADAPTIVE MONSTER CODEX");
    render_line(46.0f, 38.0f, 434.0f, 38.0f, RGBA8(120, 132, 170, 140));

    /* Live Ecosystem Adaptation Telemetry */
    render_printf(50.0f, 45.0f, RGBA8(135, 224, 255, 255), "ECOSYSTEM ADAPTATION TELEMETRY:");
    render_printf(50.0f, 58.0f, RGBA8(225, 230, 242, 255), "Melee Bias");
    draw_bar(134.0f, 59.0f, 100.0f, 6.0f, gs->adapt.melee_pressure, 70.0f,
             RGBA8(210, 224, 245, 255), RGBA8(28, 34, 52, 220));

    render_printf(250.0f, 58.0f, RGBA8(225, 230, 242, 255), "Arcane Bias");
    draw_bar(332.0f, 59.0f, 96.0f, 6.0f, gs->adapt.arcane_pressure, 70.0f,
             RGBA8(96, 228, 255, 255), RGBA8(28, 34, 52, 220));

    render_printf(50.0f, 70.0f, RGBA8(225, 230, 242, 255), "Dash/Kiting");
    draw_bar(134.0f, 71.0f, 100.0f, 6.0f, gs->adapt.dash_frequency, 60.0f,
             RGBA8(255, 214, 96, 255), RGBA8(28, 34, 52, 220));

    render_printf(250.0f, 70.0f, RGBA8(225, 230, 242, 255), "Hunt Tempo");
    draw_bar(332.0f, 71.0f, 96.0f, 6.0f, gs->adapt.aggression_score, 60.0f,
             RGBA8(255, 102, 102, 255), RGBA8(28, 34, 52, 220));

    /* Adaptive Mutation Guide */
    render_printf(50.0f, 88.0f, RGBA8(245, 218, 134, 255), "MUTATION COUNTERPLAY:");
    render_printf(50.0f, 100.0f, RGBA8(195, 220, 245, 255),
                  "IRONHIDE  : Resists Melee -> Pierce with [Square] Arcane (+32%%)");
    render_printf(50.0f, 111.0f, RGBA8(115, 236, 255, 255),
                  "SPELLWARD : Resists Bolts -> Shatter ward with [Cross] Melee");
    render_printf(50.0f, 122.0f, RGBA8(255, 216, 102, 255),
                  "PURSUER   : Evolves vs Dashers -> Side-step red lunge ring");
    render_printf(50.0f, 133.0f, RGBA8(214, 140, 255, 255),
                  "THORNS    : Retaliates Melee -> Cast Bolts or Dash i-frame");
    render_printf(50.0f, 144.0f, RGBA8(255, 120, 120, 255),
                  "FRENZY    : Faster windup on high Kill Chains (+30%% XP/Loot)");

    /* Pause Menu Options */
    static const char *opts[3] = {
        "Resume Hunt",
        "Recall to Sunlit Sanctuary",
        "Exit Game to PSP XMB"
    };
    for (int i = 0; i < 3; i++) {
        float oy = 168.0f + (float)i * 20.0f;
        int sel = (gs->pause_cursor == i);
        render_panel(112.0f, oy, 256.0f, 16.0f,
                     sel ? RGBA8(68, 86, 138, 235) : RGBA8(26, 30, 48, 220),
                     sel ? RGBA8(245, 222, 140, 240) : RGBA8(90, 100, 132, 140));
        int tw = render_text_width(opts[i], 1.0f);
        render_text_shadow(240.0f - (float)(tw / 2), oy + 4.0f,
                           sel ? RGBA8(255, 244, 175, 255) : RGBA8(210, 218, 236, 240),
                           opts[i]);
    }

    render_printf(98.0f, 236.0f, RGBA8(170, 182, 208, 235),
                  "[Up/Down] Select   [X] Confirm   [START/O] Resume");
}

static void draw_title_overlay(const GameState *gs) {
    render_rect(0.0f, 0.0f, (float)SCREEN_WIDTH, (float)SCREEN_HEIGHT, RGBA8(10, 12, 24, 135));
    render_panel(64.0f, 42.0f, 352.0f, 188.0f, RGBA8(16, 20, 36, 226), RGBA8(238, 212, 138, 225));

    const char *title = "RYUNA RIA";
    int tw = render_text_width(title, 2.2f);
    render_text_scaled(240.0f - (float)(tw / 2) + 2.0f, 60.0f + 2.0f, 2.2f, RGBA8(0, 0, 0, 200), title);
    render_text_scaled(240.0f - (float)(tw / 2), 60.0f, 2.2f, RGBA8(245, 220, 138, 255), title);

    const char *sub = "Isometric Action RPG Prototype  v" RYUNA_RIA_VERSION;
    int sw = render_text_width(sub, 1.0f);
    render_text_shadow(240.0f - (float)(sw / 2), 88.0f, RGBA8(152, 218, 255, 255), sub);

    render_line(88.0f, 104.0f, 392.0f, 104.0f, RGBA8(120, 136, 178, 150));

    render_printf(92.0f, 114.0f, RGBA8(224, 230, 245, 255),
                  "* Explore 4 Biomes around the Sunlit Sanctuary");
    render_printf(92.0f, 128.0f, RGBA8(224, 230, 245, 255),
                  "* Chain 3-Hit Melee Combos, Arcane Bolts & Nova");
    render_printf(92.0f, 142.0f, RGBA8(224, 230, 245, 255),
                  "* Grind Hunt Streaks & Equip Mythic Loot Drops");
    render_printf(92.0f, 156.0f, RGBA8(224, 230, 245, 255),
                  "* Counter Monsters that Adapt to Your Tactics");

    int pulse = (int)(155.0f + 100.0f * (0.5f + 0.5f * sinf(gs->time_sec * 4.0f)));
    const char *cta = "Press [CROSS] or [START] to Begin";
    int cw = render_text_width(cta, 1.0f);
    render_text_shadow(240.0f - (float)(cw / 2), 194.0f, RGBA8(255, 232, 136, pulse), cta);
}

void ui_draw(const GameState *gs) {
    if (gs->mode == MODE_TITLE) {
        draw_title_overlay(gs);
        return;
    }

    draw_playing_hud(gs);

    if (gs->mode == MODE_INVENTORY) {
        draw_inventory_overlay(gs);
    } else if (gs->mode == MODE_PAUSE) {
        draw_pause_overlay(gs);
    }
}
