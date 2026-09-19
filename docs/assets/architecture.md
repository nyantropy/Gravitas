# Shared asset infrastructure

`modules/assets/` contains reusable CPU infrastructure consumed by models and by
standalone textures, font/UI/particle resources, direct meshes and generic materials.
The complete model-specific workflow lives in [`modules/model/`](../model/architecture.md).

| Directory | Responsibility / non-model consumers |
| --- | --- |
| `geometry/` | Static vertex ABI, geometry metadata and CPU normal/tangent/default generation; text, procedural/dynamic meshes and `MeshResource`. |
| `importer/image/` | One memory/file image decoder; standalone texture cooking and runtime textures. |
| `cooking/` | `AssetCooker` standalone images, `TextureCooker`, shared texture cache and staged file publication; `assetc` font/UI/particle texture jobs. |
| `serialization/` | Asset IDs/references, cooked container encoding, direct mesh/material/texture contracts and codecs. |
| `loading/cooked/` | Direct mesh, generic material and texture CPU loaders. |
| `loading/mesh/` | Direct mesh loading and its flat canonical OBJ adapter; direct mesh components. |
| `loading/RuntimeAssetPolicy.h` | Shared strict/development policy for model, mesh and texture consumers. |
| `material/` | Opaque runtime material handle contract, shared by generic material services and renderer-neutral model instances. |

The direct source-OBJ mesh adapter deliberately depends on the leaf canonical OBJ
import/preparation targets in `model/`. Shared geometry, encoding, image and cooking
targets do not depend on model code. Models do not depend on that direct-mesh loader,
so this compatibility with mesh-level source consumers creates no cycle.

`.gmodel` types/codecs are owned by `model/serialization/`. Standalone image cooking
never calls model importers. `assetc` dispatches source requests to either the model
cooker or generic image cooker. Cooked-v1 bytes, IDs and source policy are unchanged.
