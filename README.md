# Gravitas Engine

A C++20 Vulkan engine with an ECS core, a two-stage frame graph renderer, and a retained UI graph. The repository now contains the reusable engine plus three bundled validation scenes: `GtsScene1`, `GtsScene2`, and `GtsScene3`.

---

## Prerequisites

### Linux
- **CMake 3.20+**
- **Vulkan SDK** — install via package manager or from [vulkan.lunarg.com](https://vulkan.lunarg.com)
  - Arch: `sudo pacman -S vulkan-devel`
  - Ubuntu/Debian: `sudo apt install libvulkan-dev vulkan-tools`
- **GLFW** — preferred from the system (`pkg-config` / CMake package). CMake falls back to `FetchContent` only when no local package is available.
- **GLM** — bundled in `external/glm/`. No install needed.
- GCC 10+ or Clang 12+

### Windows
- **CMake 3.20+**
- **Vulkan SDK** — download from [vulkan.lunarg.com](https://vulkan.lunarg.com). Install to the default location so the `VULKAN_SDK` environment variable is set automatically.
- **Visual Studio 2022** with the "Desktop development with C++" workload, or MinGW-w64 (w64devkit / MSYS2)
- **GLFW** — preferred from the system or an already-installed CMake package. CMake falls back to `FetchContent` when needed.
- **GLM** — bundled. No install needed.

---

## Build

### Linux
```bash
git clone https://github.com/nyantropy/Gravitas.git
cd Gravitas
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

### Windows — MinGW
```bash
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

### Capability development versus the complete runtime

The default build is the complete Vulkan-first Gravitas engine. `GravitasEngine`,
`gravitas_engine` and `gravitas_runtime` exist only when rendering, Vulkan, physics,
debug drawing and tools are enabled. Tool visibility can still be disabled at runtime;
that does not remove its compile-time dependency.

Reduced builds are useful for isolated capability development and CPU tests. They
build the requested leaf targets and their actual dependencies, but intentionally omit
the complete facade, bundled engine applications and full-runtime tests. For example:

```bash
cmake -S . -B build-physics -DGTS_ENABLE_RENDERING=OFF \
  -DGTS_ENABLE_VULKAN_BACKEND=OFF -DGTS_ENABLE_DEBUGDRAW=OFF \
  -DGTS_ENABLE_TOOLS=OFF -DGTS_ENABLE_PHYSICS=ON
cmake --build build-physics --target gravitas_physics PhysicsTransformIntegrationTest
ctest --test-dir build-physics -R physics_transform_integration --output-on-failure
```

See [runtime configuration](docs/modules/runtime-configuration.md) for the option/target
contract, other reduced combinations, and facade availability checks.

### Engine source subtree
```bash
cd engine
cmake -B build
cmake --build build --parallel
```

---

## Running

Shaders and engine-owned resources resolve relative to the engine root, so the engine works both standalone and when included via `add_subdirectory(engine)`.

```bash
./build/applications/GtsScene1/GtsScene1
./build/applications/GtsScene2/GtsScene2
./build/applications/GtsScene3/GtsScene3
```

---

## Key Bindings

### All Scenes
| Key | Action |
|-----|--------|
| Escape | Quit |
| F3 | Toggle debug stats overlay |
| P | Pause / Resume simulation |

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
| GLFW | System package when available, otherwise CMake FetchContent fallback |
| GLM | Bundled — `external/glm/` |
| stb_image | Bundled — `external/stb/` |
| tinyobjloader | Bundled — `external/` |
