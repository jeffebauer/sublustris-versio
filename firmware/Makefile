# Sublustris Versio firmware (standard libDaisy app, internal flash, SPEC §5.3;
# seeded from Resilio Versio's firmware, its ADR 0011).
#
# Three variants, one main.cpp, selected by MODE (SPEC §7 M0/M3):
#   make -C firmware MODE=m0test   -> build/sublustris_versio_m0test.bin  (SPEC §7 M0)
#   make -C firmware MODE=profile  -> build/sublustris_versio_profile.bin (SPEC §7 M3)
#   make -C firmware [MODE=release]-> build/sublustris_versio.bin         (default; the real instrument)
#
# `make -C firmware all-variants` builds all three and reports each size
# against the 128 KB internal-flash limit (SPEC §5.3), failing if any is over.
#
# PROFILE_BLOCK (profile variant only, default 48, SPEC §5) selects the audio
# block size so M3 can compare 48 vs 96/128:
#   make -C firmware MODE=profile PROFILE_BLOCK=96

MODE ?= release
PROFILE_BLOCK ?= 48
# Profile-only switches (overnight review, 26 Oct 2026; firmware/README.md "profile"):
#   PROFILE_FZ=0          the profile build without flush-to-zero (FPSCR/FPDSCR.FZ)
#   PROFILE_SDRAM_TIGHT=0 the corner lap at libDaisy's SDRAM timing (the micro-tests still try 3 / 3)
#   PROFILE_ITCM=0        the profile with the Core's hot code in flash, as before ITCM (round 12)
# A non-default value gets its own object directory and .bin name (sublustris_versio_profile-nofz.bin, ...).
# Release and profile run Engine.o, Fdn.o and Plate.o from ITCM (STM32H750IB_flash_itcm.lds; M3 round 12,
# 26 Oct 2026: it boots, 2.5-3.5 points off every corner); m0test doesn't link the Core.
PROFILE_FZ ?= 1
PROFILE_SDRAM_TIGHT ?= 1
PROFILE_ITCM ?= 1
# Release-only switch (diagnostic, 26 Oct 2026): RELEASE_SDRAM_TIGHT=0 keeps libDaisy's SDRAM timing
# (sublustris_versio-sdlibdaisy.bin), to tell a timing fault on rack power from a CPU overrun.
RELEASE_SDRAM_TIGHT ?= 1
# RELEASE_DIAG=1: the playing build on USB power, knobs held at noon, its own test input, a serial report
# (sublustris_versio-diag.bin; main.cpp "RELEASE_DIAG").
RELEASE_DIAG ?= 0
# RELEASE_LATE_LIGHT=1: the rightmost LED flashes red after a late block, amber after a near one
# (sublustris_versio-latelight.bin; a diagnostic, not in the normal build: flash).
RELEASE_LATE_LIGHT ?= 0
# Integration dry run (trial switches, all off by default; each gets its own .bin name):
#   SIZE_GROUPS=n SIZE's crossfade in n groups (1, 2 or 8) as the default instead of 4 (Fdn.h IV_SIZE_GROUPS; -gn)
#   CANDIDATE=1   profile only: the candidate lap (main.cpp IV_PROFILE_CANDIDATE) in place of the corner lap,
#                 the micro-tests and the size_moves hunt (-candidate)
#   SIZESPIKE=1   profile only: the SIZE-spike lap (main.cpp IV_PROFILE_SIZESPIKE; 27 Oct 2026, the owner's red light on
#                 SIZE moves with an empty tank) in place of the corner lap (-sizespike)
#   SIZESPIKE=2   the same machinery, its A / B lap: each SIZE move as v1.0.48 ran its crossfade, then on the faster
#                 crossfade paths (prof::xfFast; the same output to the bit) (-sizespikeab)
SIZE_GROUPS ?=
CANDIDATE ?=
SIZESPIKE ?=
TRIAL_TAG = $(if $(SIZE_GROUPS),-g$(SIZE_GROUPS))
TRIAL_DEFS = $(if $(SIZE_GROUPS), -DIV_SIZE_GROUPS=$(SIZE_GROUPS))$(if $(CANDIDATE), -DIV_PROFILE_CANDIDATE=1)$(if $(SIZESPIKE), -DIV_PROFILE_SIZESPIKE=$(SIZESPIKE))
RELEASE_TAG = $(if $(filter 0,$(RELEASE_SDRAM_TIGHT)),-sdlibdaisy)$(if $(filter 1,$(RELEASE_DIAG)),-diag)$(if $(filter 1,$(RELEASE_LATE_LIGHT)),-latelight)$(TRIAL_TAG)
PROFILE_TAG = $(if $(filter 0,$(PROFILE_FZ)),-nofz)$(if $(filter 0,$(PROFILE_SDRAM_TIGHT)),-sdlibdaisy)$(if $(filter 0,$(PROFILE_ITCM)),-noitcm)$(TRIAL_TAG)$(if $(CANDIDATE),-candidate)$(if $(SIZESPIKE),-sizespike$(if $(filter 2,$(SIZESPIKE)),ab))

ifeq ($(MODE),m0test)
  TARGET    = sublustris_versio_m0test
  MODE_DEFS = -DIV_MODE_M0TEST
else ifeq ($(MODE),profile)
  TARGET    = sublustris_versio_profile$(PROFILE_TAG)
  MODE_DEFS = -DIV_MODE_PROFILE -DIV_PROFILE_HOOKS -DIV_PROFILE_FZ=$(PROFILE_FZ) \
              -DIV_PROFILE_SDRAM_TIGHT=$(PROFILE_SDRAM_TIGHT) -DIV_ITCM=$(PROFILE_ITCM)
  ifeq ($(PROFILE_ITCM),1)
    LDSCRIPT = STM32H750IB_flash_itcm.lds
  endif
else ifeq ($(MODE),release)
  TARGET    = sublustris_versio$(RELEASE_TAG)
  MODE_DEFS = -DIV_MODE_RELEASE -DIV_ITCM=1 -DIV_RELEASE_SDRAM_TIGHT=$(RELEASE_SDRAM_TIGHT) \
              -DIV_RELEASE_DIAG=$(RELEASE_DIAG) $(if $(filter 1,$(RELEASE_DIAG)),-DIV_PROFILE_HOOKS) \
              -DIV_LATE_LIGHT=$(RELEASE_LATE_LIGHT)
  LDSCRIPT  = STM32H750IB_flash_itcm.lds
else
  $(error Unknown MODE '$(MODE)': use m0test, profile, or release)
endif

# m0test is a plain passthrough and doesn't link the Core.
# no_uart_spi.cpp stubs out libDaisy's UART/SPI DMA bookkeeping so the linker
# never pulls in uart.o/spi.o and the HAL code behind them (~15 KB, none of
# it used by this app in any variant; see that file for why it's safe).
#
# Profile: the Core's objects link FIRST. Inline functions defined in headers (Swell::processBlock,
# TapeOut::process, RadioStage::process / set, Swell::control, ...) are emitted in every object that
# calls them, and the linker keeps the first copy it meets. The profile's micro-tests call them from
# main.o, which is -Os, so with main.o first the Engine ran main.o's -Os copies (Swell::processBlock
# 596 B against release's 1,658 B, TapeOut::process 248 against 1,766, RadioStage::process 1,286
# against 2,516): the profile measured slower code than release ships (found in the overnight
# review, 26 Oct 2026; round 10's tape and radio numbers had it). Release keeps its order (and so its
# layout and size); its main.o calls none of them.
ifeq ($(MODE),m0test)
  CPP_SOURCES = main.cpp no_uart_spi.cpp
else ifeq ($(MODE),profile)
  CPP_SOURCES = $(wildcard ../core/dsp/*.cpp) main.cpp no_uart_spi.cpp
else
  CPP_SOURCES = main.cpp no_uart_spi.cpp $(wildcard ../core/dsp/*.cpp)
endif
C_INCLUDES += -I../core
C_DEFS     += $(MODE_DEFS) -DIV_PROFILE_BLOCK=$(PROFILE_BLOCK)$(TRIAL_DEFS)
# Firmware build: core/dsp/SizeOpt.h's IV_SIZE_OPT (-Os on control-rate code)
# and the shipped-voicings-only switch key off this.
C_DEFS     += -DIV_FIXED_VOICINGS
# The Core never allocates on the module: delay memory is attached (SPEC §5.2).
C_DEFS     += -DIV_NO_HEAP
# No fused multiply-add (Resilio M3, 29 Sep 2026; its ADR 0030). On the M7, GCC's vfma accumulates
# into its destination, so every a*b+c becomes vmov/vfma/vmov and each one
# waits for the last: ~9 cycles per multiply-add, even for independent ones
# (Resilio's M3 bench). A separate vmul + vadd is three-operand and
# pipelines. Rounding differs from the desktop build in the last bit only.
C_DEFS     += -ffp-contract=off
CPP_STANDARD = -std=gnu++17
OPT = -O3

# Object files are isolated per (MODE, PROFILE_BLOCK): all three variants
# compile the same main.cpp under different -D flags, so sharing one object
# directory would let a stale main.o from a previous MODE get silently
# relinked (same filename, unrelated content) into the "wrong" variant. Each
# combination gets its own scratch dir under build/, and the final artifacts
# are copied up to build/<TARGET>.* (flat) once linked, so the three .bin
# files always sit side by side under distinct, unambiguous names.
override BUILD_DIR = build/obj-$(MODE)-$(PROFILE_BLOCK)$(if $(filter profile,$(MODE)),$(PROFILE_TAG))$(if $(filter release,$(MODE)),$(RELEASE_TAG))
# libDaisy's own $(BUILD_DIR) rule is a plain `mkdir $@`, which fails for a
# nested path when build/ doesn't exist yet; pre-create it here (mkdir -p).
$(shell mkdir -p $(BUILD_DIR))

LIBDAISY_DIR = ../libs/libDaisy
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile

# Size-optimise only our non-DSP, control-rate code (main.cpp: knob/switch/LED
# handling, boot pattern, serial logging in profile). DSP objects
# (core/dsp/*.cpp) keep the global
# -O3 above so release and profile always measure the identical,
# maximally-optimised per-sample code path (CPU numbers from profile stay
# valid for release). Saves flash without touching per-sample DSP speed.
$(BUILD_DIR)/main.o: OPT = -Os

# Internal flash limit for NE Firmware Swap installs (SPEC §5.3).
FLASH_LIMIT = 131072

# libDaisy's included Makefile defines `all` (elf+hex+bin in $(BUILD_DIR));
# this appends the copy-up step so `make` alone (default goal = all) leaves
# a flat build/<TARGET>.bin/.elf/.hex behind.
all: copy-artifacts

.PHONY: copy-artifacts
copy-artifacts: $(BUILD_DIR)/$(TARGET).bin $(BUILD_DIR)/$(TARGET).hex
	@mkdir -p build
	@cp $(BUILD_DIR)/$(TARGET).elf build/$(TARGET).elf
	@cp $(BUILD_DIR)/$(TARGET).hex build/$(TARGET).hex
	@cp $(BUILD_DIR)/$(TARGET).bin build/$(TARGET).bin

.PHONY: size
size: build/$(TARGET).bin
	@bytes=$$(wc -c < build/$(TARGET).bin | tr -d ' '); \
	pct=$$((bytes * 100 / $(FLASH_LIMIT))); \
	echo "$(TARGET).bin: $$bytes / $(FLASH_LIMIT) bytes ($$pct%)"; \
	if [ $$bytes -gt $(FLASH_LIMIT) ]; then echo "ERROR: exceeds internal flash (SPEC §5.3)"; exit 1; fi; \
	if [ $$pct -ge 95 ]; then echo "WARNING: $(TARGET).bin is at $$pct% of the internal-flash limit (SPEC §5.3) - getting close"; fi

# Builds all three variants (release, m0test, profile) and reports each
# size against the 128 KB limit. Fails (nonzero exit) if any variant is
# over, and warns (without failing) once a variant is at 95% or more.
.PHONY: all-variants
all-variants:
	@$(MAKE) --no-print-directory MODE=release
	@$(MAKE) --no-print-directory MODE=m0test
	@$(MAKE) --no-print-directory MODE=profile
	@echo ""; \
	echo "=== Variant sizes vs $(FLASH_LIMIT)-byte internal-flash limit (SPEC §5.3) ==="; \
	fail=0; warn=0; \
	for f in sublustris_versio sublustris_versio_m0test sublustris_versio_profile; do \
		bytes=$$(wc -c < build/$$f.bin | tr -d ' '); \
		pct=$$((bytes * 100 / $(FLASH_LIMIT))); \
		if [ $$bytes -gt $(FLASH_LIMIT) ]; then status="EXCEEDS LIMIT"; fail=1; \
		elif [ $$pct -ge 95 ]; then status="WARNING: >=95%"; warn=1; \
		else status="ok"; fi; \
		printf "  %-28s %7d / %d bytes (%3d%%)  %s\n" $$f.bin $$bytes $(FLASH_LIMIT) $$pct "$$status"; \
	done; \
	if [ $$warn -eq 1 ] && [ $$fail -eq 0 ]; then \
		echo ""; echo "WARNING: at least one variant is at 95% or more of the internal-flash limit (SPEC §5.3)."; \
	fi; \
	exit $$fail
