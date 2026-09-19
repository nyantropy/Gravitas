# Canonical static cooking

The complete cooking/runtime fork is documented in the
[asset and model architecture](architecture.md).

Source formats are interpreted once into the canonical model domain. Cooking and
runtime loading consume that domain rather than maintaining independent
source-format model representations. `GtsModelCooker::cookSourceAsset` selects an
`IGtsModelImporter`; `cookModelBundle` validates cookability; `cookModelAsset`
accepts already-canonical static model data. Source path and output directory are
cooking context, never additions to canonical model semantics.

## Decomposition and storage

Exactly one identity-root node referring to exactly one mesh emits `<source>.gmesh`.
Every other model emits `<source>.gmodel` and one subordinate `.gmesh` per canonical
mesh table entry, preserving helper nodes, multiple roots, local matrices and shared
references. Mesh/material tables retain order. Parent indices derive from children;
root/sibling traversal order in v1 is reconstructed from node-table order. Neither
matrices nor vertices are transformed during cooking. Node/mesh display names are
labels; deterministic suffixes prevent filename collisions. Unassigned primitives
share one default `.gmat`. Primitive order/ranges remain separate even with equal
materials. Two equal canonical material slots still produce distinct references.

Static preparation supplies vertices, metadata, generated flags and mesh-local
bounds. The flat static adapter remains useful for lower-level direct mesh requests,
but cooking no longer uses it to flatten complete models. Multi-shape OBJ packages
therefore use `.gmodel`; direct/generated mesh APIs remain supported independently.

Material factors map directly to current .gmat fields, including separate emissive
factor/strength, alpha cutoff and sidedness. Blend retains the existing no-depth-write
policy. Images use the shared path/memory decoder. Embedded texture names use source
stem and image index; external names use the image path stem. Identity includes the
role, preserving sRGB color/emissive versus linear normal/scalar output. Scalar
channels are selected by the canonical bindings and packed using the shared helper;
no resampling is performed. Repeated combinations reuse the cooked output. Output
references remain filename-local; external texture provenance comes from resolved
canonical paths, including all external inputs of derived scalar textures.

All components are prepared and encoded before publication. Files are staged in
the destination filesystem; old files are backed up and restored on reported write
failures. The .gmodel entry publishes last. This is not a concurrent-writer or
crash-atomic filesystem transaction; cooking the same destination concurrently is
unsupported. Failed import, capability, processing, image or encoding validation
publishes no package. Image decode failures are errors rather than legacy successful
material fallback, so missing required appearance cannot silently cook successfully.

## Cooked-v1 boundary

Formats, versions, IDs/reference encoding, vertex ABI and serializer field order
are unchanged. V1 represents static prepared meshes, material/texture contracts and
matrix/parent model hierarchy. It does **not** persist skeleton definitions, skin
bindings, skeleton uses, animation clips or skinned profiles. Weighted canonical
geometry is rejected, even without an active animation. A model-less bundle is
valid for some import uses but invalid for this cooking target.

Yune remains a valid canonical animated source model and stays source-backed.
Cooking it into static-v1 fails; strict runtime requests still require a compatible
animated representation. Unsupported material UV sets fail instead of remapping to
UV0. Authored sampler limitations retain importer warnings/default engine sampling.
Future animated cooking starts from `GtsModelImportBundle`, `GtsSkeletonAsset`,
`GtsSkinBinding`, `GtsAnimationClipAsset` and model skeleton/binding associations;
it must not resurrect format-specific model/animation DTOs.

## Parity and regression evidence

`CanonicalModelCookingTest` generates every tested package through the production
cooker, then loads, realizes, instantiates and extracts it headlessly. Fixtures cover
GLTF, embedded-image GLB, external images, hierarchical multi-mesh sharing, multiple
roots, helper nodes and OBJ. It compares source/cooked draw transforms, geometry
bytes, ranges, metadata and runtime material semantics. A retained complete legacy
package for `parity.gltf` is byte-identical (.gmodel, two .gmesh, .gmat); the
[fixture provenance](../../tests/assets/fixtures/canonical-cooking/README.md)
records the baseline and fingerprints.

Intentional differences in other fixtures:

- Single identity-root glTF no longer needs a redundant .gmodel.
- Missing colors are white prepared defaults, not an authored-color metadata flag.
- Selected scene policy replaces the legacy all-node interpretation.
- Duplicate sanitized names no longer overwrite distinct definitions.
- Canonical external image paths are absolute/resolved, so provenance dependency
  bytes differ from legacy relative paths when the CLI was given a relative source.
- Scalar outputs retain selected channels; unused channels become white instead of
  retaining irrelevant source bytes. Shared-image filenames remain image-based;
  multi-image derived combinations use deterministic material-context names.
- OBJ multi-mesh hierarchy is preserved rather than concatenated for storage.
- Skin/animation loss, unsupported UV use and missing images now fail explicitly.

The removed historical architecture consisted of `GltfAssetImporter`,
`AssetImporter`/`IAssetImporter`/`AssetImporterRegistry`, `AssetImportResult`,
`ImportedMesh`/`ImportedMeshPrimitive`, `ImportedMaterial`, `ImportedNode`, and the
`ImageAssetImporter` wrapper. The entire `cooking/legacy/` directory is removed.
The useful texture input is now `TextureCookInput` in cooking, and one authoritative
`GtsImageDecode` implementation serves runtime and cooking.
