#pragma once

// Tag component that marks a renderable entity as static.
// Static entities are initialized once and only rebuilt when dirty,
// reducing per-frame snapshot work for geometry that never changes.
struct RenderStaticTag {};
