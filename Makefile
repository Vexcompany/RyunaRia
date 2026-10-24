TARGET = RyunaRia
OBJS = src/main.o

CFLAGS = -O2 -G0 -Wall -Wextra -Isrc -Iinclude
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti

LIBS = -lpspgum -lpspgu -lpspdebug -lpspdisplay -lpspctrl -lpspkernel

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = Ryuna Ria
PSP_EBOOT_VERSION = 0.1.0
PSP_EBOOT_SFO = PARAM.SFO

BUILD_PRX = 1
PSP_FW_VERSION = 500

include $(PSPSDK)/lib/build.mak

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET).elf $(TARGET).prx $(TARGET).SFO $(PSP_EBOOT_SFO) EBOOT.PBP
	rm -rf $(TARGET) $(TARGET)%
