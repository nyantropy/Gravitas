# Rendering Benchmark Baselines

Store committed or CI-managed rendering benchmark baselines here only when they
are tied to a known runner class.

Baseline identity must include:

- benchmark preset name and version
- build configuration
- render resolution and graphics settings
- runner or hardware class
- CPU, GPU, driver, and Vulkan metadata

Do not maintain one universal baseline for developer machines. Use local
baselines for comparison experiments and fixed-hardware CI baselines for
regression gates.
