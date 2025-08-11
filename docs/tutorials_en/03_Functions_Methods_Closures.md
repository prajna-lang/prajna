# Functions, Methods, and Closures (Beginner-Friendly)

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
