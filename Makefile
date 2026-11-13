# nRF52840 SDK build
#
#   make              build build/sensor_app.hex (app linked against libnrf52sdk.a)
#   make -j           parallel build (recommended)
#   make lib          build the SDK as a static library: build/libnrf52sdk.a
#   make size         show memory usage
#   make flash        program the device with nrfjprog
#   make test         build and run host unit tests
#   make sim          run the app on this computer with simulated hardware
#                     options: SIM_SECONDS=30 SIM_REALTIME=1
#   make clean        remove build output
#   make help         show this help
#
# Optimizations:
#   - Parallel builds via make -j (safe; independent object rules)
#   - Optional LTO: make LTO=1  (smaller code, slower link)
#   - Optional ccache: export CCACHE=ccache  (or just have it on PATH)
#   - Accurate header deps for modular include/ layout

PREFIX  ?= arm-none-eabi-
CCACHE  ?= $(if $(shell which ccache 2>/dev/null),ccache,)
CC      := $(CCACHE) $(PREFIX)gcc
OBJCOPY := $(PREFIX)objcopy
SIZE    := $(PREFIX)size
# gcc-ar instead of ar so the archive also works with LTO objects (LTO=1)
AR      := $(PREFIX)gcc-ar

APP     ?= apps/sensor_app
TARGET  := build/$(notdir $(APP))

MCU     := -mcpu=cortex-m4 -mthumb -mfloat-abi=soft
DEFS    := -DNRF52_HAS_P1=1
INCLUDE := -Iinclude

# Size-oriented by default. LTO=1 enables link-time optimization.
LTO     ?= 0
OPT     := -Os
ifeq ($(LTO),1)
OPT     += -flto
endif

CFLAGS  := $(MCU) $(DEFS) $(INCLUDE) -std=gnu11 $(OPT) -g \
           -Wall -Wextra -ffunction-sections -fdata-sections -MMD -MP
LDFLAGS := $(MCU) -T platform/nrf52840.ld -Wl,--gc-sections \
           -specs=nano.specs -specs=nosys.specs \
           -Wl,-Map=$(TARGET).map
ifeq ($(LTO),1)
LDFLAGS += -flto
endif

# Robust source collection (works even if new files appear in these dirs)
SDK_SRCS := $(wildcard src/*.c) $(wildcard drivers/*.c) platform/startup_nrf52840.c
APP_SRCS := $(wildcard $(APP)/*.c)
SRCS     := $(SDK_SRCS) $(APP_SRCS)
OBJS     := $(patsubst %.c,build/obj/%.o,$(SRCS))
DEPS     := $(OBJS:.o=.d)

# SDK static library. Startup code is left out: nothing references the vector
# table, so the linker would not pull it from an archive.
LIB      := build/libnrf52sdk.a
LIB_SRCS := $(wildcard src/*.c) $(wildcard drivers/*.c)
LIB_OBJS := $(patsubst %.c,build/obj/%.o,$(LIB_SRCS))

# Linked directly, not through the library
STARTUP_OBJ := build/obj/platform/startup_nrf52840.o
APP_OBJS    := $(patsubst %.c,build/obj/%.o,$(APP_SRCS))

# All public headers (nested under include/<module>/)
HEADERS  := $(wildcard include/*/*.h)

.PHONY: all lib size flash clean test sim help

all: $(TARGET).hex

# The library goes after the objects: the linker pulls only the archive members
# that resolve symbols the app and startup code still need.
$(TARGET).elf: $(APP_OBJS) $(STARTUP_OBJ) $(LIB)
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) $(APP_OBJS) $(STARTUP_OBJ) -Lbuild -lnrf52sdk -o $@

$(TARGET).hex: $(TARGET).elf
	$(OBJCOPY) -O ihex $< $@

lib: $(LIB)

$(LIB): $(LIB_OBJS)
	@mkdir -p $(dir $@)
	rm -f $@
	$(AR) rcs $@ $^

# Pattern rule: each .c → build/obj/<same-path>.o
build/obj/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

size: $(TARGET).elf
	$(SIZE) $<

flash: $(TARGET).hex
	nrfjprog --program $< --chiperase --verify --reset

# Use $(MAKE) so -j propagates into the tests subdirectory
test:
	$(MAKE) -C tests test

# Host simulation
HOST_CC   ?= cc
SIM_BIN   := build/sim/$(notdir $(APP))
SIM_SRCS  := sim/sim_hal.c src/nrf52_sensor.c src/nrf52_log.c $(wildcard drivers/*.c) $(APP_SRCS)
SIM_CFLAGS := $(DEFS) $(INCLUDE) -Isrc -std=gnu11 -O1 -g -Wall -Wextra -Werror
SIM_SECONDS  ?= 10
SIM_REALTIME ?= 0

sim: $(SIM_BIN)
	SIM_SECONDS=$(SIM_SECONDS) SIM_REALTIME=$(SIM_REALTIME) ./$(SIM_BIN)

# Depend on all public headers so a header change rebuilds the sim binary
$(SIM_BIN): $(SIM_SRCS) $(HEADERS)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(SIM_CFLAGS) $(SIM_SRCS) -o $@

clean:
	rm -rf build
	$(MAKE) -C tests clean

help:
	@echo "Targets:"
	@echo "  make / make all     build firmware hex"
	@echo "  make -j             parallel build (recommended)"
	@echo "  make LTO=1          enable link-time optimization"
	@echo "  make lib            build SDK static library (build/libnrf52sdk.a)"
	@echo "  make size           show flash/RAM usage"
	@echo "  make flash          program board (needs nrfjprog)"
	@echo "  make test           host unit tests"
	@echo "  make sim            host simulation"
	@echo "  make clean          remove build artifacts"
	@echo ""
	@echo "Tips:"
	@echo "  export CCACHE=ccache   # if ccache is installed"
	@echo "  make -j$$(nproc)       # use all cores"

-include $(DEPS)
