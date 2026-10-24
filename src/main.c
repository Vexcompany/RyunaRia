#include <stdio.h>
#include <string.h>
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspdebug.h>
#include <pspgu.h>
#include <pspgum.h>

#include "ryunaria.h"

PSP_MODULE_INFO("RyunaRia", 0, 0, 1);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_USER | PSP_THREAD_ATTR_VFPU);

static volatile int running = 1;
static float player_x = 240.0f;
static float player_y = 136.0f;
static unsigned int frame_count = 0;

static int exit_callback(int arg1, int arg2, void *common) {
    (void)arg1;
    (void)arg2;
    (void)common;
    running = 0;
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

static void init_graphics(void) {
    sceGuInit();
    sceGuStart(GU_DIRECT, (void *)0);
    sceGuDrawBuffer(GU_PSM_8888, (void *)0, 512);
    sceGuDispBuffer(SCREEN_WIDTH, SCREEN_HEIGHT, (void *)0x88000, 512);
    sceGuDepthBuffer((void *)0x110000, 512);
    sceGuOffset(2048 - SCREEN_WIDTH / 2, 2048 - SCREEN_HEIGHT / 2);
    sceGuViewport(2048, 2048, SCREEN_WIDTH, SCREEN_HEIGHT);
    sceGuDepthRange(65535, 0);
    sceGuScissor(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuFinish();
    sceGuSync(0, 0);
    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
}

int game_init(void) {
    pspDebugScreenInit();
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
    init_graphics();
    return 0;
}

void game_update(const SceCtrlData *pad) {
    const float speed = (pad->Buttons & PSP_CTRL_CIRCLE) ? 3.0f : 1.7f;

    if (pad->Buttons & PSP_CTRL_UP) player_y -= speed;
    if (pad->Buttons & PSP_CTRL_DOWN) player_y += speed;
    if (pad->Buttons & PSP_CTRL_LEFT) player_x -= speed;
    if (pad->Buttons & PSP_CTRL_RIGHT) player_x += speed;

    if (player_x < 16.0f) player_x = 16.0f;
    if (player_x > SCREEN_WIDTH - 16.0f) player_x = SCREEN_WIDTH - 16.0f;
    if (player_y < 32.0f) player_y = 32.0f;
    if (player_y > SCREEN_HEIGHT - 16.0f) player_y = SCREEN_HEIGHT - 16.0f;

    frame_count++;
}

static void draw_rect(float x, float y, float w, float h, unsigned int color) {
    struct Vertex {
        unsigned int color;
        float x, y, z;
    } __attribute__((aligned(16)));

    struct Vertex *vertices = (struct Vertex *)sceGuGetMemory(2 * sizeof(struct Vertex));
    vertices[0].color = color;
    vertices[0].x = x;
    vertices[0].y = y;
    vertices[0].z = 0.0f;
    vertices[1].color = color;
    vertices[1].x = x + w;
    vertices[1].y = y + h;
    vertices[1].z = 0.0f;

    sceGuDrawArray(GU_SPRITES, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D, 2, NULL, vertices);
}

void game_draw(void) {
    sceGuStart(GU_DIRECT, (void *)0);
    sceGuClearColor(0xFF151521);
    sceGuClear(GU_COLOR_BUFFER_BIT);

    /* Minimal world grid: intentionally cheap while the engine foundation is built. */
    for (int x = 0; x < SCREEN_WIDTH; x += 32) {
        draw_rect((float)x, 32.0f, 1.0f, (float)(SCREEN_HEIGHT - 48), 0xFF25253A);
    }
    for (int y = 32; y < SCREEN_HEIGHT; y += 32) {
        draw_rect(0.0f, (float)y, (float)SCREEN_WIDTH, 1.0f, 0xFF25253A);
    }

    /* Player placeholder. This will become the first real Ryuna sprite/model. */
    draw_rect(player_x - 8.0f, player_y - 8.0f, 16.0f, 16.0f, 0xFFE6D28A);

    sceGuFinish();
    sceGuSync(0, 0);
    sceDisplayWaitVblankStart();
    sceGuSwapBuffers();
}

void game_shutdown(void) {
    sceGuTerm();
}

int main(void) {
    setup_callbacks();
    game_init();

    SceCtrlData pad;
    memset(&pad, 0, sizeof(pad));

    while (running) {
        sceCtrlReadBufferPositive(&pad, 1);

        if (pad.Buttons & PSP_CTRL_START) {
            running = 0;
        }

        game_update(&pad);
        game_draw();
    }

    game_shutdown();
    sceKernelExitGame();
    return 0;
}
