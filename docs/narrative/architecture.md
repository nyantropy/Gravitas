# Narrative Architecture

`engine/modules/narrative/` owns the engine's existing dialogue and visual-novel
capabilities. It is a feature boundary, not a shared interpreter or a generic
story-state service. Dialogue and VN retain separate runtimes and responsibilities.

## Compartments And Targets

```text
narrative/
  CMakeLists.txt
  dialogue/                  gravitas_dialogue (INTERFACE)
    CMakeLists.txt
    runtime/                 graphs, conditions/actions, choices and progression
    components/              runtime, graph registry, start request, NPC association
    events/                  semantic dialogue events
    systems/                 DialogueSystem and DialogueContextHelpers
  visualnovel/               gravitas_visualnovel (INTERFACE)
    CMakeLists.txt
    runtime/                 scripts, commands, stage, timing and typewriter
    components/              playback, external presentation, interaction/frontend state
    systems/                 VNSystem and dialogue-to-VN adaptation
    ui/                      profiles, dialogue UI and retained composition
```

The parent registers both compartments. `gravitas_dialogue` links only
`gravitas_core` and is always available, including builds with rendering disabled.
`gravitas_visualnovel` is registered when `GTS_ENABLE_RENDERING` is enabled; it
links `gravitas_dialogue` and `gravitas_rendering` for the existing UI/rendering
contracts. Neither compartment links the `gravitas_modules` aggregate. That
aggregate exports dialogue and, when enabled, VN for existing engine consumers.

Target arrows below mean **depends on**:

```text
gravitas_visualnovel --> gravitas_dialogue --> gravitas_core
                   \--> gravitas_rendering
```

The semantic flow remains headless dialogue → VN stage/interaction → UI.
The generic UI service is consumed by VN; it does not depend on VN or dialogue.
No Vulkan types or backend-specific narrative target are introduced.

## Headless Dialogue Boundary

Callers author `DialogueGraph`, nodes, choices, conditions and actions through
`DialogueTypes.h`, register handlers through `DialogueRegistry.h`, and drive
`DialogueRuntime` through start, advance, choice selection and end. The runtime
owns its active graph and condition/action registries. It filters choices and
executes semantic handlers without requiring a scene, UI, or renderer.

ECS callers use `DialogueRuntimeComponent.h`, `DialogueEvents.h`, and
`DialogueSystem.hpp`. The world owns the runtime, graph registry and start-request
components. `DialogueSystem` consumes start requests and publishes semantic state
changes; it does not implement input handling or timed text presentation.
`DialogueContextHelpers.h` belongs beside this system because it borrows the
current ECS context and bridges action requests into world events. VN also uses
that helper when forwarding input to dialogue. These are shared integration
contracts, not VN-owned semantics.

Game code continues to own story state, graph content and condition/action
meaning. Graphs reference identifiers and argument maps; they do not define
quests, affinity storage, presentation profiles, or game rules.

## Visual-Novel Boundary

`VNTypes.h`, `VNRuntime.h`, `VNStage.h`, and `VNCommandRegistry.h` expose script
data, playback, stage state and custom command registration. `VNSystem.hpp`
provides ECS integration. Presentation callers use `VNPresentationProfile.h`,
`VNDialogueUi.h`, `VNDialogueComposition.h`, and the frontend/playback/external
presentation components.

`VNSystem` owns its `VNRuntime`. The runtime owns VN script progression, stage,
waits and typewriter state; it does not replace `DialogueRuntime`. When dialogue
is active, the adapter mirrors semantic nodes/choices into VN presentation and
routes user decisions back to dialogue. It also supports standalone VN scripts
and interaction sessions without an active dialogue graph. The retained
composition is owned by `UiSystem` and borrows the VN runtime.

The interaction frontend exposes slots for application-owned content such as
merchant UI. VN owns stage and shell presentation; application code owns the
content's business rules. Existing execution-profile handling, typewriter
behavior and system scheduling remain unchanged.

## Public Includes And Verification

Link the appropriate compartment target to obtain its public include directories.
Header basenames and the `gts::dialogue` / `gts::vn` namespaces remain unchanged.
There is no umbrella narrative library, common runtime, or forwarding-header layer.

`tests/narrative/` registers `dialogue_runtime` against `gravitas_dialogue` alone.
It covers standalone progression and the ECS request/event bridge. When VN is
available, it also registers the existing `vn_frontend_runtime` test against
`gravitas_visualnovel` alone. These tests do not require the Vulkan runtime-test
configuration: dialogue is testable with rendering disabled, and the VN frontend
is testable with the Vulkan backend disabled.

## Execution Policy Integration

`visualnovel/contracts/VNExecutionProfiles.h` owns `gts::vn::dialogueOverlay()` and
`gts::vn::fullscreenDialogue()`. The always-available header-only
`gravitas_vn_execution_contracts` target consumes `gravitas_execution_policy` only;
it does not bring in VN, rendering, UI or physics implementation. The existing
`gravitas_visualnovel` target consumes these recipes alongside its existing dependencies.

VN retains the IDs `dialogue_overlay` / `fullscreen_dialogue`, masks `0xF99` / `0xF81`,
and FullWorld / UiOnly presentation modes. It pushes a single complete selection
and restores it through expected-ID pop. Profile changes occur at the same points
in external-dialogue/interaction updates. Native script playback/input blocking,
headless dialogue, typewriter behavior and engine pause are unchanged. Rendering
reads only the common execution-policy contract and never depends on VN recipes.
