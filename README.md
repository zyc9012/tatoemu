# TatoEmu - Multi-System Emulator

TatoEmu is a multi-system emulator written in C++ with SDL3 and WebAssembly targets. It currently supports GB/GBC, NES, and CPS1 ROMs.

This project is just for fun. It aims to build minimal emulators that work for most games, with only basic features like saving and loading states. It prioritizes playability over accuracy.

## Try it Online

**Play in your browser**: [https://tatoemu.netlify.app](https://tatoemu.netlify.app)

## Requirements

- C++17 compatible compiler
- CMake 3.20 or higher
- SDL3 library

### Installing SDL3

#### macOS (using Homebrew)
```bash
brew install cmake sdl3
```

#### Linux (Ubuntu/Debian)
```bash
# You may need to build SDL3 from source
git clone https://github.com/libsdl-org/SDL.git -b main
cd SDL
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
sudo make install
```

#### Windows (MinGW-w64)
```bash
pacman -S mingw-w64-x86_64-cmake mingw-w64-x86_64-sdl3
```

## Building

### Native Build (SDL3)

```bash
mkdir build
cd build
cmake ..
make
```

### WebAssembly Build

Install and activate the Emscripten SDK

Quick start:
```bash
source /path/to/emsdk/emsdk_env.sh
mkdir build-wasm
cd build-wasm
emcmake cmake -DPLATFORM=wasm ..
emmake make

# Run (serves on http://localhost:8080)
python3 -m http.server 8080
```

## Usage

```bash
./tatoemu <path_to_rom> [options]
```

### Command-Line Options

- `--scale <n>` - Window scale factor (default: 2)
- `--scale-mode <mode>` - Scale mode: `linear` or `nearest` (default: linear)
- `--sample-rate <hz>` - Audio sample rate in Hz (default: 44100)
- `--volume <0.0-1.0>` - Audio volume level (default: 0.3)
- `--bootrom <path>` - Path to bootrom file (Game Boy only)

### Controls

#### Game Boy / NES
- **Arrow Keys** - D-Pad (Up/Down/Left/Right)
- **Z** - A Button
- **X** - B Button
- **Enter** - Start Button
- **Shift** - Select Button

#### CPS1 (Arcade)
- **Player 1:**
  - **Arrow Keys** - Movement (Up/Down/Left/Right)
  - **A/S/D** - Punch buttons (Light/Medium/Heavy)
  - **Z/X/C** - Kick buttons (Light/Medium/Heavy)
  - **1** - Start
  - **5** - Insert Coin
- **Player 2:**
  - **Keypad 8/5/4/6** - Movement (Up/Down/Left/Right)
  - **J/K/L** - Punch buttons (Light/Medium/Heavy)
  - **M/Comma/Period** - Kick buttons (Light/Medium/Heavy)
  - **2** - Start
  - **6** - Insert Coin

#### Common Controls
- **ESC** - Quit emulator
- **F5** - Quick save state
- **F9** - Quick load state
- **P** - Pause / Resume

## Supported Systems and Cartridge/Board Types

### Game Boy / Game Boy Color

- ROM only (0x00) - No banking, up to 32KB ROM
- ROM + RAM (0x08-0x09) - ROM with external RAM
- MBC1 (0x01) - Basic ROM banking, up to 2MB ROM
- MBC1 + RAM (0x02-0x03) - MBC1 with external RAM (8KB-32KB)
- MBC1M - MBC1 with multi-ROM support (enhanced banking)
- MBC2 (0x05) - 512x4 bits internal RAM
- MBC2 + Battery (0x06) - MBC2 with battery backup
- MBC3 (0x11-0x13) - ROM banking with optional RAM
- MBC3 + Timer + Battery (0x0F-0x10) - MBC3 with Real-Time Clock (RTC)
- MBC30 - MBC3 variant with larger ROM/RAM support
- MBC5 (0x19) - Up to 8MB ROM, 128KB RAM
- MBC5 + RAM (0x1A-0x1B) - MBC5 with external RAM
- MBC5 + Rumble (0x1C-0x1E) - MBC5 with rumble motor support
- MBC7 (0x22) - Accelerometer support with 256KB RAM

### Nintendo Entertainment System

- **Nintendo**: NROM (000), MMC1 (001), UxROM (002), CNROM (003), MMC3 (004), MMC5 (005), MMC2 (009), MMC4 (010), MMC3 variant with CHR RAM (074)
- **Namco**: Namco 163/129 (019)
- **Konami**: VRC2/VRC4 (023/025), VRC6 (024/026), VRC3 (073)
- **Sunsoft**: FME-7 (069)
- **Unlicensed**: Waixing (162/164/178), Nanjing (163)

### Capcom Play System 1 (CPS1)

Supports FBNeo ROM sets (partial).

ROM sets available at: [Myrient](https://myrient.erista.me/files/Internet%20Archive/chadmaster/fbnarcade-fullnonmerged/arcade/)

## Resources

- [Pan Docs](https://gbdev.io/pandocs/) - Comprehensive GameBoy technical documentation
- [GameBoy CPU Manual](http://marc.rawer.de/Gameboy/Docs/GBCPUman.pdf)
- [NanoBoyAdvance](https://github.com/nba-emu/NanoBoyAdvance)
- [NESDev Wiki](https://www.nesdev.org/wiki/Main_Page) - NES architecture, mappers, tests
- [Mesen2](https://github.com/SourMesen/Mesen2)
- [FBNeo](https://github.com/finalburnneo/FBNeo)
