# UI resource acquisition API

`gravitas_rendering_ui_resources` owns `IUiResourceProvider.h` and its paired
implementation. It depends only on lightweight rendering handle contracts and
requires no window, renderer backend, image decoder or UI runtime. UI callers
request assets and metadata; renderer integration owns realization and allocation.

## Supported operations

`IUiResourceProvider` exposes the existing texture, clamped-texture, nearest/pixel
texture, dimensions, bitmap-font request and font-metadata operations. The two
color-space overloads preserve their previous default forwarding behavior when a
provider overrides only the one-argument form. Providers with explicit color-space
support retain their overrides. `TextureDimensions::isValid` is unchanged.

`TextureColorSpace` remains asset-owned. The narrow interface uses an opaque enum
declaration, so acquisition-only consumers do not inherit asset-loading headers.
Callers choosing named color-space values include the asset header through their
asset capability. Renderer callers already obtain that capability.

The interface has a virtual destructor, defined out of line. Service pointers
are non-owning call-scoped borrows. Never delete/adopt one into a smart owner.
Returned handles/font metadata follow the supplying provider's cache lifetime;
the interface does not extend that lifetime. Existing null-font, invalid-handle,
diagnostic and fallback behavior remains provider-owned. The change adds no new
asset success guarantees or persistence policy.

`IResourceProvider` derives from this narrow interface and retains engine mesh,
memory/material texture, procedural upload, camera-buffer and object-slot methods.
Existing provider overrides remain source-compatible through inheritance. The
broad provider is an engine integration/backend contract, not a UI capability.
No forwarding adapter or duplicate resource cache is introduced.

## Consumers

- `RenderingControllerContext::uiResources` supplies the current call's UI borrow.
  Runtime populates it alongside the broad integration resource field.
- `UiCompositionContext::resources` and `UiWidgetContext::resources` are narrow.
- `UiSystem` accepts `IUiResourceProvider*`; null remains supported for CPU tests.
- `UiRenderResolver` uses only asset acquisition/metadata for UI extraction.

Narrow callers cannot request camera buffers, upload camera matrices, obtain
object slots or upload meshes. Composition/widget/System public declarations no
longer include the broad provider merely to request a font/texture. Engine clients
performing broader authoring must explicitly include their owning provider contract.

The application/engine must destroy retained compositions before the scene data
(fonts, grids, VN runtimes and callbacks) they borrow. Gravitas clears retained UI
in `unloadActiveScene` before scene unload/world clear/destruction, after the
existing module before-unload hooks. Rendering reset and subsequent scene loading
retain their existing positions. Mount teardown prunes focus, navigation, drag/drop
and callback state before their borrowed scene resources disappear.

## Verification

`ui_resource_contract` links only the narrow target. It checks default virtual
dispatch, path/handle/dimensions/font behavior and compile-time absence of backend
allocation methods or broad provider/UI runtime include directories.
`scene_lifecycle_runtime` mounts a scene-resource-borrowing composition and requires
its destruction before every scene unload, including destruction before engine
start. Existing retained UI, narrative, rendering and full runtime tests remain.
The game's UI/narrative fixture additionally exercises actual merchant controls,
focus cancellation, synchronous interaction sessions and return dialogue.
