# Repository Guidelines

## Project Structure & Module Organization
`engine/` contains the reusable C++20 engine code. Core systems live under `engine/core/` and higher-level features such as camera, rendering, physics, and transform live under `engine/modules/`. `applications/` contains the runnable demos: `GravitasTetris`, `DungeonCrawler`, `GtsScene1`, and `GtsScene2`, each with its own `CMakeLists.txt`, `main.cpp`, scene code, and app-specific components/systems. Runtime assets are stored in `resources/` and `shaders/`. Third-party code is vendored under `external/`; avoid editing it unless you are updating a dependency.

## Build, Test, and Development Commands
Configure from the repository root:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
cmake --build build --target compile_shaders
```

Use the first command to generate build files, the second to compile all engine and app targets, and the third only after changing GLSL files in `shaders/`. Run executables from the project root so relative asset paths resolve:

```bash
./build/applications/DungeonCrawler/DungeonCrawler
./build/applications/GravitasTetris/GravitasTetris
```

## Coding Style & Naming Conventions
Follow the existing C++ style: 4-space indentation, braces on the next line for functions, and aligned assignments only when it improves readability. Types and classes use PascalCase (`DungeonFloorScene`), free functions and methods use camelCase (`registerScene`), and constants/macros use all caps only when already established. Prefer `.hpp` for C++ headers and keep related app code grouped in folders such as `components/`, `systems/`, `scene/`, and `helpers/`.

## Testing Guidelines
There is no first-party automated test suite yet. Treat a clean `cmake --build build --parallel` as the baseline check, then manually run the affected application and verify the relevant gameplay, rendering, and input paths. If you add automated tests later, keep them outside `external/` and wire them into CMake.

## Commit & Pull Request Guidelines
Recent commits use short, lowercase, descriptive summaries such as `camera descriptor set fix` and `dungeon crawler prototyping`. Keep commits focused and written in that style. Pull requests should state what changed, which application(s) were exercised, and include screenshots or short videos for rendering/UI work. Link related issues when applicable and mention any Vulkan SDK or shader prerequisites for reviewers.
