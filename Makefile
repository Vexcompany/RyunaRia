TARGET = RyunaRia
OBJS = \
	src/main.o \
	src/render.o \
	src/world.o \
	src/items.o \
	src/combat.o \
	src/ui.o

# PSPDEV containers expose PSPDEV; PSPSDK may not be exported by default.
PSPSDK ?= $(PSPDEV)/psp/sdk

INCDIR = src include
CFLAGS = -O2 -G0 -Wall -Wextra
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

LIBS = -lpspgum -lpspgu -lpsprtc -lpspdebug -lpspdisplay -lpspctrl -lpspkernel -lm

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = Ryuna Ria
PSP_EBOOT_SFO = PARAM.SFO

ifneq ($(wildcard ICON0.PNG),)
PSP_EBOOT_ICON = ICON0.PNG
endif

BUILD_PRX = 1
PSP_FW_VERSION = 500

include $(PSPSDK)/lib/build.mak
