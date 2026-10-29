#ifndef RYUNARIA_RENDER_H
#define RYUNARIA_RENDER_H

#include "ryunaria.h"

#define RGBA8(r, g, b, a) \
    ((((unsigned int)(a) & 0xFFu) << 24) | \
     (((unsigned int)(b) & 0xFFu) << 16) | \
     (((unsigned int)(g) & 0xFFu) << 8)  | \
     ((unsigned int)(r) & 0xFFu))

void render_init(void);
void render_shutdown(void);
void render_begin_frame(unsigned int clear_color);
void render_end_frame(void);

void world_to_screen(float wx, float wy, float wz, float cam_wx, float cam_wy, float *sx, float *sy);

void render_rect(float x, float y, float w, float h, unsigned int color);
void render_rect_outline(float x, float y, float w, float h, float thickness, unsigned int color);
void render_panel(float x, float y, float w, float h, unsigned int bg, unsigned int border);

void render_tri(float x0, float y0, float x1, float y1, float x2, float y2, unsigned int color);
void render_quad(float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3, unsigned int color);
void render_line(float x0, float y0, float x1, float y1, unsigned int color);
void render_iso_diamond(float cx, float cy, float hw, float hh, unsigned int fill, unsigned int edge);
void render_iso_prism(float cx, float cy, float hw, float hh, float height,
                      unsigned int top_col, unsigned int left_col, unsigned int right_col);
void render_iso_ring(float cx, float cy, float rx, float ry, int segments, unsigned int color);
void render_slash_arc(float cx, float cy, float rx, float ry, float center_angle,
                      float sweep_rad, unsigned int inner_col, unsigned int outer_col);

int render_text_width(const char *text, float scale);
void render_text(float x, float y, unsigned int color, const char *text);
void render_text_shadow(float x, float y, unsigned int color, const char *text);
void render_text_scaled(float x, float y, float scale, unsigned int color, const char *text);
void render_printf(float x, float y, unsigned int color, const char *fmt, ...);

void render_world_scene(const GameState *gs);

#endif
