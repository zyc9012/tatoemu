# AGENTS.md

Guidance for AI coding agents working in this repository.

TatoEmu is a multi-system retro emulator written in C++20. It builds as a native
SDL3 application and as a WebAssembly module driven by a Preact front-end.
`README.md` covers user-facing documentation (controls, CLI options, supported
cartridge types); this file covers what you need to change the code.

## Build and verify

Both build trees are already configured. Prefer incremental builds.

```sh
# Native — produces build/tatoemu
cmake --build build -j8

# WebAssembly — produces build-wasm/emulator/tatoemu.js and build-wasm/dist/
cmake --build build-wasm -j8

# Web front-end type check only (much faster than a full wasm build)
cd platforms/wasm/web && npx tsc --noEmit
```

The emsdk lives outside the workspace, so the WASM
build needs unsandboxed execution.

Configuring from scratch, if a build tree is missing:

```sh
cmake -S . -B build -DPLATFORM=native -DCMAKE_BUILD_TYPE=Release
emcmake cmake -S . -B build-wasm -DPLATFORM=wasm -DCMAKE_BUILD_TYPE=Release
```

The WASM target runs `npm run build` in `platforms/wasm/web` as part of the
CMake build and copies the result into `build-wasm/dist/`.

Requirements: CMake 3.20+, a C++20 compiler, SDL3 (`find_package(SDL3 CONFIG)`),
and Emscripten + npm for the web target.

### Testing

There is no test suite, no CTest setup, and no test target. Verification is
manual and ROM-driven:

1. Native: `./build/tatoemu <rom>` and watch the game.
2. Browser: serve the built front-end and drive it with a real ROM, e.g.
   `python3 -m http.server 8232 --directory build-wasm/dist`.
3. Compare against the reference emulators in `ref/` when behaviour is in doubt.

Always regression-check a couple of unrelated games on the same core after a
timing or interrupt change — these are easy to break silently.

## Layout

| Path | Contents |
| --- | --- |
| `emulators/` | All emulation code. One directory and one namespace per system. |
| `emulators/components/` | Shared CPU cores (`m68k`, `z80`, `sm83`, `arm7tdmi`, `m6502`), sound cores (`ym2151`, `fm`, `qsound`, `msm6295`, `ay8910`, `sn76496`), EEPROM/Flash, save-state `Buffer`. |
| `platforms/native/` | SDL3 entry point, `tatoemu.ini` parsing, CLI options. |
| `platforms/wasm/` | Emscripten entry point (`main.cpp`) and the Preact web app under `web/`. |
| `platforms/CMakeLists.common.txt` | Shared source list and `apply_common_target_settings()`. |
| `utilities/` | `zip_reader` (miniz wrapper), `inih`. |
| `vendor/` | Vendored SDL source. Untracked. |
| `ref/` | Read-only upstream emulator sources for cross-checking. Untracked. |
| `roms/` | Local test ROMs. Untracked. |

Cores: `gb`, `gba`, `nes`, `md`, `cps` (CPS1 + CPS2), `neogeo`.

Each core follows the same file layout: `core.{h,cpp}` implementing the abstract
interface, then `cpu`, `memory`, `cartridge`, and system-specific parts
(`ppu`/`video`, `apu`/`audio`, `mmu`, `timer`, `joypad`/`controller`,
`sound_cpu`), plus `config.h` and `consts.h`.

`ref/` is a reference, not a dependency. Read it to confirm hardware behaviour,
but never copy code from it and never add it to the build.

## Architecture

**Core interface.** Every system implements `::Core` from `emulators/core.h`:
`initialize`, `setVideoDevice`, `setAudioDevice`, `loadROM`, `handleInput`,
`update` (runs exactly one frame), `updateGameSpeed`, `setAudioSampleRate`,
`setAudioVolume`, `getTargetFPS`, `getScreenWidth`, `getScreenHeight`,
`saveState`, `loadState`, `getGameTitle`. Optional overrides:
`loadBootrom`, `getDisplayAspectRatio` (CRT systems override this),
`getCheatMemory`.

**ROM detection.** `Emulator::determineCoreType()` in `emulators/emulator.cpp`
dispatches on file extension, then on ROM headers, then on the CPS and NeoGeo
game databases, and recurses into `.zip` archives.

**Save states.** Components serialize themselves through
`buffer_write(buf, &field, sizeof(field))` / `buffer_read(...)`, called
recursively from `Core::saveState`/`loadState`. If you add persistent state to a
component, add it to both functions in the same order.

**Cheats.** Each core nests an `ICheatMemory` adapter struct in its `Core` class
exposing `peek8/16/32`, `poke8/16/32`, and `getSearchRegions()` (RAM only, never
ROM or MMIO).

**Configuration.** Per-core `config.h` files declare `inline` globals inside
`namespace <core>::Config`, with key bindings under a nested `Key` namespace.
`emulators/config.h` holds the shared window/audio/hotkey settings. Both
platforms write into these globals: `platforms/native/configfile.cpp` parses
`tatoemu.ini`, and `platforms/wasm/main.cpp` exposes `setKeyBinding()` and
per-core setters to JavaScript.

### Adding a setting to the web app

1. Add the `extern "C"` setter in `platforms/wasm/main.cpp`.
2. Add its symbol to `EXPORTED_FUNCTIONS` in `platforms/wasm/CMakeLists.txt`
   (with a leading underscore).
3. Extend `EmulatorConfig` / `CONFIG_DEFAULTS` or `KEY_BINDING_SCHEMAS` in
   `platforms/wasm/web/src/configuration.ts`.
4. Call it from `applySettings()` in
   `platforms/wasm/web/src/emulator/runtime.ts`.
5. Add UI in `platforms/wasm/web/src/components/ConfigDialog.tsx`, and a touch
   layout in `VirtualControls.tsx` if the core needs one.

Key-binding sections come from `KEY_BINDING_SCHEMAS` automatically — the section
name must match the string the C++ `setKeyBinding()` switch expects.

## Style

- C++20. `#pragma once`, never include guards.
- 4-space indent, opening brace on the same line, roughly 100-column lines.
- `PascalCase` types, `camelCase` functions, `m_` prefix on member variables,
  lowercase namespaces.
- Use the `u8`/`u16`/`u32`/`u64`/`s8`/`s16`/`s32` aliases from
  `emulators/types.h`, not raw `int`/`unsigned`.
- Log with `log_info` / `log_error` from `emulators/types.h`, not `printf`.
- Comments explain *why* — typically which hardware behaviour or which game
  motivated the code. Skip comments that restate the code.
- There is no `.clang-format` or `.editorconfig`; match the surrounding file.
- Builds are warning-clean under `-Wall -Wextra -Wpedantic`. Keep them that way.

Shared code under `emulators/components/` is used by several cores. When you
change a CPU or sound core, check every core that includes it — for example the
68000 is shared by `md`, `cps`, and `neogeo`, and global CPU state such as
Musashi callbacks must be reset per core rather than left to leak.

## Git

Commit subjects are prefixed with the affected area in square brackets and
written in the imperative mood:

```
[MD] Emulate VDP interrupt acknowledge
[Web] Implement virtual controls for touch device
[CPS1] Fix compiler warnings
```

Prefixes in use: `[GB]`, `[GBA]`, `[NES]`, `[MD]`, `[CPS]`, `[CPS1]`, `[CPS2]`,
`[NeoGeo]`, `[Web]`. Cross-cutting changes (README, tooling) go without a
prefix. Use a body to explain the hardware behaviour and the games affected when
the fix is not self-evident.

`build/`, `build-wasm/`, `node_modules/`, saves and states are gitignored.
`ref/`, `roms/` and `vendor/` are untracked — do not add them.
Never commit ROM files.
