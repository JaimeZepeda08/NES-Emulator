CC = gcc
CFLAGS = -Wall -Wextra -O3 $(shell sdl2-config --cflags)
LDFLAGS = $(shell sdl2-config --libs) -lSDL2_ttf

SRC = src/main.c src/cpu.c src/memory.c src/ppu.c src/input.c src/display.c src/apu.c
OBJ = $(SRC:.c=.o)
OUT = nes-emulator

all: $(OUT)

$(OUT): $(OBJ)
	$(CC) $(CFLAGS) -o $(OUT) $(OBJ) $(LDFLAGS)

clean:
	rm -f $(OBJ) $(OUT)
