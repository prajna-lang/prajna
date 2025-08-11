# Performance, Debugging, and FAQ (Beginner-Friendly)

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
