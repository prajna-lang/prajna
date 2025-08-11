# Pointers: raw `ptr<T>` and smart `Ptr<T>` (Beginner-Friendly)

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
