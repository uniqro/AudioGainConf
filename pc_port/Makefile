# ============================================================
#  Makefile — Linux / GCC build
#
#  Usage (from AudioDspConfig/ root):
#    make -f pc_port/Makefile
#    make -f pc_port/Makefile run
#    make -f pc_port/Makefile clean
# ============================================================

CC      = gcc
TARGET  = audio_dsp_server
FLASH   = audio_flash.bin

CFLAGS  = -Wall -Wextra \
          -Wno-int-conversion \
          -Wno-unused-parameter \
          -include pc_port/include/vxWorks.h \
          -I pc_port/include \
          -I .

LDFLAGS = -lpthread -lm

SRCS = AudioConf/AudioConf.c \
       AudioConf/AudioConfigTask.c \
       AudioConf/AudioConfigHandler.c \
       AudioConf/AudioParamDef.c \
       AudioConf/AudioSpiGateway.c \
       AudioConf/AudioFlashMgr.c \
       pc_port/src/tasklib_stubs.c \
       pc_port/src/flash_stubs.c \
       pc_port/src/spi_stubs.c \
       pc_port/src/dispatch_glue.c \
       pc_port/main.c

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) $(SRCS) -o $@ $(LDFLAGS)
	@echo ""
	@echo "Build successful: $(TARGET)"

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) $(FLASH)
