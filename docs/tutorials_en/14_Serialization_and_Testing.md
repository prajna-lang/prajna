# Serialization and Testing (Beginner-Friendly)

Use the `Serializable` interface for human-readable printing, enable `Print/PrintLine`, implement custom `ToString`, and write tiny tests with `@test`.

## 1. Serializable and ToString
```prajna
interface Serializable{ func ToString()->String; }
```
Types implementing `ToString()` are printable.

## 2. Built-in serializable types (partial)
- char, bool, Int<Bits>/Uint<Bits>, Float<Bits>
- Array<Type, Dim> prints as `[a,b,c,...]`
- Tensor<Type, Dim> prints nested `[...]`

```prajna
(123i64).PrintLine(); (3.14f32).PrintLine(); (true).PrintLine();
```

## 3. Enable Print via template
```prajna
template EnablePrintable <T> {
  implement T { func Print(){ this.ToString().Print(); } func PrintLine(){ this.ToString().PrintLine(); } }
}
```

## 4. Custom ToString example
```prajna
struct Point { x: i64; y: i64; }
implement Serializable for Point {
  func ToString()->String { var s = "("; s = s + this.x.ToString() + "," + this.y.ToString() + ")"; return s; }
}
func Main(){ var p: Point; p.x = 3i64; p.y = 4i64; p.PrintLine(); }
```

## 5. Binary serialization (intro)
```prajna
var bytes = serialize::ToBytes(x);
var y = serialize::FromBytes<MyType>(bytes);
```
Notes: ensure layout/protocol consistency; design for cross-platform if needed.

## 6. Testing with @test and asserts
```prajna
@test
func MyCase(){ var a = 2i64 + 2i64; test::Assert(a == 4i64); }
```
Asserts: `test::Assert`, `AssertWithMessage`, `AssertWithPosition`.

## 7. Pitfalls
- Missing `ToString()`: implement Serializable
- Float formatting surprises: implement your own `ToString()` if needed
- Cross-version/platform serialization: define a stable protocol
- Asserts not triggered: ensure they run; failures exit with `-1`

## 8. Exercises
1) Implement `ToString()` for `Rect{w:i64,h:i64}` as `Rect(w,h)` and print it.
2) Write a test asserting `Rect{2,3}` prints `Rect(2,3)`.
3) Serialize and deserialize `Array<i64, 4>` and verify equality (consider endianness).

