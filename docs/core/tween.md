# Shared Tween Primitives

`engine/core/tween/Tween.h` belongs to `gravitas_core`. It provides `TweenEase`,
the existing easing-name table, `applyEase`, `lerp`, and `Tween<T>` in
`gts::tween`. Consumers continue to include `Tween.h`; there is no separate
tween library.

Each tween owns its endpoints, current value, duration and elapsed time.
Callers explicitly start, update, finish or clear it. The utility has no global
clock, manager, ECS scheduling, renderer dependency or resource ownership.

Retained UI animation uses tween progress and interpolation. VN stage state owns
value tweens for sprite transforms, opacity, shake and dimming. These consumers
own their timing policies independently. Shared tween code belongs below both;
core no longer obtains it through a module include directory.

UI animation and VN frontend tests exercise the shared easing and transition behavior.
