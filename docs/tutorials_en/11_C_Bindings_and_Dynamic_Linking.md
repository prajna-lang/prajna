# C Library Bindings and Dynamic Linking (Beginner-Friendly)

Call existing C dynamic libraries from Prajna: load libraries, bind symbols, map types/strings, and troubleshoot.

## 1. Requirements and search paths
- Provide a dynamic library (`.so/.dylib/.dll`)
- Make it discoverable:
  - Put in system search paths
  - Or add its directory to env vars: Windows `PATH`, Linux `LD_LIBRARY_PATH`, macOS `DYLD_LIBRARY_PATH`
  - Or co-locate with the executable (platform-dependent)

## 2. Load with `#link("libname")`
```prajna
#link("libzmq")
#link("anotherlib")
```
Writes the logical library name; platform fills prefixes/suffixes.

## 3. Bind with `@extern`
```prajna
@extern func zmq_errno()->i32;
@extern func zmq_version(major: ptr<i32>, minor: ptr<i32>, patch: ptr<i32>);
```
Use like normal functions.

## 4. Common C↔Prajna type mapping
- `int`→`i32`, `long long`→`i64`, `float`→`f32`, `double`→`f64`
- `char*`→`ptr<char>`, `void*`→`ptr<undef>`, `size_t`→`i64` (typical on 64-bit)
- Struct pointer→`ptr<MyStruct>`

Signatures must match the ABI exactly.

## 5. String interop
```prajna
var s = String::__from_char_ptr(cstr_ptr);
var cptr = s.CStringRawPointer(); // ensures trailing '\0'
```

## 6. Minimal examples
```prajna
#link("libzmq")
@extern func zmq_errno()->i32;
@extern func zmq_strerror(errnum: i32)->ptr<char>;
func Main(){ var err = zmq_errno(); var msg = zmq_strerror(err); String::__from_char_ptr(msg).PrintLine(); }
```
Custom lib:
```prajna
#link("mylib")
@extern func add_i64(a: i64, b: i64)->i64;
func Main(){ add_i64(2i64, 40i64).PrintLine(); }
```

## 7. Troubleshooting
- Library not found: set search envs; use logical name in `#link`
- Symbol missing: ensure exported and unmangled (`extern "C"`), names/signatures match
- Crashes/wrong results: verify ABI, pointer validity, 32/64-bit match
- Garbled strings: ensure trailing `\0`; consider encoding issues

## 8. Exercises
1) Bind 2–3 functions from an installed/custom lib and print results.
2) Bind a function with output pointer parameters and print values.
3) Pass a Prajna `String` to a C function using `CStringRawPointer()`.
