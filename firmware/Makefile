# Firmware libre Radtel RT-1000 Pro
#   make            -> build/rt1000.bin y build/rt1000.hex
#   make flash PORT=/dev/ttyUSB0
#   make test       -> pruebas en PC de los modulos independientes del hardware
CROSS   ?= arm-none-eabi-
CC      := $(CROSS)gcc
OBJCOPY := $(CROSS)objcopy
SIZE    := $(CROSS)size
BUILD   := build
SRCS    := $(wildcard src/*.c)
OBJS    := $(SRCS:src/%.c=$(BUILD)/%.o)

CFLAGS  := -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16 \
           -Os -g -std=gnu11 -Wall -Wextra -Wno-unused-parameter \
           -ffunction-sections -fdata-sections -fno-common -Iinclude
LDFLAGS := -T rt1000.ld -nostartfiles -Wl,--gc-sections -Wl,-Map=$(BUILD)/rt1000.map \
           --specs=nano.specs --specs=nosys.specs

all: $(BUILD)/rt1000.bin $(BUILD)/rt1000.hex

$(BUILD)/%.o: src/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/rt1000.elf: $(OBJS) rt1000.ld
	$(CC) $(CFLAGS) $(OBJS) $(LDFLAGS) -o $@
	$(SIZE) $@

$(BUILD)/rt1000.bin: $(BUILD)/rt1000.elf
	$(OBJCOPY) -O binary $< $@

$(BUILD)/rt1000.hex: $(BUILD)/rt1000.elf
	$(OBJCOPY) -O ihex $< $@

$(BUILD):
	mkdir -p $(BUILD)

flash: $(BUILD)/rt1000.bin
	python3 ../tools/flash.py -p $(PORT) $<

test:
	mkdir -p $(BUILD)
	gcc -Wall -Iinclude -o $(BUILD)/test_host test/test_host.c src/afsk.c src/nmea.c src/tones.c
	$(BUILD)/test_host

clean:
	rm -rf $(BUILD)

.PHONY: all flash test clean
