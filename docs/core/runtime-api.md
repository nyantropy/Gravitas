# Runtime facade API

Applications include `GravitasEngine.h` and link `gravitas_runtime` or the full
`gravitas_engine` facade. `GravitasEngine.cpp` owns platform, loop, module/service
state and command consumption behind `unique_ptr<Impl>`; none of those
implementation headers are required by the declaration header. The old
`GravitasEngine.hpp` implementation header has no compatibility wrapper.

The complete facade still requires rendering, Vulkan, physics, diagnostics and
tools at build time. Runtime tool visibility/enabling is separate. Reduced
capability builds use leaf APIs, not a partially functional facade.

| Operation | Contract |
| --- | --- |
| `GravitasEngine(EngineConfig)` | Own a configuration value and construct runtime services; startup failures propagate |
| `registerScene(name, SceneFactory, info)` | Retain a factory returning exclusive scene ownership |
| `setActiveScene(name, payload)` | Synchronous host integration load; return false for unregistered ID before teardown; owned payload released after load |
| `applyToolLaunchPreset(const ToolLaunchPreset&)` | Copy startup/tool screenshot options; include the separate preset header when constructing values |
| `start()` | Run simulation/controllers/rendering and deferred commands, then coordinated shutdown; requires a loaded scene and a live runtime |
| Destructor | Also unload/reset an active scene when `start()` was never called or unwound; shutdown is idempotent |

The facade is noncopyable. Factories may borrow host session objects only when
the host guarantees their lifetime. Do not adopt borrowed per-call module/frame
services into smart owners. A scene/load request does not make frame-context
pointers durable.

For an application-defined `MyScene` implementing `GtsScene`:

```cpp
#include "GravitasEngine.h"
#include "MyScene.h"

GravitasEngine engine(config);
engine.registerScene("main", [] { return std::make_unique<MyScene>(); });
if (engine.setActiveScene("main"))
    engine.start();
```

Registration/capability headers used by scene implementation must be included
explicitly; applications no longer receive them by including the facade.
See [commands](commands-api.md), [scenes](scenes-api.md),
[runtime configuration](../modules/runtime-configuration.md) and the existing
module lifecycle contracts. Teardown callbacks must not throw; this API adds no
general recovery from unload/load exceptions.
