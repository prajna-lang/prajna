# 一：运行与模式

本篇讲清楚 Prajna 的四种常用运行方式、适用场景、操作步骤与常见问题：执行 Main、脚本模式（.prajnascript）、REPL、Jupyter。

## 1. 执行包含 Main 的程序

- 适用场景：单文件或项目入口，和 C/C++ 的 main 函数类似
- 要求：源文件内包含 `func Main(){ ... }`
- 命令：
```bash
prajna exe examples/hello_world.prajna
```
- 成功标志：程序输出预期内容，进程正常退出
- 常见问题：
  - 找不到命令 `prajna`：将 Prajna 可执行文件目录加入 PATH，或从构建产物 `build_release/install/bin` 运行
  - 找不到文件：确认相对路径基于当前工作目录；建议在仓库根目录执行命令

## 2. 脚本模式（.prajnascript）

- 适用场景：像脚本一样按行顺序执行一组语句，便于快速演示或批量任务
- 示例：`examples/hello_world.prajnascript`
```prajna
"Hello World\n".PrintLine();
```
- 命令：
```bash
prajna exe examples/hello_world.prajnascript
```
- 特点：无需 `Main`，按文件顺序逐行执行；适合原型/一次性任务
- 常见问题：
  - 语句之间存在依赖时请注意定义顺序；尽量保持脚本幂等

## 3. REPL 交互模式

- 适用场景：交互式实验、调试表达式、学习语言特性
- 命令：
```bash
prajna repl
```
- 基本用法：
```text
"Hello from REPL".PrintLine();
```
- 提示：
  - 可以逐条定义变量/函数并立即调用，快速验证想法
  - 复杂代码建议整理为文件后用 `prajna exe` 运行，便于版本管理
- 常见问题：
  - 终端编码/输入法导致显示异常：优先在系统终端或 VSCode/Cursor 终端中直接运行测试

## 4. 在 Jupyter 中使用 Prajna

- 适用场景：文档化演示、教学、交互探索，适配 `.ipynb`
- 安装内核：
```bash
prajna jupyter --install
```
- 启动后，可打开随仓库提供的示例笔记本：`docs/notebooks/hello_world.ipynb`
- 提示：
  - 建议先确保本地 Jupyter 可用（如安装 `jupyterlab` 或 `notebook`）
  - 如需容器化运行，可参考仓库内 `docs/notebooks/Dockerfile`
- 常见问题：
  - 浏览器打不开或内核不可用：先在命令行确认 `prajna` 命令可用，再确认 Python/Jupyter 环境

## 5. 选择哪种模式？
- 写程序与示例：优先 “执行 Main”
- 快速串行演示或自动化小任务：脚本模式
- 交互试验/调试：REPL
- 教学与可视化文档：Jupyter

## 6. 平台与路径小贴士
- Windows：若使用 VS 工具链构建，需先进入已配置好编译环境的终端（见《指南》“编译与安装”）
- Linux/macOS：如通过源码构建，命令需在 `build_release/install/bin` 在 PATH 后运行
- 相对路径基于当前终端工作目录；建议在仓库根目录下执行示例命令

# 二：变量与类型系统

本篇面向第一次使用 Prajna 的同学，带你快速掌握“如何声明变量、如何选择和识别类型、何时需要显式转换、常见坑怎么避免”。读完你将能独立写出没有类型问题的基础程序。

你将学到：
- 如何声明/初始化变量，何时需要写类型
- Prajna 的强/静态类型规则，以及字面量后缀的用法
- 布尔、比较、字符与字符串的基本使用
- 显式转换（Cast/cast/bit_cast）的差别与使用时机
- 常见报错如何快速修好

提示：文中的示例都可以在 REPL 里一步步跟着敲（`prajna repl`），或写到一个 `.prajna` 文件用 `prajna exe` 运行。

## 1. 声明与初始化

- 使用 `var` 声明变量；可以写上类型，也可以让编译器从初始值“推断”出来。

```prajna
// 显式写类型（推荐初学时多这样写）
var a: i64;
var pi: f32 = 3.1415f32;

// 依赖推断（通过初始值的后缀推断类型）
var b = 10i64;   // 推断为 i64
```

要点：
- 必须先 `var`，再使用该变量
- 赋值/运算时，两边类型必须一致（Prajna 是强类型，不做隐式转换）

## 2. 字面量与类型后缀（非常重要）

给数字加“类型后缀”，编译器才能准确知道它是什么类型：
- 整数：`1i8 / 2i16 / 3i32 / 4i64 / 5i128`
- 无符号：`1u8 / 1u16 / 1u32 / 1u64 / 1u128`
- 浮点：`3.14f16 / 3.14f32 / 3.14f64`

示例：
```prajna
var x = 42i64;   // i64
var y = 2.0f32;  // f32
```

为什么要加后缀？因为 Prajna 不会自动把 `i64` 变 `f32`，也不会把“看起来像整型”的字面量自动套到你期望的类型里。后缀能让“类型”一眼明白，减少出错。

## 3. 强/静态类型：不做隐式转换

- 静态：类型在编译期确定
- 强类型：不同类型不能直接混用，必须显式转换

错误示例（会报错）：
```prajna
var i = 1i64;
var f = 2.0f32;
// i + f // 类型不一致
```
修正方法：把其中一个显式转换到另一个类型。
```prajna
(i.Cast<f32>() + f).PrintLine();
```

## 4. 显式转换：Cast、cast 与 bit_cast

- `.Cast<T>()`：最常用、最安全的值转换（如把 `i64` 转 `f32`）
- `cast<From, To>(value)`：模板形式的值转换，和 `.Cast<T>()` 等价
- `bit_cast<From, To>(value)`：按位“重解释”为另一个类型，要求内存布局/大小兼容（慎用）

示例：
```prajna
var a = 1i64;
var b: f32 = a.Cast<f32>();         // 值转换
var u: u64 = cast<i64, u64>(a);     // 模板写法
// var bad = bit_cast<i64, f32>(a); // 不要随意用 bit_cast，除非你清楚在做什么
```

## 5. 布尔与比较

- 类型：`bool`
- 比较：`==`, `!=`, `<`, `>`, `<=`, `>=`
- 逻辑：`&&`, `||`, `!`

示例：
```prajna
var ok: bool = (3i64 + 2i64) == 5i64;
ok.PrintLine(); // true
```

## 6. 字符与字符串（容易混）

- 字符：单引号，类型是 `char`，如 `'A'`
- 字符串：双引号，类型是 `String`，如 `"Hello"`

示例：
```prajna
var ch: char = 'Z';
var s: String = "Hi";
s.PrintLine();  // Hi
```

注意：`char` 和 `String` 不是一回事，不能直接混用；需要根据语义选择正确的类型。

## 7. 常见基础类型速览

- 有符号整数：`i8/i16/i32/i64/i128`
- 无符号整数：`u8/u16/u32/u64/u128`
- 浮点：`f16/f32/f64`
- 其他：`bool`, `char`, `void`, `undef`
- 复合类型（后续章节）：`Array<T, N>`、`DynamicArray<T>`、`String`、`Ptr<T>`、`Tensor<T,N>` 等

## 8. 综合示例：把整型参与浮点计算

目标：把循环计数（`i64`）换算为秒（`f32`）。
```prajna
func Main(){
    var frames = 120i64;     // 120 帧
    var fps    = 60.0f32;    // 60 帧/秒

    // 错误：frames / fps // 类型不一致
    var seconds = frames.Cast<f32>() / fps; // 2.0f32
    seconds.PrintLine();
}
```

## 9. 常见报错与快速修好

- “类型不匹配/不能从 X 赋值给 Y”
  - 检查两边类型是否一致；用 `.Cast<T>()` 统一
  - 给数字字面量加后缀：如 `10i64`、`3.0f32`
- “使用了未声明的变量”
  - 确保先 `var` 再用；检查作用域
- 字符 vs 字符串
  - `'a'` 是 `char`，`"a"` 是 `String`，选择正确的类型

## 10. 练习（建议在 REPL 完成）

1) 声明一个 `i64` 的变量 `count`，值为 `3`，声明一个 `f32` 的变量 `step`，值为 `0.5`，计算 `count * step`（结果应是 `1.5`）。要求不报类型错误并正确打印。
2) 写一个函数 `IsUpper(ch: char)->bool`，当 `ch` 在 `'A'..'Z'` 之间返回 `true`，否则 `false`；在 `Main` 中测试 `'A'`、`'z'`。

——
到这里，你已经掌握了 Prajna 的变量与类型基础。

# 三：函数、方法与闭包

本篇带你系统掌握 Prajna 的可执行单元：普通函数、结构体方法、静态方法与闭包（匿名函数），并补充高阶用法与常见坑的修复方式。

你将学到：
- 函数签名、返回值与“无返回值”函数
- 结构体方法与 `implement` 块、`this` 的使用
- `@static` 静态方法何时用、怎么用
- 闭包（匿名函数）语法、捕获规则、作为值传递
- 高阶函数：函数作为参数与返回值

建议边看边在 REPL 中尝试（`prajna repl`），或写到 `.prajna` 文件用 `prajna exe` 运行。

## 1. 普通函数

定义：函数是不隶属于任何实例的可重用代码块，无 `this`，形如 `func Name(params)->Return { ... }`，用于计算或组织流程。

- 基本形态：`func Name(params)->ReturnType { ... }`
- 无返回值时可省略 `-> Type`

```prajna
// 有返回值
func Add(a: i64, b: i64)->i64 {
    return a + b;
}

// 无返回值（过程函数）
func PrintSum(a: i64, b: i64) {
    (a + b).PrintLine();
}

func Main(){
    Add(2i64, 3i64).PrintLine();   // 5
    PrintSum(2i64, 3i64);          // 5
}
```

要点：
- 参数与返回值都是强/静态类型，调用时类型必须匹配（可用 `.Cast<T>()` 调整）

## 2. 结构体方法（面向数据的函数）

定义：方法绑定到某个类型（结构体），写在 `implement Type { ... }` 内，方法体用 `this` 访问当前实例字段。

使用 `implement Type { ... }` 给结构体增加方法；方法体内通过 `this` 访问当前实例。

```prajna
struct Point { x: i64; y: i64; }

implement Point {
    func Length2()->i64 {
        return this.x*this.x + this.y*this.y;
    }

    func Move(dx: i64, dy: i64) {
        this.x = this.x + dx;
        this.y = this.y + dy;
    }
}

func Main(){
    var p: Point;
    p.x = 3i64; p.y = 4i64;
    p.Length2().PrintLine(); // 25
    p.Move(1i64, -2i64);
    p.Length2().PrintLine(); //  (4,2) -> 20
}
```

## 3. 静态方法（构造/工厂/工具）

定义：静态方法与“类型”本身关联，用 `@static` 标注，无需实例即可调用；常用于工厂函数、常量创建、与实例无关的工具函数。

使用 `@static` 将方法与类型相关联、而非与实例相关：

```prajna
struct Pair { a: i64; b: i64; }

implement Pair {
    @static
    func Create(a: i64, b: i64)->Pair {
        var p: Pair; p.a = a; p.b = b; return p;
    }
}

func Main(){
    var p = Pair::Create(1i64, 2i64);
    (p.a + p.b).PrintLine();
}
```

## 4. 闭包（匿名函数）

定义：闭包是匿名的“函数值”，语法 `(params)->Ret { ... }` 或 `(){ ... }`，可自动捕获其使用到的外部变量可赋值、传参、返回。

- 语法：`(params)->Ret { ... }` 或 `(){ ... }`（无参）
- 闭包会自动捕获其使用到的外围变量（示例见下）

```prajna
func Main(){
    // 最简单的闭包
    var hello = (){ "Hello".PrintLine(); };
    hello();

    // 有参闭包
    var add = (a: i64, b: i64)->i64 { return a + b; };
    add(2i64, 3i64).PrintLine();

    // 捕获外部变量
    var x = 100i64;
    var capture_add = (v: i64)->i64 { return v + x; };
    capture_add(10i64).PrintLine();
}
```

提示：闭包是“值”，可以赋给变量、作为参数传递、作为返回值返回。

## 5. 高阶函数：函数作参数/返回值

定义：高阶函数是“以函数为参数，或返回一个函数”的函数。结合闭包可表达强大抽象。

你可以编写“接收函数”的函数：

```prajna
// 接受一个一元函数并应用两次
func ApplyTwice(f: (i64)->i64, v: i64)->i64 {
    return f(f(v));
}

func Main(){
    var inc = (x: i64)->i64 { return x + 1i64; };
    ApplyTwice(inc, 10i64).PrintLine(); // 12
}
```

小技巧（此处的“内联”指在调用处直接写出匿名函数，而不是编译优化的 @inline）：
- 代码内联书写：把匿名函数直接写在实参位置，便于就近表达。
  ```prajna
  ApplyTwice((x: i64)->i64 { return x + 1i64; }, 10i64);
  ```
- 与具名变量对照：先赋给变量再传，便于复用或让调用处更简短。
  ```prajna
  var inc = (x: i64)->i64 { return x + 1i64; };
  ApplyTwice(inc, 10i64);
  ```
注意：这里的“内联”不是编译器的函数内联优化；若需建议编译器内联，请对小而高频的函数使用 `@inline` 标注。

## 6. 常见问题与修复

- 参数/返回类型不匹配
  - 使用 `.Cast<T>()` 或为字面量添加后缀（如 `1i64`、`2.0f32`）
- 闭包捕获值类型不一致
  - 统一捕获变量与参数的类型（需要时 `.Cast<T>()`）
- 将“方法”误写成“普通函数”
  - 需要访问 `this` 就把函数写在 `implement Type { ... }` 中

## 7. 练习

1) 为 `Point` 增加方法 `Dot(rhs: Point)->i64` 计算点积，并在 `Main` 中验证 `(1,2)·(3,4)=11`。
2) 写一个函数 `MapAddOne(f: (i64)->i64, v: i64)->i64`，要求在调用前先将 `v` 加一，再把结果交给 `f`，并在 `Main` 中使用闭包测试。

——


# 四：结构体、接口与动态分发

本篇彻底讲清三件事：
- 结构体 struct：如何定义数据、给它加方法（implement 块与 this）
- 接口 interface：只描述行为（函数签名），不关心数据
- 动态分发 Dynamic<Interface>：运行时根据真实类型决定调用哪个实现

读完你将能用 Prajna 写出清晰的面向数据/行为的代码，并正确地在“静态调用”和“动态调用”之间做选择。

建议边看边在 REPL 里尝试（`prajna repl`），或把示例写入 `.prajna` 文件执行。

## 1. 结构体：承载数据的类型

```prajna
struct Point {
    x: i64;
    y: i64;
}
```
- 结构体字段都是强/静态类型
- 字段访问通过 `this`（见下文方法）或点号：`p.x`

## 2. 给结构体“加方法”：implement 与 this

使用 `implement Type { ... }` 给类型增加方法。方法里通过 `this` 访问当前实例。

```prajna
struct Point { x: i64; y: i64; }

implement Point {
    func Length2()->i64 {
        return this.x*this.x + this.y*this.y;
    }

    func Move(dx: i64, dy: i64) {
        this.x = this.x + dx;
        this.y = this.y + dy;
    }
}

func Main(){
    var p: Point; p.x = 3i64; p.y = 4i64;
    p.Length2().PrintLine(); // 25
    p.Move(1i64, -2i64);     // p = (4,2)
    p.Length2().PrintLine(); // 20
}
```
要点：
- 写成“方法”还是“普通函数”？若需要访问 `this` 或更贴合对象语义，就放到 `implement` 里

## 3. 接口：只描述“能做什么”

接口只包含“函数签名”，不包含数据与默认实现。

```prajna
interface Say {
    func Say();
}
```
- 接口定义行为契约：任何类型只要“实现”了这些函数，就可以被当作该接口使用

## 4. 为类型实现接口：implement Interface for Type

```prajna
struct SayHi { name: String; }

implement Say for SayHi {
    func Say(){
        "Hi ".Print(); this.name.PrintLine();
    }
}
```
- 这段代码声明：`SayHi` 提供了 `Say` 接口要求的 `Say()` 行为
- 你可以为不同的类型分别实现同一个接口

完整示例（来自 `examples/interface.prajna` 的思路）：
```prajna
interface Say{ func Say(); }

struct SayHi{ name: String; }
implement Say for SayHi{ func Say(){ "Hi ".Print(); this.name.PrintLine(); } }

struct SayHello{ name: String; }
implement Say for SayHello{ func Say(){ "Hello ".Print(); this.name.PrintLine(); } }
```

## 5. 动态分发：Dynamic<Interface>

有时候我们在编写“上层代码”时，并不知道变量的真实具体类型是谁，但我们知道它“至少”实现了接口 `Say`。这时可以使用 `Dynamic<Interface>` 来进行“动态分发”。

- 将具体类型提升为接口视图：`Ptr<T>::New().As<Interface>() -> Dynamic<Interface>`
- 在 `Dynamic<Interface>` 上调用接口方法时，运行时会选择具体类型的实现

```prajna
func Main(){
    var say_hi = Ptr<SayHi>::New();
    say_hi.name = "Prajna";
    var d: Dynamic<Say> = say_hi.As<Say>();
    d.Say(); // 根据实际类型 SayHi 调用其实现

    var say_hello = Ptr<SayHello>::New();
    say_hello.name = "Paramita";
    d = say_hello.As<Say>();
    d.Say(); // 现在根据 SayHello 的实现调用
}
```

常用操作：
- 判别真实类型：`d.Is<SayHi>()`
- 安全取回具体类型：`var hi = d.Cast<SayHi>();`

```prajna
if (d.Is<SayHi>()) {
    var hi = d.Cast<SayHi>();
    hi.Say();
} else {
    var hello = d.Cast<SayHello>();
    hello.Say();
}
```

小贴士：
- `Dynamic<Interface>` 是一个具体类型，内部存放了接口方法表等信息，用来在运行时决议调用
- 不需要时，优先用静态分发（直接用具体类型的方法），更高效；需要“多态/插件化”时再使用 `Dynamic`

## 6. 选择静态 or 动态？

- 静态（直接调用具体类型方法）：性能更高，编译期确定；适合大多数情况
- 动态（`Dynamic<Interface>`）：更灵活，支持在运行时替换实现；适合策略模式、插件架构、统一处理不同实现等

## 7. 常见问题与修复

- 忘了把实现写成“实现接口”而写到了“实现类型”
  - 记法：给类型加普通方法 → `implement Type { ... }`；给类型实现接口 → `implement Interface for Type { ... }`
- 取回具体类型失败/崩溃
  - 先 `Is<T>()` 再 `Cast<T>()`；不同于 C++，非法转换会直接报错退出
- 动态分发滥用导致性能抖动
  - 能静态就静态；接口用于“需要统一抽象的场景”

## 8. 练习

1) 定义接口 `Area { func Area()->i64; }`，并为 `Rect{w,h}`、`Square{s}` 分别实现；在 `Main` 中把它们放入 `Dynamic<Area>` 列表并逐个打印面积。
2) 为 `Point{x,y}` 实现接口 `Say`，要求打印 `(x,y)`，并与 `SayHi`/`SayHello` 一起验证 `Dynamic<Say>` 的动态调用。

——

# 五：模板与泛型
本篇解释 Prajna 的“泛型”（模板）能力：如何编写对“类型”和“编译期常量”都通用的代码，并结合数组/张量与接口实现给出实战示例。

你将学到：
- 泛型结构体与方法：`template <T>`、`implement Box<T>`
- 泛型函数：以类型参数编写可复用算法
- 模板“常量参数”：如 `Length_`、`Dim` 等编译期整数
- 为“泛型类型的实例”实现接口（多态的组合）
- 类型别名与可读性
- 常见坑与练习

## 1. 泛型结构体与方法

定义：在类型名上引入“类型参数”，使结构体对多种数据类型通用。

```prajna
template <T>
struct Box { value: T; }

// 给泛型结构体实现方法，方法同样依赖类型参数 T
implement Box<T> {
    func Get()->T { return this.value; }
    func Set(v: T) { this.value = v; }
}

func Main(){
    var i: Box<i64>; i.value = 42i64; i.Get().PrintLine();
    var s: Box<String>; s.value = "Hi"; s.Get().PrintLine();
}
```

要点：
- `template <T>` 声明类型参数 T；使用时显式写出具体类型：`Box<i64>`、`Box<String>`
- 在 `implement` 上同样带上 `<T>`，表明这些方法适用于所有 `Box<T>`

## 2. 泛型函数

定义：在函数上引入“类型参数”，让函数能处理不同类型的参数，只要它们满足函数体中使用的操作。

```prajna
template <T>
func Swap(a: ptr<T>, b: ptr<T>) {
    var tmp = *a;
    *a = *b;
    *b = tmp;
}

func Main(){
    var x = 1i64; var y = 2i64; Swap<i64>(&x, &y);
    var s1 = "A"; var s2 = "B"; Swap<String>(&s1, &s2);
}
```

提示：
- 需显式给出类型实参（如 `Swap<i64>`），以保证类型清晰
- 函数体内对 `T` 的操作必须对“替换进来的类型”是合法的（如可赋值）

## 3. 模板“常量参数”（编译期整数）

定义：除“类型参数”外，还可传入编译期整数，常用于“长度/维度”等场景。

```prajna
template <Type, Length_>
struct FixedVec { data: Array<Type, Length_>; }

implement <Type, Length_> FixedVec<Type, Length_> {
    func At(i: i64)->Type { return this.data[i]; }
}

func Main(){
    var v: FixedVec<i64, 4>;
    // v.data 的类型是 Array<i64, 4>
}
```

常见用法：
- 定长数组：`Array<Type, Length_>`（见内置 `_array.prajna`）
- 张量：`Tensor<Type, Dim>`（二维/三维等，见 `builtin_packages/tensor.prajna`）

## 4. 与数组/张量的结合

- 数组：
```prajna
var a: Array<i64, 4> = [1,2,3,4];
a[2].PrintLine();
```
- 张量（主机侧）：
```prajna
var A = Tensor<f32, 2>::Create([2,3]);
A[0,1] = 1.0f32;
```
- GPU 张量（参考 GPU 教程与 `nvgpu/amdgpu`）：
```prajna
var G = A.ToGpu();
var H = G.ToHost();
```

说明：`Array<Type, Length_>` 与 `Tensor<Type, Dim>` 都通过“类型+编译期常量”的模板组合表达数据形状与存储细节。

## 5. 为“泛型类型的实例”实现接口

你可以为“所有匹配模式的泛型类型实例”实现某个接口，这在算法抽象中非常常见。

示例：为 `Array<Type, Length_>`、`DynamicArray<ValueType>` 等实现 `Sortable`（插入排序），见 `builtin_packages/sortable.prajna` 与 `sort.prajna` 的思路：

```prajna
interface Sortable { func Sort(); func SortReverse(); }

template <Type, Length_>
implement Sortable for Array<Type, Length_> {
    func Sort() { /* ...插入排序... */ }
    func SortReverse() { /* ...倒序插入排序... */ }
}
```

意义：
- “对任意元素类型、任意长度”的数组都获得排序能力
- 接口 + 模板让“算法”和“数据结构”可以灵活组合

## 6. 类型别名与可读性

当模板参数较长时，使用 `as` 起别名提升可读性：

```prajna
use ::gpu::Tensor<f32, 2> as GpuMatrixf32;

func Main(){
    var A = GpuMatrixf32::Create([64, 64]);
}
```

## 7. 常见问题与修复

- 未显式写类型实参导致歧义
  - 现象：编译器无法推断或推断错误
  - 修复：显式写出：`Swap<i64>(&x, &y)`
- 在模板常量参数处使用了运行期变量
  - 现象：报“期望编译期常量”
  - 修复：把形状/长度作为模板参数或使用运行时容器（如 `DynamicArray`）
- 为泛型实现接口时遗漏模板参数
  - 现象：实现不生效或匹配不到类型
  - 修复：在 `implement` 与类型位点都带上相同模板参数

## 8. 练习

1) 实现 `template <T> struct Pair{first:T; second:T;}`，补充 `Swap()` 方法交换两项，并在 `Main` 中对 `i64` 与 `String` 验证。
2) 写 `template <T, N> func Sum(a: Array<T, N>)->T`，返回定长数组元素之和（从 `0` 初值累加）。
3) 参考 `sort.prajna` 的思路，为 `DynamicArray<T>` 实现 `Sortable` 接口的 `SortReverse()`。

——

# 六：指针与内存：ptr<T> 与 Ptr<T>

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

# 七：容器 I：Array、DynamicArray、List

本篇介绍三类最常用容器：定长数组 `Array<T, N>`、动态数组 `DynamicArray<T>`、双向链表 `List<T>`。学完即可在 Prajna 中自如地存取、遍历与排序常见集合。

你将学到：
- `Array<T, N>`：编译期长度、索引与遍历
- `DynamicArray<T>`：运行期可变长度、Push/Pop/Resize/Length
- `List<T>`：双向链表、PushFront/Back、PopFront/Back、节点遍历
- 遍历模式与排序（结合 `Sortable`）
- 常见坑与练习

## 1. 定长数组 Array<T, N>

定义：长度 `N` 在编译期固定，元素类型为 `T`（来源：`builtin_packages/_array.prajna`）。

- 初始化与索引
```prajna
var a: Array<i64, 4> = [1,2,3,4];
a[2].PrintLine();      // 3
a[2] = 99i64;
```

- 遍历（使用索引）
```prajna
for i in 0 to 4 {
    a[i].PrintLine();
}
```

提示：`N` 为编译期常量，常用 `for i in 0 to N` 遍历。适合小型、固定尺寸的数据（如小向量、矩阵块等）。

## 2. 动态数组 DynamicArray<T>

定义：运行期可变长度的顺序容器（来源：`builtin_packages/dynamic_array.prajna`）。

- 创建与基本操作
```prajna
var d = DynamicArray<i64>::Create(0); // 初始长度 0
d.Push(10i64);
d.Push(20i64);
d.Length().PrintLine(); // 2
```

- 索引与遍历
```prajna
for i in 0 to d.Length() {
    d[i].PrintLine();
}
```

- 调整长度
```prajna
d.Resize(5i64);      // 可能填充默认值
d[4] = 42i64;
```

适用：需要追加/缩容的顺序数据。注意下标合法性与 `Resize` 的语义（长度变化不等于“容量”，按需增长）。

## 3. 双向链表 List<T>

定义：带头尾哨兵节点的双向链表（来源：`builtin_packages/list.prajna`）。

- 基本操作
```prajna
var l: List<i64>;
l.PushBack(1i64);
l.PushFront(0i64);
l.PushBack(2i64);
```

- 长度与判空（这里提供了 `Length()`）
```prajna
l.Length().PrintLine();
```

- 弹出元素
```prajna
l.PopFront();  // 删除头部元素（确保非空）
l.PopBack();   // 删除尾部元素（确保非空）
```

- 遍历（按节点指针走）
```prajna
var node = l.Begin();
while (node != l.End()) {
    node->value.PrintLine();
    node = node->next;
}
```

适用：频繁在两端插入/删除，或需要链式结构时。单次随机访问比数组慢，遍历按节点前进。

## 4. 遍历模式与注意事项

- Array/DynamicArray：用索引 `for i in 0 to len` 更高效
- List：用节点指针从 `Begin()` 走到 `End()`，不要越界访问
- 修改容器时注意迭代器/指针有效性（尤其对链表）

## 5. 排序（结合 Sortable 接口）

`builtin_packages/sortable.prajna`/`sort.prajna` 为多种容器实现了插入排序：
- `Array<T, N>`、`DynamicArray<T>`、`List<T>`、`Queue<T>`、`Stack<T>` 等均实现了 `Sortable`

- 示例：
```prajna
var arr: Array<i64, 5> = [3,1,4,1,5];
arr.Sort();         // 升序
arr.SortReverse();  // 降序

var d = DynamicArray<i64>::Create(0);
d.Push(3i64); d.Push(1i64); d.Push(2i64);
d.Sort();

a[0].PrintLine();
```

提示：`List<T>` 也实现了 `Sortable`，可直接 `l.Sort()`；插入排序适合小规模数据，海量数据请根据场景自定义更高效算法。

## 6. 常见坑与修复

- 索引越界（数组/动态数组）
  - 修复：严格使用合法范围；动态数组先 `Resize` 再写入
- 链表遍历忘记更新节点
  - 修复：每次循环末尾移动到 `node = node->next;`
- 误以为 `Length()` 是容量
  - 修复：`Length()` 是当前长度；需要更多空间时使用 `Resize` 或 `Push`
- 排序后未验证顺序
  - 修复：排序后用遍历打印或断言检查

## 7. 练习

1) 写 `func Sum(a: Array<i64, 4>)->i64` 与 `func SumD(d: DynamicArray<i64>)->i64`，分别计算和并打印。
2) 创建 `List<i64>`，按顺序插入若干值，分别用遍历打印、`Sort()`、再打印确认顺序正确。
3) 用 `DynamicArray<String>` 收集多行字符串，排序后逐行打印。

——

# 八：容器 II：字典、Set、字符串与排序

本篇介绍字典与集合两类关联容器，以及字符串 `String` 的常用操作，并演示如何对多种容器进行排序。

你将学到：
- `ListDict<Key, Value>`：基于链表的字典，易实现、适合小规模数据
- `HashDict<Key, Value>` 与 `Set<T>`：基于哈希桶，插入/查询更高效（当前适用于整数键）
- `String`：构造、长度、拼接、打印、与 C 字符串互转
- `Sortable`：为数组、动态数组、链表等提供排序能力
- 常见坑与练习

## 1. 字典：ListDict<Key, Value>

定义：使用链表存储键值对，按需插入到链表头，查找通过遍历链表完成。适合条目较少、实现简单明确的场景。

- 插入 / 获取 / 更新 / 删除（手动）
```prajna
var d: ListDict<i64, i64>;

d.Insert(1i64, 10i64);
d.Insert(2i64, 20i64);

// 获取（键不存在会报错）
d.Get(1i64).PrintLine(); // 10

// “更新”可用再次插入 + 自定义去重策略，或先删除再插入
// 这里演示安全获取与安全删除：

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
    var prev = dict->_key_value_list._head;
    var cur = prev->next;
    while (cur != dict->_key_value_list._end) {
        if (cur->value.key == key) {
            // 链表移除当前节点
            prev->next = cur->next; cur->next->prev = prev; cur.Free();
            return true;
        }
        prev = cur; cur = cur->next;
    }
    return false;
}
```

- 遍历全部键值对
```prajna
var node = d._key_value_list.Begin();
while (node != d._key_value_list.End()) {
    node->value.key.Print(); " => ".Print(); node->value.value.PrintLine();
    node = node->next;
}
```

时间复杂度（近似）：查找 O(n)，插入头部 O(1)。

## 2. 哈希字典：HashDict<Key, Value>

定义：使用“多个桶 + 每个桶一条链”的方式存储。查找流程：先对键做哈希得到桶索引，再在该桶的链表中查找。当前实现将键转为 `i64` 后取模，因此适合“整数键”。

- 初始化 / 插入 / 获取 / 删除 / 判空
```prajna
var h: HashDict<i64, String>;
h.__initialize__();           // 必须初始化

h.Insert_node(100i64, String::__from_char_ptr("Alice\0"));
(h.Get(100i64)).PrintLine();

h.Remove(100i64);
(h.IsEmpty()).PrintLine();
```

- 遍历全部键值对（按桶扫描）
```prajna
for i in 0 to h.size {
    var node = h.buckets[i];
    while (!node.IsNull()) {
        (*node).key.Print(); " => ".Print(); (*node).value.PrintLine();
        node = (*node).next;
    }
}
```

- “安全获取”示例
```prajna
func TryGet(h: HashDict<i64, String>, key: i64, out: ptr<String>)->bool {
    var index = h.Hash_index(key);
    var node = h.buckets[index];
    while (!node.IsNull()) {
        if ((*node).key == key) { *out = (*node).value; return true; }
        node = (*node).next;
    }
    return false;
}
```

提示：若需要非整数键（如 `String`、结构体）作为 key，需要自行定义“把 key 映射到 `i64` 的规则”，再将其作为 HashDict 的 KeyType；或在上层封装一层 `DictString`，内部用你定义的哈希把字符串映射到 `i64` 再转委到 `HashDict<i64, V>`。

时间复杂度（平均）：插入/查找 O(1)，最坏 O(n)。当前实现包含装载因子阈值字段但未启用自动扩容，数据规模大时建议手动迁移到更大的 `size`。

## 3. 集合：Set<T>

定义：在哈希字典之上，仅存储“元素是否存在”。提供是否包含、并集/交集/差集等操作。

- 基本操作
```prajna
var s: Set<i64>;
s.__initialize__();

s.Add(3i64);
s.Contains(3i64).PrintLine(); // true
s.Remove(3i64);
(s.IsEmpty()).PrintLine();
```

- 集合运算（并、交、差）与遍历
```prajna
var a: Set<i64>; a.__initialize__(); a.Add(1i64); a.Add(2i64);
var b: Set<i64>; b.__initialize__(); b.Add(2i64); b.Add(3i64);

var u = a.Union(b);            // {1,2,3}
var inter = a.Intersection(b); // {2}
var diff = a.Difference(b);    // {1}

// 遍历集合（按底层 buckets 扫描）
for i in 0 to u.dict.size {
    var node = u.dict.buckets[i];
    while (!node.IsNull()) { (*node).key.PrintLine(); node = (*node).next; }
}
```

注意：重复插入同一元素不会产生重复项。

## 4. 字符串：String

定义：可变长度字符串，内部以字符数组存储，并在末尾维护一个 `\0` 以便与 C 接口互通。`Length()` 返回的是“可视字符数”，不包含末尾 `\0`。

- 构造与长度
```prajna
var s = String::Create(2i64); // 预留长度 2（实际会额外维护结尾 '\0'）
s.Length().PrintLine();
```

- 从 C 字符串转换、打印
```prajna
var s1 = String::__from_char_ptr("Hi\0");
s1.Print(); s1.PrintLine();
```

- 拼接、相等/不等比较
```prajna
var s2 = String::__from_char_ptr("Prajna\0");
var s3 = s1 + s2;   // 连接
s3.PrintLine();
(s1 == String::__from_char_ptr("Hi\0")).PrintLine(); // true
(s1 != s2).PrintLine();                                  // true
```

- 读写字符、Push/Pop 与调整大小
```prajna
s1.Push('!');
s1[s1.Length() - 1].PrintLine(); // 打印最后一个字符
s1.Pop();

s1.Resize(10i64);  // 扩容后末尾仍保持 '\0'
```

- 传递给 C 接口（确保以 `\0` 结尾）
```prajna
var cptr = s1.CStringRawPointer(); // 调用前会确保末尾 '\0'
```

## 5. 排序（Sortable 接口 + 插入排序）

`Sortable` 接口为多种容器提供 `Sort/SortReverse`。内置实现采用插入排序（平均 O(n^2)；对近乎有序数据表现较好；通常是稳定排序）。

- 数组、动态数组、链表
```prajna
var arr: Array<i64, 5> = [3,1,4,1,5];
"before:".PrintLine();
for i in 0 to 5 { arr[i].PrintLine(); }

arr.Sort();
"after:".PrintLine();
for i in 0 to 5 { arr[i].PrintLine(); }

var d = DynamicArray<i64>::Create(0);
d.Push(3i64); d.Push(1i64); d.Push(2i64);
d.SortReverse();

var l: List<i64>;
l.PushBack(3i64); l.PushBack(1i64); l.PushBack(2i64);
l.Sort();
```

提示：海量数据请根据场景实现更高效的排序策略（如快速排序、归并排序），并可仿照 `Sortable` 的接口风格进行扩展。

## 6. 常见坑与修复

- ListDict 的 `Get` 在键不存在时会报错
  - 修复：先安全检查（如上 `TryGet`），或封装带默认值的获取
- HashDict 适用于整数键；非整数键需要映射到整数
  - 修复：为自定义键设计哈希到 `i64` 的规则；或改用 ListDict
- Set 的语义是“存在与否”，重复插入不会增加计数
  - 修复：插入前可先 `Contains` 检查
- String 与 `char` 混用
  - 修复：`'a'` 是 `char`，`"a"` 是 `String`；必要时自行转换
- 排序后未验证
  - 修复：排序完遍历打印或断言，确保顺序正确

## 7. 练习

1) 使用 `ListDict<i64, String>` 存三个人名，演示插入与安全获取（不存在时不崩溃）。
2) 用 `Set<i64>` 演示 `Union/Intersection/Difference`，并打印结果集合的元素。
3) 写一个函数 `Join(words: DynamicArray<String>, sep: String)->String`，把若干字符串用分隔符连接（可用 `+` 拼接），并在 `Main` 中对结果排序后打印。


# 九：张量与数值计算

本篇介绍通用的多维数组容器 `Tensor<Type, Dim>`：如何创建、索引、打印、以及与 GPU 张量互拷。

你将学到：
- `Layout<Dim>`：`shape`（各维长度）与 `stride`（线性步长）的概念与计算
- `Tensor<Type, Dim>` 的创建、索引与打印（索引顺序、下标从 0 开始）
- 主机侧张量与 GPU 侧张量的相互拷贝（入门）
- 初始化与边界的注意事项、内存与生命周期

## 0. 什么是张量？

- 从直觉理解：张量就是“多维数组”，是标量/向量/矩阵的统一概念。
  - 标量（0 维）：`Tensor<T, 0>`（在 Prajna 中通常从 1 维开始更常见）
  - 向量（1 维）：`Tensor<T, 1>`，形状形如 `[N]`
  - 矩阵（2 维）：`Tensor<T, 2>`，形状形如 `[H, W]`
  - 更高维：`Tensor<T, 3>`（如 `[D, H, W]`），`Tensor<T, 4>`（如批量/通道/高/宽）
- 统一数据类型：一个张量内所有元素类型相同（`Type`），例如 `f32`、`i64`。
- 形状（shape）：每一维的长度；维度数（`Dim`）固定在编译期。
- 在 Prajna 中：`Tensor<Type, Dim>` 提供行主序（最后一维连续）存储、0 起始下标、多维索引 `A[i, j, ...]`，适合数值计算、图像/批量数据等场景。

## 1. 形状与步长（Layout）

张量使用 `shape` 表示各维长度，用 `stride` 把多维索引映射到线性下标。约定“最后一维”连续存储（行主序）。

- 计算规则（`Dim` 为维数）：
  - `stride[Dim-1] = 1`
  - `stride[i] = shape[i+1] * stride[i+1]`（从后往前）
- 线性下标：`linear_index = sum(idx[i] * stride[i])`

举例（二维 `[H, W] = [2, 3]`）：
- `stride = [3, 1]`
- 元素 `(0,0)` → 线性 `0*3 + 0*1 = 0`
- 元素 `(0,1)` → 线性 `0*3 + 1*1 = 1`
- 元素 `(1,0)` → 线性 `1*3 + 0*1 = 3`

再举例（三维 `[D, H, W] = [2, 3, 4]`）：
- `stride = [12, 4, 1]`
- 元素 `(d,h,w)` → 线性 `d*12 + h*4 + w`

索引顺序说明：`A[i, j]` 的 `i` 是第 0 维（高 H），`j` 是第 1 维（宽 W），下标均从 0 开始。

## 2. 创建张量与读写元素

- 创建二维 `f32` 张量 `[2,3]`，并写入元素：
```prajna
var A = Tensor<f32, 2>::Create([2,3]);
A[0,1] = 1.0f32;    // 下标 (0,1)
```

- 读取与打印
```prajna
A[0,1].PrintLine();
A.PrintLine();      // 以嵌套 [] 的形式打印整个张量（便于直观查看）
```

注意：`Create` 只分配内存，不保证内容清零；请在使用前显式赋值或初始化。

## 3. 基本操作示例：逐元素赋值与相加

- 逐元素赋值：
```prajna
func FillOnes(mat: Tensor<f32, 2>) {
    var h = mat.Shape()[0];
    var w = mat.Shape()[1];
    for i in 0 to h {
        for j in 0 to w {
            mat[i, j] = 1.0f32;
        }
    }
}
```

- 两个同形状矩阵逐元素相加：
```prajna
func Add2(A: Tensor<f32, 2>, B: Tensor<f32, 2>, C: Tensor<f32, 2>) {
    var h = A.Shape()[0];
    var w = A.Shape()[1];
    for i in 0 to h {
        for j in 0 to w {
            C[i, j] = A[i, j] + B[i, j];
        }
    }
}
```

## 4. 形状、长度与打印

- 形状：
```prajna
var shape = A.Shape();   // 例如 [2,3]
shape[0].PrintLine();    // 高 H
shape[1].PrintLine();    // 宽 W
```

- 打印：`Print()` 与 `PrintLine()` 会把多维内容格式化为 `[...]` 的层级结构（最内层为标量数组），用于调试与检查。

## 5. 与 GPU 张量交互（入门）

当执行 GPU 计算时，需要把主机侧张量拷到显存，计算完成后再拷回主机：

- 主机 -> GPU（需在支持的 GPU 环境，且加载相应平台支持后）：
```prajna
var H = Tensor<f32, 2>::Create([2,3]);
FillOnes(H);
var G = H.ToGpu();            // 上传到显存（NV/AMD 后端会提供实现）
```

- GPU -> 主机：
```prajna
var Back = G.ToHost();        // 从显存下载回主机
Back.PrintLine();
```

说明：
- ToGpu/ToHost 的可用性取决于所选 GPU 平台（例如 NV 或 AMD）；请确保在该平台运行环境中调用
- 主机侧 `Tensor` 的使用方式（创建、索引、打印）与 GPU 侧张量保持一致，便于移植

## 6. 初始化、边界与生命周期

- 初始化：`Create` 仅分配空间，内容未定义。请显式填充（如 `FillOnes`）
- 边界：索引不做自动越界检查；请确保 `0 <= idx[d] < shape[d]`
- 生命周期：张量内部的数据使用智能指针 `Ptr<Type>` 管理；当张量离开作用域、且无其它引用时，内存会被释放

## 7. 实战小练习

1) 创建两个形状为 `[4,4]` 的 `Tensor<f32, 2>`，分别用 `i+j` 与 `i-j` 填充，调用 `Add2` 把结果写入第三个张量并打印。
2) 将一个 `[32,32]` 的主机张量上传到 GPU，做一次简单的核函数运算（可参考 GPU 教程的核函数框架），再下载回主机打印前两行内容。

小提示（性能与选择）：
- 对于非常小且固定尺寸的数据，`Array<T, N>` 更轻量；对可变长或二维以上数据，`Tensor` 更易读易用
- 打印用于调试，非性能路径；数值计算请尽量采用向量化/并行的实现（如 GPU 核函数）

# 十：GPU 编程总览

本篇带你从 0 跑通 Prajna 的 GPU 编程：如何写核函数、如何配置网格/线程块并启动、如何在主机与显存之间拷贝数据。本文使用统一抽象 API（`::gpu::...`），可在不同后端上工作（如 NV/AMD）。

你将学到：
- 前置环境与可用后端
- 统一索引 API 与维度顺序（重要）
- 核函数的定义（`@kernel`/`@target`）与共享内存/同步
- 启动语法：`K<|grid_shape, block_shape|>(...)`
- `Tensor`：主机/显存互拷与 `Synchronize`
- 完整示例：向量加法（Vector Add）
- 常见坑与练习

## 1. 前置环境与后端

- NV 平台（NVIDIA）：需要可用的 CUDA 驱动与设备，目标为 `@target("nvptx")`
- AMD 平台：需要 ROCm/HIP 运行时，目标为 `@target("amdgpu")`
- 请确保你的机器拥有对应 GPU 及驱动；无 GPU 时可先阅读理解 API 与示例

## 2. 统一索引 API 与维度顺序

- 获取线程/块/网格信息：
```prajna
var tid = ::gpu::ThreadIndex();   // [z, y, x]
var bid = ::gpu::BlockIndex();    // [z, y, x]
var bshape = ::gpu::BlockShape(); // [z, y, x]
var gshape = ::gpu::GridShape();  // [z, y, x]
```
- 维度顺序是 `[z, y, x]`（注意与 CUDA 常见写法的顺序不同）。例如二维计算常用 `tid[1]`（y）、`tid[2]`（x）。

## 3. 定义核函数（kernel）

- 用 `@kernel` 标注核函数，用 `@target("nvptx"|"amdgpu")` 指定后端：
```prajna
@kernel
@target("nvptx")
func VecAdd(a: Tensor<f32, 1>, b: Tensor<f32, 1>, c: Tensor<f32, 1>) {
    var tx = ::gpu::ThreadIndex()[2]; // x 维
    var bx = ::gpu::BlockIndex()[2];
    var bsx = ::gpu::BlockShape()[2];
    var i = bx * bsx + tx;  // 线性全局下标

    if (i < a.Shape()[0]) {
        c[i] = a[i] + b[i];
    }
}
```

- 共享内存与同步：
```prajna
@kernel
@target("nvptx")
func ExampleShared(...) {
    @shared
    var tile: Array<f32, 1024>; // 例如 32x32 的共享块
    // ... 写入 tile ...
    ::gpu::BlockSynchronize();  // 同步一个线程块内的线程
    // ... 读取 tile ...
}
```

## 4. 启动语法与维度形状

- 启动核函数使用尖括号管道：`K<|grid_shape, block_shape|>(args...)`
- 形状数组顺序统一为 `[z, y, x]`：
```prajna
var block = [1, 1, 256];   // 256 线程的 1D 线程块（z=1,y=1,x=256）
var n = 1i64 << 20;        // 元素数
var grid  = [1, 1, (n + block[2] - 1) / block[2]]; // 1D 网格

VecAdd<|grid, block|>(A, B, C);
::gpu::Synchronize(); // 等待设备完成（重要）
```

## 5. 主机/显存拷贝（配合 Tensor）

- 在主机上创建张量并填充：
```prajna
var A = Tensor<f32, 1>::Create([n]);
var B = Tensor<f32, 1>::Create([n]);
var C = Tensor<f32, 1>::Create([n]);
for i in 0 to n { A[i] = i.Cast<f32>(); B[i] = 2.0f32; }
```
- 上传到显存、执行、下载回主机：
```prajna
var dA = A.ToGpu();
var dB = B.ToGpu();
var dC = C.ToGpu();

VecAdd<|grid, block|>(dA, dB, dC);
::gpu::Synchronize();

C = dC.ToHost();            // 覆盖主机 C
C[0].PrintLine();           // 0 + 2 = 2
C[12345].PrintLine();       // 12345 + 2 = 12347（示例）
```

说明：`ToGpu/ToHost` 的可用性取决于当前后端是否加载；同步在读取结果前必不可少。

## 6. 完整示例：向量加法（整合以上步骤）

```prajna
use ::gpu::*;

@kernel
@target("nvptx")
func VecAdd(a: Tensor<f32, 1>, b: Tensor<f32, 1>, c: Tensor<f32, 1>) {
    var tx = ThreadIndex()[2];
    var bx = BlockIndex()[2];
    var bsx = BlockShape()[2];
    var i = bx * bsx + tx;
    if (i < a.Shape()[0]) { c[i] = a[i] + b[i]; }
}

func Main(){
    var n = 1i64 << 20;
    var A = Tensor<f32, 1>::Create([n]);
    var B = Tensor<f32, 1>::Create([n]);
    var C = Tensor<f32, 1>::Create([n]);
    for i in 0 to n { A[i] = i.Cast<f32>(); B[i] = 2.0f32; }

    var dA = A.ToGpu(); var dB = B.ToGpu(); var dC = C.ToGpu();

    var block = [1, 1, 256];
    var grid  = [1, 1, (n + block[2] - 1) / block[2]];

    VecAdd<|grid, block|>(dA, dB, dC);
    Synchronize();

    C = dC.ToHost();
    C[0].PrintLine();
    C[123456].PrintLine();
}
```

## 7. 常见坑与修复

- 维度顺序错误：统一使用 `[z, y, x]`，索引用 `[2]` 取 x
- 未同步就读回：在 `ToHost()` 前调用 `::gpu::Synchronize()`
- 索引越界：检查 `i < a.Shape()[0]` 等条件
- 后端不可用：确认 `@target` 与设备/驱动匹配；NV 用 `nvptx`，AMD 用 `amdgpu`
- 块/网格配置不当：保证 `grid * block` 覆盖所有元素，并考虑边界保护

## 8. 练习

1) 将本示例改为二维矩阵逐元素相加（`Tensor<f32, 2>`），要求正确使用 `[z,y,x]` 索引。
2) 在核函数中加入共享内存 tile，按块读取后再写回，实践 `@shared` 与 `BlockSynchronize()` 的用法。

# 十一：C 库绑定与动态链接
本篇讲清如何在 Prajna 中调用现有的 C 动态库函数：把库加载进来（动态链接）、声明要用的符号（函数/变量）、完成类型与字符串的互操作，并给出常见排错方法。

你将学到：
- 动态库放在哪里、程序如何找到它（不同系统的搜索路径）
- `#link("libname")` 的作用与多库加载
- 用 `@extern` 绑定 C 函数到 Prajna
- C 与 Prajna 的基础类型对应关系
- C 字符串与 Prajna `String` 互操作
- 例子与常见错误排查

## 1. 前置条件与库搜索路径

- 需要一个已编译好的动态库（例如 `libzmq.so`/`libzmq.dylib`/`libzmq.dll`）
- 程序如何找到库（至少满足其一）：
  - 把动态库放在系统搜索路径内
  - 或把库所在目录加入环境变量：
    - Windows：`PATH`
    - Linux：`LD_LIBRARY_PATH`
    - macOS：`DYLD_LIBRARY_PATH`
  - 或与可执行文件位于同一目录（具体取决于系统/加载器）

命名提示：
- Linux：通常以 `libXxx.so` 命名
- macOS：通常以 `libXxx.dylib` 命名
- Windows：通常以 `Xxx.dll` 命名

## 2. 加载库：`#link("libname")`

在源文件中写入：
```prajna
#link("libzmq")
```
含义：将名为 `libzmq` 的动态库加载到当前 JIT 引擎，并把其导出的符号加入可解析范围。可以写多次加载多个库：
```prajna
#link("libzmq")
#link("anotherlib")
```

注意：这里写的是“库名”，而不是文件全名；底层会按平台补全前后缀并搜索（前提是库在可搜索路径中）。

## 3. 绑定 C 函数：`@extern`

用 `@extern` 声明需要使用的 C 符号（函数/变量）。示例：
```prajna
@extern func zmq_errno()->i32;             // 无参，返回 i32
@extern func zmq_version(major: ptr<i32>, minor: ptr<i32>, patch: ptr<i32>);
```

使用时就像本地函数一样调用：
```prajna
func Main(){
    var major: i32; var minor: i32; var patch: i32;
    zmq_version(&major, &minor, &patch);
    major.Print(); ".".Print(); minor.Print(); ".".Print(); patch.PrintLine();
}
```

## 4. 常见 C ↔ Prajna 类型对应

- `int` → `i32`
- `long long` → `i64`
- `float` → `f32`
- `double` → `f64`
- `char*` → `ptr<char>`（C 字符串）
- `void*` → `ptr<undef>`（无具体类型的指针）
- `size_t` → 通常为 `i64`（在 64 位平台）；请以你的目标平台为准
- 结构体指针 → `ptr<MyStruct>`（按需在 Prajna 侧定义镜像结构体）

关键点：
- 函数签名必须“按 ABI 精确匹配”，否则会崩溃或得到错误结果
- 指针参数代表“可被修改”的输出/输入输出；要传入地址（`&var`）

## 5. 字符串互操作

- 从 C `char*` 转为 Prajna `String`：
```prajna
var s = String::__from_char_ptr(cstr_ptr);
s.PrintLine();
```
- 从 Prajna `String` 转为 C `char*`：
```prajna
var cptr = s.CStringRawPointer(); // 内部会在末尾补 '\0'
```

## 6. 最小可运行示例

以 ZeroMQ 里两个简单符号为例（需已安装该库且可被找到）：
```prajna
#link("libzmq")
@extern func zmq_errno()->i32;
@extern func zmq_strerror(errnum: i32)->ptr<char>;

func Main(){
    var err = zmq_errno();
    var msg = zmq_strerror(err);
    String::__from_char_ptr(msg).PrintLine();
}
```
如库不可用，可将示例改为你本机有的库与符号，例如一个自编译的 `mylib`：
```prajna
#link("mylib")
@extern func add_i64(a: i64, b: i64)->i64;

func Main(){
    add_i64(2i64, 40i64).PrintLine(); // 42
}
```

## 7. 常见错误与排查

- 找不到库（加载失败）
  - 检查库是否存在；把所在目录加入 `PATH/LD_LIBRARY_PATH/DYLD_LIBRARY_PATH`
  - 库名不对：去掉前缀 `lib` 与后缀 `.so/.dylib/.dll` 后再写入 `#link("...")`
- 找不到符号
  - 确认库确实导出了该函数（用 `nm/objdump/dumpbin` 检查）
  - 名字修饰/调用约定不一致：确保使用 C 接口导出（`extern "C"`）且签名匹配
- 调用崩溃/结果异常
  - 函数签名（参数与返回值）是否完全一致；指针是否有效；是否传对了地址
  - 32/64 位不一致；平台 ABI 不匹配
- 字符串乱码
  - 确保传入/返回的是以 `\0` 结尾的 C 字符串；注意编码（UTF-8/GBK 等）

## 8. 练习

1) 选择一个已安装库（或自编译动态库），绑定 2~3 个函数，打印它们的返回值或字符串信息。
2) 绑定一个带“输出指针参数”的函数（如写入版本号），正确传入地址并打印结果。
3) 把 `String` 作为输入传给 C 函数（先 `CStringRawPointer()`），让 C 函数把该字符串打印出来或处理后返回。

# 十二：模块与包

本篇讲清 Prajna 的“模块/包”与 `use` 导入：如何组织多文件代码、如何导入子模块、如何用别名提升可读性。读完即可按照项目结构把代码拆分到不同文件并正确引用。

你将学到：
- 模块与包的基本概念
- `use` 导入语法与别名 `as`
- 子模块的路径引用与目录布局
- 一个最小可运行的包结构示例

## 1. 基本概念

- 模块：一个 `.prajna` 源文件即可视为一个模块
- 包：由若干模块组成的目录（可包含子目录作为子包/子模块集合）
- 顶层命名空间使用 `::` 访问；同一工程内按相对层级组织

## 2. use 导入与别名

- 导入整个模块/命名空间：
```prajna
use ::gpu::*;                     // 导入 gpu 下的符号
```

- 导入具体符号并起别名：
```prajna
use ::gpu::Tensor<f32, 2> as GpuMatrixf32; // 给长类型起短名
```

说明：`as` 仅改变“本文件内”的引用名称，不改变原符号的真实名称。

## 3. 子模块路径与目录布局

示例目录（来自工程中的实际风格）：
```
examples/package_test/
  mod_a/
    mod_aa.prajna
  mod_b.prajna
  mod_c/
    mod_ca.prajna
```
- `mod_a/mod_aa.prajna` 定义了一个函数：
```prajna
func Test() { "mod_a::mod_aa::test".PrintLine(); }
```
- `mod_c/mod_ca.prajna`：
```prajna
func Test() { "mod_c::mod_ca::test".PrintLine(); }
```
- `mod_b.prajna`：
```prajna
func Test_b() {}
```

在其它文件中引用时，按“目录结构 → 命名空间层级”来写路径，例如：
```prajna
use ::examples::package_test::mod_a::mod_aa::*; // 导入 mod_aa 下所有符号
use ::examples::package_test::mod_c::mod_ca::Test as TestC; // 仅导入 Test 并起别名

func Main(){
    Test();   // 来自 mod_aa
    TestC();  // 来自 mod_ca，已起别名
}
```

说明：最顶层可根据你的工程作为根（例如把项目根目录视为 `::`），然后用子目录名逐层访问到具体模块文件。

## 4. 最小可运行示例

假设你在一个新目录 `my_pkg/` 下放两个模块：
```
my_pkg/
  math.prajna
  io.prajna
```
- `math.prajna`：
```prajna
func Add(a: i64, b: i64)->i64 { return a + b; }
```
- `io.prajna`：
```prajna
func PrintNum(x: i64) { x.PrintLine(); }
```
- 任意位置创建主程序 `main.prajna`：
```prajna
use ::my_pkg::math::*;
use ::my_pkg::io::PrintNum;

func Main(){
    var r = Add(20i64, 22i64);
    PrintNum(r);
}
```
运行：
```bash
prajna exe main.prajna
```

## 5. 小贴士与常见坑

- 路径与命名空间一一对应：目录名/文件名要清晰、稳定
- 起别名提升可读性：长类型/长路径可用 `as` 简化
- 同名符号冲突：用全路径（`::a::b::C`）或 `as` 区分
- 模块拆分策略：把“独立职责”的代码放入独立模块，主程序仅 `use` 需要的部分

## 6. 练习

1) 以你的工程为根，创建 `utils/strings.prajna` 与 `utils/math.prajna`，分别实现字符串拼接与整数加法，在 `Main` 中通过 `use` 调用。
2) 为 `Tensor<f32, 2>` 起别名，例如 `use ::gpu::Tensor<f32, 2> as Mat;`，在 `Main` 中创建矩阵并打印形状。

# 十三：IO 与时间（chrono）

本篇介绍最常用的输入输出与计时：如何打印文本、读取一行输入、把输入解析为数字，如何测量一段代码的耗时、以及让程序暂停一会儿。

你将学到：
- 打印与换行：`Print` / `PrintLine`
- 读取输入：`io::Input()` 返回一行 `String`
- 基础解析：把 `String` 转为数字（思路示例）
- 计时与睡眠：`chrono::Clock()`、`chrono::Sleep(sec)`
- 小练习：简易秒表

## 1. 打印文本

```prajna
"Hello".Print();
"World".PrintLine();  // 自动带换行
```

## 2. 读取一行输入

- `io::Input()` 从标准输入读取一行，返回 `String`
```prajna
var s = io::Input();
s.PrintLine();
```

- 将输入解析为整数（示例思路：逐字符累加；也可以在项目中封装通用解析函数）
```prajna
func ParseI64(str: String)->i64 {
    var sign = 1i64;
    var i = 0i64;
    if (str.Length()>0 && str[0] == '-') { sign = -1i64; i = 1i64; }

    var v = 0i64;
    while (i < str.Length()) {
        var ch = str[i];
        if (ch<'0' || ch>'9') { break; }
        v = v*10i64 + (cast<char, i64>(ch) - cast<char, i64>('0'));
        i = i + 1i64;
    }
    return sign * v;
}
```

## 3. 计时与睡眠

- `chrono::Clock()` 返回当前时间戳（`f32`，单位秒）；可两次取差来度量耗时
```prajna
var t0 = chrono::Clock();
// ... 执行任务 ...
var t1 = chrono::Clock();
(t1 - t0).PrintLine();
```

- `chrono::Sleep(t)` 让程序睡眠 `t` 秒（`f32`）
```prajna
chrono::Sleep(0.5f32); // 睡 0.5 秒
```

## 4. 小练习：简易秒表

```prajna
func Main(){
    "Press Enter to start...".PrintLine();
    io::Input();
    var t0 = chrono::Clock();

    "Press Enter to stop...".PrintLine();
    io::Input();
    var t1 = chrono::Clock();

    (t1 - t0).PrintLine();
}
```

提示：
- 打印用于交互与调试；性能测试请多次测量取平均值
- 输入解析可根据需要扩展浮点、带空格的列表等


# 十四：序列化与测试

本篇介绍“把值变成可打印字符串”的 `Serializable` 接口、内置可序列化类型的打印能力、如何给自定义类型启用打印，以及用 `@test` 编写最小化测试。

你将学到：
- `Serializable` 接口与 `ToString()` 约定
- 内置类型（整数/浮点/布尔/字符/数组/张量等）的打印规则
- 通过模板启用 `Print/PrintLine`
- 自定义类型的 ToString 实现
- `serialize::ToBytes/FromBytes` 说明（入门）
- `@test` 的用法与断言

## 1. Serializable 接口与 ToString

接口很简单：
```prajna
interface Serializable{
    func ToString()->String;
}
```
任何实现了 `ToString()` 的类型都可视为可序列化，并可用于打印。

## 2. 内置可序列化类型（节选）

系统已为以下类型提供了 `ToString()`（并配套 `Print/PrintLine`）：
- 字符 `char`：单字符
- 布尔 `bool`：`true/false`
- 整数类 `Int<Bits>/Uint<Bits>`：十进制输出（带符号或无符号）
- 浮点 `Float<Bits>`：小数/科学计数法打印
- 定长数组 `Array<Type, Dim>`：形如 `[a,b,c,...]`
- 张量 `Tensor<Type, Dim>`：嵌套 `[...]` 的层级打印（见张量教程）

你可以直接：
```prajna
(123i64).PrintLine();
(3.14f32).PrintLine();
(true).PrintLine();
```

## 3. 通过模板启用 Print/PrintLine

系统模板会为支持 `ToString()` 的类型注入 `Print/PrintLine` 方法：
```prajna
template EnablePrintable <T> {
    implement T {
        func Print()     { this.ToString().Print(); }
        func PrintLine() { this.ToString().PrintLine(); }
    }
}
```
整数/无符号/浮点等族也通过类似模板批量启用。

## 4. 自定义类型实现 ToString

示例：为结构体实现 `ToString()`，即可方便打印：
```prajna
struct Point { x: i64; y: i64; }

implement Serializable for Point {
    func ToString()->String {
        var s = "(";
        s = s + this.x.ToString() + "," + this.y.ToString() + ")";
        return s;
    }
}

func Main(){
    var p: Point; p.x = 3i64; p.y = 4i64;
    p.PrintLine(); // (3,4)
}
```

## 5. 二进制序列化（入门说明）

`serialize` 模块提供了将值转换为二进制字节序列的能力（接口与实现可能随版本演进而更新）。常见形式：
```prajna
var bytes = serialize::ToBytes(x);
var y = serialize::FromBytes<MyType>(bytes);
```
注意：
- 需要确保类型的布局与序列化协议一致
- 不同平台/版本间的兼容性取决于你的协议设计；跨平台请优先使用自描述/文本协议或自行定义稳定格式

## 6. 测试：@test 与断言

写一个最小化测试用例：
```prajna
@test
func MyCase(){
    var a = 2i64 + 2i64;
    test::Assert(a == 4i64);
}
```

断言工具（节选）：
- `test::Assert(condition: bool)`：失败时退出
- `test::AssertWithMessage(cond, msg)`：带消息
- `test::AssertWithPosition(cond, file, line)`：带源位置信息

## 7. 常见坑与修复

- `ToString()` 未实现导致无法打印
  - 修复：为自定义类型实现 `Serializable`；或转为已支持的类型打印
- 浮点打印不符合预期（如非常小的数）
  - 修复：系统会在固定小数与科学计数法间选择；需要特定格式可自行实现 `ToString()`
- 序列化跨版本/跨平台不兼容
  - 修复：定义稳定协议或使用文本/JSON/YAML 等格式
- 测试中断言未生效
  - 修复：确认调用了断言函数；失败后会 `exit(-1)` 终止

## 8. 练习

1) 为 `struct Rect{ w:i64; h:i64; }` 实现 `ToString()`，输出 `Rect(w,h)`，并打印。
2) 写一个测试：断言 `Rect{2,3}` 的字符串为 `Rect(2,3)`。
3) 试着为 `Array<i64, 4>` 生成字节序列并反序列化，验证一致性（思考跨平台大小端等问题）。


# 十五：性能优化、调试与 FAQ

本篇提供一套能立即上手的“性能 + 调试”清单，并收录常见问题。建议先写对、再写快；性能优化以数据结构选择和算法为先。

你将学到：
- CPU 端常用优化要点
- 内存/分配与数据布局
- 用 `chrono::Clock()` 做简单基准测试
- GPU 优化入门（索引/共享内存/拷贝）
- 调试工作流与断言
- 常见问题排查

## 1) CPU 端性能要点

- 选对数据结构：
  - 小且固定长度：`Array<T,N>`
  - 追加/删减较多：`DynamicArray<T>`、`List<T>`（随机访问少）
  - 关联查找：小规模用 `ListDict`，大规模用 `HashDict`
- 避免不必要复制：按需传指针/智能指针，或在循环外预分配缓冲区并复用
- 热路径少用动态分发：在高频循环中优先静态调用，必要时再用 `Dynamic<Interface>`
- 小函数可 `@inline`：
  ```prajna
  @inline
  func Square(x: i64)->i64 { return x * x; }
  ```
- 简化分支与数据依赖：计算顺序尽量线性、连续

## 2) 内存与数据布局

- 连续内存更友好：`Array`/`Tensor` 的连续布局通常优于链表
- 预分配与复用：
  ```prajna
  var d = DynamicArray<i64>::Create(0);
  // 批量使用时，可以先 Resize 到目标长度再写入，减少多次 Push 的扩容成本
  d.Resize(1000i64);
  ```
- 指针选择：
  - `ptr<T>` 轻量但需手工 `Free()`
  - `Ptr<T>` 自动管理生命周期，易于跨函数传递

## 3) 用 chrono 做基准测试（简易）

- 多轮测试、丢弃热身，统计平均耗时：
```prajna
func Benchmark(iter: i64){
    // 热身
    for i in 0 to 10 { /* call your function */ }

    var rounds = iter;
    var t0 = chrono::Clock();
    for i in 0 to rounds { /* call your function */ }
    var t1 = chrono::Clock();

    var avg = (t1 - t0) / rounds.Cast<f32>();
    avg.PrintLine();
}
```
提示：打印不是性能路径，基准中尽量少打印。

## 4) GPU 优化入门

- 维度顺序固定为 `[z, y, x]`，常用 `x` 维做线性索引：
  ```prajna
  var i = BlockIndex()[2]*BlockShape()[2] + ThreadIndex()[2];
  ```
- 合理的块大小：128~256/512 常见，按设备调参；保证 `grid*block` 覆盖全部元素
- 尽量合并全局访问（coalesced）：邻近线程访问邻近地址
- 共享内存复用：按 tile 加载数据，块内 `BlockSynchronize()` 后再计算
- 减少 Host↔Device 拷贝：把拷贝放到循环外，批量执行多个 kernel 后再回传
- 每次核函数后适时 `Synchronize()`；在读回数据或测时前一定要同步

## 5) 调试工作流

- 最小复现：把问题缩成几十行；能在 REPL/小文件重现
- 断言防线：
  ```prajna
  test::Assert(condition);
  test::AssertWithMessage(cond, "bad input");
  ```
- 分治定位：逐步注释代码块/打印中间变量，二分定位问题点
- 检查边界：数组/张量 
  - 索引必须满足 `0 <= idx < shape`
  - 维度顺序是否写反（`[z,y,x]`）
- 指针/资源：
  - `ptr<T>` 是否已 `Free()` 再使用（悬垂）
  - 动态库/文件路径是否在搜索路径

## 6) FAQ（常见问题）

- Q: 动态库找不到？
  - A: 把库目录加入环境变量（Windows: `PATH`，Linux: `LD_LIBRARY_PATH`，macOS: `DYLD_LIBRARY_PATH`），或放到可执行同目录；`#link("libname")` 写库名而非文件名
- Q: `@extern` 调用崩溃？
  - A: 签名必须与 C 函数 ABI 完全一致；指针有效且传址；位宽（32/64）与平台匹配
- Q: GPU 不工作/结果不变？
  - A: 检查 `@target("nvptx"|"amdgpu")`、驱动与设备；确保 `Synchronize()`；索引越界保护 `if (i<...)`
- Q: Host↔Device 拷贝频繁影响性能？
  - A: 移到循环外，批量计算后一次性 `ToHost()`
- Q: 输出乱码或字符串异常？
  - A: C 字符串需以 `\0` 结尾；注意终端编码
- Q: 为什么用了动态分发变慢？
  - A: 热路径优先静态调用；必要时才用 `Dynamic<Interface>`

---
小结：优先选好数据结构与算法；用简单基准量化收益；GPU 侧关注索引、共享内存与拷贝；调试时用断言与最小复现快速定位。
