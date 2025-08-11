# 指针与内存：ptr<T> 与 Ptr<T>

本篇解释 Prajna 的两类指针：
- 原生指针 `ptr<T>`：轻量、需要手动管理内存
- 智能指针 `Ptr<T>`（以及 `WeakPtr<T>`）：引用计数，自动管理生命周期

你将学到：
- 如何分配/释放内存、判空与字段访问
- 何时选择 `ptr<T>` vs `Ptr<T>`
- 如何用指针在函数中“原地修改”数据
- 引用计数与弱引用，避免常见内存错误

## 1. 原生指针 `ptr<T>`

定义：`ptr<T>` 是指向 `T` 的裸指针，提供 `Allocate/New/Free/Null/IsNull/IsValid` 等能力（见 `builtin_packages/_ptr.prajna`）。

- 分配与释放
```prajna
var p = ptr<i64>::Allocate(1); // 分配 1 个 i64 的空间
*p = 42;
p.Free();                     // 释放，并将 p 置空
```

- 判空与地址
```prajna
if (p.IsNull()) { "null".PrintLine(); }
var addr = p.ToInt64(); addr.PrintLine();
```

- 结构体字段访问（使用 `->`）
```prajna
struct Counter { v: i64; }
var cp = ptr<Counter>::New();
cp->v = 0i64;       // 通过 -> 访问字段
cp->v = cp->v + 1i64;
cp.Free();
```

适用场景：极致轻量、临时对象、与 C 接口交互；需要严格遵守分配/释放与判空规则。

## 2. 智能指针 `Ptr<T>`（引用计数）

定义：`Ptr<T>` 内部维护控制块（`strong_count/weak_count`），按需自动释放对象；拷贝会增加计数，最后一个强引用消失时释放。

- 创建与字段访问（项目示例风格：点号访问）
```prajna
struct Box { value: i64; }
var sp = Ptr<Box>::New();
sp.value = 7i64;      // 智能指针可直接点号访问字段/方法
sp.ReferenceCount().PrintLine();
```

- 与函数配合：按引用修改
```prajna
func Bump(b: Ptr<Box>) {
    b.value = b.value + 1i64;
}

func Main(){
    var sp = Ptr<Box>::New();
    sp.value = 0i64;
    Bump(sp);              // 修改生效
    sp.value.PrintLine();  // 1
}
```

- 计数与判空
```prajna
sp.ReferenceCount().PrintLine();
sp.IsValid().PrintLine(); // true/false
```

适用场景：需要共享所有权的对象、跨函数/模块传递、异常/早退路径下仍需可靠释放。

## 3. 弱引用 `WeakPtr<T>`（了解）

- 作用：不增加强引用计数，解决循环引用等问题
- 基本思路：从 `Ptr<T>` 派生 `WeakPtr<T>`，在需要临时访问前再“提升”为强引用（具体 API 参见实现与后续文档）

## 4. 在函数中“原地修改”：指针参数

当你需要在函数内部修改调用方的数据：
- 使用 `ptr<T>`：轻量，但要注意生命周期；字段用 `->` 访问
- 使用 `Ptr<T>`：更安全，自动管理生命周期；字段用 `.` 访问

```prajna
struct Counter { v: i64; }

func IncRaw(c: ptr<Counter>) { c->v = c->v + 1i64; }
func IncSmart(c: Ptr<Counter>) { c.v = c.v + 1i64; }

func Main(){
    var r = ptr<Counter>::New(); r->v = 0i64;
    IncRaw(r); r->v.PrintLine(); r.Free();

    var s = Ptr<Counter>::New(); s.v = 0i64;
    IncSmart(s); s.v.PrintLine();
}
```

## 5. 常用 API 速览（节选）

- `ptr<T>::Allocate(n)` / `ptr<T>::New()` / `Free()` / `Null()` / `IsNull()` / `IsValid()` / `ToInt64()`
- `Ptr<T>::New()` / `ReferenceCount()` / `IsValid()` / `As<Interface>()` / `ToUndef()` / `FromUndef()`（高级，用于跨类型/接口桥接）

## 6. 常见坑与修复

- 忘记释放 `ptr<T>`（内存泄漏）
  - 修复：使用 `Free()`；或改用 `Ptr<T>` 由引用计数托管
- 释放后继续使用（悬垂指针）
  - 修复：`Free()` 后立即判空；避免持有过期引用
- 错用访问符
  - 约定：`ptr<T>` 用 `->` 访问字段；`Ptr<T>` 用 `.` 访问字段（与项目示例一致）
- 循环引用（两个 `Ptr` 互相持有）
  - 修复：一侧改用 `WeakPtr<T>` 打断循环
- 不必要的 `Ptr<T>` 滥用导致开销升高
  - 修复：短生命周期、局部临时数据可用值语义/`ptr<T>`

## 7. 练习

1) 定义 `struct Node{ v:i64; }`，用 `ptr<Node>` 创建节点并把 `v` 设为 `123`，写 `func BumpRaw(n: ptr<Node>)` 将其加一，验证调用方被修改。
2) 定义 `struct Buf{ len:i64; }`，用 `Ptr<Buf>` 创建并实现 `Grow(b: Ptr<Buf>, delta:i64)`，多次调用后打印 `len`；比较 `ReferenceCount()` 的变化。

——

