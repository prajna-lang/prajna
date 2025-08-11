# GPU Programming (Unified API, Beginner-Friendly)

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
