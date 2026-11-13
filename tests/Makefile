# Host unit tests. Uses the native C compiler, not the ARM toolchain.
#
#   make test                  build and run the tests
#   make test SANITIZE=1       also enable AddressSanitizer and UBSan
#   make -j test               parallel (safe)
#   make clean                 remove test output

CC       ?= cc
SANITIZE ?= 0

CFLAGS  := -std=gnu11 -Os -Wall -Wextra -Werror \
           -ffunction-sections -fdata-sections -I../include -I../src
# macOS ld64 spells dead-code stripping differently from GNU ld.
ifeq ($(shell uname -s),Darwin)
GC_SECTIONS := -Wl,-dead_strip
else
GC_SECTIONS := -Wl,--gc-sections
endif

LDFLAGS := $(GC_SECTIONS) -s

ifeq ($(SANITIZE),1)
CFLAGS  += -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer
LDFLAGS := $(GC_SECTIONS)
endif

SRCS := ../src/nrf52_sensor.c ../src/nrf52_log.c ../drivers/nrf52_i2c_reg_sensor.c \
        mocks.c test_main.c test_driver.c test_sensor.c test_log.c
BIN  := build/test_sdk

.PHONY: test clean

test: $(BIN)
	./$(BIN)

$(BIN): $(SRCS) test_support.h test_harness.h ../src/nrf52_log_port.h
	@mkdir -p build
	$(CC) $(CFLAGS) $(SRCS) $(LDFLAGS) -o $@

clean:
	rm -rf build
