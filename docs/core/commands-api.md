# Command producer API

`GtsCommandBuffer.h` is exported by `gravitas_core`. It is a movable,
noncopyable enqueue-only owner; its storage and drain operations are private.
Core does not depend on or friend the runtime facade.

Supported producer operations:

| Operation | Ownership and behavior |
| --- | --- |
| `requestTogglePause()` | Append one simulation-pause toggle; controllers continue running |
| `requestQuit()` | Append a shutdown request |
| `requestChangeScene(name, payload)` | Move a scene ID and optional `unique_ptr<GtsSceneTransitionData>` into the queue |
| `requestExtensionCommand(name, payload)` | Move an owned `std::any`; lvalues are copied on conversion. Capability helpers own names/types. |

The queue retains independent FIFO entries, including repeated names and
multiple scene changes. A moved-from producer remains usable. Payloads must own
their values; copying a raw pointer into `std::any` does not extend its lifetime.
Producer allocation/copy failures propagate normally. This is single-threaded
main-loop transport, not a concurrent queue.

Applications use the lifecycle operations only from integration. Feature callers
use the owning typed helper, such as
[rendering requests](../rendering/commands-api.md). Obtaining a frame's borrowed
command pointer does not grant permission to drain it or retain that pointer.

```cpp
if (ctx.engineCommands != nullptr)
    ctx.engineCommands->requestChangeScene("registered_scene", std::move(envelope));
```

`gravitas_command_consumer` is engine-private consumption infrastructure. Runtime
links it privately; it does not publish consumer headers to applications or
ordinary game tests. Its core-owned drain detaches a batch, so writes during
lifecycle dispatch are safely queued for the next frame. Recognized rendering
extensions are consumed before rendering; remaining lifecycle/pause/quit entries
are processed afterward. Rendering consumption preserves relative order of
unhandled entries and writes made during dispatch. Unknown extensions are ignored
by the existing post-render consumer.

Screenshot plus quit can capture the final frame. Quit marks the loop to stop
after processing the batch; it does not discard later FIFO scene requests.
Invalid scene IDs reject without unloading; see [scene API](scenes-api.md).
No generic scene-load rollback is provided.
