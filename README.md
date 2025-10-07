# Gravitas: an experimental Vulkan Engine

Gravitas is a work-in-progress C++ game and graphics engine built on top of the Vulkan API.  
The project is still in an early stage and features a lot of experimental and exploratory code.

Despite that, **basic 3D rendering is already supported**, including:
- Creating and displaying textured 3D objects
- Applying transformations: translation, rotation, and scaling
- Rendering multiple entities within a scene

## 🧭 Project Goals
Gravitas is primarily a **learning and experimentation platform** — a space to explore engine architecture, Vulkan abstractions, and modern C++ patterns.  
The long-term aim is to evolve into a modular engine that supports:
- Clean Vulkan object management via a unified context
- Event-driven architecture for real-time updates
- Extensible rendering and simulation modules

---

## 🧩 Planned Extensions
- **Entity Component System (ECS)** – for modular scene organization
- **Physics Module** – for real-time object simulation and collisions
- **Improved Shader Management** – automated SPIR-V compilation and pipeline reflection
- **Enhanced Scenegraph** – with hierarchical object transforms
- **Event System Expansion** – input, frame, and scene update events

---

## How to run

### LINUX
1. Clone repository
    ```bash
    git clone git@github.com:nyantropy/Gravitas.git
    cd Gravitas

2. Build Engine + Test Applications
    ```bash
    ./build.sh
