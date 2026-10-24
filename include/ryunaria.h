#ifndef RYUNARIA_H
#define RYUNARIA_H

#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspdebug.h>

#define RYUNA_RIA_TITLE "Ryuna Ria"
#define RYUNA_RIA_VERSION "0.1.0"
#define SCREEN_WIDTH 480
#define SCREEN_HEIGHT 272

int game_init(void);
void game_update(const SceCtrlData *pad);
void game_draw(void);
void game_shutdown(void);

#endif
