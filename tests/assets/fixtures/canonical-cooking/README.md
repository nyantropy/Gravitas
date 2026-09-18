# Cooking fixtures

`simple.gltf` is a single static triangle. `hierarchy.gltf` is a two-mesh forest
with five nodes (one helper), four mesh occurrences and shared mesh 0.
`external.gltf` and `embedded.glb` add a 1×1 RGBA (32,96,160,255) PNG consumed as
color, normal, emissive, metallic/roughness and AO. `triangle.obj` exercises OBJ
convergence and generated normals. `parity.gltf` adds authored white colors to the
hierarchy fixture, making legacy and canonical preparation semantics identical.
All assets are generated synthetic fixtures, not third-party content.

`legacy-golden/` was produced before cutover using the production assetc built from
engine revision `db8d42d146d04c37ebcbf045f55ad674bd995019`:

```sh
assetc import parity.gltf --output legacy-golden
```

SHA-256:

```text
c9de75b6988baaec4ff07099092412590dea7e1ebf84f2538e12243c219bb49b  parity.gmodel
d3c33967883ab1436a9c4d96b85de95c85b005324f00c8608961b3437cf61052  parity_second.gmesh
aceb3dfab30191621e1bbfb4e761cc1a3977ba722b19c3a8f1858ba6d396c71f  parity_surface.gmat
a55b53718db183491048ad30c32dae0c0a00551cfb6df6661bffbf1466db2339  parity_triangle.gmesh
```

The integration test generates new packages with the new production cooker and
compares every parity output byte to these immutable baseline files. Do not
regenerate the baseline to mask a regression. Existing serialization goldens were
not changed.
