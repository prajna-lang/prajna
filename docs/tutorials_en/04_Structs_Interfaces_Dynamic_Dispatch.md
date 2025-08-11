# Structs, Interfaces, and Dynamic Dispatch (Beginner-Friendly)

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
