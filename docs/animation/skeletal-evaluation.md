# CPU skeletal pose evaluation

`modules/animation/skeletal/` owns the independent `gravitas_skeletal_animation`
target. It links `gravitas_animation_assets`, transitively skeleton assets and
core math. It does not link model/skin assets, importers, the ECS module aggregator,
rendering, or Vulkan. Existing object-animation components are unchanged.

```text
GtsSkeletonAsset defaults
    + compatible GtsAnimationClipAsset + explicit time
    -> per-track binary-search sampling
    -> local TRS property overrides (exact matrix helpers stay fixed)
    -> parent-first hierarchy composition
    -> GtsSkeletonPose
```

## Pose and API

`GtsSkeletonPose` is evaluated occurrence data, outside the immutable asset graph:

```cpp
std::vector<GtsSkeletonLocalTransform> localTransforms;
std::vector<glm::mat4> modelTransforms;
```

Both arrays have exactly `skeleton.nodes.size()` entries in canonical node order.
Local values retain TRS or the original exact affine matrix; no decomposition is
needed. Keeping locals permits inspection and future property-level processing
without losing the matrix alternative. Keeping composed matrices avoids a second
hierarchy pass for consumers. No skeleton pointer, compatibility copy, source
indices, pose IDs, weights, inverse binds, or runtime state are stored in the pose.
The caller associates the returned arrays with the supplied skeleton definition.

Public entry points are:

```cpp
evaluateGtsDefaultPose(const GtsSkeletonAsset& skeleton);
evaluateGtsAnimationPose(const GtsSkeletonAsset& skeleton,
                         const GtsAnimationClipAsset& clip,
                         float timeSeconds);
```

Both return `GtsSkeletonPoseEvaluationResult`, exposing `succeeded()`, `pose()`
(const view, null on failure), and `diagnostics()` (code/message/location errors).
The result owns its pose; callers can copy that value for mutable downstream work.
No partial pose is exposed on failure. Ordinary invalid input or arithmetic
failure produces diagnostics, not exceptions.

Default evaluation validates the skeleton and copies every default local value.
Animated evaluation composes `validateGtsAnimationClip(clip, skeleton)` with time
validation, then starts from the same defaults. Only the property targeted by each
track is replaced. Missing translation, rotation or scale tracks preserve the
corresponding default, including non-identity values. Multiple roots are independent.

## Time and endpoints

The supplied float time must be finite and within inclusive `[0, durationSeconds]`,
with no tolerance, wrapping or clip-time clamping. A zero-duration constant clip
accepts only zero. Playback speed, looping and time advancement are not represented.

Each track uses `std::lower_bound` to locate its own interval. Exact-key times
return the authored value directly. Within the valid clip domain, times before a
track's first key or after its last key return that endpoint value. One-key tracks
are constant for every interpolation mode, including canonical cubic constants.
An authored track overrides its property throughout the clip; it does not revert
to defaults outside its own key range.

Exact/endpoint quaternion keys retain their stored components and signs, including
the near-unit values accepted by canonical validation. Only interpolated rotation
results are normalized. This preserves the explicit exact-key contract.

## Interpolation

- STEP holds the preceding key until the next timestamp, then returns the next key.
- LINEAR vectors use `(1-u)*v0 + u*v1`, with no easing.
- LINEAR rotation normalizes temporary endpoint copies, uses bundled GLM's
  shortest-path SLERP (temporary sign correction and near-parallel fallback), and
  normalizes the sampled result. Canonical quaternion keys are never changed.
- CUBICSPLINE vectors use Hermite interpolation. For `dt=t1-t0` and
  `u=(t-t0)/dt`, the derivative terms are scaled by `dt`:

  `h00*p0 + h10*dt*outTangent0 + h01*p1 + h11*dt*inTangent1`.

- CUBICSPLINE rotation converts endpoints to XYZW component vectors, evaluates
  the same Hermite curve using XYZW derivatives, then constructs a quaternion as
  `(w,x,y,z)` and normalizes the result. It does not SLERP, independently change
  signs, normalize tangents, or modify stored derivatives.

Interval arithmetic and interpolation intermediates use double precision to
avoid overflow from finite float times/derivatives; stored output remains float.
Vector results outside finite float range fail. A zero/nonfinite interpolated
quaternion fails with `POSE_SAMPLE_INVALID`, rather than inventing a rotation.
For example, opposing cubic quaternion keys with zero tangents can reach zero
even though each authored key is individually valid. This is an evaluation-time
curve failure, not a reason to weaken or rewrite the canonical clip contract.

`GtsAnimationSampling.h` is an internal implementation interface in
`gts::animation::detail`. It assumes already validated tracks and clip-domain time;
the public evaluation API establishes those preconditions. No standalone unchecked
sampling API is offered as the consumer boundary.

## Composition and spaces

Local TRS composition matches canonical glTF/default conventions exactly:
`L = translate(T) * mat4_cast(R) * scale(S)`, using GLM column vectors. Matrix-form
local values are copied without decomposition. Matrix nodes cannot have TRS tracks
under canonical clip validation.

One forward pass uses the validated parent-before-child order:

```text
root:  G[i] = L[i]
child: G[i] = G[parent[i]] * L[i]
```

Nonfinite local or composed matrices fail with `POSE_TRANSFORM_NONFINITE` and no
partial pose. Finite canonical inputs can still overflow float matrix storage
during composition, so canonical validation alone is insufficient for this check.
No invertibility requirement is added; finite singular transforms remain allowed.

`modelTransforms[i]` maps node `i` local coordinates into the skeleton's canonical
reference space. It is **not a world transform, model occurrence transform, inverse
bind, or final skin matrix**. The evaluator never applies `GtsModelNode` transforms.
A later skin consumer may multiply a mapped pose matrix by its binding-specific
inverse bind; that multiplication is not implemented here.

There are no mutable caches, traversal state, wall-clock inputs, frame deltas, or
unordered iteration. Repeating identical inputs gives identical output on the same
build/platform regardless of previous evaluations. This is not a cross-platform
bitwise floating-point serialization guarantee.

## Tests and scope

`tests/animation/skeletal/GtsSkeletonPoseTest.cpp` links only the evaluation target.
It covers defaults, multi-root/helper hierarchies, exact keys/endpoints/constants,
all interpolation paths, shortest-arc signs, cubic duration scaling and XYZW
derivatives, independent property overrides, input validation, overflow/degenerate
curves, determinism and immutable inputs. Checks remain active in Release builds.

`GtsImportedAnimationPoseTest.cpp` separately links the canonical importer. It uses
`GltfFixtureBuilder` to generate a rigged animated GLB with an unweighted helper,
nontrivial source-to-evaluation indices, LINEAR rotation and cubic translation/
rotation. Known timestamps verify descendant reference-space positions and
quaternion components. A mesh-node translation of 1000 remains outside the pose.
The four existing project GLBs inspected for optional real-asset coverage contain
no animations or skins; no binary asset was added.

Using the CPU-only configuration in [model-domain.md](../assets/model-domain.md):

```sh
cmake --build /tmp/gravitas-model-domain-cpu --parallel 4
ctest --test-dir /tmp/gravitas-model-domain-cpu -L cpu --output-on-failure
```

No canonical contracts, importers, bundles, skin matrices, playback controllers,
blending, cooking, ECS, rendering, shaders, or Vulkan integration change here.
