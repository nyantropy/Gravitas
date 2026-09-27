# Gravitas Engine

A C++20 Vulkan engine with an ECS core, a two-stage frame graph renderer, and a retained UI graph. Bundled applications include `GtsScene1`, `GtsScene2`, `GtsScene3`, `GtsPbrValidation`, `GtsRenderingBenchmarks` and the independent CPU harness `GtsPerformanceBenchmarks`.

---

## Prerequisites

### Linux
- **CMake 3.20+**
- **Vulkan SDK** — install via package manager or from [vulkan.lunarg.com](https://vulkan.lunarg.com)
  - Arch: `sudo pacman -S vulkan-devel`
  - Ubuntu/Debian: `sudo apt install libvulkan-dev vulkan-tools`
- **GLFW** — preferred from the system (`pkg-config` / CMake package). CMake falls back to `FetchContent` only when no local package is available.
- **GLM** — bundled in `external/glm/`. No install needed.
- A compiler and standard library with C++20 support

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
git submodule update --init --recursive
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

### Engine submodule inside DungeonCrawler

Commands in engine docs use the engine repository root unless explicitly labeled
as game commands. In a DungeonCrawler checkout, configure the standalone engine
from the game root with:

```sh
cmake -S engine -B engine/build_release -DCMAKE_BUILD_TYPE=Release
cmake --build engine/build_release --parallel
ctest --test-dir engine/build_release --output-on-failure
```

The game build defaults `GTS_BUILD_TEST_SCENES=OFF` and registers only game tests.
A standalone engine build defaults validation scenes and engine CTest registration
on. GPU tests may report skipped when a compatible Vulkan device is unavailable;
a headless renderer still requires Vulkan. Module smoke tests configure nested
reduced builds; use CTest filters for focused checks during iteration.

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
| X | Pause / Resume simulation |
| F6 | Toggle tooling (when enabled) |
| F12 | Screenshot |

Application scenes may add or override actions. The default camera and tool bindings
live in `engine/modules/rendering/ecssetup/camera/input/CameraDefaultBindings.hpp`
and `engine/modules/tools/input/ToolDefaultBindings.hpp`. Game keybindings and saved
preferences are owned by the consuming application.

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

## Documentation

Start with [ARCHITECTURE.md](ARCHITECTURE.md) for ownership and runtime contracts,
then [docs/README.md](docs/README.md) for feature guides. Build options and reduced
configurations are specified in [runtime configuration](docs/modules/runtime-configuration.md).
CPU scaling and rendering measurements have separate guides:
[CPU benchmarks](docs/performance-benchmarks.md),
[rendering benchmarks](docs/rendering/benchmarks.md).
