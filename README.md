# Gravitas Engine

A work-in-progress C++20 Vulkan game engine with an ECS core, a two-stage frame graph renderer, and a separate retained UI tree. Four demo applications are included: Tetris, a dungeon crawler prototype, and two test scenes.

---

## Prerequisites

### Linux
- **CMake 3.20+**
- **Vulkan SDK** — install via package manager or from [vulkan.lunarg.com](https://vulkan.lunarg.com)
  - Arch: `sudo pacman -S vulkan-devel`
  - Ubuntu/Debian: `sudo apt install libvulkan-dev vulkan-tools`
- **GLFW** — fetched and built automatically by CMake via FetchContent. No manual install needed.
- **GLM** — bundled in `external/glm/`. No install needed.
- GCC 10+ or Clang 12+

### Windows
- **CMake 3.20+**
- **Vulkan SDK** — download from [vulkan.lunarg.com](https://vulkan.lunarg.com). Install to the default location so the `VULKAN_SDK` environment variable is set automatically.
- **Visual Studio 2022** with the "Desktop development with C++" workload, or MinGW-w64 (w64devkit / MSYS2)
- **GLFW** — fetched automatically by CMake. No install needed.
- **GLM** — bundled. No install needed.

---

## Build

### Linux
```bash
git clone <repo>
cd Gravitas
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel
```

### Windows — Visual Studio 2022
```bash
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

### Windows — MinGW
```bash
mkdir build && cd build
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel
```

---

## Running

All executables must be run from the **project root** so asset paths resolve correctly.

```bash
./build/applications/GravitasTetris/GravitasTetris
./build/applications/DungeonCrawler/DungeonCrawler
./build/applications/GtsScene1/GtsScene1
./build/applications/GtsScene2/GtsScene2
```

---

## Key Bindings

### All Applications
| Key | Action |
|-----|--------|
| Escape | Quit |
| F3 | Toggle debug stats overlay |
| P | Pause / Resume simulation |

### Dungeon Crawler
| Key | Action |
|-----|--------|
| W / S | Move forward / backward |
| A / D | Strafe left / right |
| Q / E | Turn left / right |
| Space | Attack (1 damage, 0.4 s cooldown) |
| T | Toggle debug bird's-eye camera |

### GtsScene2 — Frustum Culling Test
| Key | Action |
|-----|--------|
| W / S / A / D | Move camera |
| Q / E | Camera up / down |
| R / F | Camera roll / down |
| C | Toggle frustum culling on / off |
| F | Freeze / unfreeze cull frustum |

---

## Shader Compilation

Pre-compiled SPIR-V blobs are checked in to `shaders/` and loaded at runtime. You do **not** need to recompile them unless you modify the GLSL source files.

When `glslc` is available (installed with the Vulkan SDK), CMake registers a `compile_shaders` target:

```bash
cmake --build build --target compile_shaders
```

If `glslc` is not found, CMake warns and the checked-in SPIRVs are used as-is.

---

## Dependencies

| Dependency | How provided |
|------------|-------------|
| Vulkan SDK | System — `find_package(Vulkan REQUIRED)` |
| GLFW | CMake FetchContent (auto-downloaded at configure time) |
| GLM | Bundled — `external/glm/` |
| stb_image | Bundled — `external/stb/` |
| tinyobjloader | Bundled — `external/` |
