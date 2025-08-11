# Tensors and Numerics (Beginner-Friendly)

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
