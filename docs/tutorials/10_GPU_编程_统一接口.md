# GPU 编程总览（统一接口）

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
