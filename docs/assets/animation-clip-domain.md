# Canonical skeletal animation clips

`modules/assets/animation/` owns `GtsAnimationClipAsset` and CPU-only validation
in the independent `gravitas_animation_assets` target. It depends on
`gravitas_skeleton_assets` and its core math dependency. It does not depend on
model/skin assets, source importers, the existing runtime/object-animation module,
tweening, rendering, ECS, or Vulkan.

```text
GtsSkeletonAsset        = default evaluation hierarchy
GtsSkeletonCompatibility = exact indexed structural contract
GtsAnimationClipAsset   = immutable timed overrides for that contract
future pose            = mutable evaluated result
```

These are producer-authored values, published to consumers as immutable data.
Clips own no skeleton pointers, asset identities, model associations, or playback
state. Compatibility survives destruction of its source skeleton and is unrelated
to pointer identity or shared pose ownership.

## Clip and tracks

`GtsAnimationClipAsset` contains `name`, `targetSkeletonCompatibility`,
`durationSeconds`, and `tracks`. `GtsAnimationTrack` contains an explicitly assigned
`skeletonNodeIndex`, `target`, `interpolation`, `timesSeconds`, and `values`.
The default node index is `InvalidSkeletonNodeIndex`, never valid node zero.
Times and duration are uncompressed float seconds.

Targets are `GtsAnimationTarget::{Translation, Rotation, Scale}`. Each track owns
one property's timing; translation, rotation, and scale need not share keys.
Track order remains producer order. Duplicate node/property pairs are invalid.
Missing nodes/properties are allowed: future evaluation starts from skeleton
defaults and overrides only authored properties. No defaults are copied into clips.

`GtsAnimationKeyValues` is an explicit typed variant:

| Target | Step / Linear | CubicSpline |
|---|---|---|
| Translation, Scale | `vector<glm::vec3>` | `vector<GtsAnimationCubicVec3Key>` |
| Rotation | `vector<glm::quat>` | `vector<GtsAnimationCubicRotationKey>` |

The mutable construction API can express a mismatched variant, just as semantic
vertex streams can contain a mismatched payload. Validation rejects mismatches;
no vec3 is reinterpreted as a rotation. Unknown target/interpolation enum values
also fail. No arbitrary animated properties or matrix tracks are defined.

`Step` means holding the preceding key. `Linear` means component interpolation
for vectors and spherical interpolation for rotations. `CubicSpline` means cubic
Hermite interpolation using the incoming/value/outgoing triple at each key.
This task stores these semantics; it implements no sampling.

## Cubic derivatives and rotations

`GtsAnimationCubicVec3Key` stores vec3 `inTangent`, `value`, and `outTangent`.
`GtsAnimationCubicRotationKey` stores vec4 `inTangent`, quat `value`, and vec4
`outTangent`. Rotation derivatives explicitly use **XYZW component order** and
units per second. They are not rotations and may be zero or non-unit.
Future Hermite sampling multiplies derivatives by segment duration; quaternion
results then require normalization. Stored tangents must not be normalized or
pre-scaled by a segment duration.

This representation preserves the component-wise derivative semantics in
[glTF interpolation Appendix C](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#interpolation-cubic)
without storing glTF accessors or sampler objects. Source quaternion XYZW values
must be mapped explicitly to GLM's quaternion components/constructor convention.
Values retain their signs; neither `q` versus `-q` nor associated spline data is
canonicalized here. Avoiding an interpolated zero quaternion is a later producer/
sampling concern, not a hidden tangent repair in canonical validation.

## Validation

`validateGtsAnimationClip(clip)` returns `GtsAnimationClipValidationResult` with
deterministic error diagnostics (`code`, `message`, `location`). `isValid()` means
there are no errors. Input is never normalized, decomposed, sorted, or repaired.

- Compatibility is validated by the existing skeleton-domain validator.
- Duration must be finite and nonnegative. Empty clips and empty tracks fail.
- Node indices must be assigned and within the compatibility records.
- Track targets must have TRS defaults. The descriptor already contains transform
  forms, so this check is possible **structurally**, without a skeleton object.
  Untargeted exact matrix helper nodes remain valid. No decomposition occurs.
- Each track's times must be finite, nonnegative, strictly increasing, and within
  inclusive `[0, durationSeconds]`. Endpoint comparison has no tolerance. Keys do
  not have to start at zero or reach the duration.
- Each time has exactly one typed value or complete cubic triple. One-key tracks
  are valid constants for every interpolation mode, including cubic; a zero-duration
  clip requires every track to have exactly one key at zero.
- Vector values are finite; zero/negative animated scales are allowed.
- Rotation values are finite and have squared magnitude within `1e-4` of one,
  matching skeleton default validation. Magnitude accumulates in double precision.
  Zero/non-unit rotations fail; accepted roundoff is retained exactly.
- All cubic tangent components are finite, including endpoint tangents. They have
  no unit-length requirement.
- Each node/property pair occurs at most once. No semantic track ordering is imposed.

`validateGtsAnimationClip(clip, skeleton)` composes structural validation with
validation/compatibility derivation of the supplied skeleton. It uses
`areGtsSkeletonCompatibilitiesEqual`, then checks actual target bounds and TRS
forms. Names do not affect matching; IDs, order, parents, exact transform values,
quaternion signs, and TRS versus matrix forms retain existing exact semantics.
No alternative skeleton-matching algorithm is introduced.

## Tests and scope

`tests/assets/animation/GtsAnimationClipAssetTest.cpp` links only
`gravitas_animation_assets`; assertions remain active in Release builds. It covers
typed TRS values, every interpolation, independent keys, constants, cubic
derivatives, malformed counts/times/values/enums, duplicate targets, exact
compatibility, matrix targets, source-lifetime independence, and deterministic
non-mutating validation.

With the CPU configuration from [model-domain.md](model-domain.md):

```sh
cmake --build /tmp/gravitas-model-domain-cpu --target GtsAnimationClipAssetTest --parallel 4
ctest --test-dir /tmp/gravitas-model-domain-cpu -R '^gts_animation_clip_asset$' --output-on-failure
```

Importers and the import bundle remain unchanged. Animated glTF still fails
explicitly. Future glTF animation import must enforce its source rules (including
at least two keys for CUBICSPLINE), remap source nodes to skeleton evaluation
indices, preserve derivative component order, and reject unsupported ordinary
model-node/morph animation. The canonical one-key cubic constant policy does not
relax those source rules.

No sampling, pose, playback, animation bundle enumeration, cooking, serialization,
runtime, ECS, rendering, or Vulkan integration is implemented here.
