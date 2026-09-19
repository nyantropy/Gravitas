# Transform animation

`modules/animation/transform/` owns `TransformAnimationComponent`,
`TransformAnimationMode`, `TransformAnimationSystem`, and the scene installer
`AnimationSceneFeature.h`. These are lightweight
ECS headers owned by `gravitas_transform_animation`, which depends on
`gravitas_transform` and remains exposed by `gravitas_modules`. Neither target
has a renderer dependency.
The CPU skeletal evaluator remains in its independent `gravitas_skeletal_animation`
target under `modules/animation/skeletal/`.

## Installation and authoring

Install once during scene loading:

```cpp
#include "AnimationSceneFeature.h"
#include "TransformAnimationComponent.h"

gts::animation::installAnimationFeature(*this);

TransformAnimationComponent animation;
animation.enableMode(TransformAnimationMode::Rotate);
animation.rotationEulerFactors = {0.0f, 1.0f, 0.0f};
animation.rotationSpeed = glm::radians(90.0f);
ecsWorld.addComponent(entity, animation); // entity also has TransformComponent
```

The `GtsScene&` installer guards against duplicate installation and can be used
again after scene unload resets the world. The `ECSWorld&` overload is for
low-level worlds and must be called once per world lifetime. The installer
registers a fixed-step simulation system in `EcsSystemGroup::Animation`.
Scenes must run their simulation systems; adding a component alone does not
install behavior. The GtsScene1 demo uses this shared feature.

Install the transform feature separately to resolve the resulting local
transforms into world transforms. Renderer feature installation already provides
that integration. Animation marks changed transforms through the normal transform
invalidation path, at most once per entity per tick. Unchanged transforms do not
advance their version or enqueue transform work.

## Behavior

The component combines authored settings and per-entity playback state. On the
first enabled tick it captures the initial position, Euler rotation, and scale,
then advances `time` by the simulation delta. Enabled modes can be combined:

- Translate: `initialPosition + translationAxis * translationAmplitude * sin(time * translationSpeed)`.
- Rotate: `initialRotation + rotationEulerFactors * rotationSpeed * time`.
- Scale: `initialScale + scaleAmplitude * sin(time * scaleSpeed)`.

All values write `TransformComponent` in parent-local space. Translation axes
are used as authored, without normalization or rotation by the entity's own
orientation. Rotation factors weight the transform's X/Y/Z Euler angles;
`rotationSpeed` is in radians per second. Translation and scale speeds are angular
frequencies in radians per second. Scale amplitude is an additive per-axis offset;
authors must choose amplitudes that keep the desired scale range.

Setting `enabled = false` freezes both pose and time. Execution profiles that
mask the Animation group also freeze playback. Disabling an individual mode
leaves its last transform value in place while the shared clock keeps advancing.
To restart from the current transform, set `initialized = false` and `time = 0`.
Enabled properties are owned by animation relative to the captured baseline;
another system should not simultaneously author those same properties.

The previous generic `AnimationComponent`/`AnimationMode` names and demo-owned
system were replaced without compatibility aliases. The unused `localSpace`
flag was removed, and `rotationAxis` is now `rotationEulerFactors` to express
its existing Euler behavior. Scaling now oscillates around the captured scale;
the earlier demo formula added a permanent amplitude offset.

## Boundaries and validation

This feature animates whole-entity transforms. Skeletal clip playback remains
unimplemented; the existing CPU pose evaluator takes an explicit clip time and
owns no ECS playback clock. Texture UV animation remains in rendering because
it updates renderer-owned object data. GtsScene3's specialized cube orbit/bounce
behavior remains demo-owned.

`TransformAnimationRuntimeTest` exercises combined modes, scale endpoints,
unchanged/disabled entities, scene installation and unload, pause/resume, and
world-transform propagation through a rotated parent. Run it with:

```sh
ctest --test-dir build_release -R '^transform_animation_runtime$' --output-on-failure
```
