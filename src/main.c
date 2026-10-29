#include <stdio.h>
#include <string.h>
#include <math.h>
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <psprtc.h>

#include "ryunaria.h"
#include "render.h"
#include "world.h"
#include "items.h"
#include "combat.h"

PSP_MODULE_INFO("RyunaRia", 0, 0, 2);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_USER | PSP_THREAD_ATTR_VFPU);

static volatile int g_running = 1;
static GameState g_game;

static int exit_callback(int arg1, int arg2, void *common) {
    (void)arg1;
    (void)arg2;
    (void)common;
    g_running = 0;
    return 0;
}

static int callback_thread(SceSize args, void *argp) {
    (void)args;
    (void)argp;
    int cbid = sceKernelCreateCallback("RyunaRiaExit", exit_callback, NULL);
    sceKernelRegisterExitCallback(cbid);
    sceKernelSleepThreadCB();
    return 0;
}

static int setup_callbacks(void) {
    int thid = sceKernelCreateThread("RyunaRiaCallback", callback_thread, 0x11, 0xFA0, 0, NULL);
    if (thid >= 0) {
        sceKernelStartThread(thid, 0, NULL);
    }
    return thid;
}

unsigned int game_rand(GameState *gs) {
    unsigned int x = gs->rng_state;
    if (x == 0u) x = 0x5259554Eu; /* "RYUN" */
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    gs->rng_state = x;
    return x;
}

float game_randf(GameState *gs) {
    return (float)(game_rand(gs) & 0xFFFFu) / 65535.0f;
}

void game_set_banner(GameState *gs, const char *msg, unsigned int color, float duration) {
    if (!msg) return;
    snprintf(gs->banner_msg, sizeof(gs->banner_msg), "%s", msg);
    gs->banner_color = color;
    gs->banner_timer = duration;
}

void game_spawn_float_text(GameState *gs, float wx, float wy, float wz, const char *text, unsigned int color) {
    int slot = -1;
    float oldest_rem = 999.0f;

    for (int i = 0; i < MAX_FLOAT_TEXTS; i++) {
        if (!gs->floats[i].active) {
            slot = i;
            break;
        }
        float rem = gs->floats[i].duration - gs->floats[i].timer;
        if (rem < oldest_rem) {
            oldest_rem = rem;
            slot = i;
        }
    }
    if (slot < 0) return;

    FloatingText *ft = &gs->floats[slot];
    ft->active = 1;
    ft->wx = wx + (game_randf(gs) - 0.5f) * 0.25f;
    ft->wy = wy + (game_randf(gs) - 0.5f) * 0.25f;
    ft->wz = wz;
    ft->timer = 0.0f;
    ft->duration = 0.85f;
    ft->color = color;
    snprintf(ft->text, sizeof(ft->text), "%s", text);
}

static void build_input_state(const SceCtrlData *pad, unsigned int prev_buttons, InputState *out) {
    memset(out, 0, sizeof(*out));
    out->held = pad->Buttons;
    out->pressed = pad->Buttons & (~prev_buttons);

    /* Analog stick with clean radial deadzone */
    float ax = ((float)pad->Lx - 127.5f) / 127.5f;
    float ay = ((float)pad->Ly - 127.5f) / 127.5f;
    float mag = sqrtf(ax * ax + ay * ay);
    if (mag < 0.23f) {
        ax = 0.0f;
        ay = 0.0f;
    } else {
        float scaled = (mag - 0.23f) / (1.0f - 0.23f);
        if (scaled > 1.0f) scaled = 1.0f;
        ax = (ax / mag) * scaled;
        ay = (ay / mag) * scaled;
    }

    /* Combine D-Pad for digital movement */
    if (pad->Buttons & PSP_CTRL_LEFT)  ax -= 1.0f;
    if (pad->Buttons & PSP_CTRL_RIGHT) ax += 1.0f;
    if (pad->Buttons & PSP_CTRL_UP)    ay -= 1.0f;
    if (pad->Buttons & PSP_CTRL_DOWN)  ay += 1.0f;

    float total = sqrtf(ax * ax + ay * ay);
    if (total > 1.0f) {
        ax /= total;
        ay /= total;
    }
    out->move_x = ax;
    out->move_y = ay;

    if (out->pressed & PSP_CTRL_UP)    out->menu_dy = -1;
    if (out->pressed & PSP_CTRL_DOWN)  out->menu_dy =  1;
    if (out->pressed & PSP_CTRL_LEFT)  out->menu_dx = -1;
    if (out->pressed & PSP_CTRL_RIGHT) out->menu_dx =  1;
}

int game_init(GameState *gs) {
    memset(gs, 0, sizeof(*gs));
    gs->rng_state = 0x5259554Eu;
    gs->mode = MODE_TITLE;
    gs->locked_monster = -1;

    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
    render_init();

    Player *p = &gs->player;
    p->wx = (float)(MAP_SIZE / 2) + 0.5f;
    p->wy = (float)(MAP_SIZE / 2) + 2.2f;
    p->facing_x = 0.7071f;
    p->facing_y = 0.7071f;
    p->level = 1;
    p->xp = 0;
    p->xp_next = 45;
    p->gold = 30;
    p->max_tonics = 3;
    p->tonics = 3;
    p->shrine_rank = 0;

    world_generate(gs);
    items_init_starter_gear(gs);
    combat_init(gs);

    gs->cam_wx = p->wx;
    gs->cam_wy = p->wy;
    game_set_banner(gs, "Welcome to Sunlit Sanctuary! Explore & Hunt!", RGBA8(245, 222, 138, 255), 3.4f);
    return 0;
}

static void update_visual_effects(GameState *gs, float dt) {
    if (gs->banner_timer > 0.0f) {
        gs->banner_timer -= dt;
    }
    if (gs->shake_timer > 0.0f) {
        gs->shake_timer -= dt;
    }

    for (int i = 0; i < MAX_FLOAT_TEXTS; i++) {
        FloatingText *ft = &gs->floats[i];
        if (!ft->active) continue;
        ft->timer += dt;
        ft->wz += 18.0f * dt;
        if (ft->timer >= ft->duration) {
            ft->active = 0;
        }
    }
}

void game_update(GameState *gs, const InputState *in, float dt) {
    gs->time_sec += dt;
    update_visual_effects(gs, dt);

    if (gs->mode == MODE_TITLE) {
        /* Gentle scenic camera orbit around Sanctuary */
        gs->cam_wx = (float)(MAP_SIZE / 2) + 0.5f + cosf(gs->time_sec * 0.45f) * 1.4f;
        gs->cam_wy = (float)(MAP_SIZE / 2) + 1.4f + sinf(gs->time_sec * 0.45f) * 1.4f;
        if (in->pressed & (PSP_CTRL_CROSS | PSP_CTRL_START)) {
            gs->mode = MODE_PLAYING;
            gs->cam_wx = gs->player.wx;
            gs->cam_wy = gs->player.wy;
        }
        return;
    }

    if (gs->mode == MODE_INVENTORY) {
        if (in->menu_dy != 0) {
            gs->inv_cursor = (gs->inv_cursor + in->menu_dy + MAX_INVENTORY) % MAX_INVENTORY;
        }
        if (in->pressed & PSP_CTRL_CROSS) {
            items_equip_from_bag(gs, gs->inv_cursor);
        } else if (in->pressed & PSP_CTRL_SQUARE) {
            items_salvage_from_bag(gs, gs->inv_cursor);
        } else if (in->pressed & (PSP_CTRL_CIRCLE | PSP_CTRL_SELECT | PSP_CTRL_START)) {
            gs->mode = MODE_PLAYING;
        }
        return;
    }

    if (gs->mode == MODE_PAUSE) {
        if (in->menu_dy != 0) {
            gs->pause_cursor = (gs->pause_cursor + in->menu_dy + 3) % 3;
        }
        if (in->pressed & (PSP_CTRL_START | PSP_CTRL_CIRCLE)) {
            gs->mode = MODE_PLAYING;
        } else if (in->pressed & PSP_CTRL_CROSS) {
            if (gs->pause_cursor == 0) {
                gs->mode = MODE_PLAYING;
            } else if (gs->pause_cursor == 1) {
                gs->player.wx = (float)(MAP_SIZE / 2) + 0.5f;
                gs->player.wy = (float)(MAP_SIZE / 2) + 2.2f;
                gs->locked_monster = -1;
                gs->mode = MODE_PLAYING;
                game_set_banner(gs, "Recalled to Sunlit Sanctuary", RGBA8(245, 224, 142, 255), 2.2f);
            } else {
                gs->request_exit = 1;
            }
        }
        return;
    }

    /* MODE_PLAYING */
    if (in->pressed & PSP_CTRL_SELECT) {
        gs->mode = MODE_INVENTORY;
        return;
    }
    if (in->pressed & PSP_CTRL_START) {
        gs->pause_cursor = 0;
        gs->mode = MODE_PAUSE;
        return;
    }

    combat_update(gs, in, dt);
    world_update_chests(gs, dt);
    items_update_ground_loot(gs, dt);

    /* Smooth isometric camera tracking with subtle facing lead */
    float target_cx = gs->player.wx + gs->player.facing_x * 0.45f;
    float target_cy = gs->player.wy + gs->player.facing_y * 0.45f;
    float blend = 1.0f - expf(-9.5f * dt);
    gs->cam_wx += (target_cx - gs->cam_wx) * blend;
    gs->cam_wy += (target_cy - gs->cam_wy) * blend;
}

void game_draw(const GameState *gs) {
    render_begin_frame(RGBA8(15, 17, 29, 255));
    render_world_scene(gs);
    ui_draw(gs);
    render_end_frame();
}

void game_shutdown(void) {
    render_shutdown();
}

int main(void) {
    setup_callbacks();
    game_init(&g_game);

    SceCtrlData pad;
    memset(&pad, 0, sizeof(pad));
    unsigned int prev_buttons = 0;

    u64 prev_tick = 0;
    u32 tick_res = sceRtcGetTickResolution();
    if (tick_res == 0) tick_res = 1000000;
    sceRtcGetCurrentTick(&prev_tick);

    while (g_running && !g_game.request_exit) {
        u64 now_tick = prev_tick;
        sceRtcGetCurrentTick(&now_tick);
        float dt = (float)(now_tick - prev_tick) / (float)tick_res;
        prev_tick = now_tick;
        if (dt < 0.001f) dt = 0.0166f;
        if (dt > 0.050f) dt = 0.050f;

        sceCtrlPeekBufferPositive(&pad, 1);
        InputState in;
        build_input_state(&pad, prev_buttons, &in);
        prev_buttons = pad.Buttons;

        game_update(&g_game, &in, dt);
        game_draw(&g_game);
    }

    game_shutdown();
    sceKernelExitGame();
    return 0;
}
