# Canonical OBJ Importer

`engine/modules/model/import/obj/GtsObjModelImporter` is the first concrete
`IGtsModelImporter`. It accepts a filesystem source path and returns a validated
`GtsModelAsset` through `GtsModelImportResult`:

```text
OBJ / MTL -> TinyOBJ -> GtsObjModelImporter -> GtsModelAsset
                                               |
                                           FORMAT WALL
                                               |
                              static preparation -> cooking / runtime realization
```

It does not use legacy imported DTOs, renderer vertices, runtime resources,
Vulkan, ECS, image decoders, or cooked formats. It is the sole OBJ interpretation
implementation. Both production consumers use it; the former renderer-coupled
OBJ importer and backend OBJ loader have been deleted.

## Build boundary and reuse

`gravitas_obj_importer` depends publicly on `gravitas_model_domain` and privately on
`gravitas_tinyobj`. TinyOBJ headers and implementation remain in this strategy's
folder. Rendering links the strategy for its cooking/runtime consumers and has
no direct TinyOBJ target dependency. Canonical domain and preparation remain
independently testable with rendering disabled.

The implementation reuses TinyOBJ's `LoadObj`, triangulation, independent corner
indices, `LoadMtl`, and map-option decoding. `ObjSourceReader` handles source
validation, authored color detection, and MTL-relative image identity.

See [OBJ consumers](obj-consumers.md) for cooking and runtime adaptation.

## Geometry

- Each nonempty OBJ shape becomes one named model mesh. Contiguous material runs
  within the shape become independent primitives; A/B/A remains three runs.
  Each mesh gets one identity root node referencing it. There are no parent/child
  relationships or invented source transforms. Empty shapes are skipped.
- Each primitive owns its attribute arrays and local triangle indices. Full
  `(position, normal, texcoord)` tuples define vertex identity. Colors are indexed
  by position in OBJ, so this also preserves color identity. Different normals
  and UVs split vertices even when positions match; omitted partial streams do
  not cause the original tuples to be merged.
- Positions are copied directly. Complete authored normals are copied without
  normalization or repair. UV0 becomes `(u, 1-v)`, preserving the intentional
  existing OBJ V convention at the importer boundary.
- An optional stream is emitted only when every relevant corner supplies it.
  Entirely absent streams are omitted. Partially populated normals, UVs, or colors
  are omitted with `OBJ_PARTIAL_NORMALS`, `OBJ_PARTIAL_UVS`, or `OBJ_PARTIAL_COLORS`
  warnings naming the affected shape/face run. Other primitives retain their
  complete streams. No normals, tangents, UVs, or colors are generated.
- TinyOBJ's enabled color fallback maintains per-position array alignment, but
  an auxiliary source scan records which `v` records actually supplied RGB.
  The detection rule matches this bundled parser: at least six numeric values
  means XYZ followed by RGB. Four-value homogeneous vertices are not colored;
  authored white remains authored. The imported color alpha is 1 because this
  parser's vertex-color extension supplies RGB only.
- Only polygon faces are supported in this step, matching the old path. Line
  and point records are omitted with `OBJ_UNSUPPORTED_PRIMITIVE` warnings; a
  source with no face geometry fails. No smoothing-group-driven generation,
  freeform surfaces, skinning, or tangents are implemented.

## Source checks and failure behavior

`ObjSourceReader` is an importer-private source helper. Before TinyOBJ
triangulation it checks face syntax, signed/relative indices against already
declared attributes, numeric geometry data, and minimum face size. This is
necessary because the bundled parser uses permissive numeric conversion and
can drop malformed polygons while triangulating. Forward references are rejected;
positive and negative indices into already declared data are supported. Index
overflow, zero, and invalid optional indices are errors, not missing data.

The scan removes inline comments before parsing and records the expected
triangulated face count. A mismatch after TinyOBJ triangulation fails instead
of returning silently truncated geometry. Source indices and corner stream
counts are checked again before canonical array access. All successful output
passes `GtsModelImportResult::success`, which applies canonical model validation.
Ordinary file/parse/conversion failures return diagnostics and no asset.

## Materials and images

TinyOBJ still parses MTL values and options. A narrow custom `MaterialReader`
records the originating MTL directory and authored property presence. Image
paths resolve relative to that MTL, become absolute normalized external paths,
and are deduplicated across all materials and roles by lexical path equality.
Symlink or filesystem case aliases are not merged. Images are not decoded.
Missing image files warn while retaining the canonical references. Missing MTL
files and unresolved material names retain TinyOBJ warnings; unresolved material
assignments remain absent.

| OBJ / MTL | Canonical appearance |
| --- | --- |
| Material name | `name` |
| `Kd`, `d` / `Tr` | `baseColor`; alpha below 1 selects Blend |
| `Pm`, `Pr` | Independent metallic and roughness factors |
| `Ke` | Emissive RGB; strength remains 1 |
| `map_Kd` | `baseColorImage` |
| `map_Pm`, `map_Pr` | Independent scalar image bindings; no packing |
| `norm`, its `-bm` | Tangent-space `normalImage` and `normalScale` |
| `map_Ka` | Scalar `ambientOcclusionImage`, preserving existing AO interpretation |
| `map_Ke` | `emissiveImage` |

All image bindings select `TexCoord[0]`. Scalar maps default to Red; an explicit
`-imfchan r/g/b/m` maps to Red/Green/Blue/Alpha. TinyOBJ's implicit `m` default
does not count as an authored alpha selection. Explicit luminance/depth or unknown
channels cannot be represented by the current RGBA selector, so the binding is
omitted with `OBJ_IMAGE_CHANNEL_UNSUPPORTED`. Shared packed images can still
provide separate scalar channels without source-specific downstream behavior.

Unauthored Kd/Pm/Pr/Ke use canonical defaults, rather than TinyOBJ's zero-filled
material initialization. This deliberately differs from the old conversion for
unspecified Kd/Pr; cooking and development source loading now use these canonical defaults.
TinyOBJ handles `d` versus `Tr` precedence. Canonical validation rejects malformed
factor ranges instead of silently clamping them.

Height/bump maps are not tangent-space normal maps. They are omitted with
`OBJ_BUMP_MAP_UNSUPPORTED`, preserving a real `norm` map if supplied. Separate
opacity maps likewise produce `OBJ_OPACITY_MAP_UNSUPPORTED`; conversion into
base-color alpha requires later image processing. Sampler options, UV transforms,
color-space overrides, and other MTL material workflows are not interpreted in
this step, as they have no current canonical destination.

If a material's image binding requires UV0 but the primitive's UV stream is absent
or omitted as partial, import fails canonical validation with
`MODEL_TEXCOORD_REQUIRED`. The importer does not fabricate UVs, strip the material
binding, or weaken the domain validator to make such geometry succeed.

## Provenance gap

The current request carries only a source path; the result carries an asset and
diagnostics. There is no importer-facing dependency/provenance carrier. External
image paths remain available in the model, and diagnostics identify source
locations, but OBJ/MTL dependency lists are not exported. Add an appropriate
import/cooker-facing contract when dependency tracking requires it; no dependency
tables or cooked IDs were added to `GtsModelAsset` here.

## Tests

`tests/assets/importers/obj/GtsObjModelImporterTest` links only the standalone
importer target. It covers triangle and indexed geometry, triangulation, positive
and relative indices, all supported streams, seams, absent/partial streams,
authored colors, shape and material runs, flat roots, MTL factors/opacity, all image
roles, paths relative to nested MTL files, deduplication, scalar channels, missing
sources, malformed indices, unsupported maps, and canonical validation failures.

Using the backend-free configuration in [model-domain.md](model-domain.md):

```sh
cmake --build /tmp/gravitas-model-domain-cpu \
  --target GtsObjModelImporterTest GtsModelAssetTest GtsModelMaterialTest --parallel 2
ctest --test-dir /tmp/gravitas-model-domain-cpu --output-on-failure
```
