# Shared JSON

## Ownership

`engine/core/json/` owns JSON syntax for the engine. It uses only the C++ standard
library. There is no parser registry, singleton, schema framework, or external
JSON dependency.

- `GtsJsonValue.h` defines the owned value tree: null, bool, number, string, array,
  and object. Objects preserve insertion order; `find(key)` returns a borrowed
  pointer or null. Check the type before calling a typed accessor.
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

Saved files remain human-readable, but whitespace, escape spelling, and number
formatting can change. Particle and font floats are no longer truncated to four
or three decimal places; benchmark integer counters retain all 64 bits.

`GtsJsonParserTest` covers syntax failures, transactional parsing, UTF-8,
surrogates, controls, exact integers, floating-point roundtrips, nesting, and
invalid writer inputs. It also roundtrips checked-in engine JSON documents.
`JsonMigrationTest` checks input, font, particle, and benchmark conversions,
including escaping and failed saves preserving existing files. Existing UI,
glTF/GLB, particle, preset, and benchmark tests cover their feature contracts.
