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
Runtime occurrence/placement ownership must be settled when draw extraction is
introduced; this backend path deliberately does not infer it.

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

`MeshResource` and `DynamicMeshComponent` remain static-profile-only to avoid a
resource/ECS redesign in this ABI task. Their broad names should be revisited
during a future broader static resource naming cleanup. Static rendering has no new
skinning allocation or shader work. Canonical assets, CPU preparation, pose and
palette evaluation remain unchanged; the backend only includes their data types.

Minimal playback now lives upstream in `animation/skeletal/runtime`; world
extraction and frame-safe palette submission are described below. Animated culling
bounds, geometry staging pools, shadow/depth skinned variants, cooked formats and
modular character assembly remain deferred. The shader performs linear-blend skinning, not dual-quaternion
skinning. Finite CPU input can still overflow extreme shader arithmetic; this stage
does not add per-vertex finite/bounds checks to the production shader.

## Change inventory and verification

Paths below are relative to the engine repository:

- Renamed `engine/modules/rendering/backend/vulkan/resources/mesh/VulkanVertexDescription.h`
  to `VulkanStaticVertexDescription.h`; added neighboring `VulkanSkinnedVertexDescription.h`.
- Added `resources/skinning/VulkanSkinPaletteBuffer.h/.cpp`,
  `VulkanSkinnedMeshResource.h/.cpp` and the backend-private `VulkanSkinningBuffer.h/.cpp`
  under that Vulkan backend.
- Added `rendering/pipeline/VulkanSkinnedPipelineConfig.h`; updated static naming
  in `VulkanPipelineConfig.h` and `rendering/particles/ParticleRenderStage.h`.
- Added `shaders/skinned_vertexshader.vert` and `skinned_vert.spv`; updated
  `shaders/compile.sh` and root `CMakeLists.txt` shader compilation.
- Added `tests/rendering/VulkanSkinnedGeometryTest.cpp`, `VulkanSkinnedGpuSmoke.cpp`,
  `VulkanSkinningResourceTest.cpp`, and `SkinnedShaderAbiTest.cmake`. Updated
  `tests/CMakeLists.txt` and static naming in `GeometryPbrPreparationTest.cpp`.
- Updated `engine/modules/rendering/backend/vulkan/CMakeLists.txt` for the focused
  resource include directory. Backend-owned `.cpp` glob includes the new resources;
  no CPU target gains a renderer dependency and no new target link edge is needed.
- Updated this document, the architecture index, rendering architecture, and static
  geometry documentation. No canonical/profile/evaluation data files changed.

Verification from the game repository root:

```sh
cmake -S engine -B engine/build_release
cmake --build engine/build_release --parallel
cmake --build /tmp/gravitas-model-domain-cpu --parallel 4
ctest --test-dir /tmp/gravitas-model-domain-cpu -L cpu --output-on-failure
ctest --test-dir engine/build_release -R '^(vulkan_skin.*|geometry_pbr_preparation|asset_serialization|cooked_asset_pipeline|runtime_asset_policy|canonical_obj_pipeline|gltf_asset_importer|render_lifecycle_ownership|material_runtime_architecture|first_lit_renderer_architecture|object_material_color_ownership|rendering_benchmark_support|rendering_benchmark_static_geometry_smoke)$' --output-on-failure
cmake --build build --parallel 4
```

The full engine build, CPU-only build and game build all succeed.
The CPU-only configuration has rendering and Vulkan disabled. All 19 canonical,
preparation and animation tests pass. All 15 selected device-independent backend
and static regression tests pass. The real-device smoke test skips because this
environment has no Vulkan GPU (`/dev/dri` is absent). Shader source compilation,
SPIR-V validation and ABI inspection pass; the shipped skinned SPIR-V matches the
freshly compiled output. Device execution and rendered output remain unverified in
this environment. No test-only vertex deformation implementation was introduced.


## Opaque world submission

`GtsSkinnedModelData` shares immutable prepared mesh parts, binding indices and
existing material frame values. Each `GtsSkinnedModelInstance` is a distinct
presentation occurrence. `SkinnedModelComponent` holds that occurrence, an
immutable snapshot of binding palettes, and `actorFromReference` placement.
`extractSkinnedFrame` combines the resolved world transform with this presentation
transform and retains the entire palette snapshot. It does not inspect clips,
skeletons or source nodes. Cached-world frames preserve that snapshot.

`RenderingRuntime` passes this separate packet through `IGtsGraphicsModule` and
`ForwardRenderer` to the scene frame graph. `VulkanSkinnedSceneRenderer` lazily
realizes geometry once per shared render model and palette buffers once per
occurrence/binding. GPU instances are cached while their CPU occurrence exists;
each in-flight frame additionally retains the instances it submitted. Removed
instances are released only after all referencing frame fences complete. Frame
graph/device reconstruction can rebuild these resources; ordinary animation
frames never recreate or upload mesh geometry.

The prepare call occurs during recording **after ForwardRenderer waits the current
frame fence and resets its command buffer**. It writes only that frame's palette
allocation and object SSBO slot. Other frame allocations remain untouched. No
per-frame `vkDeviceWaitIdle`, animation sampling, or inverse-bind multiplication
is added to Vulkan. World placement goes exclusively to the object SSBO.

Each prepared part selects its binding's palette; each primitive retains its
material range. Existing PBR/unlit fragments, material push constants, camera,
object, environment and material descriptors are reused. Missing texture handles
resolve to the existing semantic fallback textures. Palette descriptors remain
set 4 and mesh joint slots index them directly. The shared 64-byte material push
constant structure is now `VulkanSceneMaterialPushConstants`.

This first bridge explicitly supports opaque depth-writing materials. It draws
skinned opaque geometry before the existing static queues in the same scene pass,
so static transparent surfaces still follow opaque geometry. Static-only frames
retain the existing parallel recording path and allocate no palette resources.
Frames with skinned draws record inline. Transparent skinned sorting is deferred
and rejected explicitly. Animated geometry currently bypasses static frustum
culling: bind bounds cannot safely cull animated limbs.

`SkinnedFrameExtractionTest` verifies snapshot ownership, binding-specific palette
values and independent object placement without a GPU. Existing Vulkan ABI,
shader reflection and intercepted-resource tests remain applicable. Actual
rendered-image verification requires a Vulkan device; CPU tests do not establish
visual correctness.
