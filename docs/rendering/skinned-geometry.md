# Vulkan skinned geometry and palettes

The backend has an explicitly callable skinned geometry path and an opaque
world-submission bridge. It consumes evaluated palettes and never evaluates animation.

```text
CPU (Vulkan-independent)                 Vulkan backend
GtsPreparedSkinnedMesh / GtsSkinnedVertex -> VulkanSkinnedMeshResource
                                        -> VulkanSkinnedVertexDescription
GtsSkeletonPose + GtsSkinBinding
    -> GtsSkinPalette                   -> VulkanSkinPaletteBuffer
                                        -> skinned_vertexshader.vert
                                        -> existing material fragment shader
```

## Vertex ABI

`VulkanStaticVertexDescription` is the renamed static-only description. Its
binding, attributes and all static shader inputs are unchanged. Static pipelines
keep their four descriptor sets and never bind a skin palette.

`VulkanSkinnedVertexDescription` interprets binding 0, per vertex, stride 96 bytes,
CPU alignment 4. It uses `offsetof(GtsSkinnedVertex, ...)`:

| Location | Field | Vulkan format | Offset |
|---|---|---|---|
| 0 | pos | R32G32B32_SFLOAT | 0 |
| 1 | normal | R32G32B32_SFLOAT | 12 |
| 2 | tangent | R32G32B32A32_SFLOAT | 24 |
| 3 | color | R32G32B32A32_SFLOAT | 40 |
| 4 | texCoord | R32G32_SFLOAT | 56 |
| 5 | joints | R32G32B32A32_UINT | 64 |
| 6 | weights | R32G32B32A32_SFLOAT | 80 |

The skinned scene configuration adds the existing uint32 object-SSBO-slot
instance stream at binding 1, stride 4, **location 7**. Static scene instances
continue using location 5. The CPU profile structs have no backend methods.

## Palette ABI and ownership

`VulkanSkinPaletteBuffer` owns one mapped host-visible/coherent storage allocation
and descriptor per frame slot. Each buffer represents one externally evaluated
pose interpreted through one mesh binding. Body and clothing may use separate
palette resources for the same pose. There is no material or skeleton ownership.

The skinned pipeline appends **set 4, binding 0**, one read-only storage buffer
visible to the vertex stage. Existing camera/object/material/environment sets
0–3 retain their existing layouts. Devices must support at least five bound sets
and two vertex-stage storage buffers; construction checks these capabilities.

The shader uses `std430, column_major` and an unsized `mat4 matrices[]`. Each
matrix is copied unchanged from `glm::mat4`: 64-byte array stride, 16-byte column
stride, first element at offset 0. There is no transpose or fixed bone array.
The descriptor range follows the uploaded count, even when capacity is retained.
The device's `maxStorageBufferRange` limits palette size; zero/oversized palettes
and non-finite matrices fail before changing an existing upload. Singular finite
matrices are accepted.

`vertex.joints` contains **skin-local palette slots**. CPU palette evaluation has
already incorporated inverse binds and skeleton remapping. Neither skeleton-node
indices nor inverse binds enter the shader.

The matrix layout follows the [Vulkan shader memory layout rules](https://docs.vulkan.org/guide/latest/shader_memory_layout.html).
Host-coherent writes made before submission use Vulkan's
[submission host-write guarantee](https://docs.vulkan.org/guide/latest/synchronization_examples.html).

## Upload, lifetime and draw contract

Create resources with the existing backend device and physical device. Both must
outlive the resources. Like existing per-frame dynamic rendering buffers, the
caller owns frame-fence synchronization:

1. Wait for every submission using the chosen frame slot to complete.
2. Reset/discard that slot's previously recorded command buffers.
3. Call `palette.update(frame, cpuPalette)` before recording/submission.
4. Record draws using that slot's descriptor.
5. Keep the resource alive and do not update that slot until its GPU use ends.

Stable-size updates reuse mapped memory. Growth allocates a replacement before
releasing the completed slot's old allocation; another frame's memory and
descriptor are independent. Descriptor updates require re-recording affected
command buffers. No allocator pool, automatic scheduling or per-update global
wait is introduced. Destruction requires all outstanding uses to complete.

`VulkanSkinnedMeshResource` uploads prepared vertices and uint32 indices directly
to dedicated host-visible/coherent buffers, preserving every byte and primitive
material association. Geometry is immutable after upload. This deliberately simple
initial allocation policy avoids a new staging/synchronization system; device-local
staging can be added when profiling justifies it. The checked mapped allocation
helper is private to these backend resources and reuses `MemoryUtil`.

GPU-bound geometry validation checks finite attributes, nonnegative unit-sum
weights, triangle index/range validity and representable skin slots. The resource
retains the required palette count including **zero-weight** joint components.
`drawPrimitive` checks that the selected uploaded palette is large enough before
recording any commands. It binds set 4, geometry at binding 0, the caller's object
index stream at binding 1 and uint32 indices, then draws the requested primitive.
All instances in this draw share that palette. Different palettes require separate
draws. The caller supplies a sufficiently large object-index stream and valid object
SSBO entries, as for ordinary scene instancing.

`makeVulkanSkinnedPipelineConfig` takes an existing scene configuration, its four
scene descriptor layouts, the palette layout and the skinned shader path. It
replaces only vertex inputs/shader and appends the palette layout. Instantiate
with the existing `VulkanPipeline`; fragment shader, material push constants,
rasterization, blending and depth policy stay caller-selected. The caller must
supply the normal scene material push-constant range (64 bytes today), not the
historical four-byte default in a bare `VulkanPipelineConfig`.

Before `drawPrimitive`, the caller binds that pipeline, scene descriptor sets
0–3, material constants, viewport and scissor inside a compatible render pass.
The API does not select materials, advance clips or apply model-node placement.

## Deformation and surface directions

```text
skin = sum(weights[c] * palette[joints[c]])  // c = 0..3
worldFromBind = object.model * skin
worldPosition = worldFromBind * vec4(position, 1)
clipPosition = camera.proj * camera.view * worldPosition
```

Weights are not renormalized in the shader. Normals use the inverse transpose of
`mat3(worldFromBind)`, followed by safe normalization. This handles nonuniform
scale for the weighted linear transform. As in the static shader, a determinant
magnitude at most `1e-8` uses identity for the normal transform; a collapsed
surface has no unique correct normal. Singular skin blends are not inverted.

Tangents use the combined linear basis, are normalized and orthogonalized against
the transformed normal. Handedness is authored tangent W multiplied by the sign
of the combined basis determinant, as required for reflections. Translation never
acts on normal or tangent directions. Color and UV follow the same material
interface as static geometry: RGB passes through and the existing per-object UV
transform remains applied; skinning itself changes neither.

Palette output is skeleton reference-space deformation. `object.model` must be
the desired character/reference-space placement. A glTF skinned mesh node's
ordinary transform must **not** be blindly supplied again as object placement.
Generic model extraction supplies the entity world transform and owns this
composition rule; the backend consumes the resulting draw data.

## Validation and remaining work

- `VulkanSkinnedGeometryTest`: exact CPU/Vulkan offsets, sizes, formats, matrix
  storage, static defaults and malformed prepared GPU input; no device required.
- `SkinnedShaderAbiTest.cmake`: glslc compilation, spirv-val validation and
  spirv-dis interface assertions for integer joints, input locations, descriptor
  binding, column-major storage and matrix/array strides. Both existing material
  fragment shaders compile/validate too. No custom shader parser is introduced.
- `VulkanSkinningResourceTest`: test-only Vulkan entry-point capture verifies
  upload bytes, frame isolation, resizing, allocation-failure cleanup, draw slot
  checks, primitive ranges and descriptor/buffer bindings without a device.
- `VulkanSkinnedGpuSmoke`: headless real-device geometry/palette upload and update,
  plus creation of skinned pipelines using the existing unlit and PBR fragments.
  It skips with code 77 if no Vulkan GPU exists. This is pipeline/resource smoke
  coverage, not a rendered-image/readback test.

`MeshResource` and `DynamicMeshComponent` are static-profile APIs. Model instances
use separate static/skinned frame draws. Minimal playback lives upstream in
`modules/animation/skeletal/runtime/`. Animated culling bounds, geometry staging
pools, shadow/depth skinned variants, animated cooked formats and modular character
assembly are unsupported. The shader performs linear-blend skinning. Extreme finite
CPU values can still overflow shader arithmetic.

## Verification

From the engine repository root, after configuring the default build described
in [README.md](../../README.md):

```sh
cmake --build build_release --parallel
ctest --test-dir build_release -R '^vulkan_skin.*$' --output-on-failure
```

CPU profile/evaluation tests can use the backend-free configuration in
[model-domain.md](../model/model-domain.md). GPU tests may skip without a compatible
device; inspect actual rendered captures for visual verification. Historical test
counts and machine-specific results are not part of this contract.

## Generic model submission

See [model extraction](../model/model-extraction.md) for the authoritative instance/ECS route,
shared immutable geometry references, runtime material synchronization, renderer
palette snapshots and frame-fence ownership. `VulkanSkinnedSceneRenderer` now consumes
primitive frame draws and caches geometry independently of occurrence resources.
The ABI and low-level resource contracts described above remain unchanged.
