![logo](/tatoemu.png)

# TatoEmu - Multi-System Retro Game Emulator

TatoEmu is a multi-system retro game emulator written in C++ with SDL3 and WebAssembly targets. It currently supports GB/GBC, GBA, NES, Mega Drive/Genesis, CPS1, CPS2, and NeoGeo ROMs.

This project is just for fun. It aims to build minimal emulators that work for most games, with only basic features like saving and loading states. It prioritizes playability over accuracy.

**"Write the emulator before playing the game."** This is my motivation for the project.

## Try it Online

**Play in your browser**: [https://emu.tatoz.net](https://emu.tatoz.net)

## Requirements

- C++20 compatible compiler
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

Install and activate the Emscripten SDK. The web frontend also requires Node.js and npm.

Quick start:
```bash
source /path/to/emsdk/emsdk_env.sh
cd platforms/wasm/web
npm install
cd ../../..
emcmake cmake -S . -B build-wasm -DPLATFORM=wasm
cmake --build build-wasm -j4

# Run (serves on http://localhost:8080)
python3 -m http.server 8080 --directory build-wasm/dist
```

The build generates the Emscripten module in `build-wasm/emulator` and assembles the deployable Preact application in `build-wasm/dist`.

## Usage

```bash
./tatoemu <path_to_rom> [options]
```

On the first launch, a config file (`tatoemu.ini`) will be generated in the same directory as the executable.

The following options and control mappings can be customized within this config file.

### Command-Line Options

- `--scale <n>` - Window scale factor (default: 0, auto)
- `--scale-mode <mode>` - Scale mode: `linear` or `nearest` (default: linear)
- `--sample-rate <hz>` - Audio sample rate in Hz (default: 44100)
- `--volume <0-100>` - Audio volume level (default: 60)
- `--bootrom <path>` - Path to bootrom/BIOS file (Game Boy and GBA)
- `--neo-sys <system>` - NeoGeo system type: `aes` or `mvs` (default: mvs)
- `--neo-bios <index>` - NeoGeo BIOS index: 0 ~ 34 (default: 19 - Universe BIOS 4.0)

### Default Controls

#### Game Boy / NES
- **Arrow Keys** - D-Pad (Up/Down/Left/Right)
- **Z** - A Button
- **X** - B Button
- **Enter** - Start Button
- **Shift** - Select Button

#### Game Boy Advance
- **Arrow Keys** - D-Pad (Up/Down/Left/Right)
- **Z** - A Button
- **X** - B Button
- **A** - L Button
- **S** - R Button
- **Enter** - Start Button
- **Right Shift** - Select Button

#### Mega Drive / Genesis
- **Arrow Keys** - D-Pad (Up/Down/Left/Right)
- **Z** - A Button
- **X** - B Button
- **C** - C Button
- **A** - X Button
- **S** - Y Button
- **D** - Z Button
- **Enter** - Start Button
- **Right Shift** - Mode Button

#### CPS1 / CPS2
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
- **System**
  - **F2** - Diagnostic
  - **F3** - Service

#### NeoGeo
- **Player 1:**
  - **Arrow Keys** - Movement (Up/Down/Left/Right)
  - **A/S/D/F** - Buttons A/B/C/D
  - **1** - Start
  - **3** - Select
  - **5** - Insert Coin
- **Player 2:**
  - **Keypad 8/5/4/6** - Movement (Up/Down/Left/Right)
  - **J/K/L/Semicolon** - Buttons A/B/C/D
  - **2** - Start
  - **4** - Select
  - **6** - Insert Coin
- **System**
  - **F2** - Test
  - **F3** - Service

#### Common Controls
- **ESC** - Quit emulator
- **F5** - Quick save state
- **F9** - Quick load state
- **F10** - Quick load state (backup 1)
- **F11** - Quick load state (backup 2)
- **F12** - Quick load state (backup 3)
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

### Game Boy Advance

- SRAM - Static RAM saves (32KB)
- Flash 64K - Flash memory saves (64KB)
- Flash 128K - Flash memory saves (128KB)
- EEPROM 512 - EEPROM saves (512 bytes)
- EEPROM 8K - EEPROM saves (8KB)

### Nintendo Entertainment System

- **Nintendo**: NROM (000), MMC1 (001), UxROM (002), CNROM (003), MMC3 (004), MMC5 (005), MMC2 (009), MMC4 (010), MMC3 variant with CHR RAM (074)
- **Namco**: Namco 163/129 (019)
- **Konami**: VRC2/VRC4 (023/025), VRC6 (024/026), VRC3 (073)
- **Sunsoft**: FME-7 (069)
- **Unlicensed**: Waixing (162/164/178), Nanjing (163)

### Mega Drive / Genesis

- Plain ROM - Up to 4MB, no banking
- SRAM / FRAM - Battery-backed saves declared in the cartridge header
- SSF2 mapper - Eight 512KB banks via the registers at 0xA130F3-0xA130FF (for ROMs larger than 4MB)

### Capcom Play System 1/2

Supports FBNeo ROM sets (partial).

ROM sets available at: [Internet Archive](https://archive.org/download/fbnarcade-fullnonmerged/arcade/)

### NeoGeo (AES/MVS)

Supports FBNeo ROM sets (partial).

ROM sets available at: [Internet Archive](https://archive.org/download/fbnarcade-fullnonmerged/arcade/)

## Resources

- [Pan Docs](https://gbdev.io/pandocs/) - Comprehensive GameBoy technical documentation
- [GameBoy CPU Manual](http://marc.rawer.de/Gameboy/Docs/GBCPUman.pdf)
- [NanoBoyAdvance](https://github.com/nba-emu/NanoBoyAdvance)
- [NESDev Wiki](https://www.nesdev.org/wiki/Main_Page) - NES architecture, mappers, tests
- [Mesen2](https://github.com/SourMesen/Mesen2)
- [FBNeo](https://github.com/finalburnneo/FBNeo) - Reference implementation for CPS1/CPS2/NeoGeo
- [Sega Retro](https://segaretro.org/Sega_Mega_Drive/Technical_specifications) - Mega Drive hardware documentation
- [NeoGeo Development Wiki](https://wiki.neogeodev.org/) - NeoGeo hardware documentation
