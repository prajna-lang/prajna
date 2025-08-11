# Templates and Generics (Beginner-Friendly)

This page explains Prajna’s generics: writing code that is generic over types and compile-time constants, with practical examples combining arrays/tensors and interfaces.

You will learn:
- Generic structs and methods: `template <T>` and `implement Box<T>`
- Generic functions
- Compile-time constant parameters (length/dimension)
- Implementing interfaces for generic type instances
- Type aliases for readability
- Pitfalls and exercises

## 1. Generic structs and methods

```prajna
template <T>
struct Box { value: T; }

implement Box<T> {
    func Get()->T { return this.value; }
    func Set(v: T) { this.value = v; }
}

func Main(){
    var i: Box<i64>; i.value = 42i64; i.Get().PrintLine();
    var s: Box<String>; s.value = "Hi"; s.Get().PrintLine();
}
```
Notes:
- `template <T>` introduces a type parameter
- Put `<T>` on `implement` as well to make methods generic

## 2. Generic functions

```prajna
template <T>
func Swap(a: ptr<T>, b: ptr<T>) {
    var tmp = *a; *a = *b; *b = tmp;
}

func Main(){
    var x = 1i64; var y = 2i64; Swap<i64>(&x, &y);
    var s1 = "A"; var s2 = "B"; Swap<String>(&s1, &s2);
}
```
Tips:
- Be explicit about type arguments (e.g., `Swap<i64>`) for clarity
- Operations on `T` must be valid for substituted types

## 3. Compile-time constant parameters

```prajna
template <Type, Length_>
struct FixedVec { data: Array<Type, Length_>; }

implement <Type, Length_> FixedVec<Type, Length_> {
    func At(i: i64)->Type { return this.data[i]; }
}

func Main(){
    var v: FixedVec<i64, 4>;
}
```
Common use:
- `Array<Type, Length_>` (fixed-size arrays)
- `Tensor<Type, Dim>` (see Tensors tutorial)

## 4. With arrays/tensors

```prajna
var a: Array<i64, 4> = [1,2,3,4]; a[2].PrintLine();

var A = Tensor<f32, 2>::Create([2,3]); A[0,1] = 1.0f32;
var G = A.ToGpu(); var H = G.ToHost();
```
Both array/tensor shapes are expressed via type+compile-time constants.

## 5. Implement interfaces for generic instances

Example: implement `Sortable` for `Array<Type, Length_>` (insertion sort style):
```prajna
interface Sortable { func Sort(); func SortReverse(); }

template <Type, Length_>
implement Sortable for Array<Type, Length_> {
    func Sort() { /* insertion sort */ }
    func SortReverse() { /* reverse insertion sort */ }
}
```
Benefits:
- Any element type and length gain sorting
- Interfaces + templates compose algorithms and data structures

## 6. Type aliases

```prajna
use ::gpu::Tensor<f32, 2> as GpuMatrixf32;
func Main(){ var A = GpuMatrixf32::Create([64, 64]); }
```

## 7. Pitfalls

- Missing explicit type arguments → inference ambiguity
  - Fix: write `Swap<i64>(&x, &y)` explicitly
- Using runtime values for template constants
  - Fix: pass compile-time constants or use runtime containers like `DynamicArray`
- Forgetting template params on `implement`
  - Fix: ensure both type and implement carry the same parameter list

## 8. Exercises

1) Implement `template <T> struct Pair{ first:T; second:T; }` with a `Swap()` method; test for `i64` and `String`.
2) Implement `template <T, N> func Sum(a: Array<T, N>)->T` summing elements from zero.
3) Implement `SortReverse()` for `DynamicArray<T>` following the insertion-sort pattern.
