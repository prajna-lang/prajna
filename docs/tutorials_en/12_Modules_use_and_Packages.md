# Modules, `use`, and Packages (Beginner-Friendly)

Organize multi-file code, import submodules, and use aliases for readability.

## 1. Concepts
- Module: a `.prajna` file
- Package: a directory of modules (may contain subpackages)
- Use `::` from repo root as the top-level

## 2. `use` and aliases
```prajna
use ::gpu::*; // import symbols under gpu
use ::gpu::Tensor<f32, 2> as GpuMatrixf32; // shorter alias
```

## 3. Submodule paths and layout
Example layout:
```
examples/package_test/
  mod_a/
    mod_aa.prajna
  mod_b.prajna
  mod_c/
    mod_ca.prajna
```
Import by directory path → namespace hierarchy:
```prajna
use ::examples::package_test::mod_a::mod_aa::*;
use ::examples::package_test::mod_c::mod_ca::Test as TestC;
func Main(){ Test(); TestC(); }
```

## 4. Minimal runnable example
```
my_pkg/
  math.prajna
  io.prajna
```
`math.prajna`:
```prajna
func Add(a: i64, b: i64)->i64 { return a + b; }
```
`io.prajna`:
```prajna
func PrintNum(x: i64) { x.PrintLine(); }
```
`main.prajna`:
```prajna
use ::my_pkg::math::*;
use ::my_pkg::io::PrintNum;
func Main(){ var r = Add(20i64, 22i64); PrintNum(r); }
```
Run:
```bash
prajna exe main.prajna
```

## 5. Tips and pitfalls
- Keep directory and namespace in sync
- Use `as` for long paths/types
- Disambiguate conflicts with full paths or `as`
- Split modules by responsibility; main only imports what it needs

## 6. Exercises
1) Create `utils/strings.prajna` and `utils/math.prajna`; import and use.
2) Alias `Tensor<f32, 2>` as `Mat` and print its shape.
