# Canonical skeleton asset domain

`modules/assets/skeleton/` owns `GtsSkeletonAsset`, an immutable reusable
evaluation hierarchy and its default local transforms. Like the model domain,
these are authorable value structs: producers construct them, validation reads
them, and consumers treat validated definitions as immutable assets. They are
not runtime pose buffers.

`gravitas_skeleton_assets` is a separate CPU-only target linked only to
`gravitas_core` for core math/build conventions. Its headers use the standard
library and `GlmConfig.h`, without model, parser, renderer, or ECS types. The
existing model/importer targets do not link it.

## Ownership and identity

A skeleton owns an optional display `name` and a nonempty `nodes` array. Each
`GtsSkeletonNode` owns `id`, display `name`, optional `parentIndex`, and
`defaultLocalTransform`. Nodes may be deforming joints or necessary non-deforming
ancestors/helpers. There is no separate deform-joint list.

`GtsSkeletonNodeId` wraps an opaque string `value`. Empty is invalid; values must
be unique within a skeleton. Equality is exact and case-sensitive, with no
trimming, Unicode normalization, or name matching. Display names may be empty or
duplicated and never define compatibility. Producers must assign deterministic
identifiers and retain them across renaming/reordering. The domain does not
generate IDs from names, array positions, source-format indices, or runtime
handles. Cross-file correspondence remains an explicit future import/link task;
the validator cannot prove historical ID stability.

Parents are the only stored hierarchy relationship. Every parent must be in
range and strictly precede its child, so cycles and self-parenting are rejected
without a separate traversal. Multiple roots (absent parent indices) form a valid
forest. No synthetic common root, child lists, or traversal cache is added.
The maximum node count is `uint32_t` max. Validation never sorts nodes.

## Default local transforms

`GtsSkeletonLocalTransform` is `variant<GtsSkeletonTrs, glm::mat4>`. Default
construction selects identity TRS: translation zero, quaternion identity
(`w=1`, `x=y=z=0`), scale one. An explicit matrix retains its exact components,
including shear. TRS and matrix forms are mutually exclusive.

Transforms use GLM column vectors. TRS means `T * R * S`, rotations are
quaternions, and local transforms are relative to the parent; root transforms
are relative to the skeleton reference frame. Eventual composition is
`parentGlobal * local`. No evaluation is implemented. There is one convention
for all assets, not a per-skeleton format/coordinate flag. Source conversion
must make geometry, binding spaces, and the chosen skeleton frame consistent;
asset identity and character/world placement are separate future contracts.

Validation rejects nonfinite translation, quaternion, scale, or matrix values.
Quaternion squared magnitude must differ from one by at most `1e-4`, matching
the existing canonical glTF import threshold; double accumulation avoids float
overflow while checking finite inputs. Zero/non-unit quaternions are rejected.
This threshold accepts floating-point roundoff, not approximate rig matching.
Validation never normalizes quaternions or decomposes matrices.

A matrix's bottom row must be exactly `[0, 0, 0, 1]` (GLM `[column][row]`
indexing). There is no shared affine validator currently available; this narrow
check follows the existing canonical importer rule without importing its code.
Finite zero/negative scale and singular affine matrices are valid: this asset
contract does not require inverses or global-transform evaluation.

Default local transforms are not mesh bind data. A future skin binding owns
mesh-specific joint remapping and inverse bind matrices; a future runtime pose
owns mutable evaluated transforms. Neither belongs in this skeleton.

## Validation and exact compatibility

`validateGtsSkeletonAsset` returns `GtsSkeletonValidationResult`, whose
`diagnostics` are errors containing code, message, and canonical location.
`isValid()` means there are no errors. This follows model validation's result
shape without making the skeleton depend on model-named diagnostics or adding a
generic diagnostic framework. Errors are deterministic in node/field order.

`areGtsSkeletonsCompatible(left, right)` validates both definitions and returns
false if either is invalid. For valid definitions it compares node count/order,
stable IDs, parent indices, transform alternative, and every stored transform
component. Asset/node display names are ignored. There is no redundant stored
descriptor that could become stale and no persistent hash.

Comparison uses exact numeric equality, not a tolerance or matrix equivalence:
signed zero compares equal, quaternion `q` and `-q` compare differently, and
identity TRS differs from identity matrix. Even accepted near-unit quaternion
values must match exactly. This deliberately compares the canonical indexed
contract; it does not discover reordered/subset rigs or perform retargeting.
Scalar components are compared directly: the bundled GLM uses bit comparisons
for floating-point vectors and epsilon comparisons for quaternions, so its
aggregate equality operators do not implement this contract consistently.
Compatibility inputs include the fixed reference-space/composition convention
above; no asset-specific convention field exists.

The comparison is an asset validation/linking operation, not a per-frame hot
path. It does not cache validation or mutate either input.

## Tests and scope

`tests/assets/skeleton/GtsSkeletonAssetTest` links only `gravitas_skeleton_assets`.
Checks remain active in Release builds. Coverage includes empty/minimal/deep
forests, helpers, names versus IDs, invalid hierarchy, both transform forms,
numeric errors, singular matrices, exact compatibility, and deterministic
non-mutating validation/comparison.

Using the CPU-only configuration documented in [model-domain.md](model-domain.md):

```sh
cmake --build /tmp/gravitas-model-domain-cpu --target GtsSkeletonAssetTest --parallel 2
ctest --test-dir /tmp/gravitas-model-domain-cpu -R '^gts_skeleton_asset$' --output-on-failure
```

No skin binding, animation clip, pose evaluation, model association, import
bundle, cooker, runtime, ECS, or rendering integration is implemented. The glTF
importer still rejects actual skin references. Introducing this asset does not
change the existing model/import result or the static rendering profile.
