TARGET = pcsx4all
PORT   = sdl

# If V=1 was passed to 'make', don't hide commands:
ifeq ($(V),1)
	HIDECMD:=
else
	HIDECMD:=@
endif

# Using 'gpulib' adapted from PCSX Rearmed is default, specify
#  USE_GPULIB=0 as param to 'make' when building to disable it.
USE_GPULIB ?= 1
SUPPORT_CHD ?= 1
HAVE_RUMBLE ?= 1
PROFILE = APPLY

#GPU   = gpu_dfxvideo
#GPU   = gpu_drhell
#GPU   = gpu_null
GPU   = gpu_unai

SPU   = spu_pcsxrearmed

RECOMPILER = mips

RM     = rm -f
MD     = mkdir
CC     = /opt/gcw0-toolchain/bin/mipsel-linux-gcc
CXX    = /opt/gcw0-toolchain/bin/mipsel-linux-g++
LD     = /opt/gcw0-toolchain/bin/mipsel-linux-g++

SYSROOT     := $(shell $(CC) --print-sysroot)
SDL_CONFIG  := $(SYSROOT)/usr/bin/sdl-config
SDL_CFLAGS  := $(shell $(SDL_CONFIG) --cflags)
SDL_LIBS    := $(shell $(SDL_CONFIG) --libs)

LDFLAGS := $(SDL_LIBS) -lSDL_mixer -lSDL_image -lrt -lz -lshake
LDFLAGS		+= -Wl,-zcommon-page-size=2097152 -Wl,-zmax-page-size=2097152 -lhugetlbfs

C_ARCH = -mno-memcpy -mips32r2 -DGCW_ZERO -DRUMBLE -DRG350 -DSHMEM_MIRRORING

ifeq ($(DEV),1)
	# If DEV=1 is passed to 'make', don't mmap anything to virtual address 0.
	#  This is for development/debugging: it allows null pointer dereferences
	#  to segfault as they normally would. We'll map PS1 address space higher.
	#
	# Furthermore, div-by-zero is also checked for at runtime (default for GCC)
else
	# Default: Allows address-conversion optimization in dynarec. PS1 addresses
	#          will be mapped/mirrored at virtual address 0.
	#
	# Null pointer dereferences will NOT segfault.
	# Locate program at virtual address 0x4000_0000. Anything below is free for
	#  fixed mappings by dynarec, etc. Otherwise, the Linux default seems to be
	#  0x40_0000 i.e., offset 4MB from 0. That's not enough room to map and
	#  mirror PS1 address space.
	#C_ARCH  += -DMMAP_TO_ADDRESS_ZERO
	#LDFLAGS += -Wl,-Ttext-segment=0x40000000
	LDFLAGS = $(SDL_LIBS) -lrt -lz -Wl,--as-needed -Wl,--gc-sections -s -lshake
	LDFLAGS += -flto
	# Furthermore, div-by-zero checks are disabled (helps software rendering)
	C_ARCH  += -mno-check-zero-division
endif

ifeq ($(PROFILE),YES)
	C_ARCH  += -fprofile-generate=/usr/local/home/pcsx_gcda
	LDFLAGS += -lgcov
endif

ifeq ($(PROFILE),APPLY)
	C_ARCH  += -fprofile-use=$(PWD)
endif

CFLAGS := $(C_ARCH) -mplt -mno-shared -Ofast -fexpensive-optimizations -flto -DTIME_IN_MSEC \
	-Wall -Wunused -Wpointer-arith \
	-Wno-sign-compare -Wno-cast-align \
	-Isrc -Isrc/spu/$(SPU) -D$(SPU) -Isrc/gpu/$(GPU) \
	-Isrc/port/$(PORT) \
	-Isrc/plugin_lib \
	-Isrc/external_lib -Isrc/port/common \
	-DXA_HACK \
	-DINLINE="static __inline__" -Dasm="__asm__ __volatile__" \
	$(SDL_CFLAGS)

# Convert plugin names to uppercase and make them CFLAG defines
CFLAGS += -D$(shell echo $(GPU) | tr a-z A-Z)
CFLAGS += -D$(shell echo $(SPU) | tr a-z A-Z)
CFLAGS += -DUSE_BGR15

ifdef RECOMPILER
CFLAGS += -DPSXREC -D$(RECOMPILER)
endif

OBJDIRS = obj obj/gpu obj/gpu/$(GPU) obj/spu obj/spu/$(SPU) \
	  obj/recompiler obj/recompiler/$(RECOMPILER) \
	  obj/port obj/port/$(PORT) \
	  obj/plugin_lib obj/external_lib obj/port/common

all: maketree $(TARGET)

OBJS = \
	obj/r3000a.o obj/misc.o obj/plugins.o obj/psxmem.o obj/psxhw.o \
	obj/psxcounters.o obj/psxdma.o obj/psxbios.o obj/psxhle.o obj/psxevents.o \
	obj/psxcommon.o \
	obj/plugin_lib/plugin_lib.o obj/plugin_lib/pl_sshot.o \
	obj/psxinterpreter.o \
	obj/mdec.o obj/decode_xa.o \
	obj/cdriso.o obj/cdrom.o obj/ppf.o obj/cheat.o \
	obj/sio.o obj/pad.o \
	obj/external_lib/ioapi.o obj/external_lib/unzip.o \
	obj/port/common/frontend.o obj/port/common/cdrom_hacks.o

ifdef RECOMPILER
OBJS += \
	obj/recompiler/mips/recompiler.o \
	obj/recompiler/mips/host_asm.o \
	obj/recompiler/mips/mem_mapping.o \
	obj/recompiler/mips/mips_codegen.o \
	obj/recompiler/mips/mips_disasm.o
endif

######################################################################
#  GPULIB from PCSX Rearmed:
#  Fixes many game incompatibilities and centralizes/improves many
#  things that once were the responsibility of individual GPU plugins.
#  NOTE: For now, only GPU Unai has been adapted.
ifeq ($(USE_GPULIB),1)
CFLAGS += -DUSE_GPULIB
OBJDIRS += obj/gpu/gpulib
OBJS += obj/gpu/$(GPU)/gpulib_if.o
OBJS += obj/gpu/gpulib/gpu.o obj/gpu/gpulib/vout_port.o
else
OBJS += obj/gpu/$(GPU)/gpu.o
endif
######################################################################

OBJS += obj/gte.o
OBJS += obj/spu/$(SPU)/spu.o

OBJS += obj/port/$(PORT)/port.o

OBJS += obj/plugin_lib/perfmon.o

ifeq ($(SUPPORT_CHD),1)
OBJDIRS += obj/libchdr obj/libchdr/deps/lzma-19.00/src obj/libchdr/src
CFLAGS += -DHAVE_CHD -D_7ZIP_ST -Isrc/libchdr/include/libchdr -Isrc/libchdr/include/dr_libs -Isrc/libchdr/include -Isrc/libchdr/deps/lzma-19.00/include
OBJS += obj/libchdr/src/libchdr_bitstream.o
OBJS += obj/libchdr/src/libchdr_cdrom.o
OBJS += obj/libchdr/src/libchdr_chd.o
OBJS += obj/libchdr/src/libchdr_flac.o
OBJS += obj/libchdr/src/libchdr_huffman.o
OBJS += obj/libchdr/deps/lzma-19.00/src/Alloc.o obj/libchdr/deps/lzma-19.00/src/Bra86.o obj/libchdr/deps/lzma-19.00/src/BraIA64.o obj/libchdr/deps/lzma-19.00/src/CpuArch.o obj/libchdr/deps/lzma-19.00/src/Delta.o
OBJS += obj/libchdr/deps/lzma-19.00/src/LzFind.o obj/libchdr/deps/lzma-19.00/src/Lzma86Dec.o obj/libchdr/deps/lzma-19.00/src/LzmaDec.o obj/libchdr/deps/lzma-19.00/src/LzmaEnc.o obj/libchdr/deps/lzma-19.00/src/Sort.o
endif

ifeq ($(HAVE_RUMBLE),1)
CFLAGS += -DRUMBLE -Isrc/libShake/include
OBJDIRS += obj/libShake/src/common obj/libShake/src/linux
OBJS += obj/libShake/src/common/error.o obj/libShake/src/common/helpers.o
OBJS += obj/libShake/src/common/presets.o obj/libShake/src/linux/shake.o
endif

#******************************************
# spu_pcsxrearmed section BEGIN
#******************************************

##########
# Use a non-default SPU update frequency for these slower devices
#  to avoid audio dropouts. 0: once-per-frame (default)   5: 32-times-per-frame
#
#  On faster GCW Zero platform, update 2 times per frame
CFLAGS += -DSPU_UPDATE_FREQ_DEFAULT=1
##########

##########
# Similarly, set higher XA audio update frequency for slower devices
#
#  On faster GCW Zero platform, use auto-update
CFLAGS += -DFORCED_XA_UPDATES_DEFAULT=1
##########

ifeq ($(SPU),spu_pcsxrearmed)
# Specify which audio backend to use:
SOUND_DRIVERS=sdl
#SOUND_DRIVERS=alsa
#SOUND_DRIVERS=oss
#SOUND_DRIVERS=pulseaudio

# Note: obj/spu/spu_pcsxrearmed/spu.o will already have been added to OBJS
#		list previously in Makefile
OBJS += obj/spu/spu_pcsxrearmed/dma.o obj/spu/spu_pcsxrearmed/freeze.o \
	obj/spu/spu_pcsxrearmed/out.o obj/spu/spu_pcsxrearmed/nullsnd.o \
	obj/spu/spu_pcsxrearmed/registers.o
ifeq "$(ARCH)" "arm"
OBJS += obj/spu/spu_pcsxrearmed/arm_utils.o
endif
ifeq "$(HAVE_C64_TOOLS)" "1"
obj/spu/spu_pcsxrearmed/spu.o: CFLAGS += -DC64X_DSP
obj/spu/spu_pcsxrearmed/spu.o: obj/spu/spu_pcsxrearmed/spu_c64x.c
frontend/menu.o: CFLAGS += -DC64X_DSP
endif
ifneq ($(findstring oss,$(SOUND_DRIVERS)),)
obj/spu/spu_pcsxrearmed/out.o: CFLAGS += -DHAVE_OSS
OBJS += obj/spu/spu_pcsxrearmed/oss.o
endif
ifneq ($(findstring alsa,$(SOUND_DRIVERS)),)
obj/spu/spu_pcsxrearmed/out.o: CFLAGS += -DHAVE_ALSA
OBJS += obj/spu/spu_pcsxrearmed/alsa.o
LDFLAGS += -lasound
endif
ifneq ($(findstring sdl,$(SOUND_DRIVERS)),)
obj/spu/spu_pcsxrearmed/out.o: CFLAGS += -DHAVE_SDL
OBJS += obj/spu/spu_pcsxrearmed/sdl.o
endif
ifneq ($(findstring pulseaudio,$(SOUND_DRIVERS)),)
obj/spu/spu_pcsxrearmed/out.o: CFLAGS += -DHAVE_PULSE
OBJS += obj/spu/spu_pcsxrearmed/pulseaudio.o
endif
ifneq ($(findstring libretro,$(SOUND_DRIVERS)),)
obj/spu/spu_pcsxrearmed/out.o: CFLAGS += -DHAVE_LIBRETRO
endif

endif
#******************************************
# spu_pcsxrearmed END
#******************************************

CXXFLAGS := $(CFLAGS) -fno-rtti -fno-exceptions

$(TARGET): $(OBJS)
	@echo Linking $(TARGET)...
	$(HIDECMD)$(CXX) $(OBJS) -o $@ $(CXXFLAGS) $(LDFLAGS)
	/opt/gcw0-toolchain/bin/hugeedit --text --data $(TARGET)
	@echo
ifeq ($(DEV),1)
	@echo "-> WARNING: This is a development build.                                  "
	@echo "            Mapping to virtual address zero is disabled. As a result,     "
	@echo "            some address-related dynarec optimizations are disabled.      "
	@echo "            Furthermore, div-by-zero checks are enabled in C/C++ code.    "
else
	@echo "-> This is a release build.                                               "
	@echo "    * Null pointer dereferences will NOT segfault (mmap to 0 is allowed). "
	@echo "    * Div-by-zero in C/C++ code will NOT trap & abort execution.          "
	@echo "   If developing/debugging, you can pass DEV=1 on make command-line to    "
	@echo "   disable these behaviors. Be sure to do a clean rebuild.                "
endif

obj/%.o: src/%.c
	@echo Compiling $<...
	$(HIDECMD)$(CC) $(CFLAGS) -c $< -o $@

obj/%.o: src/%.cpp
	@echo Compiling $<...
	$(HIDECMD)$(CXX) $(CXXFLAGS) -c $< -o $@

obj/%.o: src/%.s
	@echo Compiling $<...
	$(HIDECMD)$(CXX) $(CFLAGS) -c $< -o $@

obj/%.o: src/%.S
	@echo Compiling $<...
	$(HIDECMD)$(CXX) $(CFLAGS) -c $< -o $@

$(sort $(OBJDIRS)):
	$(HIDECMD)$(MD) -p $@

maketree: $(sort $(OBJDIRS))

clean:
	$(RM) -r obj
	$(RM) $(TARGET)
