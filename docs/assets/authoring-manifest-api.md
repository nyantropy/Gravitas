# Authoring manifest API

Link `gravitas_tool_asset_manifest` and include `AssetManifest.h` from
`engine/modules/tools/assets/manifest/`. This compiled CPU library depends only
on `gravitas_asset_contracts`/core. It is available with rendering, tools and the
runtime disabled. The established `gts::tools` namespace and JSON version remain.
The tools aggregate consumes this leaf library; loading a manifest does not
initialize a browser, preview world or renderer.

`AssetManifest` owns its strings, scale, preview settings and plain
`AssetManifestBounds` min/max vectors. The bounds are authoring values, not an ECS
component or collision/placement rule. Preview/presentation code explicitly
constructs its own `BoundsComponent` when attaching a renderable. The application
owns authoritative gameplay footprint/blocking metadata independently of imported
visual bounds.

`loadAssetManifest(path, output, error)` performs synchronous file/JSON/schema
validation, returning false with an optional diagnostic on failure. It assigns
the output only after parsing/validation succeeds. `loadAssetManifestOrThrow(path)`
returns owned values or throws `std::runtime_error` with the same path-bearing
diagnostic. `assetMaterialModeName(mode)` returns the existing serialization name.
No pointer into parser/file storage is retained, and the library has no global
catalog or world lifetime.

Version 1 fields, numeric/range checks, relative model/texture/source paths and
material modes retain their existing behavior. File/schema validation does not
realize a model or guarantee that every referenced runtime product is available;
the public model/mesh/texture request reports realization failures separately.
Call the loader at an explicit setup/content boundary, then pass owned values to
consumers. Do not invoke authoring IO from procedural placement or gameplay rules.

`AssetManifestAccessTest` links this leaf directly and rejects inherited rendering,
runtime or preview headers at compilation. It exercises checked integer/float
access, schema ranges and failed-output preservation independently of tool setup.
See [shared assets](architecture.md), [model instances](../model/runtime-instances.md)
and [cooking/source policy](../cooked-asset-pipeline.md).
