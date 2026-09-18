# Skeletal playback occurrences

`animation/skeletal/runtime/GtsAnimationPlayback` holds an optional caller-owned
clip index, seconds, speed, looping and the latest evaluated pose. It owns no
canonical asset, model, skin binding, Vulkan object or wall clock.

`selectClip(index)` resets time and pose only when the selection changes.
`advanceGtsAnimationPlayback(state, skeleton, clip, deltaSeconds)` validates finite,
nonnegative time/speed/delta, advances by `delta * speed`, wraps looping time with
`fmod`, or stops non-looping playback at the endpoint. Zero-duration clips sample
at zero. The authoritative contextual pose evaluator validates compatibility;
time and pose are published together on success. Failures throw actionable
standard exceptions. The caller must keep the selected clip collection and
skeleton alive and pass the clip identified by its selection.

A game chooses clips from gameplay state. One occurrence evaluates one pose per
frame, then that pose may feed any number of binding-specific palettes. Separate
occurrences can share immutable definitions and clips while retaining different
selection, time and pose. There is no blending, root-motion extraction, state
machine, asynchronous evaluation or animation event system.

Yune's game integration uses the controller context's `unscaledDeltaTime`, the
engine's supplied ordinary frame delta. `deltaTime` in that context is currently
the fixed simulation step and must not be mistaken for the elapsed render frame.
Playback runs only while its RenderPrep controller group is enabled. The low-level
sampler remains policy-free; it still rejects times outside clip duration.

The generic [model instance runtime](../model/runtime-instances.md) now owns this
playback state per skeleton use, protects resource-scoped clip selection and stages
pose plus binding palettes atomically. Game code supplies clip choices and delta.
