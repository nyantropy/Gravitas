# Canonical skin-binding domain

`modules/assets/skin/` owns `GtsSkinBinding` and its CPU-only validation. The
`gravitas_skin_assets` target depends on `gravitas_skeleton_assets`, with core
math transitively available. It has no model, importer, renderer, ECS, or cooker
dependency. Value types are header-only; validation lives in its own `.cpp`.

## Ownership and index spaces

`GtsSkinBinding` owns an optional debug `name`, a self-contained
`targetSkeletonCompatibility` value, and an ordered nonempty `joints` array. Each `GtsSkinJointBinding`
pairs `skeletonNodeIndex` with `inverseBindMatrix`, so independent remap/matrix
counts cannot disagree.

```text
vertex JOINTS value = skin-local slot (model association)
          |
binding.joints[slot]
          |-- skeletonNodeIndex -> canonical evaluation hierarchy index
          `-- inverseBindMatrix -> authored binding transform for that slot
```

The binding stores no vertex streams. Local slot order need not follow skeleton
node order; subsets and repeated mappings are valid. Repeated mappings retain
their own matrices and are not merged. Source formats may impose stricter rules:
glTF's `skin.joints` requires unique source node entries, which a future source
importer must validate, rather than imposing that source rule on this domain.
See the [Khronos skin schema](https://raw.githubusercontent.com/KhronosGroup/glTF/main/specification/2.0/schema/skin.schema.json).

An uninitialized entry uses `InvalidSkeletonNodeIndex` (`uint32_t` max), following
the canonical invalid-index convention. It never silently selects valid node 0.
An explicitly assigned index 0 is valid when the supplied skeleton contains it.
The default inverse bind is identity. Binding names have no validation or
compatibility role.

## Target compatibility

`targetSkeletonCompatibility` is a `GtsSkeletonCompatibility` value containing
ordered stable node IDs, parent indices, and exact default local transforms.
It stores no display names, skeleton asset reference, pointers, handles, or asset IDs.
Derive it with `makeGtsSkeletonCompatibility(skeleton)`, check `succeeded()`, then
copy the result's `compatibility()` value into the binding. Failed derivation
returns diagnostics and no descriptor.

The expectation survives source destruction and cannot be changed by subsequent
source edits. Records are exposed read-only. A binding has no effect on the
source skeleton's lifetime; its structural validation requires no skeleton
object. Default-constructed/explicitly malformed descriptors fail validation.

Contextual validation receives a candidate skeleton externally, derives its
contract, and compares it with the stored expectation through the skeleton
domain's authoritative exact comparison. It retains the same indexed semantics,
including nodes not mapped by this binding. Names are ignored; quaternion signs
and TRS/matrix representations remain distinct. No fuzzy matching or remapping
is performed.

```text
GtsSkeletonAsset         = reusable definition
GtsSkeletonCompatibility = exact structural requirement, independent value
GtsSkinBinding          = requires a compatible structure
GtsModelSkeletonUse     = selects a particular skeleton definition
future runtime pose     = mutable occurrence
```

Compatibility does not imply asset/pointer identity or a shared runtime pose.
Persistent asset references remain intentionally undefined. Descriptor storage
is linear in the skeleton node count and includes copied ID strings and
transforms; no shared ownership or hashing is added to hide that cost. Future
cooking may choose deduplication/storage independently of this canonical value.

## Inverse-bind semantics

For local slot `k`, its matrix maps **stored mesh vertex coordinates into the
binding coordinates associated with the mapped skeleton node**. The future
composition is `G[skeletonNodeIndex] * inverseBindMatrix`, where `G` comes from
the shared evaluated skeleton pose. This module does not compute that product.

Default skeleton transform is not inverse bind data. Validation never derives
matrices from default transforms, compares them to a rest pose, or repairs them.
Body and dress may share one skeleton/pose while carrying different remaps and
inverse binds: a shared skeleton does not imply shared final skin matrices.

## Validation

Both overloads return `GtsSkinBindingValidationResult`, with an `isValid()` query
and errors containing `code`, `message`, and `location`, matching the skeleton
validation convention. Validation is read-only and deterministic.

`validateGtsSkinBinding(binding)` checks:

- integrity of the stored compatibility descriptor, using shared skeleton-domain validation;
- nonempty joint table, at most `uint32_t` max entries;
- explicit node mappings (rejects only the unassigned sentinel here);
- finite inverse-bind elements and exact affine bottom row `[0, 0, 0, 1]`, using
  GLM `[column][row]` indexing.

This structural overload does not check remap bounds, even against the stored
expectation. Bounds belong to contextual validation. It does not inspect meshes.
Finite shear, reflection, nonuniform scale, and singular affine matrices are
accepted. There are no invertibility, orthogonality, or default-pose constraints.

`validateGtsSkinBinding(binding, skeleton)` also checks:

- supplied skeleton validity;
- exact compatibility with the stored valid expectation;
- every assigned node mapping against the supplied skeleton's node count.

Malformed descriptors/supplied skeletons report `SKIN_TARGET_INVALID` or
`SKIN_SKELETON_INVALID`, with the underlying skeleton diagnostic code/message and
prefixed location (`targetSkeletonCompatibility.nodes[...]` or `skeleton.nodes[...]`).
Valid but incompatible definitions report
`SKIN_SKELETON_INCOMPATIBLE`. Structural diagnostics come first, then supplied
skeleton/compatibility diagnostics, then remap bounds. Invalid sentinels get
their structural error rather than a duplicate range error.

These are asset validation operations, not per-frame checks. No validation
cache, palette, upload generation, or current-pose state is stored.

## Tests and scope

`tests/assets/skin/GtsSkinBindingTest` links only the skin target. Release-active
checks cover minimal/multiple/reordered/repeated mappings, equal compatibility requirements with
distinct inverse binds, structural versus contextual errors, every matrix
element's finite checks, singular affine data, exact compatibility, name
independence, source-lifetime independence, input preservation, and deterministic diagnostics.

With the rendering/Vulkan-disabled configuration in [model-domain.md](model-domain.md):

```sh
cmake --build /tmp/gravitas-model-domain-cpu --target GtsSkinBindingTest --parallel 2
ctest --test-dir /tmp/gravitas-model-domain-cpu --output-on-failure
```

The [model domain](model-domain.md#skeleton-uses-and-skin-associations) now owns
model-local binding/use tables and contextual JOINTS/WEIGHTS validation. Those
responsibilities do not move into this independent skin domain. The [import bundle](import-bundle.md) now enumerates the definitions shared by
model uses. Canonical glTF skin decoding now fills these values and associations;
source-specific rules remain in that importer. No animation, runtime, serialization,
ECS, or rendering integration is added.
