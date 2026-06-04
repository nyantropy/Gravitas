# Coding Style

This document records the repository style contract for the Gravitas engine. It should stay practical and exact. When code cleanup establishes a stronger convention, update this file in the same change, but cleanup work should NEVER fully replace the coding style established in this document.

## Formatting

- Use `.clang-format` for C++ formatting.
- Use 4 spaces for indentation. Do not use tabs.
- Use Allman braces for C++ types, functions, namespaces, and control blocks.
- Keep lines reasonably short; prefer readable wrapping over dense expressions.
- Keep related declarations aligned only when it improves scanning. Do not create fragile alignment across unrelated blocks.
- Include a trailing newline in every text file.

## KEEP IT STUPID SIMPLE
- do not throw code into functions if the code only runs once, as soon as it is used twice, it needs to be put into a function.
- algorithms should never be overengineered.
- multiple layers of indirection are not needed.

## Design Patterns
- use an interface wherever possible to hide implementation, but do not use it for small tasks or operations.


## Files

- Use `.hpp` for a combined codefile - try to avoid splitting code into .cpp and .hpp files.
- Use `#pragma once` for headers.
- Keep one primary type or tightly related type group per file.
- Put code in the module that owns the behavior. Avoid adding catch-all helper files.
- Do not add compatibility wrappers for APIs that are unused inside the engine.
- Files related to an implemented feature should always be grouped together, the ideal goal is having each feature abstracted in its own folder.

## Naming

- Use `PascalCase` for types.
- Use `camelCase` for functions, methods, variables, and data members.
- Use `UPPER_SNAKE_CASE` for compile-time constants only when the surrounding code already uses that convention.
- Use names that describe engine concepts directly. Avoid vague names such as `Manager2`, `Helper`, or `Data` unless the surrounding module already gives the name precise meaning.
- ECS component types should end in `Component`.
- ECS systems should end in `System`.

## Includes

- Include what a file uses directly.
- Prefer forward declarations in headers when a pointer or reference is enough.
- Keep include ordering simple: standard library, external libraries, then engine headers.
- Avoid hidden dependencies through transitive includes.

## Ownership

- Express ownership explicitly.
- Use values for small local state.
- Use references for required non-owning dependencies with a guaranteed lifetime.
- Use raw pointers for optional non-owning dependencies.
- Use `std::unique_ptr` for exclusive ownership.
- Avoid shared ownership unless the lifetime is genuinely shared and cannot be represented more simply.
- Do not cache pointers from per-frame contexts beyond the current call.

## ECS

- Simulation systems are fixed-timestep and deterministic with respect to simulation time - these are to be used for actual game systems.
- Controller systems run once per rendered frame and may read frame time, input, UI, graphics, and tooling state - these are to be used for important engine side systems.
- Use command buffers for structural ECS changes while systems are iterating.
- Descriptor components represent authored or high-level intent.
- GPU/runtime components represent renderer-owned bindings, slots, handles, or cached runtime state.
- Keep descriptor-to-GPU synchronization explicit. Do not hide resource allocation inside unrelated gameplay systems.
- Mark transform-dependent renderer state dirty through the established transform dirty path.

## Rendering And Vulkan

- Keep Vulkan lifetime ownership local to rendering/resource modules.
- Do not expose Vulkan handles through gameplay-facing APIs unless a rendering module explicitly owns that contract.
- Resource creation, binding, upload, and destruction should stay in the renderer/resource layer.
- Prefer stable IDs and renderer-owned runtime components over gameplay code storing backend handles.
- Keep frame-graph pass declarations accurate. If a pass reads, writes, or changes layout for a resource, declare it there.
- Avoid per-frame allocation in hot render paths unless the allocation is bounded and intentionally cached.

## Error Handling

- Use clear early returns for invalid optional state.
- Use assertions for programmer errors and impossible states.
- Use explicit failure paths for runtime failures that can happen on user machines.
- Do not silently swallow resource creation, file IO, or Vulkan failures.

## Comments

- DO NOT EVER REMOVE COMMENTS THAT I WROTE MYSELF
- Prefer not adding new comments unless they are explicitly requested or needed to avoid misunderstanding non-obvious code.
- Comments should explain intent, invariants, lifetime rules, or synchronization only when the code cannot make that clear by itself.
- Keep comments short and plain, like a programmer leaving a note for the next pass. Do not write polished documentation prose in implementation files.
- Do not comment code that is already self-explanatory.
- Remove stale comments when changing behavior.
- Prefer updating `ARCHITECTURE.md` over adding broad architecture explanations inside implementation files.

## Cleanup Policy

- Remove dead code after confirming there are no first-party references.
- Prefer deleting obsolete abstractions over layering new code around them.
- Keep behavioral changes small and verifiable.
- When a cleanup changes engine behavior, document the behavior change and run a build.
- Do not reformat unrelated files during behavioral changes.
