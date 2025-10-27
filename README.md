# GameBoy Emulator

A GameBoy (DMG) emulator written in C++ with SDL3.

## Features

- Full Sharp LR35902 CPU emulation (modified Z80)
- Picture Processing Unit (PPU) with background, window, and sprite rendering
- Memory Management Unit (MMU) with cartridge banking support (MBC1)
- Joypad input handling
- Timer system
- Interrupt handling
- 60 FPS rendering

## Requirements

- C++17 compatible compiler
- CMake 3.20 or higher
- SDL3 library

### Installing SDL3

#### macOS (using Homebrew)
```bash
brew install sdl3
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

#### Windows
Download SDL3 development libraries from [SDL GitHub](https://github.com/libsdl-org/SDL) or use vcpkg:
```bash
vcpkg install sdl3
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

## Architecture

The emulator consists of several main components:

- **CPU**: Implements the Sharp LR35902 instruction set
- **MMU**: Manages memory mapping and access
- **PPU**: Handles graphics rendering (background, window, sprites)
- **Cartridge**: ROM loading and banking
- **Joypad**: Input handling
- **Timer**: System timing and timer interrupts
- **Emulator**: Main loop and SDL integration

## Compatibility

This emulator aims to run most commercial GameBoy games. Some games with advanced features or unusual timing requirements may not work perfectly.

## Known Limitations

- Audio is not implemented
- Only MBC1 cartridge type is fully supported
- Some edge cases in CPU timing may not be cycle-accurate
- Serial communication is not implemented

## License

This project is provided as-is for educational purposes.

## Resources

- [Pan Docs](https://gbdev.io/pandocs/) - Comprehensive GameBoy technical documentation
- [GameBoy CPU Manual](http://marc.rawer.de/Gameboy/Docs/GBCPUman.pdf)
- [GBEDG](https://hacktix.github.io/GBEDG/) - GameBoy Emulator Development Guide

