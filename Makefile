CC = gcc
CFLAGS = -Wall -Wextra -O3 $(shell sdl2-config --cflags)
LDFLAGS = $(shell sdl2-config --libs) -lSDL2_ttf

SRC = src/main.c src/cpu.c src/ppu.c src/input.c src/display.c src/apu.c src/nes.c src/cartridge.c src/mapper.c $(wildcard src/mappers/*.c)
BUILD_DIR = build
OBJ = $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(SRC))
OUT = nes-emulator

all: $(OUT)

$(OUT): $(OBJ)
	$(CC) $(CFLAGS) -o $(OUT) $(OBJ) $(LDFLAGS)

$(BUILD_DIR)/%.o: src/%.c
	@mkdir -p $(BUILD_DIR)
	@mkdir -p $(BUILD_DIR)/mappers
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(OUT)
	rm -rf $(BUILD_DIR)