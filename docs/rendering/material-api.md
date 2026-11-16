# Material authoring API

`gravitas_material_frontend` owns the declaration/implementation pairs under
`engine/modules/rendering/core/material/authoring/`. It is a compiled CPU library
with no dependency on the rendering aggregate or Vulkan backend. Include
`MaterialRuntime.h`, `MaterialReferenceHelpers.h` and `MaterialReferenceComponent.h`
through that target. The full rendering/model frontends provide it transitively.

Applications author values and references. Rendering/model implementation owns
texture realization, GPU synchronization, queues, cache registries and backend
lifetime. The private `gravitas_material_runtime_access` capability is for those
engine implementations and their explicitly permitted engine fixtures. It is not
exported as a material authoring include path.

| Operation | Contract |
| --- | --- |
| `materialRuntime(world)` | Borrow/create the world-owned CPU material runtime; never retain across world clear/destruction |
| `createDefinition`, `createInstance`, `cloneInstance` | Create world-scoped material identities. Cloning gives independent mutable instance state |
| `getDefinition`, `getInstance` | Const borrowed views; reacquire after mutation/creation that can move storage, destruction or runtime teardown |
| `setInstance`, `modifyInstance` | Change CPU values, sanitize parameters, advance the instance version and enqueue synchronization. A modify callback runs immediately and reports whether it changed values |
| `destroyInstance`, `destroyDefinition` | Reject built-in identities and invalid/stale handles; definitions still referenced by live instances cannot be destroyed |
| `defaultMaterial`, `vertexColorOnlyMaterial`, `defaultStandardSurfaceMaterial`, `resolveInstanceHandle` | Built-in/fallback identities in this runtime |
| `frameState` | Owned read-only presentation snapshot of current semantic/material realization values |
| `lifetimeToken` | Weak lifetime observation used by model associations, not ownership of a world |
| `createUnlitMaterial` | Unique mutable material from an `UnlitMaterialDescriptor` |
| `getOrCreateSharedUnlitMaterial`, `sharedUnlitMaterialReference` | Share an identical descriptor within one live world; shared mutations affect its users |
| `getOrCreateSharedCookedMaterial` | Share a cooked `.gmat` material path within the world |

```cpp
const auto handle = gts::rendering::createUnlitMaterial(world, descriptor);
world.addComponent(entity, MaterialReferenceComponent{handle});
gts::rendering::materialRuntime(world).modifyInstance(handle,
    [&color](MaterialInstance& instance)
    {
        instance.baseColor = color;
        return true;
    });
```

Do not retain the callback's mutable instance reference or mutate const query
results. Material versions trigger normal renderer synchronization; application
code does not drain queues, inspect GPU cache entries or allocate bindings.
Setting a descriptor texture path ending in `.gmat` retains the cooked-material
loading behavior; it is not interpreted as an image path. Other paths retain the
existing image texture route. Load errors remain explicit exceptions/diagnostics.

`MaterialAssetRealization.h`/`.cpp` bridges owned cooked material values into CPU
material instances and resolves relative texture paths. Model material realization
uses that same runtime and an engine resource-provider borrow internally.

World teardown owns material/cache release, including CPU-only worlds. Explicit
registry resets and GPU synchronization are engine-private. Clear/destroy the
owning world rather than issuing material-registry cleanup from application code.
Instance handles and model material associations are world scoped; they are not
persistent identities to pass into another world. See [world lifetime](architecture.md#world-state-lifetime)
and [model materials](../model/model-material-realization.md).

Font/texture acquisition through the broader resource-provider surface is a
separate boundary; its remaining callers do not gain material-cache permissions.
