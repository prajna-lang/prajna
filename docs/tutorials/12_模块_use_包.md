# 模块与包（给初学者）

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
