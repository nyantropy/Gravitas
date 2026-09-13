# CPU skin matrix palettes

`modules/animation/skinning/` owns `gravitas_skin_palette`. A palette is evaluated
state for one pose interpreted through one mesh binding. It is neither an asset
nor vertex geometry. It contains only:

```cpp
struct GtsSkinPalette
{
    std::vector<glm::mat4> matrices;
};
```

## API and index spaces

```cpp
evaluateGtsSkinPalette(const GtsSkeletonPose& pose,
                       const GtsSkinBinding& binding,
                       const GtsSkeletonAsset& skeleton);
```

The returned `GtsSkinPaletteEvaluationResult` exposes `succeeded()`, `palette()`
(a const pointer, null on failure), and `diagnostics()` with code/message/location.
Ordinary validation or arithmetic failure never publishes a partial palette.

For every skin-local slot `k`, in unchanged binding order:

```text
palette.matrices[k] =
    pose.modelTransforms[binding.joints[k].skeletonNodeIndex]
    * binding.joints[k].inverseBindMatrix
```

The engine uses column vectors. Multiplication applies the authored inverse bind
first, then the evaluated node-to-skeleton-reference transform. No inverse bind is
rebuilt from the default pose, and no world, model-node, or character placement is
included. In particular the glTF skinned mesh node's transform is not multiplied
again onto this output.

```text
skeleton node index = pose array index
skin-local slot    = palette array index = prepared vertex JOINTS value
binding entry     = slot -> skeleton node + inverse bind
```

The evaluator does not sort, compact, or deduplicate slots. A subset binding
produces a subset-sized palette. Duplicate node mappings may have different
inverse binds and remain distinct slots. No remap or inverse bind is duplicated
inside the palette itself.

## Validation and pose contract

Binding validation delegates to `validateGtsSkinBinding(binding, skeleton)`:
valid skeleton, exact binding compatibility, nonempty/assigned mappings, range
checks and finite affine inverse binds. Singular matrices are permitted.

`GtsSkeletonPose` now carries `skeletonCompatibility`, a self-contained exact
contract populated by both default and animated evaluation. Previously the pose
contained only arrays; counts alone could not distinguish different rigs with the
same node count. The descriptor has no skeleton pointer, asset ID or display-name
identity and does not retain the source skeleton. It includes the established exact
ordered IDs, parent relationships, transform forms and default components.

`validateGtsSkeletonPose(pose, skeleton)` belongs to `animation/skeletal/` and checks:

- The supplied skeleton and stored pose descriptor are valid and exactly compatible,
  using the authoritative skeleton-domain compatibility APIs.
- Both pose array sizes equal the skeleton node count.
- Local transform variants retain their skeleton TRS/matrix form; TRS components
  are finite and rotations meet the canonical squared-length tolerance `1e-4`.
- Matrix locals and model/reference matrices are finite affine matrices with exact
  bottom row `[0,0,0,1]`. No invertibility requirement is imposed.

Untyped/untagged manual arrays are insufficient: a default-constructed descriptor
fails validation. Display-name changes and separate but exactly compatible
skeleton objects are accepted. Different IDs, hierarchy, defaults, quaternion
signs or TRS/matrix representations are incompatible even when counts match.

Pose producers must keep local and composed model arrays synchronized. The current
pose evaluator does this in its hierarchy pass. Pose validation checks structure
and value validity without recomposing the hierarchy or proving local/model
arithmetic equality. Palette evaluation reads the already evaluated model matrices;
it does not reevaluate animation or rebuild a pose for each binding.

After validation, each matrix product is checked for finite elements. Finite
inputs can overflow float storage during multiplication: `SKIN_PALETTE_NONFINITE`
reports the failing `slots[k].skeletonNodes[n]` and returns no palette, including
when earlier slots were computable. Invalid input diagnostics retain the existing
skin/skeleton/pose codes and useful binding or pose context. No repairs,
normalization or singular-matrix rejection are performed.

## Modular clothing and ownership

```text
one evaluated GtsSkeletonPose
    + body binding  -> body GtsSkinPalette
    + dress binding -> dress GtsSkinPalette
    + cloak binding -> cloak GtsSkinPalette
```

The pose is evaluated once and reusable across every binding. Slot subsets,
ordering and inverse binds may differ, so the resulting palettes may differ even
for exactly the same pose. There is no cache keyed only by pose or skeleton and no
per-mesh pose ownership. Results own their matrices; inputs are not mutated or
retained. Repeated identical inputs produce identical results on the same build
and platform.

Compatibility snapshots add storage proportional to skeleton size to each pose.
This is a deliberate correctness baseline, without persistent hashes or ownership
of a skeleton object. Future validated evaluation contexts may reduce repeated
validation/copying after profiling; none are introduced here.

## Dependencies and scope

`gravitas_skin_palette` links `gravitas_skeletal_animation` and
`gravitas_skin_assets`, transitively skeleton/animation asset contracts and core
math. The existing pose target remains independent of the skin domain. Neither
production target links model assets, prepared geometry, importers, ECS, renderer
or Vulkan. Only the integration test connects those independent CPU branches.

A palette is dynamic deformation data, never cooked/serialized asset data. It
contains no playback time, clip selection, runtime controller, GPU allocation,
vertex input declaration or shader state. No production or test CPU vertex
skinning helper was necessary: analytical matrix tests directly verify ordering
and inverse-bind use. GPU upload/ABI, normal deformation, object placement and
animated bounds remain downstream concerns.

## Tests

`tests/animation/skinning/GtsSkinPaletteTest.cpp` exercises identity/translated
poses, nonidentity inverse binds, noncommuting multiplication order, parent-child
mapping, reversed/subset/duplicate slots, one pose feeding body/dress bindings,
exact compatibility, malformed inputs, singular affine transforms, finite-input
overflow, deterministic results/diagnostics and immutable inputs.

`GtsImportedSkinPaletteTest.cpp` generates an animated rigged GLB using
`GltfFixtureBuilder`. Source skin order differs from skeleton evaluation order;
two authored inverse binds cancel the default transforms. At times 0, 1 and 2,
expected slot translations are respectively `(0,0)`, `(3,2)` and `(6,4)`. The
mesh-node translation of 1000 never enters the palette. Every prepared vertex
joint component directly indexes the resulting palette. No GPU initialization,
external binary asset, or animation playback instance is needed.

With the CPU configuration in [model-domain.md](../assets/model-domain.md):

```sh
cmake --build /tmp/gravitas-model-domain-cpu --parallel 4
ctest --test-dir /tmp/gravitas-model-domain-cpu -L cpu --output-on-failure
```
