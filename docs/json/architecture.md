# Shared JSON

## Ownership

`engine/core/json/` owns JSON syntax for the engine. It uses only the C++ standard
library. There is no parser registry, singleton, schema framework, or external
JSON dependency.

- `GtsJsonValue.h` defines the owned value tree: null, bool, number, string, array,
  and object. Objects preserve insertion order; `find(key)` returns a borrowed
  pointer or null. Checked access and lookup live in `GtsJsonValue.cpp`.
- `GtsJsonParser.h/.cpp` provides stateless text parsing and serialization.
  The implementation is compiled once into `gravitas_core`.
- Feature loaders own file/resource access, schema validation, defaults, version
  migration, and conversion between JSON values and their data structures.
  Runtime systems consume those data structures, not JSON trees.

The flow stays explicit:

```text
feature file/resource loader
  -> GtsJsonParser::parse
  -> feature schema conversion
  -> typed feature data

typed feature data
  -> feature JSON value construction
  -> GtsJsonParser::serialize
  -> feature file writer
```

Include the parser in loader/serializer `.cpp` files. Data-only headers should
not pull it in. A feature API that intentionally exchanges JSON values, such as
UI widget schema conversion, may expose `GtsJsonValue`. Do not introduce another
token scanner, escape helper, or hand-written JSON stream writer in a module.
Small schema-specific helpers belong with their feature; they are not reasons
to expand the core parser into a general conversion framework.

## API And Limits

### Value Access

Use `findBool`, `findString`, `findNumber`, `findInt32`, `findUInt32`,
`findUInt64`, and `findFloat` for typed object-member lookup. They return
`std::optional`; missing, null, wrong-type, or unconvertible values produce an
empty result. A present `false`, zero, or empty string remains a present value.
For a value already obtained from an array or `find`, use the matching `try*`
operation. Member lookup reuses these conversions rather than defining its own.

```cpp
visible = tools.findBool("visible").value_or(visible);
const auto version = root.findInt32("version");
if (!version)
    return false;
```

There are no string-to-number or bool-to-number coercions. Integer conversion
requires a whole number within the target range; signed and unsigned integer
storage is checked directly without converting through double. `tryUInt64`
preserves the full unsigned range. `tryNumber` converts to finite double and can
round large integers, just like `asNumber`. `tryFloat` rejects non-finite values
and overflow; normal floating-point rounding, including underflow to zero, is
allowed. Integer sign policy is separate from representation: a positive value
stored as `int64_t` can pass `tryUInt32`.

`tryArray` and `tryObject` return borrowed pointers to containers or null;
`findArray` and `findObject` combine member lookup with that check. `at(index)`
returns a borrowed value pointer, or null for a non-array or out-of-bounds index.
These pointers require the owning tree to remain alive and unmodified. String
optionals own their text. Existing `as*` accessors remain available when the
caller has established the type; they do not perform checked narrowing.

Fallbacks stay at call sites via `value_or`, not extra `*Or` wrappers. Strict
loaders use `find` first when they need distinct diagnostics for missing, null,
and invalid fields. Required/nonempty fields, enum interpretation, positive
dimensions, vector shapes, and array element filtering remain feature rules.
Do not recreate generic lookup or numeric-conversion helpers inside loaders.

Stable enum names are declared once beside the enum using `gts::EnumName`
tables from `core/types/EnumName.h`. Shared `enumValue` and `enumName` lookups
return an optional; the loader owns rejection or fallback for unknown values.
The tables do not depend on JSON. Particle descriptors, module parameter types,
retained UI, widget parameter types, input bindings, presentation/window settings,
and tool workspace names use this mapping. Existing serialized spellings and
fallback policies remain unchanged. Tables may list accepted aliases after the
canonical name; enum-to-text lookup returns the first entry for a value.
Write simple vector arrays directly at the call site; retain helpers for actual
validation or multi-step conversion, not single-expression construction.

UI numeric parameters that become text use `GtsJsonParser::serialize` for the
number itself. They must not pass through default stream precision or convert
exact integers to double. String parameters remain unquoted text.

Particle descriptors read each defined section directly with the shared typed
accessors. When a section is not an object, its fields are read directly from
the emitter to support flat presets (including the flat `shape` string).
An existing section owns its fields: missing or invalid members retain their
defaults, without searching other sections or arbitrary nested objects.
Preset negative-count clamping also remains explicit feature policy.

### Text Parsing

`GtsJsonParser::parse(source, output, error)` returns false for invalid input,
leaves output unchanged, and optionally reports a reason with a zero-based byte
offset. Successful parsing clears the error. The tree owns its strings and does
not borrow the source buffer.

Parsing accepts standard JSON whitespace and strict JSON number syntax.
Strings support all JSON escapes, Unicode escapes, surrogate pairs, and valid
UTF-8. Invalid UTF-8, unescaped controls, duplicate object keys, trailing commas,
comments, and trailing input are rejected. Escaped and literal spellings of the
same key count as duplicates. A UTF-8 BOM is not accepted.

Integer tokens are stored exactly as signed 64-bit values when negative and
unsigned 64-bit values otherwise. Out-of-range integer tokens are rejected.
Fractional/exponent tokens use finite `double`; overflow and underflow reported
by the standard conversion routine are rejected. Negative zero is preserved.
`asNumber()` converts any numeric representation to double, so callers needing
exact large integers must use the matching integer accessor. Narrower ranges
and semantic constraints remain feature responsibilities, not syntax checks.

`GtsJsonParser::serialize(value, indent = 0)` returns readable JSON with two-space
nesting and insertion-ordered object members. The optional indent is the initial
indentation offset, not the indentation step; its allowed range is 0..256.
Numbers use locale-independent, roundtrippable formatting without fixed decimal
truncation. Strings are validated and escaped centrally.

Serialization throws `std::invalid_argument` for invalid UTF-8, non-finite numbers,
duplicate object keys, or excessive nesting/indentation. String-returning feature
serializers propagate this error. Feature file-save APIs convert it to false
(and an error message where their existing API provides one) before opening the
destination. This protects existing files from invalid-value truncation; it is
not atomic replacement or protection against a later disk write failure.
Allocation and filesystem exceptions retain their normal C++ behavior.

Both directions allow values through nesting depth 128, counting the root as
depth zero. This bounds recursive stack use, not document byte size. These
loaders are intended for local engine assets, not unbounded network input.

## Migrated Consumers

- Input binding documents.
- Particle effects, emitter descriptors, module stacks, and graphs.
- glTF JSON, including the JSON chunk inside GLB.
- Font atlas metadata.
- Retained UI assets, reusable widget assets, packages, and localization.
- Tool launch presets and asset manifests.
- Rendering benchmark JSON reports.

GLB chunk decoding, base64 buffer decoding, binary asset formats, and feature
schema conversions remain with their owners. The standalone Python benchmark
comparison tool still uses Python's standard-library JSON support; it is not an
engine C++ parser or an external dependency.

## Compatibility And Verification

Existing schemas and feature lifecycle behavior remain in place. Legacy particle
schema migration remains particle-owned. Invalid documents previously tolerated
by individual readers can now fail consistently, notably duplicate keys and
malformed numbers/escapes. Unicode escapes no longer degrade to placeholder
characters. Numeric input codes require the whole code string to be numeric.
Font dimensions and particle unsigned fields require an in-range integer.
Checked access also rejects fractional/out-of-range integer fields in UI and
presets instead of truncating them or risking an invalid C++ cast. Optional
fields keep their feature defaults when conversion fails; strict fields fail
with feature diagnostics. Particle burst counts reject negative/fractional or
overflowing values, and finite values too large for float no longer become
infinities in vector, curve, or scalar conversions.

Saved files remain human-readable, but whitespace, escape spelling, and number
formatting can change. Particle and font floats are no longer truncated to four
or three decimal places; benchmark integer counters retain all 64 bits.

`GtsJsonParserTest` covers syntax failures, transactional parsing, UTF-8,
surrogates, controls, exact integers, floating-point roundtrips, nesting, and
invalid writer inputs. It also roundtrips checked-in engine JSON documents.
`JsonMigrationTest` checks input, font, particle, and benchmark conversions,
including escaping and failed saves preserving existing files. Existing UI,
glTF/GLB, particle, preset, and benchmark tests cover their feature contracts.
`GtsJsonValueTest` covers typed lookup, missing/null/false distinctions, borrowed
containers, numeric boundaries, and exact 64-bit access. `AssetManifestAccessTest`
checks required-field diagnostics, optional nulls, and numeric range failures.
