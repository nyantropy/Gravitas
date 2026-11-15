# Rendering request API

`ScreenshotCommand.h` and `RenderEngineCommands.h` are owned and exported by
`gravitas_rendering_command_contracts`. Their paired `.cpp` implementations link
only core transport and window/config contracts; requests do not require the
renderer, backend, UI or resource provider implementation.

| Typed helper | Owned payload |
| --- | --- |
| `requestScreenshot(commands, directory = {})` | Copy/move the directory verbatim; empty selects configured output |
| `requestSetFrustumCullingEnabled(commands, enabled)` | Boolean culling preference |
| `requestSetFrustumFreeze(commands, frozen)` | Boolean frustum freeze preference |
| `requestApplyGraphicsSettings(commands, const GraphicsSettings&)` | Copy of requested graphics preferences |

Callers borrow the command writer for the call. Helpers own the extension
names/types; application feature code does not invent names or inspect/drain
the queue. These enqueue operations do not report that the request was applied.
Graphics saved/requested/applied result reporting is separate from this producer
contract; see [settings architecture](../settings/architecture.md).

```cpp
#include "RenderEngineCommands.h"

if (ctx.engineCommands != nullptr)
    gts::rendering::requestApplyGraphicsSettings(*ctx.engineCommands, draft);
```

Rendering consumes recognized typed extensions before frame rendering, retaining
the existing screenshot/quit ordering. Malformed payloads with a recognized
name are consumed without application; unknown names remain unhandled and are
ignored by the existing lifecycle drain. No GPU/cache state or raw window
control becomes part of this API.
