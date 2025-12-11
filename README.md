# NES Emulator

A cycle-accurate Nintendo Entertainment System (NES) emulator written in C with SDL2. This emulator simulates the NES hardware at the clock cycle level, providing accurate timing synchronization between all components.

## Features

### Core Emulation
- **Cycle-Accurate CPU (6502)**: Full implementation of all 256 opcodes, including unofficial instructions
- **Cycle-Accurate PPU**: Picture Processing Unit running at 3x CPU speed, generating 256×240 pixel output
- **APU Support**: Audio Processing Unit for sound synthesis with pulse, triangle, and noise channels.
- **Accurate Timing**: Proper synchronization between CPU and PPU (~29,830 CPU cycles per frame at 60 FPS)

### Mapper Support
- **Mapper 0 (NROM)**: Basic cartridge without bank switching
- **Mapper 1 (MMC1)**: With bank switching support
- **Mapper 2 (UxROM)**: Switchable PRG ROM banks
- **Mapper 4 (MMC3)**: Advanced mapper with IRQ support

### Input
- Full controller support for two NES controllers
- Keyboard controls

### Save Support
- Simulation of battery-backed RAM persistence for games like "The Legend of Zelda"

## Building and Running

### Prerequisites

Before building, you need to install SDL2 and SDL2_ttf:

**macOS** (using Homebrew):
```bash
brew install sdl2 sdl2_ttf
```

**Ubuntu**:
```bash
sudo apt-get install libsdl2-dev libsdl2-ttf-dev
```

**Windows**:
- Download SDL2 development libraries from [libsdl.org](https://github.com/libsdl-org/SDL/releases/tag/release-3.2.28)
- Download SDL2_ttf from [libsdl.org/projects/SDL_ttf](https://github.com/libsdl-org/SDL_ttf)
- Follow the SDL2 installation instructions for your compiler

### Building

Clone the repository and build:
```bash
git clone https://github.com/JaimeZepeda08/NES-Emulator.git
cd NES-Emulator
make
```

### Running

Basic usage:
```bash
./nes-emulator <rom.nes>
```

Example:
```bash
./nes-emulator "roms/Super Mario Bros.nes"
```

### Controls

**In-Game**:
- Arrow Keys: D-pad
- Z: A button
- X: B button
- Enter: Start
- Right Shift: Select
- Q: Quit emulator