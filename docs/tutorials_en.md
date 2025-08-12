# Execution Modes

This page explains four common ways to run Prajna: run Main, script mode (.prajnascript), REPL, and Jupyter, including scenarios, steps, and common issues.

## 1. Run a file with Main

- When: single-file or project entry, like C/C++ main
- Requirement: file contains `func Main(){ ... }`
- Command:
```bash
prajna exe examples/hello_world.prajna
```
- Success: expected output and normal exit
- Troubleshooting:
  - Command not found: add Prajna’s `bin` to PATH, or run from `build_release/install/bin`
  - File not found: check working directory; run from repo root

## 2. Script mode (.prajnascript)

- When: run statements in file order like a script, for quick demos/batch tasks
- Example: `examples/hello_world.prajnascript`
```prajna
"Hello World\n".PrintLine();
```
- Command:
```bash
prajna exe examples/hello_world.prajnascript
```
- Notes: no Main required; executes top-to-bottom

## 3. REPL

- When: interactive experiments, debugging expressions, learning the language
- Command:
```bash
prajna repl
```
- Use:
```text
"Hello from REPL".PrintLine();
```
- Tips:
  - Define variables/functions incrementally and try immediately
  - For larger code, put in a file and `prajna exe` it

## 4. Jupyter

- When: literate examples, teaching, interactive exploration (.ipynb)
- Install the kernel:
```bash
prajna jupyter --install
```
- Then open the provided notebook: `docs/notebooks/hello_world.ipynb`
- Tips:
  - Ensure Jupyter (jupyterlab/notebook) is installed
  - Containerized use: see `docs/notebooks/Dockerfile`

## 5. Which mode to choose?
- Programs/examples: run Main
- Quick serial demos/batch: script mode
- Interactive trials/debugging: REPL
- Teaching/visual docs: Jupyter

## 6. Platform and path tips
- Windows: with VS toolchain builds, enter the configured shell before building/running
- Linux/macOS: add `build_release/install/bin` to PATH when running local builds
- Paths are relative to current working directory; run from repo root for examples

# Variables and Type System 

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

# Functions, Methods, and Closures

This page covers standalone functions, struct methods, static methods, and closures (anonymous functions), plus higher-order usage and common pitfalls.

You will learn:
- Function signatures, return values, and procedures (no return)
- Methods via `implement Type { ... }` and using `this`
- Static methods with `@static`
- Closures, captures, and passing functions around
- Higher-order functions: functions as parameters/returns

Try in REPL (`prajna repl`) or put code into a `.prajna` file and run with `prajna exe`.

## 1. Functions

Definition: a reusable code block not bound to an instance; no `this`. Use `func Name(params)->Return { ... }`.

- Basic form: `func Name(params)->ReturnType { ... }`
- No return type can be omitted for procedures

```prajna
// With return
func Add(a: i64, b: i64)->i64 { return a + b; }

// Procedure
func PrintSum(a: i64, b: i64) { (a + b).PrintLine(); }

func Main(){
    Add(2i64, 3i64).PrintLine();
    PrintSum(2i64, 3i64);
}
```

Note: parameters and returns are statically/strongly typed; cast explicitly as needed.

## 2. Methods on Structs

Definition: functions bound to a type, written inside `implement Type { ... }`, using `this` to access fields.

```prajna
struct Point { x: i64; y: i64; }

implement Point {
    func Length2()->i64 { return this.x*this.x + this.y*this.y; }
    func Move(dx: i64, dy: i64) { this.x = this.x + dx; this.y = this.y + dy; }
}

func Main(){
    var p: Point; p.x = 3i64; p.y = 4i64;
    p.Length2().PrintLine();
    p.Move(1i64, -2i64);
    p.Length2().PrintLine();
}
```

## 3. Static Methods (Factories/Utilities)

Definition: associated with the type, not instances; annotate with `@static`.

```prajna
struct Pair { a: i64; b: i64; }

implement Pair {
    @static
    func Create(a: i64, b: i64)->Pair { var p: Pair; p.a = a; p.b = b; return p; }
}

func Main(){
    var p = Pair::Create(1i64, 2i64);
    (p.a + p.b).PrintLine();
}
```

## 4. Closures (Anonymous Functions)

Definition: first-class function values, `(params)->Ret { ... }` or `(){ ... }`, automatically capture used outer variables.

```prajna
func Main(){
    var hello = (){ "Hello".PrintLine(); }; hello();

    var add = (a: i64, b: i64)->i64 { return a + b; };
    add(2i64, 3i64).PrintLine();

    var x = 100i64;
    var capture_add = (v: i64)->i64 { return v + x; };
    capture_add(10i64).PrintLine();
}
```

Tip: closures can be stored, passed, and returned.

## 5. Higher-Order Functions

Definition: functions that take functions or return functions.

```prajna
func ApplyTwice(f: (i64)->i64, v: i64)->i64 { return f(f(v)); }

func Main(){
    var inc = (x: i64)->i64 { return x + 1i64; };
    ApplyTwice(inc, 10i64).PrintLine(); // 12
}
```

Inline note (style, not compiler optimization): you may write the anonymous function directly at the call site:
```prajna
ApplyTwice((x: i64)->i64 { return x + 1i64; }, 10i64);
```
Or bind to a name first for reuse/readability:
```prajna
var inc = (x: i64)->i64 { return x + 1i64; };
ApplyTwice(inc, 10i64);
```

## 6. Common Pitfalls

- Type mismatch for params/returns: use `.Cast<T>()` or literal suffixes
- Capture type inconsistencies: normalize types with casts
- Needing `this` but wrote a free function: move into `implement Type { ... }`

## 7. Exercises

1) Add `Dot(rhs: Point)->i64` to `Point` and verify `(1,2)·(3,4)=11`.
2) Implement `MapAddOne(f: (i64)->i64, v: i64)->i64` that increments `v` before applying `f`, test with a closure.

# Structs, Interfaces, and Dynamic Dispatch 

We explain three things:
- Structs: define data and add methods with `implement` and `this`
- Interfaces: describe behavior (function signatures) without data
- Dynamic<Interface>: decide the concrete implementation at runtime

## 1. Structs: data containers

```prajna
struct Point { x: i64; y: i64; }
```
- Fields are statically/strongly typed
- Access via `this` in methods, or by `p.x` outside

## 2. Methods via implement and this

```prajna
struct Point { x: i64; y: i64; }

implement Point {
    func Length2()->i64 { return this.x*this.x + this.y*this.y; }
    func Move(dx: i64, dy: i64) { this.x = this.x + dx; this.y = this.y + dy; }
}

func Main(){
    var p: Point; p.x = 3i64; p.y = 4i64;
    p.Length2().PrintLine();
    p.Move(1i64, -2i64);
    p.Length2().PrintLine();
}
```
Tip: prefer methods when you need `this` or it reads more naturally.

## 3. Interfaces: describe capabilities

```prajna
interface Say { func Say(); }
```
- Any type implementing the required functions can be treated as this interface

## 4. Implementing an interface for a type

```prajna
struct SayHi { name: String; }

implement Say for SayHi {
    func Say(){ "Hi ".Print(); this.name.PrintLine(); }
}
```
You can implement the same interface for multiple types.

Example:
```prajna
interface Say{ func Say(); }

struct SayHi{ name: String; }
implement Say for SayHi{ func Say(){ "Hi ".Print(); this.name.PrintLine(); } }

struct SayHello{ name: String; }
implement Say for SayHello{ func Say(){ "Hello ".Print(); this.name.PrintLine(); } }
```

## 5. Dynamic dispatch with Dynamic<Interface>

When upper layers only know a value implements `Say` but not its concrete type, use `Dynamic<Say>`.

- Elevate a concrete type to an interface view: `Ptr<T>::New().As<Interface>() -> Dynamic<Interface>`
- Calling on the `Dynamic<Interface>` chooses the concrete implementation at runtime

```prajna
func Main(){
    var say_hi = Ptr<SayHi>::New();
    say_hi.name = "Prajna";
    var d: Dynamic<Say> = say_hi.As<Say>();
    d.Say();

    var say_hello = Ptr<SayHello>::New();
    say_hello.name = "Paramita";
    d = say_hello.As<Say>();
    d.Say();
}
```

Common operations:
```prajna
if (d.Is<SayHi>()) {
    var hi = d.Cast<SayHi>(); hi.Say();
} else {
    var hello = d.Cast<SayHello>(); hello.Say();
}
```

Notes:
- `Dynamic<Interface>` is a concrete type with method table info inside
- Prefer static dispatch (direct methods on concrete types) when you don’t need polymorphism

## 6. Static vs Dynamic
- Static: faster, resolved at compile time; default choice
- Dynamic: flexible, hot-swappable implementations; use for strategy/plugins

## 7. Pitfalls
- Wrote `implement Type {}` instead of `implement Interface for Type {}` (or vice versa)
- Casting without checking: call `Is<T>()` before `Cast<T>()`
- Overusing dynamic dispatch on hot paths

## 8. Exercises
1) Define `Area { func Area()->i64; }` and implement it for `Rect{w,h}` and `Square{s}`, store them in `Dynamic<Area>` and print areas.
2) Implement `Say` for `Point{x,y}` to print `(x,y)`, and verify dynamic dispatch with `SayHi`/`SayHello`.

# Templates and Generics 

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

# Pointers: raw `ptr<T>` and smart `Ptr<T>` 

This page explains two pointer kinds:
- Raw pointer `ptr<T>`: lightweight, manual memory management
- Smart pointer `Ptr<T>` (and `WeakPtr<T>`): reference-counted lifetime management

You will learn:
- Allocate/free, null checks, field access
- When to choose raw vs smart pointers
- Mutating caller data via pointer parameters
- Reference counting and weak references

## 1. Raw pointer `ptr<T>`

```prajna
var p = ptr<i64>::Allocate(1); *p = 42; p.Free();

if (p.IsNull()) { "null".PrintLine(); }
var addr = p.ToInt64(); addr.PrintLine();

struct Counter { v: i64; }
var cp = ptr<Counter>::New(); cp->v = 0i64; cp->v = cp->v + 1i64; cp.Free();
```
Use cases: minimal overhead, C interop; strictly manage allocate/free and nullness.

## 2. Smart pointer `Ptr<T>` (reference counting)

```prajna
struct Box { value: i64; }
var sp = Ptr<Box>::New(); sp.value = 7i64; sp.ReferenceCount().PrintLine();

func Bump(b: Ptr<Box>) { b.value = b.value + 1i64; }
func Main(){ var sp = Ptr<Box>::New(); sp.value = 0i64; Bump(sp); sp.value.PrintLine(); }

sp.IsValid().PrintLine();
```
Use cases: shared ownership, cross-function/module passing, safe cleanup on early-exits.

## 3. WeakPtr (FYI)

- Holds a non-owning reference to break cycles; elevate to strong before use.

## 4. In-place mutation via pointer params

```prajna
struct Counter { v: i64; }
func IncRaw(c: ptr<Counter>) { c->v = c->v + 1i64; }
func IncSmart(c: Ptr<Counter>) { c.v = c.v + 1i64; }

func Main(){
  var r = ptr<Counter>::New(); r->v = 0i64; IncRaw(r); r->v.PrintLine(); r.Free();
  var s = Ptr<Counter>::New(); s.v = 0i64; IncSmart(s); s.v.PrintLine();
}
```

## 5. Quick API
- `ptr<T>::Allocate/New/Free/Null/IsNull/IsValid/ToInt64`
- `Ptr<T>::New/ReferenceCount/IsValid/As<Interface>/ToUndef/FromUndef`

## 6. Pitfalls
- Leaks with `ptr<T>`: always `Free()`; or prefer `Ptr<T>`
- Use-after-free: null after `Free()`; avoid stale references
- Wrong access operator: use `->` for `ptr<T>`, `.` for `Ptr<T>`
- Cycles with `Ptr<T>`: break with `WeakPtr<T>`
- Overusing `Ptr<T>`: prefer value or raw pointer for short-lived locals

## 7. Exercises
1) Create `ptr<Node>` with `Node{v:i64}=123`, write `BumpRaw(n: ptr<Node>)`, verify mutation.
2) Create `Ptr<Buf>` with `Buf{len:i64}`, implement `Grow(b: Ptr<Buf>, delta:i64)`, call multiple times and print `len` and `ReferenceCount()`.

# Containers I: Array, DynamicArray, List 

We cover fixed-size arrays `Array<T, N>`, growable arrays `DynamicArray<T>`, and doubly-linked lists `List<T>`.

## 1. Array<T, N>

```prajna
var a: Array<i64, 4> = [1,2,3,4]; a[2].PrintLine(); a[2] = 99i64;
for i in 0 to 4 { a[i].PrintLine(); }
```
Notes: `N` is compile-time; iterate with `for i in 0 to N`.

## 2. DynamicArray<T>

```prajna
var d = DynamicArray<i64>::Create(0); d.Push(10i64); d.Push(20i64); d.Length().PrintLine();
for i in 0 to d.Length() { d[i].PrintLine(); }
d.Resize(5i64); d[4] = 42i64;
```
Use for variable-length sequences; respect index bounds and `Resize` semantics.

## 3. List<T>

```prajna
var l: List<i64>; l.PushBack(1i64); l.PushFront(0i64); l.PushBack(2i64);
l.Length().PrintLine();
l.PopFront(); l.PopBack();
var node = l.Begin(); while (node != l.End()) { node->value.PrintLine(); node = node->next; }
```
Use for frequent end insertions/removals; random access is slow.

## 4. Traversal
- Array/DynamicArray: index loops
- List: walk nodes from `Begin()` to `End()` and advance each iteration

## 5. Sorting (Sortable)

```prajna
var arr: Array<i64, 5> = [3,1,4,1,5]; arr.Sort(); arr.SortReverse();
var d = DynamicArray<i64>::Create(0); d.Push(3i64); d.Push(1i64); d.Push(2i64); d.Sort();
```
Insertion sort is provided for several containers; suitable for small datasets.

## 6. Pitfalls
- Index out of bounds: ensure valid ranges; `Resize` before writing
- List traversal: don’t forget `node = node->next`
- `Length()` is length, not capacity
- Verify sorted order after sort

## 7. Exercises
1) Implement `Sum(a: Array<i64, 4>)->i64` and `SumD(d: DynamicArray<i64>)->i64`.
2) Create `List<i64>`, print, `Sort()`, then print again.
3) Collect `DynamicArray<String>`, sort, print lines.

# Containers II: Dict, Set, String, and Sorting 

We introduce dictionary-like containers and sets, the `String` type, and sorting across containers.

## 1. ListDict<Key, Value>

Linked-list-backed dictionary; simple inserts; lookup by traversal.

```prajna
var d: ListDict<i64, i64>;
d.Insert(1i64, 10i64); d.Insert(2i64, 20i64);
d.Get(1i64).PrintLine(); // 10
```
Safe access and removal helpers:
```prajna
template <K, V>
func TryGet(dict: ListDict<K, V>, key: K, out: ptr<V>)->bool {
  var node = dict._key_value_list.Begin();
  while (node != dict._key_value_list.End()) {
    if (node->value.key == key) { *out = node->value.value; return true; }
    node = node->next;
  }
  return false;
}

template <K, V>
func TryRemove(dict: ptr<ListDict<K, V>>, key: K)->bool {
  var prev = dict->_key_value_list._head; var cur = prev->next;
  while (cur != dict->_key_value_list._end) {
    if (cur->value.key == key) { prev->next = cur->next; cur->next->prev = prev; cur.Free(); return true; }
    prev = cur; cur = cur->next;
  }
  return false;
}
```
Iterate all pairs:
```prajna
var node = d._key_value_list.Begin();
while (node != d._key_value_list.End()) { node->value.key.Print(); " => ".Print(); node->value.value.PrintLine(); node = node->next; }
```

## 2. HashDict<Key, Value>

Bucketed hash table (current hash maps integer keys to buckets).

```prajna
var h: HashDict<i64, String>; h.__initialize__();
h.Insert_node(100i64, String::__from_char_ptr("Alice\0")); (h.Get(100i64)).PrintLine();
h.Remove(100i64); (h.IsEmpty()).PrintLine();
```
Iterate buckets:
```prajna
for i in 0 to h.size {
  var node = h.buckets[i];
  while (!node.IsNull()) { (*node).key.Print(); " => ".Print(); (*node).value.PrintLine(); node = (*node).next; }
}
```
Safe get:
```prajna
func TryGet(h: HashDict<i64, String>, key: i64, out: ptr<String>)->bool {
  var index = h.Hash_index(key); var node = h.buckets[index];
  while (!node.IsNull()) { if ((*node).key == key) { *out = (*node).value; return true; } node = (*node).next; }
  return false;
}
```
Note: for non-integer keys, design a mapping to `i64` first.

## 3. Set<T>

Set built on `HashDict<T, bool>`.

```prajna
var s: Set<i64>; s.__initialize__(); s.Add(3i64);
s.Contains(3i64).PrintLine(); s.Remove(3i64); (s.IsEmpty()).PrintLine();

var a: Set<i64>; a.__initialize__(); a.Add(1i64); a.Add(2i64);
var b: Set<i64>; b.__initialize__(); b.Add(2i64); b.Add(3i64);
var u = a.Union(b); var inter = a.Intersection(b); var diff = a.Difference(b);
for i in 0 to u.dict.size { var node = u.dict.buckets[i]; while (!node.IsNull()) { (*node).key.PrintLine(); node = (*node).next; } }
```

## 4. String

Mutable string with trailing `\0` for C interop; `Length()` excludes the null terminator.

```prajna
var s = String::Create(2i64); s.Length().PrintLine();
var s1 = String::__from_char_ptr("Hi\0"); s1.Print(); s1.PrintLine();
var s2 = String::__from_char_ptr("Prajna\0"); var s3 = s1 + s2; s3.PrintLine();
(s1 == String::__from_char_ptr("Hi\0")).PrintLine(); (s1 != s2).PrintLine();

s1.Push('!'); s1[s1.Length() - 1].PrintLine(); s1.Pop(); s1.Resize(10i64);
var cptr = s1.CStringRawPointer();
```

## 5. Sorting (Sortable)

Insertion sort across arrays/dynamic arrays/lists:
```prajna
var arr: Array<i64, 5> = [3,1,4,1,5];
"before:".PrintLine(); for i in 0 to 5 { arr[i].PrintLine(); }
arr.Sort();
"after:".PrintLine(); for i in 0 to 5 { arr[i].PrintLine(); }
```

## 6. Pitfalls
- `Get` on missing key: use `TryGet` or provide default
- HashDict assumes integer keys by default: map other keys first
- Set ignores duplicates by design
- Char vs String confusion
- Verify after sorting

## 7. Exercises
1) Store three names in `ListDict<i64, String>`, demonstrate safe retrieval.
2) Use `Set<i64>` to show union/intersection/difference and print elements.
3) Implement `Join(words: DynamicArray<String>, sep: String)->String` and sort+print results.


# Tensors and Numerics 

We introduce `Tensor<Type, Dim>`: how to create, index, print, and copy between host and GPU.

## 0. What is a tensor?
- Intuitively: a multidimensional array; unifies scalar/vector/matrix
- Vector: `Tensor<T,1>` with shape `[N]`; Matrix: `Tensor<T,2>` with shape `[H,W]`; higher dims as needed
- All elements share the same `Type` (e.g., `f32`, `i64`); dimension count `Dim` is compile-time
- Row-major (last dimension contiguous); 0-based indices and `A[i, j, ...]`

## 1. Shape and stride (Layout)
- `stride[Dim-1] = 1`; `stride[i] = shape[i+1] * stride[i+1]`
- Linear index: `sum(idx[i] * stride[i])`
Examples:
- `[H,W]=[2,3]` → `stride=[3,1]`; (1,0) → `1*3+0*1=3`
- `[D,H,W]=[2,3,4]` → `stride=[12,4,1]`; (d,h,w) → `d*12+h*4+w`

## 2. Create and read/write
```prajna
var A = Tensor<f32, 2>::Create([2,3]); A[0,1] = 1.0f32;
A[0,1].PrintLine(); A.PrintLine();
```
Note: `Create` allocates but does not zero-initialize; fill before use.

## 3. Examples: fill and add
```prajna
func FillOnes(mat: Tensor<f32, 2>) {
  var h = mat.Shape()[0]; var w = mat.Shape()[1];
  for i in 0 to h { for j in 0 to w { mat[i, j] = 1.0f32; } }
}

func Add2(A: Tensor<f32, 2>, B: Tensor<f32, 2>, C: Tensor<f32, 2>) {
  var h = A.Shape()[0]; var w = A.Shape()[1];
  for i in 0 to h { for j in 0 to w { C[i, j] = A[i, j] + B[i, j]; } }
}
```

## 4. Shape/print helpers
```prajna
var shape = A.Shape(); shape[0].PrintLine(); shape[1].PrintLine();
A.PrintLine();
```

## 5. Host↔GPU copies (intro)
```prajna
var H = Tensor<f32, 2>::Create([2,3]); FillOnes(H);
var G = H.ToGpu(); var Back = G.ToHost(); Back.PrintLine();
```
Notes:
- `ToGpu/ToHost` depend on the active GPU backend (NV/AMD)
- Tensor usage is consistent on host and device for easy portability

## 6. Init, bounds, lifetime
- Init: contents undefined after `Create`; fill explicitly
- Bounds: ensure `0 <= idx[d] < shape[d]`
- Lifetime: data stored in `Ptr<Type>`; freed when no references remain

## 7. Exercises
1) Build two `[4,4]` tensors with values `i+j` and `i-j`, add into a third and print.
2) Upload `[32,32]` to GPU, launch a simple kernel, download, and print first two rows.

Tips: for very small fixed data prefer `Array<T,N>`; for larger or >=2D data `Tensor` is more convenient.

# GPU Programming (Unified API)

From zero to a running GPU kernel: define a kernel, configure grid/block, and copy data between host and device, using the unified `::gpu::...` API.

## 1. Prerequisites and backends
- NV (NVIDIA): CUDA driver/device, use `@target("nvptx")`
- AMD: ROCm/HIP runtime, use `@target("amdgpu")`

## 2. Unified indices and order
```prajna
var tid = ::gpu::ThreadIndex();   // [z,y,x]
var bid = ::gpu::BlockIndex();    // [z,y,x]
var bshape = ::gpu::BlockShape(); // [z,y,x]
var gshape = ::gpu::GridShape();  // [z,y,x]
```
Order is `[z,y,x]`; in 1D use `[2]` for x.

## 3. Define a kernel
```prajna
@kernel @target("nvptx")
func VecAdd(a: Tensor<f32, 1>, b: Tensor<f32, 1>, c: Tensor<f32, 1>) {
  var tx = ::gpu::ThreadIndex()[2]; var bx = ::gpu::BlockIndex()[2]; var bsx = ::gpu::BlockShape()[2];
  var i = bx * bsx + tx; if (i < a.Shape()[0]) { c[i] = a[i] + b[i]; }
}

@kernel @target("nvptx")
func ExampleShared(...) { @shared var tile: Array<f32, 1024>; ::gpu::BlockSynchronize(); }
```

## 4. Launch syntax
```prajna
var block = [1, 1, 256]; var n = 1i64 << 20;
var grid  = [1, 1, (n + block[2] - 1) / block[2]];
VecAdd<|grid, block|>(A, B, C); ::gpu::Synchronize();
```

## 5. Host↔device with Tensor
```prajna
var A = Tensor<f32, 1>::Create([n]); var B = Tensor<f32, 1>::Create([n]); var C = Tensor<f32, 1>::Create([n]);
for i in 0 to n { A[i] = i.Cast<f32>(); B[i] = 2.0f32; }
var dA = A.ToGpu(); var dB = B.ToGpu(); var dC = C.ToGpu();
VecAdd<|grid, block|>(dA, dB, dC); ::gpu::Synchronize();
C = dC.ToHost(); C[0].PrintLine(); C[12345].PrintLine();
```

## 6. Full example
```prajna
use ::gpu::*;
@kernel @target("nvptx")
func VecAdd(a: Tensor<f32, 1>, b: Tensor<f32, 1>, c: Tensor<f32, 1>) {
  var tx = ThreadIndex()[2]; var bx = BlockIndex()[2]; var bsx = BlockShape()[2]; var i = bx * bsx + tx;
  if (i < a.Shape()[0]) { c[i] = a[i] + b[i]; }
}
func Main(){ var n = 1i64 << 20; var A = Tensor<f32, 1>::Create([n]); var B = Tensor<f32, 1>::Create([n]); var C = Tensor<f32, 1>::Create([n]);
  for i in 0 to n { A[i] = i.Cast<f32>(); B[i] = 2.0f32; }
  var dA = A.ToGpu(); var dB = B.ToGpu(); var dC = C.ToGpu(); var block = [1,1,256]; var grid = [1,1,(n+block[2]-1)/block[2]];
  VecAdd<|grid, block|>(dA, dB, dC); Synchronize(); C = dC.ToHost(); C[0].PrintLine(); C[123456].PrintLine(); }
```

## 7. Pitfalls
- Wrong dimension order: always `[z,y,x]`
- Readback without sync: call `Synchronize()` before `ToHost()` consumption
- Out-of-bounds: guard `i < a.Shape()[0]`
- Backend mismatch: `@target` must match device/driver
- Grid/block misconfig: cover all elements and guard edges

## 8. Exercises
1) Extend to 2D elementwise add `Tensor<f32, 2>` using `[z,y,x]`.
2) Add a shared-memory tile and use `BlockSynchronize()`.


# C Library Bindings and Dynamic Linking 
Call existing C dynamic libraries from Prajna: load libraries, bind symbols, map types/strings, and troubleshoot.

## 1. Requirements and search paths
- Provide a dynamic library (`.so/.dylib/.dll`)
- Make it discoverable:
  - Put in system search paths
  - Or add its directory to env vars: Windows `PATH`, Linux `LD_LIBRARY_PATH`, macOS `DYLD_LIBRARY_PATH`
  - Or co-locate with the executable (platform-dependent)

## 2. Load with `#link("libname")`
```prajna
#link("libzmq")
#link("anotherlib")
```
Writes the logical library name; platform fills prefixes/suffixes.

## 3. Bind with `@extern`
```prajna
@extern func zmq_errno()->i32;
@extern func zmq_version(major: ptr<i32>, minor: ptr<i32>, patch: ptr<i32>);
```
Use like normal functions.

## 4. Common C↔Prajna type mapping
- `int`→`i32`, `long long`→`i64`, `float`→`f32`, `double`→`f64`
- `char*`→`ptr<char>`, `void*`→`ptr<undef>`, `size_t`→`i64` (typical on 64-bit)
- Struct pointer→`ptr<MyStruct>`

Signatures must match the ABI exactly.

## 5. String interop
```prajna
var s = String::__from_char_ptr(cstr_ptr);
var cptr = s.CStringRawPointer(); // ensures trailing '\0'
```

## 6. Minimal examples
```prajna
#link("libzmq")
@extern func zmq_errno()->i32;
@extern func zmq_strerror(errnum: i32)->ptr<char>;
func Main(){ var err = zmq_errno(); var msg = zmq_strerror(err); String::__from_char_ptr(msg).PrintLine(); }
```
Custom lib:
```prajna
#link("mylib")
@extern func add_i64(a: i64, b: i64)->i64;
func Main(){ add_i64(2i64, 40i64).PrintLine(); }
```

## 7. Troubleshooting
- Library not found: set search envs; use logical name in `#link`
- Symbol missing: ensure exported and unmangled (`extern "C"`), names/signatures match
- Crashes/wrong results: verify ABI, pointer validity, 32/64-bit match
- Garbled strings: ensure trailing `\0`; consider encoding issues

## 8. Exercises
1) Bind 2–3 functions from an installed/custom lib and print results.
2) Bind a function with output pointer parameters and print values.
3) Pass a Prajna `String` to a C function using `CStringRawPointer()`.

# Modules, `use`, and Packages 

Organize multi-file code, import submodules, and use aliases for readability.

## 1. Concepts
- Module: a `.prajna` file
- Package: a directory of modules (may contain subpackages)
- Use `::` from repo root as the top-level

## 2. `use` and aliases
```prajna
use ::gpu::*; // import symbols under gpu
use ::gpu::Tensor<f32, 2> as GpuMatrixf32; // shorter alias
```

## 3. Submodule paths and layout
Example layout:
```
examples/package_test/
  mod_a/
    mod_aa.prajna
  mod_b.prajna
  mod_c/
    mod_ca.prajna
```
Import by directory path → namespace hierarchy:
```prajna
use ::examples::package_test::mod_a::mod_aa::*;
use ::examples::package_test::mod_c::mod_ca::Test as TestC;
func Main(){ Test(); TestC(); }
```

## 4. Minimal runnable example
```
my_pkg/
  math.prajna
  io.prajna
```
`math.prajna`:
```prajna
func Add(a: i64, b: i64)->i64 { return a + b; }
```
`io.prajna`:
```prajna
func PrintNum(x: i64) { x.PrintLine(); }
```
`main.prajna`:
```prajna
use ::my_pkg::math::*;
use ::my_pkg::io::PrintNum;
func Main(){ var r = Add(20i64, 22i64); PrintNum(r); }
```
Run:
```bash
prajna exe main.prajna
```

## 5. Tips and pitfalls
- Keep directory and namespace in sync
- Use `as` for long paths/types
- Disambiguate conflicts with full paths or `as`
- Split modules by responsibility; main only imports what it needs

## 6. Exercises
1) Create `utils/strings.prajna` and `utils/math.prajna`; import and use.
2) Alias `Tensor<f32, 2>` as `Mat` and print its shape.

# IO and Time (chrono)

Basics of printing, reading input, parsing numbers, timing, and sleeping.

## 1. Printing
```prajna
"Hello".Print();
"World".PrintLine();
```

## 2. Reading a line
```prajna
var s = io::Input(); s.PrintLine();
```
Parse to integer (example):
```prajna
func ParseI64(str: String)->i64 {
  var sign = 1i64; var i = 0i64; if (str.Length()>0 && str[0] == '-') { sign = -1i64; i = 1i64; }
  var v = 0i64; while (i < str.Length()) {
    var ch = str[i]; if (ch<'0' || ch>'9') { break; }
    v = v*10i64 + (cast<char, i64>(ch) - cast<char, i64>('0')); i = i + 1i64;
  }
  return sign * v;
}
```

## 3. Timing and sleep
```prajna
var t0 = chrono::Clock(); /* work */ var t1 = chrono::Clock(); (t1 - t0).PrintLine();
chrono::Sleep(0.5f32);
```

## 4. Mini stopwatch
```prajna
func Main(){
  "Press Enter to start...".PrintLine(); io::Input(); var t0 = chrono::Clock();
  "Press Enter to stop...".PrintLine(); io::Input(); var t1 = chrono::Clock();
  (t1 - t0).PrintLine();
}
```

Tips: average over multiple runs for benchmarking; extend parsing as needed.


# Serialization and Testing 

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

# Performance, Debugging, and FAQ 

Practical checklist for performance and debugging.

## 1) CPU-side performance
- Choose the right structure: small/fixed `Array<T,N>`; append-heavy `DynamicArray<T>`/`List<T>`; Dicts: small `ListDict`, large `HashDict`
- Avoid copies: reuse buffers, pass pointers/smart pointers
- Minimize dynamic dispatch on hot paths
- Consider `@inline` for tiny hot functions

## 2) Memory and layout
- Prefer contiguous data (`Array`/`Tensor`) over lists for scanning
- Pre-size and reuse:
```prajna
var d = DynamicArray<i64>::Create(0); d.Resize(1000i64);
```
- Choose pointer kind wisely: `ptr<T>` vs `Ptr<T>`

## 3) Simple timing with chrono
```prajna
func Benchmark(iter: i64){
  for i in 0 to 10 { /* warmup */ }
  var rounds = iter; var t0 = chrono::Clock();
  for i in 0 to rounds { /* call */ }
  var t1 = chrono::Clock(); ((t1 - t0) / rounds.Cast<f32>()).PrintLine();
}
```

## 4) GPU optimization (intro)
- Index order `[z,y,x]`; `i = BlockIndex()[2]*BlockShape()[2] + ThreadIndex()[2]`
- Reasonable block sizes (128–256/512), cover all elements
- Coalesced global access; shared-memory tiling + `BlockSynchronize()`
- Reduce Host↔Device copies; batch kernels; `Synchronize()` before readback

## 5) Debugging workflow
- Minimal repro; REPL/small files
- Asserts:
```prajna
test::Assert(cond); test::AssertWithMessage(cond, "bad input");
```
- Bisect with prints; check bounds and `[z,y,x]` order
- Pointer/resource checks (`Free()`, search paths)

## 6) FAQ
- Library not found: set PATH/LD_LIBRARY_PATH/DYLD_LIBRARY_PATH; `#link("libname")`
- `@extern` crashes: ABI/signatures/pointers/word-size must match
- GPU no-op: check `@target`, drivers, `Synchronize()`, and bounds guards
- Copy overhead: move ToGpu/ToHost out of loops
- Garbled output: ensure `\0` and proper encoding
- Dynamic dispatch slowdown: prefer static calls on hot paths

---
Summary: pick data structures/algorithms first; measure; on GPU, focus on indices/tiling/copies; use asserts and minimal repros to debug fast.
