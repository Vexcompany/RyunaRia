#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspgum.h>

#include "render.h"
#include "world.h"
#include "items.h"
#include "combat.h"

#define FRAME_BYTE_SIZE (BUF_WIDTH * SCREEN_HEIGHT * 4)
#define FONT_TEX_W      128
#define FONT_TEX_H      64

static unsigned int __attribute__((aligned(16))) display_list[262144];
static unsigned int __attribute__((aligned(16))) font_atlas[FONT_TEX_W * FONT_TEX_H];
static int tex_enabled = 0;

typedef struct {
    unsigned int color;
    float x, y, z;
} ColorVertex;

typedef struct {
    float u, v;
    unsigned int color;
    float x, y, z;
} TexColorVertex;

/* 95 printable ASCII characters (32..126), 8 rows each, MSB (0x80) = leftmost pixel */
static const unsigned char font8x8_basic[95][8] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* 32 ' ' */
    {0x18,0x3C,0x3C,0x18,0x18,0x00,0x18,0x00}, /* 33 '!' */
    {0x6C,0x6C,0x24,0x00,0x00,0x00,0x00,0x00}, /* 34 '"' */
    {0x6C,0x6C,0xFE,0x6C,0xFE,0x6C,0x6C,0x00}, /* 35 '#' */
    {0x18,0x7E,0xC0,0x7C,0x06,0xFC,0x18,0x00}, /* 36 '$' */
    {0x00,0xC6,0xCC,0x18,0x30,0x66,0xC6,0x00}, /* 37 '%' */
    {0x38,0x6C,0x38,0x76,0xDC,0xCC,0x76,0x00}, /* 38 '&' */
    {0x18,0x18,0x30,0x00,0x00,0x00,0x00,0x00}, /* 39 ''' */
    {0x0C,0x18,0x30,0x30,0x30,0x18,0x0C,0x00}, /* 40 '(' */
    {0x30,0x18,0x0C,0x0C,0x0C,0x18,0x30,0x00}, /* 41 ')' */
    {0x00,0x66,0x3C,0xFF,0x3C,0x66,0x00,0x00}, /* 42 '*' */
    {0x00,0x18,0x18,0x7E,0x18,0x18,0x00,0x00}, /* 43 '+' */
    {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x30}, /* 44 ',' */
    {0x00,0x00,0x00,0x7E,0x00,0x00,0x00,0x00}, /* 45 '-' */
    {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00}, /* 46 '.' */
    {0x06,0x0C,0x18,0x30,0x60,0xC0,0x80,0x00}, /* 47 '/' */
    {0x7C,0xC6,0xCE,0xD6,0xE6,0xC6,0x7C,0x00}, /* 48 '0' */
    {0x18,0x38,0x18,0x18,0x18,0x18,0x7E,0x00}, /* 49 '1' */
    {0x7C,0xC6,0x06,0x1C,0x70,0xC6,0xFE,0x00}, /* 50 '2' */
    {0x7C,0xC6,0x06,0x3C,0x06,0xC6,0x7C,0x00}, /* 51 '3' */
    {0x1C,0x3C,0x6C,0xCC,0xFE,0x0C,0x1E,0x00}, /* 52 '4' */
    {0xFE,0xC0,0xFC,0x06,0x06,0xC6,0x7C,0x00}, /* 53 '5' */
    {0x38,0x60,0xC0,0xFC,0xC6,0xC6,0x7C,0x00}, /* 54 '6' */
    {0xFE,0xC6,0x0C,0x18,0x30,0x30,0x30,0x00}, /* 55 '7' */
    {0x7C,0xC6,0xC6,0x7C,0xC6,0xC6,0x7C,0x00}, /* 56 '8' */
    {0x7C,0xC6,0xC6,0x7E,0x06,0x0C,0x78,0x00}, /* 57 '9' */
    {0x00,0x18,0x18,0x00,0x00,0x18,0x18,0x00}, /* 58 ':' */
    {0x00,0x18,0x18,0x00,0x00,0x18,0x18,0x30}, /* 59 ';' */
    {0x0C,0x18,0x30,0x60,0x30,0x18,0x0C,0x00}, /* 60 '<' */
    {0x00,0x00,0x7E,0x00,0x7E,0x00,0x00,0x00}, /* 61 '=' */
    {0x60,0x30,0x18,0x0C,0x18,0x30,0x60,0x00}, /* 62 '>' */
    {0x7C,0xC6,0x0C,0x18,0x18,0x00,0x18,0x00}, /* 63 '?' */
    {0x7C,0xC6,0xDE,0xDE,0xDE,0xC0,0x78,0x00}, /* 64 '@' */
    {0x38,0x6C,0xC6,0xFE,0xC6,0xC6,0xC6,0x00}, /* 65 'A' */
    {0xFC,0x66,0x66,0x7C,0x66,0x66,0xFC,0x00}, /* 66 'B' */
    {0x3C,0x66,0xC0,0xC0,0xC0,0x66,0x3C,0x00}, /* 67 'C' */
    {0xF8,0x6C,0x66,0x66,0x66,0x6C,0xF8,0x00}, /* 68 'D' */
    {0xFE,0x62,0x68,0x78,0x68,0x62,0xFE,0x00}, /* 69 'E' */
    {0xFE,0x62,0x68,0x78,0x68,0x60,0xF0,0x00}, /* 70 'F' */
    {0x3C,0x66,0xC0,0xC0,0xCE,0x66,0x3E,0x00}, /* 71 'G' */
    {0xC6,0xC6,0xC6,0xFE,0xC6,0xC6,0xC6,0x00}, /* 72 'H' */
    {0x3C,0x18,0x18,0x18,0x18,0x18,0x3C,0x00}, /* 73 'I' */
    {0x1E,0x0C,0x0C,0x0C,0xCC,0xCC,0x78,0x00}, /* 74 'J' */
    {0xE6,0x66,0x6C,0x78,0x6C,0x66,0xE6,0x00}, /* 75 'K' */
    {0xF0,0x60,0x60,0x60,0x62,0x66,0xFE,0x00}, /* 76 'L' */
    {0xC6,0xEE,0xFE,0xFE,0xD6,0xC6,0xC6,0x00}, /* 77 'M' */
    {0xC6,0xE6,0xF6,0xDE,0xCE,0xC6,0xC6,0x00}, /* 78 'N' */
    {0x7C,0xC6,0xC6,0xC6,0xC6,0xC6,0x7C,0x00}, /* 79 'O' */
    {0xFC,0x66,0x66,0x7C,0x60,0x60,0xF0,0x00}, /* 80 'P' */
    {0x7C,0xC6,0xC6,0xC6,0xD6,0xDE,0x7C,0x06}, /* 81 'Q' */
    {0xFC,0x66,0x66,0x7C,0x6C,0x66,0xE6,0x00}, /* 82 'R' */
    {0x7C,0xC6,0x60,0x38,0x0C,0xC6,0x7C,0x00}, /* 83 'S' */
    {0x7E,0x7E,0x5A,0x18,0x18,0x18,0x3C,0x00}, /* 84 'T' */
    {0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0x7C,0x00}, /* 85 'U' */
    {0xC6,0xC6,0xC6,0xC6,0xC6,0x6C,0x38,0x00}, /* 86 'V' */
    {0xC6,0xC6,0xD6,0xFE,0xFE,0xEE,0xC6,0x00}, /* 87 'W' */
    {0xC6,0xC6,0x6C,0x38,0x6C,0xC6,0xC6,0x00}, /* 88 'X' */
    {0x66,0x66,0x66,0x3C,0x18,0x18,0x3C,0x00}, /* 89 'Y' */
    {0xFE,0xC6,0x8C,0x18,0x32,0x66,0xFE,0x00}, /* 90 'Z' */
    {0x3C,0x30,0x30,0x30,0x30,0x30,0x3C,0x00}, /* 91 '[' */
    {0xC0,0x60,0x30,0x18,0x0C,0x06,0x02,0x00}, /* 92 '\' */
    {0x3C,0x0C,0x0C,0x0C,0x0C,0x0C,0x3C,0x00}, /* 93 ']' */
    {0x10,0x38,0x6C,0xC6,0x00,0x00,0x00,0x00}, /* 94 '^' */
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF}, /* 95 '_' */
    {0x30,0x18,0x0C,0x00,0x00,0x00,0x00,0x00}, /* 96 '`' */
    {0x00,0x00,0x78,0x0C,0x7C,0xCC,0x76,0x00}, /* 97 'a' */
    {0xE0,0x60,0x7C,0x66,0x66,0x66,0xDC,0x00}, /* 98 'b' */
    {0x00,0x00,0x7C,0xC6,0xC0,0xC6,0x7C,0x00}, /* 99 'c' */
    {0x1C,0x0C,0x7C,0xCC,0xCC,0xCC,0x76,0x00}, /* 100 'd' */
    {0x00,0x00,0x7C,0xC6,0xFE,0xC0,0x7C,0x00}, /* 101 'e' */
    {0x3C,0x66,0x60,0xF8,0x60,0x60,0xF0,0x00}, /* 102 'f' */
    {0x00,0x00,0x76,0xCC,0xCC,0x7C,0x0C,0xF8}, /* 103 'g' */
    {0xE0,0x60,0x6C,0x76,0x66,0x66,0xE6,0x00}, /* 104 'h' */
    {0x18,0x00,0x38,0x18,0x18,0x18,0x3C,0x00}, /* 105 'i' */
    {0x06,0x00,0x06,0x06,0x06,0x66,0x66,0x3C}, /* 106 'j' */
    {0xE0,0x60,0x66,0x6C,0x78,0x6C,0xE6,0x00}, /* 107 'k' */
    {0x38,0x18,0x18,0x18,0x18,0x18,0x3C,0x00}, /* 108 'l' */
    {0x00,0x00,0xEC,0xFE,0xD6,0xC6,0xC6,0x00}, /* 109 'm' */
    {0x00,0x00,0xDC,0x66,0x66,0x66,0x66,0x00}, /* 110 'n' */
    {0x00,0x00,0x7C,0xC6,0xC6,0xC6,0x7C,0x00}, /* 111 'o' */
    {0x00,0x00,0xDC,0x66,0x66,0x7C,0x60,0xF0}, /* 112 'p' */
    {0x00,0x00,0x76,0xCC,0xCC,0x7C,0x0C,0x1E}, /* 113 'q' */
    {0x00,0x00,0xDC,0x76,0x60,0x60,0xF0,0x00}, /* 114 'r' */
    {0x00,0x00,0x7E,0xC0,0x7C,0x06,0xFC,0x00}, /* 115 's' */
    {0x30,0x30,0xFC,0x30,0x30,0x36,0x1C,0x00}, /* 116 't' */
    {0x00,0x00,0xCC,0xCC,0xCC,0xCC,0x76,0x00}, /* 117 'u' */
    {0x00,0x00,0xC6,0xC6,0xC6,0x6C,0x38,0x00}, /* 118 'v' */
    {0x00,0x00,0xC6,0xD6,0xFE,0xFE,0x6C,0x00}, /* 119 'w' */
    {0x00,0x00,0xC6,0x6C,0x38,0x6C,0xC6,0x00}, /* 120 'x' */
    {0x00,0x00,0xC6,0xC6,0xC6,0x7E,0x06,0xFC}, /* 121 'y' */
    {0x00,0x00,0xFE,0x8C,0x18,0x62,0xFE,0x00}, /* 122 'z' */
    {0x0E,0x18,0x18,0x70,0x18,0x18,0x0E,0x00}, /* 123 '{' */
    {0x18,0x18,0x18,0x00,0x18,0x18,0x18,0x00}, /* 124 '|' */
    {0x70,0x18,0x18,0x0E,0x18,0x18,0x70,0x00}, /* 125 '}' */
    {0x76,0xDC,0x00,0x00,0x00,0x00,0x00,0x00}  /* 126 '~' */
};

static void set_textured(int enable) {
    if (tex_enabled != enable) {
        if (enable) {
            sceGuEnable(GU_TEXTURE_2D);
        } else {
            sceGuDisable(GU_TEXTURE_2D);
        }
        tex_enabled = enable;
    }
}

static void build_font_atlas(void) {
    for (int i = 0; i < FONT_TEX_W * FONT_TEX_H; i++) {
        font_atlas[i] = 0x00FFFFFFu;
    }

    for (int ch = 32; ch <= 126; ch++) {
        int cell_x = (ch & 15) * 8;
        int cell_y = ((ch >> 4) & 7) * 8;
        const unsigned char *glyph = font8x8_basic[ch - 32];

        for (int row = 0; row < 8; row++) {
            unsigned char bits = glyph[row];
            for (int col = 0; col < 8; col++) {
                if (bits & (0x80u >> col)) {
                    font_atlas[(cell_y + row) * FONT_TEX_W + (cell_x + col)] = 0xFFFFFFFFu;
                }
            }
        }
    }

    sceKernelDcacheWritebackAll();
}

void render_init(void) {
    build_font_atlas();

    sceGuInit();
    sceGuStart(GU_DIRECT, display_list);
    sceGuDrawBuffer(GU_PSM_8888, (void *)0, BUF_WIDTH);
    sceGuDispBuffer(SCREEN_WIDTH, SCREEN_HEIGHT, (void *)FRAME_BYTE_SIZE, BUF_WIDTH);
    sceGuDepthBuffer((void *)(FRAME_BYTE_SIZE * 2), BUF_WIDTH);
    sceGuOffset(2048 - (SCREEN_WIDTH / 2), 2048 - (SCREEN_HEIGHT / 2));
    sceGuViewport(2048, 2048, SCREEN_WIDTH, SCREEN_HEIGHT);
    sceGuDepthRange(65535, 0);
    sceGuScissor(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_CULL_FACE);
    sceGuShadeModel(GU_SMOOTH);

    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);

    sceGuTexMode(GU_PSM_8888, 0, 0, 0);
    sceGuTexImage(0, FONT_TEX_W, FONT_TEX_H, FONT_TEX_W, font_atlas);
    sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
    sceGuTexFilter(GU_NEAREST, GU_NEAREST);
    sceGuDisable(GU_TEXTURE_2D);
    tex_enabled = 0;

    sceGuFinish();
    sceGuSync(0, 0);
    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
}

void render_shutdown(void) {
    sceGuDisplay(GU_FALSE);
    sceGuTerm();
}

void render_begin_frame(unsigned int clear_color) {
    sceGuStart(GU_DIRECT, display_list);
    sceGuClearColor(clear_color);
    sceGuClear(GU_COLOR_BUFFER_BIT);
    sceGuDisable(GU_TEXTURE_2D);
    tex_enabled = 0;
}

void render_end_frame(void) {
    sceGuFinish();
    sceGuSync(0, 0);
    sceDisplayWaitVblankStart();
    sceGuSwapBuffers();
}

void world_to_screen(float wx, float wy, float wz, float cam_wx, float cam_wy, float *sx, float *sy) {
    float dx = wx - cam_wx;
    float dy = wy - cam_wy;
    *sx = (SCREEN_WIDTH * 0.5f) + (dx - dy) * ISO_TILE_HALF_W;
    *sy = (SCREEN_HEIGHT * 0.53f) + (dx + dy) * ISO_TILE_HALF_H - wz;
}

void render_rect(float x, float y, float w, float h, unsigned int color) {
    if (w <= 0.0f || h <= 0.0f) return;
    set_textured(0);
    ColorVertex *v = (ColorVertex *)sceGuGetMemory(2 * sizeof(ColorVertex));
    if (!v) return;
    v[0].color = color; v[0].x = x;     v[0].y = y;     v[0].z = 0.0f;
    v[1].color = color; v[1].x = x + w; v[1].y = y + h; v[1].z = 0.0f;
    sceGuDrawArray(GU_SPRITES, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D, 2, NULL, v);
}

void render_rect_outline(float x, float y, float w, float h, float thickness, unsigned int color) {
    render_rect(x, y, w, thickness, color);
    render_rect(x, y + h - thickness, w, thickness, color);
    render_rect(x, y + thickness, thickness, h - 2.0f * thickness, color);
    render_rect(x + w - thickness, y + thickness, thickness, h - 2.0f * thickness, color);
}

void render_panel(float x, float y, float w, float h, unsigned int bg, unsigned int border) {
    render_rect(x + 2.0f, y + 2.0f, w, h, RGBA8(0, 0, 0, 110));
    render_rect(x, y, w, h, bg);
    render_rect_outline(x, y, w, h, 1.0f, border);
}

void render_tri(float x0, float y0, float x1, float y1, float x2, float y2, unsigned int color) {
    set_textured(0);
    ColorVertex *v = (ColorVertex *)sceGuGetMemory(3 * sizeof(ColorVertex));
    if (!v) return;
    v[0].color = color; v[0].x = x0; v[0].y = y0; v[0].z = 0.0f;
    v[1].color = color; v[1].x = x1; v[1].y = y1; v[1].z = 0.0f;
    v[2].color = color; v[2].x = x2; v[2].y = y2; v[2].z = 0.0f;
    sceGuDrawArray(GU_TRIANGLES, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D, 3, NULL, v);
}

void render_quad(float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3, unsigned int color) {
    set_textured(0);
    ColorVertex *v = (ColorVertex *)sceGuGetMemory(6 * sizeof(ColorVertex));
    if (!v) return;
    v[0].color = color; v[0].x = x0; v[0].y = y0; v[0].z = 0.0f;
    v[1].color = color; v[1].x = x1; v[1].y = y1; v[1].z = 0.0f;
    v[2].color = color; v[2].x = x2; v[2].y = y2; v[2].z = 0.0f;
    v[3].color = color; v[3].x = x0; v[3].y = y0; v[3].z = 0.0f;
    v[4].color = color; v[4].x = x2; v[4].y = y2; v[4].z = 0.0f;
    v[5].color = color; v[5].x = x3; v[5].y = y3; v[5].z = 0.0f;
    sceGuDrawArray(GU_TRIANGLES, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D, 6, NULL, v);
}

void render_line(float x0, float y0, float x1, float y1, unsigned int color) {
    set_textured(0);
    ColorVertex *v = (ColorVertex *)sceGuGetMemory(2 * sizeof(ColorVertex));
    if (!v) return;
    v[0].color = color; v[0].x = x0; v[0].y = y0; v[0].z = 0.0f;
    v[1].color = color; v[1].x = x1; v[1].y = y1; v[1].z = 0.0f;
    sceGuDrawArray(GU_LINES, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D, 2, NULL, v);
}

void render_iso_diamond(float cx, float cy, float hw, float hh, unsigned int fill, unsigned int edge) {
    render_quad(cx, cy - hh, cx + hw, cy, cx, cy + hh, cx - hw, cy, fill);
    if ((edge >> 24) != 0) {
        set_textured(0);
        ColorVertex *v = (ColorVertex *)sceGuGetMemory(5 * sizeof(ColorVertex));
        if (!v) return;
        v[0].color = edge; v[0].x = cx;      v[0].y = cy - hh; v[0].z = 0.0f;
        v[1].color = edge; v[1].x = cx + hw; v[1].y = cy;      v[1].z = 0.0f;
        v[2].color = edge; v[2].x = cx;      v[2].y = cy + hh; v[2].z = 0.0f;
        v[3].color = edge; v[3].x = cx - hw; v[3].y = cy;      v[3].z = 0.0f;
        v[4].color = edge; v[4].x = cx;      v[4].y = cy - hh; v[4].z = 0.0f;
        sceGuDrawArray(GU_LINE_STRIP, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D, 5, NULL, v);
    }
}

void render_iso_prism(float cx, float cy, float hw, float hh, float height,
                      unsigned int top_col, unsigned int left_col, unsigned int right_col) {
    /* Left face: (cx - hw, cy - height) -> (cx, cy + hh - height) -> (cx, cy + hh) -> (cx - hw, cy) */
    render_quad(
        cx - hw, cy - height,
        cx,      cy + hh - height,
        cx,      cy + hh,
        cx - hw, cy,
        left_col
    );
    /* Right face: (cx, cy + hh - height) -> (cx + hw, cy - height) -> (cx + hw, cy) -> (cx, cy + hh) */
    render_quad(
        cx,      cy + hh - height,
        cx + hw, cy - height,
        cx + hw, cy,
        cx,      cy + hh,
        right_col
    );
    /* Top diamond raised by height */
    render_iso_diamond(cx, cy - height, hw, hh, top_col, RGBA8(255, 255, 255, 38));
}

void render_iso_ring(float cx, float cy, float rx, float ry, int segments, unsigned int color) {
    if (segments < 6) segments = 6;
    if (segments > 32) segments = 32;
    set_textured(0);
    ColorVertex *v = (ColorVertex *)sceGuGetMemory((segments + 1) * sizeof(ColorVertex));
    if (!v) return;
    for (int i = 0; i <= segments; i++) {
        float a = (6.2831853f * (float)i) / (float)segments;
        v[i].color = color;
        v[i].x = cx + cosf(a) * rx;
        v[i].y = cy + sinf(a) * ry;
        v[i].z = 0.0f;
    }
    sceGuDrawArray(GU_LINE_STRIP, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D, segments + 1, NULL, v);
}

void render_slash_arc(float cx, float cy, float rx, float ry, float center_angle,
                      float sweep_rad, unsigned int inner_col, unsigned int outer_col) {
    const int steps = 8;
    set_textured(0);
    ColorVertex *v = (ColorVertex *)sceGuGetMemory(steps * 6 * sizeof(ColorVertex));
    if (!v) return;

    float a0 = center_angle - sweep_rad * 0.5f;
    float da = sweep_rad / (float)steps;

    for (int i = 0; i < steps; i++) {
        float t0 = a0 + da * (float)i;
        float t1 = t0 + da;

        float ix0 = cx + cosf(t0) * (rx * 0.42f);
        float iy0 = cy + sinf(t0) * (ry * 0.42f);
        float ox0 = cx + cosf(t0) * rx;
        float oy0 = cy + sinf(t0) * ry;

        float ix1 = cx + cosf(t1) * (rx * 0.42f);
        float iy1 = cy + sinf(t1) * (ry * 0.42f);
        float ox1 = cx + cosf(t1) * rx;
        float oy1 = cy + sinf(t1) * ry;

        int b = i * 6;
        v[b + 0].color = inner_col; v[b + 0].x = ix0; v[b + 0].y = iy0; v[b + 0].z = 0.0f;
        v[b + 1].color = outer_col; v[b + 1].x = ox0; v[b + 1].y = oy0; v[b + 1].z = 0.0f;
        v[b + 2].color = outer_col; v[b + 2].x = ox1; v[b + 2].y = oy1; v[b + 2].z = 0.0f;

        v[b + 3].color = inner_col; v[b + 3].x = ix0; v[b + 3].y = iy0; v[b + 3].z = 0.0f;
        v[b + 4].color = outer_col; v[b + 4].x = ox1; v[b + 4].y = oy1; v[b + 4].z = 0.0f;
        v[b + 5].color = inner_col; v[b + 5].x = ix1; v[b + 5].y = iy1; v[b + 5].z = 0.0f;
    }

    sceGuDrawArray(GU_TRIANGLES, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D, steps * 6, NULL, v);
}

int render_text_width(const char *text, float scale) {
    if (!text) return 0;
    return (int)((float)strlen(text) * 7.0f * scale);
}

void render_text_scaled(float x, float y, float scale, unsigned int color, const char *text) {
    if (!text || !text[0]) return;
    int len = (int)strlen(text);
    if (len > 120) len = 120;

    set_textured(1);
    TexColorVertex *v = (TexColorVertex *)sceGuGetMemory(2 * len * sizeof(TexColorVertex));
    if (!v) return;

    float glyph_w = 8.0f * scale;
    float glyph_h = 8.0f * scale;
    float advance = 7.0f * scale;
    float cur_x = x;

    int out = 0;
    for (int i = 0; i < len; i++) {
        unsigned char c = (unsigned char)text[i];
        if (c < 32 || c > 126) c = '?';

        if (c != ' ') {
            float u0 = (float)((c & 15) * 8);
            float v0 = (float)(((c >> 4) & 7) * 8);

            v[out].u = u0;
            v[out].v = v0;
            v[out].color = color;
            v[out].x = cur_x;
            v[out].y = y;
            v[out].z = 0.0f;

            v[out + 1].u = u0 + 8.0f;
            v[out + 1].v = v0 + 8.0f;
            v[out + 1].color = color;
            v[out + 1].x = cur_x + glyph_w;
            v[out + 1].y = y + glyph_h;
            v[out + 1].z = 0.0f;
            out += 2;
        }
        cur_x += advance;
    }

    if (out > 0) {
        sceGuDrawArray(
            GU_SPRITES,
            GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
            out,
            NULL,
            v
        );
    }
}

void render_text(float x, float y, unsigned int color, const char *text) {
    render_text_scaled(x, y, 1.0f, color, text);
}

void render_text_shadow(float x, float y, unsigned int color, const char *text) {
    unsigned int alpha = (color >> 24) & 0xFFu;
    unsigned int shadow_a = (alpha * 180u) / 255u;
    render_text_scaled(x + 1.0f, y + 1.0f, 1.0f, RGBA8(0, 0, 0, shadow_a), text);
    render_text_scaled(x, y, 1.0f, color, text);
}

void render_printf(float x, float y, unsigned int color, const char *fmt, ...) {
    char buf[128];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    render_text_shadow(x, y, color, buf);
}

/* --- Isometric Scene Rendering (Tiles + Depth-Sorted Entities) --- */

typedef enum {
    DRAW_PILLAR = 0,
    DRAW_SPIRE = 1,
    DRAW_SHRINE = 2,
    DRAW_CHEST = 3,
    DRAW_LOOT = 4,
    DRAW_MONSTER = 5,
    DRAW_PLAYER = 6,
    DRAW_PROJECTILE = 7
} DrawKind;

typedef struct {
    float depth;
    DrawKind kind;
    int index;
    int tx;
    int ty;
} SortableDraw;

#define MAX_SORTABLES 256

static unsigned int tile_floor_color(TileType t, int tx, int ty, float time_sec) {
    int alt = (tx + ty) & 1;
    switch (t) {
        case TILE_STONE_PLAZA:
            return alt ? RGBA8(74, 70, 88, 255) : RGBA8(66, 62, 80, 255);
        case TILE_SHRINE_PAD: {
            int pulse = (int)(18.0f * (0.5f + 0.5f * sinf(time_sec * 3.2f)));
            return RGBA8(92 + pulse, 84 + pulse, 118 + pulse, 255);
        }
        case TILE_MOSS_RUIN:
            return alt ? RGBA8(44, 68, 52, 255) : RGBA8(38, 60, 46, 255);
        case TILE_CRYSTAL_FLOOR:
            return alt ? RGBA8(38, 56, 86, 255) : RGBA8(32, 48, 76, 255);
        case TILE_ASH_BASALT:
            return alt ? RGBA8(56, 42, 46, 255) : RGBA8(48, 36, 40, 255);
        case TILE_WATER_CHASM: {
            int wave = (int)(14.0f * (0.5f + 0.5f * sinf(time_sec * 2.4f + (float)(tx - ty))));
            return RGBA8(18, 34 + wave, 62 + wave, 255);
        }
        case TILE_EMBER_VENT: {
            int glow = (int)(55.0f * (0.5f + 0.5f * sinf(time_sec * 4.5f + (float)(tx + ty))));
            return RGBA8(130 + glow, 48 + glow / 3, 28, 255);
        }
        case TILE_WALL_PILLAR:
            return RGBA8(48, 46, 58, 255);
        case TILE_CRYSTAL_SPIRE:
            return RGBA8(34, 52, 80, 255);
        case TILE_GRASS:
        default:
            return alt ? RGBA8(42, 62, 54, 255) : RGBA8(37, 56, 49, 255);
    }
}

static void draw_player_entity(const GameState *gs, float cam_wx, float cam_wy) {
    const Player *p = &gs->player;
    float sx, sy;
    world_to_screen(p->wx, p->wy, 0.0f, cam_wx, cam_wy, &sx, &sy);

    /* Ground shadow & facing indicator */
    render_iso_diamond(sx, sy + 1.0f, 9.0f, 4.5f, RGBA8(0, 0, 0, 125), 0);

    /* Dash ghost afterimage */
    if (p->dash_timer > 0.0f) {
        float gx, gy;
        world_to_screen(p->wx - p->vx * 0.055f, p->wy - p->vy * 0.055f, 0.0f, cam_wx, cam_wy, &gx, &gy);
        render_iso_prism(gx, gy, 5.5f, 3.0f, 15.0f,
                         RGBA8(120, 220, 255, 110),
                         RGBA8(80, 180, 240, 95),
                         RGBA8(60, 140, 210, 95));
    }

    /* Blink subtly during invulnerability frames */
    if (p->iframe_timer > 0.0f && (((int)(gs->time_sec * 24.0f)) & 1)) {
        render_iso_ring(sx, sy, 12.0f, 6.0f, 12, RGBA8(160, 235, 255, 180));
    }

    float bob = (fabsf(p->vx) + fabsf(p->vy) > 0.15f) ? fabsf(sinf(p->walk_anim * 9.0f)) * 2.2f : 0.0f;

    unsigned int armor_col = items_rarity_color(p->equipped[SLOT_ARMOR].rarity);
    unsigned int weapon_col = items_rarity_color(p->equipped[SLOT_WEAPON].rarity);
    if (p->hurt_flash > 0.0f) {
        armor_col = RGBA8(255, 95, 95, 255);
    }

    /* Boots */
    render_rect(sx - 4.5f, sy - 4.0f, 3.5f, 4.0f, RGBA8(52, 42, 38, 255));
    render_rect(sx + 1.0f, sy - 4.0f, 3.5f, 4.0f, RGBA8(52, 42, 38, 255));

    /* Isometric tunic prism */
    unsigned int body_top = armor_col;
    unsigned int body_l   = RGBA8(58, 92, 138, 255);
    unsigned int body_r   = RGBA8(40, 66, 104, 255);
    if (p->hurt_flash > 0.0f) {
        body_l = RGBA8(255, 120, 120, 255);
        body_r = RGBA8(220, 70, 70, 255);
    }
    render_iso_prism(sx, sy - 2.0f - bob, 5.8f, 3.2f, 11.0f, body_top, body_l, body_r);

    /* Ryuna signature golden scarf & hair crown (homage to 0xFFE6D28A) */
    render_iso_prism(sx, sy - 13.0f - bob, 4.6f, 2.6f, 6.0f,
                     RGBA8(238, 216, 142, 255),
                     RGBA8(222, 192, 118, 255),
                     RGBA8(192, 160, 92, 255));
    /* Flowing scarf tail opposite facing */
    float f_sx = (p->facing_x - p->facing_y) * 7.0f;
    float f_sy = (p->facing_x + p->facing_y) * 3.5f;
    render_tri(sx, sy - 11.0f - bob,
               sx - f_sx * 0.9f - 2.5f, sy - 8.0f - f_sy * 0.6f,
               sx - f_sx * 0.9f + 2.5f, sy - 7.0f - f_sy * 0.6f,
               RGBA8(225, 86, 98, 235));

    /* Equipped blade held along facing or slash arc */
    float wx_tip = p->wx + p->facing_x * 0.85f;
    float wy_tip = p->wy + p->facing_y * 0.85f;
    float bsx, bsy;
    world_to_screen(wx_tip, wy_tip, 8.0f + bob, cam_wx, cam_wy, &bsx, &bsy);
    render_line(sx + 2.0f, sy - 9.0f - bob, bsx, bsy, weapon_col);
    render_line(sx + 2.0f, sy - 10.0f - bob, bsx, bsy - 1.0f, RGBA8(255, 255, 255, 200));

    /* Active Melee Combo Slash Crescent */
    if (p->attack_timer > 0.0f) {
        /* Convert world swing angle into screen-space ellipse angle */
        float wdx = cosf(p->swing_angle);
        float wdy = sinf(p->swing_angle);
        float scr_ang = atan2f((wdx + wdy) * ISO_TILE_HALF_H, (wdx - wdy) * ISO_TILE_HALF_W);

        float sweep = (p->combo_step == 2) ? 2.85f : 2.05f;
        float rx = (p->combo_step == 2) ? 38.0f : 31.0f;
        float ry = rx * 0.52f;
        unsigned int outer = (p->combo_step == 2)
            ? RGBA8(255, 230, 120, 235)
            : weapon_col;
        unsigned int inner = RGBA8(255, 255, 255, 55);
        render_slash_arc(sx, sy - 6.0f, rx, ry, scr_ang, sweep, inner, outer);
    }
}

static void draw_monster_entity(const GameState *gs, const Monster *m, int is_locked, float cam_wx, float cam_wy) {
    float sx, sy;
    world_to_screen(m->wx, m->wy, 0.0f, cam_wx, cam_wy, &sx, &sy);
    if (sx < -40.0f || sx > SCREEN_WIDTH + 40.0f || sy < -40.0f || sy > SCREEN_HEIGHT + 40.0f) {
        return;
    }

    float scale = m->is_elite ? 1.38f : 1.0f;

    /* Adaptive trait aura ring under monster */
    if (m->traits != ADAPT_NONE) {
        unsigned int aura_col = monster_primary_trait_color(m->traits);
        render_iso_ring(sx, sy + 1.0f, 11.5f * scale, 5.8f * scale, 14, aura_col);
    }
    if (is_locked) {
        render_iso_ring(sx, sy + 1.0f, 14.5f * scale, 7.2f * scale, 16, RGBA8(255, 232, 115, 235));
    }

    /* Shadow */
    render_iso_diamond(sx, sy + 1.0f, 8.5f * scale, 4.2f * scale, RGBA8(0, 0, 0, 130), 0);

    unsigned int top_c, left_c, right_c, eye_c;
    if (m->hurt_flash > 0.0f) {
        top_c = RGBA8(255, 245, 245, 255);
        left_c = RGBA8(255, 150, 150, 255);
        right_c = RGBA8(230, 100, 100, 255);
        eye_c = RGBA8(255, 255, 255, 255);
    } else if (m->adapt_flash > 0.0f) {
        top_c = RGBA8(150, 255, 245, 255);
        left_c = RGBA8(90, 220, 230, 255);
        right_c = RGBA8(60, 180, 210, 255);
        eye_c = RGBA8(255, 255, 160, 255);
    } else {
        switch (m->kind) {
            case MON_STALKER:
                top_c   = RGBA8(206, 78, 86, 255);
                left_c  = RGBA8(162, 52, 62, 255);
                right_c = RGBA8(120, 36, 46, 255);
                eye_c   = RGBA8(255, 230, 110, 255);
                break;
            case MON_GOLEM:
                top_c   = RGBA8(136, 144, 164, 255);
                left_c  = RGBA8(96, 104, 124, 255);
                right_c = RGBA8(68, 74, 92, 255);
                eye_c   = RGBA8(255, 130, 70, 255);
                break;
            case MON_WEAVER:
                top_c   = RGBA8(138, 92, 218, 255);
                left_c  = RGBA8(102, 62, 178, 255);
                right_c = RGBA8(74, 42, 138, 255);
                eye_c   = RGBA8(120, 245, 255, 255);
                break;
            case MON_WARDEN:
            default:
                top_c   = RGBA8(235, 112, 64, 255);
                left_c  = RGBA8(148, 44, 68, 255);
                right_c = RGBA8(96, 28, 48, 255);
                eye_c   = RGBA8(255, 240, 130, 255);
                break;
        }
    }

    float hover = (m->kind == MON_WEAVER) ? (3.0f + 2.0f * sinf(gs->time_sec * 4.2f + m->spawn_wx)) : 0.0f;
    float crouch = (m->windup_timer > 0.0f) ? 2.5f : 0.0f;
    float body_h = ((m->kind == MON_GOLEM) ? 15.0f : (m->kind == MON_WARDEN ? 19.0f : 12.0f)) * scale - crouch;
    float body_w = ((m->kind == MON_GOLEM) ? 8.2f : 6.4f) * scale;

    render_iso_prism(sx, sy - hover, body_w, body_w * 0.52f, body_h, top_c, left_c, right_c);

    /* Horns / Crest for Elite or Warden */
    if (m->is_elite || m->kind == MON_WARDEN) {
        float top_y = sy - hover - body_h;
        render_tri(sx - 6.0f * scale, top_y, sx - 2.0f * scale, top_y, sx - 8.0f * scale, top_y - 7.0f * scale,
                   RGBA8(255, 214, 112, 255));
        render_tri(sx + 2.0f * scale, top_y, sx + 6.0f * scale, top_y, sx + 8.0f * scale, top_y - 7.0f * scale,
                   RGBA8(255, 214, 112, 255));
    }

    /* Glowing eyes */
    render_rect(sx - 3.5f * scale, sy - hover - body_h * 0.68f, 2.2f, 2.2f, eye_c);
    render_rect(sx + 1.3f * scale, sy - hover - body_h * 0.68f, 2.2f, 2.2f, eye_c);

    /* Compact world-space HP bar above damaged or elite monsters */
    if (m->hp < m->max_hp || m->is_elite || is_locked) {
        float bw = 24.0f * scale;
        float bx = sx - bw * 0.5f;
        float by = sy - hover - body_h - (m->is_elite ? 12.0f : 7.0f);
        float ratio = m->hp / (m->max_hp > 1.0f ? m->max_hp : 1.0f);
        if (ratio < 0.0f) ratio = 0.0f;
        if (ratio > 1.0f) ratio = 1.0f;

        render_rect(bx - 1.0f, by - 1.0f, bw + 2.0f, 5.0f, RGBA8(10, 10, 18, 210));
        render_rect(bx, by, bw * ratio, 3.0f,
                    m->is_elite ? RGBA8(255, 176, 64, 255) : RGBA8(232, 74, 84, 255));
    }
}

void render_world_scene(const GameState *gs) {
    float cam_wx = gs->cam_wx;
    float cam_wy = gs->cam_wy;

    if (gs->shake_timer > 0.0f) {
        cam_wx += sinf(gs->time_sec * 65.0f) * gs->shake_mag;
        cam_wy += cosf(gs->time_sec * 53.0f) * gs->shake_mag;
    }

    int min_tx = (int)floorf(cam_wx) - 16;
    int max_tx = (int)ceilf(cam_wx) + 16;
    int min_ty = (int)floorf(cam_wy) - 16;
    int max_ty = (int)ceilf(cam_wy) + 16;
    if (min_tx < 0) min_tx = 0;
    if (min_ty < 0) min_ty = 0;
    if (max_tx >= MAP_SIZE) max_tx = MAP_SIZE - 1;
    if (max_ty >= MAP_SIZE) max_ty = MAP_SIZE - 1;

    SortableDraw sortables[MAX_SORTABLES];
    int sort_count = 0;

    /* Pass 1: Isometric Ground Tiles + Collect Vertical Tiles */
    for (int sum = min_tx + min_ty; sum <= max_tx + max_ty; sum++) {
        int tx_start = (sum - max_ty > min_tx) ? (sum - max_ty) : min_tx;
        int tx_end   = (sum - min_ty < max_tx) ? (sum - min_ty) : max_tx;

        for (int tx = tx_start; tx <= tx_end; tx++) {
            int ty = sum - tx;
            float sx, sy;
            world_to_screen((float)tx + 0.5f, (float)ty + 0.5f, 0.0f, cam_wx, cam_wy, &sx, &sy);
            if (sx < -ISO_TILE_HALF_W - 6.0f || sx > SCREEN_WIDTH + ISO_TILE_HALF_W + 6.0f ||
                sy < -ISO_TILE_HALF_H - 6.0f || sy > SCREEN_HEIGHT + ISO_TILE_HALF_H + 34.0f) {
                continue;
            }

            TileType t = (TileType)gs->tiles[ty][tx];
            unsigned int fill = tile_floor_color(t, tx, ty, gs->time_sec);
            unsigned int edge = RGBA8(255, 255, 255, 15);
            if (t == TILE_WATER_CHASM) {
                edge = RGBA8(80, 150, 220, 35);
            } else if (t == TILE_SHRINE_PAD) {
                edge = RGBA8(238, 214, 140, 80);
            }
            render_iso_diamond(sx, sy, ISO_TILE_HALF_W, ISO_TILE_HALF_H, fill, edge);

            if ((t == TILE_WALL_PILLAR || t == TILE_CRYSTAL_SPIRE ||
                 (t == TILE_SHRINE_PAD && tx == MAP_SIZE / 2 && ty == MAP_SIZE / 2)) &&
                sort_count < MAX_SORTABLES) {
                sortables[sort_count].depth = (float)tx + (float)ty + 1.0f;
                sortables[sort_count].kind = (t == TILE_WALL_PILLAR) ? DRAW_PILLAR
                                           : (t == TILE_CRYSTAL_SPIRE) ? DRAW_SPIRE : DRAW_SHRINE;
                sortables[sort_count].index = 0;
                sortables[sort_count].tx = tx;
                sortables[sort_count].ty = ty;
                sort_count++;
            }
        }
    }

    /* Pass 2: Ground Auras, Sanctuary Ring, Monster Telegraph Warnings, Player Nova */
    {
        float sh_sx, sh_sy;
        world_to_screen((float)(MAP_SIZE / 2) + 0.5f, (float)(MAP_SIZE / 2) + 0.5f, 0.0f, cam_wx, cam_wy, &sh_sx, &sh_sy);
        if (sh_sx > -80.0f && sh_sx < SCREEN_WIDTH + 80.0f && sh_sy > -60.0f && sh_sy < SCREEN_HEIGHT + 60.0f) {
            float pulse = 36.0f + 4.0f * sinf(gs->time_sec * 2.5f);
            render_iso_ring(sh_sx, sh_sy, pulse, pulse * 0.5f, 20, RGBA8(240, 215, 138, 125));
        }
    }

    for (int i = 0; i < MAX_MONSTERS; i++) {
        const Monster *m = &gs->monsters[i];
        if (!m->active || m->windup_timer <= 0.0f) continue;
        float sx, sy;
        world_to_screen(m->wx, m->wy, 0.0f, cam_wx, cam_wy, &sx, &sy);
        float prog = 1.0f - (m->windup_timer / (m->windup_total > 0.05f ? m->windup_total : 0.5f));
        float rad = (m->kind == MON_GOLEM || m->kind == MON_WARDEN) ? 34.0f : 22.0f;
        render_iso_ring(sx, sy, rad, rad * 0.5f, 16, RGBA8(255, 75, 75, 210));
        render_iso_ring(sx, sy, rad * prog, rad * 0.5f * prog, 14, RGBA8(255, 190, 90, 235));
    }

    if (gs->player.nova_flash_timer > 0.0f) {
        float px, py;
        world_to_screen(gs->player.wx, gs->player.wy, 2.0f, cam_wx, cam_wy, &px, &py);
        float t = 1.0f - (gs->player.nova_flash_timer / 0.35f);
        float r = 18.0f + t * 58.0f;
        render_iso_ring(px, py, r, r * 0.5f, 22, RGBA8(120, 225, 255, (unsigned int)((1.0f - t) * 240.0f)));
        render_iso_ring(px, py, r * 0.72f, r * 0.36f, 18, RGBA8(210, 165, 255, (unsigned int)((1.0f - t) * 200.0f)));
    }

    /* Pass 3: Populate & Depth-Sort Vertical World Entities */
    for (int i = 0; i < MAX_CHESTS && sort_count < MAX_SORTABLES; i++) {
        if (!gs->chests[i].active) continue;
        sortables[sort_count].depth = gs->chests[i].wx + gs->chests[i].wy;
        sortables[sort_count].kind = DRAW_CHEST;
        sortables[sort_count].index = i;
        sort_count++;
    }
    for (int i = 0; i < MAX_GROUND_LOOT && sort_count < MAX_SORTABLES; i++) {
        if (!gs->loot[i].active) continue;
        sortables[sort_count].depth = gs->loot[i].wx + gs->loot[i].wy;
        sortables[sort_count].kind = DRAW_LOOT;
        sortables[sort_count].index = i;
        sort_count++;
    }
    for (int i = 0; i < MAX_MONSTERS && sort_count < MAX_SORTABLES; i++) {
        if (!gs->monsters[i].active) continue;
        sortables[sort_count].depth = gs->monsters[i].wx + gs->monsters[i].wy;
        sortables[sort_count].kind = DRAW_MONSTER;
        sortables[sort_count].index = i;
        sort_count++;
    }
    for (int i = 0; i < MAX_PROJECTILES && sort_count < MAX_SORTABLES; i++) {
        if (!gs->projectiles[i].active) continue;
        sortables[sort_count].depth = gs->projectiles[i].wx + gs->projectiles[i].wy;
        sortables[sort_count].kind = DRAW_PROJECTILE;
        sortables[sort_count].index = i;
        sort_count++;
    }
    if (sort_count < MAX_SORTABLES) {
        sortables[sort_count].depth = gs->player.wx + gs->player.wy;
        sortables[sort_count].kind = DRAW_PLAYER;
        sortables[sort_count].index = 0;
        sort_count++;
    }

    /* Insertion sort back-to-front by isometric depth (wx + wy) */
    for (int i = 1; i < sort_count; i++) {
        SortableDraw key = sortables[i];
        int j = i - 1;
        while (j >= 0 && sortables[j].depth > key.depth) {
            sortables[j + 1] = sortables[j];
            j--;
        }
        sortables[j + 1] = key;
    }

    for (int i = 0; i < sort_count; i++) {
        const SortableDraw *d = &sortables[i];
        switch (d->kind) {
            case DRAW_PILLAR: {
                float sx, sy;
                world_to_screen((float)d->tx + 0.5f, (float)d->ty + 0.5f, 0.0f, cam_wx, cam_wy, &sx, &sy);
                render_iso_prism(sx, sy, ISO_TILE_HALF_W * 0.78f, ISO_TILE_HALF_H * 0.78f, 20.0f,
                                 RGBA8(112, 110, 128, 255),
                                 RGBA8(82, 80, 96, 255),
                                 RGBA8(58, 56, 70, 255));
                break;
            }
            case DRAW_SPIRE: {
                float sx, sy;
                world_to_screen((float)d->tx + 0.5f, (float)d->ty + 0.5f, 0.0f, cam_wx, cam_wy, &sx, &sy);
                int shimmer = (int)(35.0f * (0.5f + 0.5f * sinf(gs->time_sec * 3.0f + (float)d->tx)));
                render_iso_prism(sx, sy, ISO_TILE_HALF_W * 0.62f, ISO_TILE_HALF_H * 0.62f, 24.0f,
                                 RGBA8(110 + shimmer, 215, 255, 240),
                                 RGBA8(64, 158 + shimmer / 2, 224, 240),
                                 RGBA8(42, 108, 178, 240));
                break;
            }
            case DRAW_SHRINE: {
                float sx, sy;
                world_to_screen((float)d->tx + 0.5f, (float)d->ty + 0.5f, 0.0f, cam_wx, cam_wy, &sx, &sy);
                float hover = 3.0f + 2.5f * sinf(gs->time_sec * 2.8f);
                render_iso_prism(sx, sy, 12.0f, 6.0f, 10.0f,
                                 RGBA8(210, 198, 168, 255),
                                 RGBA8(164, 152, 126, 255),
                                 RGBA8(124, 114, 94, 255));
                render_iso_prism(sx, sy - 12.0f - hover, 6.0f, 3.2f, 14.0f,
                                 RGBA8(255, 232, 148, 255),
                                 RGBA8(230, 190, 96, 255),
                                 RGBA8(186, 144, 64, 255));
                break;
            }
            case DRAW_CHEST: {
                const WorldChest *c = &gs->chests[d->index];
                float sx, sy;
                world_to_screen(c->wx, c->wy, 0.0f, cam_wx, cam_wy, &sx, &sy);
                if (sx < -30.0f || sx > SCREEN_WIDTH + 30.0f || sy < -30.0f || sy > SCREEN_HEIGHT + 30.0f) break;
                if (c->opened) {
                    render_iso_prism(sx, sy, 8.0f, 4.2f, 5.0f,
                                     RGBA8(72, 58, 48, 200),
                                     RGBA8(56, 44, 36, 200),
                                     RGBA8(42, 32, 26, 200));
                } else {
                    render_iso_prism(sx, sy, 8.5f, 4.5f, 9.5f,
                                     RGBA8(236, 192, 84, 255),
                                     RGBA8(168, 108, 46, 255),
                                     RGBA8(122, 74, 30, 255));
                    render_iso_ring(sx, sy + 1.0f, 11.0f, 5.5f, 10, RGBA8(255, 224, 118, 120));
                }
                break;
            }
            case DRAW_LOOT: {
                const GroundLoot *l = &gs->loot[d->index];
                float sx, sy;
                float bob = 3.0f + 2.2f * sinf(gs->time_sec * 4.5f + l->bob_phase);
                world_to_screen(l->wx, l->wy, 0.0f, cam_wx, cam_wy, &sx, &sy);
                if (sx < -24.0f || sx > SCREEN_WIDTH + 24.0f || sy < -24.0f || sy > SCREEN_HEIGHT + 24.0f) break;
                unsigned int col = l->is_tonic ? RGBA8(110, 245, 160, 255) : items_rarity_color(l->item.rarity);
                /* Vertical loot beam */
                render_rect(sx - 1.0f, sy - 22.0f, 2.0f, 20.0f, (col & 0x00FFFFFFu) | RGBA8(0, 0, 0, 110));
                render_iso_diamond(sx, sy - bob, 5.0f, 5.0f, col, RGBA8(255, 255, 255, 210));
                break;
            }
            case DRAW_MONSTER:
                draw_monster_entity(gs, &gs->monsters[d->index], d->index == gs->locked_monster, cam_wx, cam_wy);
                break;
            case DRAW_PLAYER:
                draw_player_entity(gs, cam_wx, cam_wy);
                break;
            case DRAW_PROJECTILE: {
                const Projectile *pr = &gs->projectiles[d->index];
                float sx, sy;
                world_to_screen(pr->wx, pr->wy, 8.0f, cam_wx, cam_wy, &sx, &sy);
                if (sx < -20.0f || sx > SCREEN_WIDTH + 20.0f || sy < -20.0f || sy > SCREEN_HEIGHT + 20.0f) break;
                render_iso_diamond(sx, sy + 8.0f, 4.0f, 2.0f, RGBA8(0, 0, 0, 90), 0);
                render_iso_diamond(sx, sy, 5.5f, 4.0f, pr->color, RGBA8(255, 255, 255, 220));
                break;
            }
        }
    }

    /* Pass 4: Floating World Combat Texts */
    for (int i = 0; i < MAX_FLOAT_TEXTS; i++) {
        const FloatingText *ft = &gs->floats[i];
        if (!ft->active) continue;
        float sx, sy;
        world_to_screen(ft->wx, ft->wy, ft->wz, cam_wx, cam_wy, &sx, &sy);
        if (sx < -40.0f || sx > SCREEN_WIDTH + 40.0f || sy < -20.0f || sy > SCREEN_HEIGHT + 20.0f) continue;
        int tw = render_text_width(ft->text, 1.0f);
        render_text_shadow(sx - (float)(tw / 2), sy, ft->color, ft->text);
    }
}
