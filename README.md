# GameBoy Emulator

A GameBoy emulator written in C++ with support for SDL3 and WebAssembly.

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
cd web && python3 -m http.server 8080
```

## Usage

```bash
./gb-emu <path_to_rom.gb> [bootrom_file]
```

### Controls

- **Arrow Keys** - D-Pad (Up/Down/Left/Right)
- **Z** - A Button
- **X** - B Button
- **Enter** - Start Button
- **Shift** - Select Button
- **ESC** - Quit emulator
- **F5** - Quick save
- **F9** - Quick load
- **P** - Pause / Resume

## Supported Cartridge Types

- **ROM only** (Type 0x00) - No banking, up to 32KB ROM
- **ROM + RAM** (Type 0x08-0x09) - ROM with external RAM
- **MBC1** (Type 0x01) - Basic ROM banking, up to 2MB ROM
- **MBC1 + RAM** (Type 0x02-0x03) - MBC1 with external RAM (8KB-32KB)
- **MBC1M** - MBC1 with multi-ROM support (enhanced banking)
- **MBC2** (Type 0x05) - 512x4 bits internal RAM
- **MBC2 + Battery** (Type 0x06) - MBC2 with battery backup
- **MBC3** (Type 0x11-0x13) - ROM banking with optional RAM
- **MBC3 + Timer + Battery** (Type 0x0F-0x10) - MBC3 with Real-Time Clock (RTC)
- **MBC30** - MBC3 variant with larger ROM/RAM support
- **MBC5** (Type 0x19) - Up to 8MB ROM, 128KB RAM
- **MBC5 + RAM** (Type 0x1A-0x1B) - MBC5 with external RAM
- **MBC5 + Rumble** (Type 0x1C-0x1E) - MBC5 with rumble motor support
- **MBC7** (Type 0x22) - Accelerometer support with 256KB RAM

## Resources

- [Pan Docs](https://gbdev.io/pandocs/) - Comprehensive GameBoy technical documentation
- [GameBoy CPU Manual](http://marc.rawer.de/Gameboy/Docs/GBCPUman.pdf)

