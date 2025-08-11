# Variables and Type System (Beginner-Friendly)

Goal: learn how to declare variables, choose/recognize types, when to use explicit casts, and how to fix common errors.

You will learn:
- Declarations and initialization
- Literal suffixes and inference
- Booleans, comparisons, chars and strings
- Explicit conversions: Cast/cast/bit_cast
- Quick fixes for common errors

## 1. Declarations and Initialization

Use `var` to declare variables. You can annotate the type or let the compiler infer from the initializer.
```prajna
// Explicit type (recommended for beginners)
var a: i64;
var pi: f32 = 3.1415f32;

// Inferred from the literal suffix
var b = 10i64; // inferred as i64
```
Notes:
- Declare with `var` before use
- Assignments and arithmetic require exactly matching types (strong typing; no implicit casts)

## 2. Literal Suffixes (Important)

Add a suffix to tell the compiler the exact type:
- Integers: `1i8 / 2i16 / 3i32 / 4i64 / 5i128`
- Unsigned: `1u8 / 1u16 / 1u32 / 1u64 / 1u128`
- Floats: `3.14f16 / 3.14f32 / 3.14f64`

```prajna
var x = 42i64;   // i64
var y = 2.0f32;  // f32
```
Why: Prajna won’t implicitly convert `i64` to `f32`, nor guess your intended type. Suffixes make intent explicit.

## 3. Static/Strong Typing: No Implicit Conversions

- Static: types are decided at compile time
- Strong: no mixing of different types without explicit casts

Wrong:
```prajna
var i = 1i64;
var f = 2.0f32;
// i + f // type mismatch
```
Fix:
```prajna
(i.Cast<f32>() + f).PrintLine();
```

## 4. Conversions: Cast, cast, bit_cast

- `.Cast<T>()`: common and safe value conversion (e.g., `i64` to `f32`)
- `cast<From, To>(value)`: template form, equivalent to `.Cast<T>()`
- `bit_cast<From, To>(value)`: reinterpret bits; requires layout/size compatibility (use carefully)

```prajna
var a = 1i64;
var b: f32 = a.Cast<f32>();
var u: u64 = cast<i64, u64>(a);
// Avoid casual bit_cast unless you know exactly what you’re doing
```

## 5. Booleans and Comparisons

- Type: `bool`
- Comparisons: `==`, `!=`, `<`, `>`, `<=`, `>=`
- Logic: `&&`, `||`, `!`

```prajna
var ok: bool = (3i64 + 2i64) == 5i64;
ok.PrintLine(); // true
```

## 6. Char vs String

- Char: single quotes, type `char`, e.g., `'A'`
- String: double quotes, type `String`, e.g., `"Hello"`

```prajna
var ch: char = 'Z';
var s: String = "Hi";
s.PrintLine();
```

## 7. Common Built-ins

- Signed: `i8/i16/i32/i64/i128`
- Unsigned: `u8/u16/u32/u64/u128`
- Floats: `f16/f32/f64`
- Others: `bool`, `char`, `void`, `undef`
- Composites: `Array<T,N>`, `DynamicArray<T>`, `String`, `Ptr<T>`, `Tensor<T,N>`

## 8. Example: Integer participates in float arithmetic
```prajna
func Main(){
    var frames = 120i64;  // 120 frames
    var fps    = 60.0f32; // 60 fps
    var seconds = frames.Cast<f32>() / fps; // 2.0f32
    seconds.PrintLine();
}
```

## 9. Quick Fixes

- “type mismatch/cannot assign”: make types match; use `.Cast<T>()`; add literal suffixes
- “undeclared variable”: declare with `var` and check scope
- Char vs String confusion: `'a'` is `char`, `"a"` is `String`

## 10. Exercises

1) Declare `count: i64 = 3` and `step: f32 = 0.5`, compute `count*step` without type errors and print the result.
2) Implement `IsUpper(ch: char)->bool` for `'A'..'Z'`, then test `'A'` and `'z'`.
