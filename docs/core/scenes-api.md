# Scene registration and lifetime API

`SceneManager.h`, `GtsScene.hpp`, `RegisteredSceneInfo.h` and
`GtsSceneTransitionData.h` belong to `gravitas_core`. The manager's implementation
is in `SceneManager.cpp`; applications normally use the
[runtime facade](runtime-api.md) to coordinate graphics/module lifetime.

`SceneManager::registerScene(name, factory, info)` stores factories and stable
metadata entries. Re-registering an ID replaces its factory/metadata without
duplicating the catalog. `hasScene(name) const` observes availability without
creating a scene. `setActiveScene(name)` creates the requested scene or throws
for an unknown ID/null factory result. This low-level operation does not perform
module unload/load hooks or graphics synchronization; runtime owns those steps.

`getActiveScene()` is an optional non-owning pointer, valid until replacement or
clear. `getActiveSceneName()` and `getRegisteredScenes()` return const borrowed
views; do not retain them across owner destruction or catalog mutation.
`clearActiveScene()` destroys the scene. Runtime calls `GtsScene::unload(ctx)`
first to invoke `onUnload`, reset the world and release scene resources.

The runtime validates an ID before graphics-idle synchronization or destructive
unload. `GravitasEngine::setActiveScene` returns false for an unknown ID; queued
unknown requests log rejection and leave the active scene loaded. Registered
factory/load exceptions propagate explicitly after teardown; this API does not
promise rollback. Starting without a loaded scene fails explicitly.

With an application-defined `DungeonScene` and host-owned session:

```cpp
engine.registerScene("dungeon", [&session]
{
    return std::make_unique<DungeonScene>(session);
});
const bool loaded = engine.setActiveScene("dungeon");
```

The host owns `session` beyond engine/scene lifetime and checks `loaded`.
Transition commands own their optional envelope throughout destination `onLoad`.
The destination copies/applies values before returning and retains no envelope,
outgoing-world or frame-service pointer. Application integration derives the
envelope; ordinary game values do not inherit engine transport.

Scene teardown runs module `beforeSceneUnload` hooks in reverse installation
order, then scene unload/reset/destruction, then reverse `afterSceneUnload`
hooks. Load runs forward module hooks around `onLoad`. World clear releases
systems/subscriptions and enrolled world caches before the next scene loads.
Scene resources die after world reset. Direct destruction of an isolated
`GtsScene` still does not call its virtual `onUnload`; the runtime coordinates it.
