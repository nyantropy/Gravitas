# Canonical associated-asset import bundle

`modules/assets/importer/GtsModelImportBundle.h` describes the canonical products
of one import operation. Bundle validation lives beside it, separately from model
validation. Both compile into the existing CPU-only `gravitas_assets` target;
no parser, runtime, renderer or new target dependency is introduced.
`GtsModelImportResult.h/.cpp` retain their existing `model/` paths so current
consumers need no include changes.

```cpp
struct GtsModelImportBundle
{
    std::optional<GtsModelAsset> model;
    std::vector<std::shared_ptr<const GtsSkeletonAsset>> skeletons;
};
```

The model is optional. A model-only bundle and a skeleton-only bundle are valid;
a bundle without either product is rejected. An explicitly present empty model
retains the existing model-domain validity policy. No current source importer
produces skeleton-only results; the contract now permits that future mode.
There are no speculative animation, provenance or external-resolution fields.

## Definitions, occurrences and ownership

- `GtsSkeletonAsset` is an immutable definition.
- `GtsSkeletonCompatibility` is a structural requirement, independent of identity.
- `GtsModelImportBundle::skeletons` enumerates the definitions produced by this
  operation, once per object.
- `GtsModelSkeletonUse` is one occurrence referencing an actual definition.
- `GtsSkinBinding` retains compatibility, local-slot remapping and inverse binds.
- A future runtime pose will be mutable evaluated state; it is absent here.

Create one shared immutable definition object per intended imported definition.
Copy that shared pointer into the bundle list and all corresponding model uses.
Two occurrences can share one list entry; they remain two occurrences. Never
create a second skeleton copy merely to populate the list. Published definitions
must not be modified through retained mutable aliases.

Duplicate list entries for the same object are errors, not silently removed.
Separately created definitions remain separate even when structurally compatible.
There is no automatic compatibility-based deduplication or persistent identity.
Pointer/object sharing checks verify the explicitly authored in-memory ownership
graph only; they do not define skeleton compatibility, a stable asset ID, or pose
sharing. Bundle membership is not compatibility.

The bundle list and model uses both retain shared lifetime. Either may outlive
the other after extraction from a successful result. Unused produced definitions
are valid and remain owned by the list, including when no model exists.

## Validation and result contract

`validateGtsModelImportBundle(bundle)` returns
`GtsModelImportBundleValidationResult`: canonical error diagnostics and `isValid()`.
It composes the existing skeleton and model validators without reproducing their
structural, compatibility, binding, or influence rules. It checks:

1. At least one product exists.
2. Every listed definition is non-null, valid, and has shared lifetime ownership.
3. No definition object is listed twice.
4. The optional model passes complete model validation.
5. Every model skeleton use refers to a listed definition object and shares its
   ownership control block. Matching addresses under independent ownership are
   rejected; an alias with no ownership is also rejected.

Errors identify `skeletons[i]` or `model.skeletonUses[i].skeleton`, while delegated
model/skeleton errors retain their codes with bundle-relative location prefixes.
Validation is deterministic, read-only, and performs no collection or repair.
A shared pointer's deleter/lifetime correctness still belongs to its producer;
the validator cannot prove the behavior of arbitrary custom deleters.

Enumeration is currently closed: all uses must refer to listed products. This
rule lives only in bundle validation, not in `GtsModelAsset` or the skeleton/skin
domains. A future explicit external-definition/link context can extend this
check without replacing the bundle/result representation. There is no implicit
assumption that an unlisted pointer is an externally resolved asset today.

`GtsModelImportResult::success(model, diagnostics)` constructs a model-only bundle
with an empty skeleton list and delegates to the full validation path. It does
not discover skeletons hidden in model uses. Producers of associated skeletons
must use `success(bundle, diagnostics)` and enumerate their definitions explicitly.

Both overloads expose a successful bundle only after validation. Any importer or
validation error discards all products; warnings retain the valid bundle.
`failure(diagnostics)` exposes no bundle and ensures at least one error exists.
Diagnostics belong exclusively to the result operation, never to the bundle's
canonical assets.

- `succeeded()` indicates a successful bundle.
- `bundle()` returns that bundle, or null on failure.
- `asset()` remains a convenient view of the primary model; **null can also mean
  successful model-less import**, so it is not a general success indicator.
- `diagnostics()` and `hasWarnings()` retain their existing meaning.

`IGtsModelImporter` keeps its name and return signature. OBJ remains model-only. Canonical glTF now emits associated skeleton definitions
when selected nodes use skins; static glTF keeps an empty skeleton list.
Animations and morph targets still fail explicitly. The legacy glTF cooker path remains unchanged.

## Tests and next boundary

`tests/assets/importers/GtsModelImportBundleTest.cpp` covers model-only and
model-less products, multiple definitions/occurrences, composed validation,
nulls, duplicates, missing enumeration, ownership mismatch, warning/error
semantics, deterministic diagnostics, and independent list/model lifetimes.
Existing OBJ/static glTF tests assert successful bundles have zero skeletons;
rigged glTF tests cover shared definition ownership and model binding coherence. All run without Vulkan.

During glTF skin decoding, importers intentionally choose which
skins share definitions, create each definition once, enumerate it, and wire
model uses to that same object. Complete model validation already gates skin
compatibility and weight totals. See the implemented [glTF skin policy](gltf-importer.md#skins-definitions-and-occurrences).
Persistent IDs, external skeleton resolution, dependency/provenance tracking,
animation clips, cooking, pose evaluation and runtime realization remain deferred.
