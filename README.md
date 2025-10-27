# GameBoy Emulator

A GameBoy emulator written in C++ with SDL3.

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

```bash
mkdir build
cd build
cmake ..
make
```

## Usage

```bash
./gb-emu <path_to_rom.gb>
```

### Controls

- **Arrow Keys** - D-Pad (Up/Down/Left/Right)
- **Z** - A Button
- **X** - B Button
- **Enter** - Start Button
- **Shift** - Select Button
- **ESC** - Quit emulator

## Supported Cartridge Types

- ROM only (Type 0x00)
- MBC1 (Type 0x01-0x03)

## Resources

- [Pan Docs](https://gbdev.io/pandocs/) - Comprehensive GameBoy technical documentation
- [GameBoy CPU Manual](http://marc.rawer.de/Gameboy/Docs/GBCPUman.pdf)

