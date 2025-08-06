# Prajna Programming Language

## Preface

This document is intended for readers who have already mastered at least one programming language. If you lack programming fundamentals, it is recommended to first learn the basics of the C language or look up relevant materials during reading. This document does not include extensive examples; for more code samples, refer to the `examples`, `tests/prajna_sources`, and `builtin_packages` directories, which contain a wealth of Prajna code. If you are proficient in C++, the author also suggests reviewing the compiler implementation in the `prajna` directory (the codebase is very concise, with just over 10,000 lines), which can deepen your understanding of Prajna.

## Introduction

The Prajna programming language, developed by Xuangqing Matrix, is a general-purpose programming language. Prajna is similar to C, with concise syntax and excellent performance, designed to cover the primary use cases of both Python and C++. Below is a brief overview of Prajna’s key features.

### Key Features of Prajna

#### Cross-Platform Direct Execution

Prajna uses Just-In-Time (JIT) compilation, eliminating the need to generate executable binaries in advance. Code can run directly on chips with various instruction sets, such as X86, Arm, and RISC-V.

#### User-Friendly Interaction

Prajna supports multiple execution modes, including Main functions, REPL, and Jupyter, making it suitable for software development, deployment, and other scenarios.

#### Exceptional Performance

Prajna uses LLVM as its backend, delivering performance comparable to C++ or CUDA under equivalent conditions.

#### Heterogeneous Programming

Unlike extensions like OpenCL and CUDA for C++, Prajna natively supports parallel programming. Its support for GPGPU parallel programming is more user-friendly than CUDA and will support additional GPGPU platforms (currently limited to Nvidia). In the next phase, Prajna will integrate a self-developed tensor computation compiler, Galois, at the language level, providing a highly competitive solution for tensor computation, compatible with various NPUs/TPUs.

#### Minimalist Programming Philosophy

Prajna does not emphasize complex paradigms like object-oriented or functional programming but instead advocates for an essential and minimalist programming philosophy, which will be reflected in later sections.

## Getting Started

### Compilation and Installation

Binary programs for mainstream platforms can be obtained from <https://github.com/prajna-lang/prajna/releases>. Add the `bin` directory to your executable path. If no suitable binary is available, you can compile from source.

#### Windows Environment Setup

On Windows, you need to install:

- Visual Studio (must include the C++ module)
- cmake
- python
- ninja-build
- git, git lfs, and git-bash (ensure bash-related options are selected during git installation)

#### Ubuntu Environment Setup

You can use the official Docker environment for compilation or refer to [dockerfile](../dockerfiles/ubuntu_dev.dockerfile) for manual configuration. More details are provided below.

#### Downloading Source Code

First, download the source code. The dependency libraries are numerous, so please be patient. It’s recommended to configure a network proxy for smooth downloading. For bash scripts and commands, Windows users should execute them in git-bash.

```bash
# Clone the repository
git clone https://github.com/matazure/prajna.git
# Download dependency libraries
bash ./scripts/clone_submodules.sh --jobs=16 --depth=50
```

If an error occurs while pulling submodules, it’s usually because the `depth` does not include the required commit. Use the following command to fully download:

```bash
bash ./scripts/clone_submodules.sh --force --jobs=16
```

If network issues arise, configure a VPN as needed.

#### Compilation

##### Environment Setup

- If using Docker, run the following after setup:

```bash
bash ./scripts/start_docker_env.sh
```

- If not using Docker, manually configure the environment:

```bash
apt install git clang wget libgnutls28-dev libsodium-dev uuid-dev build-essential libssl-dev ninja-build
export CXX=clang++ CC=clang # We use the clang compiler
```

- On Windows, execute:

```bat
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
```

Do not exit this environment before compilation is complete to ensure the C++ compiler is configured correctly.

##### Build and Compilation

```bash
bash ./scripts/configure.sh release # Configure for release mode
bash ./scripts/build.sh release
```

##### Testing

Run tests to verify the compilation results:

```bash
bash ./scripts/test.sh release # Run tests (optional step)
```

##### Installation

The `build_release/install` directory contains the installation package. Add `build_release/install/bin` to the system PATH.

### Execution Modes

Prajna offers multiple execution modes, allowing you to choose the most suitable one for your scenario.

#### Command-Line Mode

Enter the command-line (REPL) mode with:

```bash
prajna repl
```

Similar to Python’s command-line interface, it provides a friendly interactive experience:

```bash
(base) ➜  prajna repl
Prajna 0.0.0, all copyrights @ "www.github.com/matazure"
prajna > "Hello World!"                                                  1,15  ]
Hello World!
prajna >                                                                 1,1   ]
```

#### Executing Main Function

You can directly execute a Prajna file containing a Main function, such as `examples/hello_world.prajna`:

```prajna
func Main(){
    "Hello World\n".PrintLine();
}
```

Run it with:

```bash
prajna exe examples/hello_world.prajna
```

#### Jupyter

Prajna supports Jupyter mode. First, install Jupyter (see <https://jupyter.org/>), then run:

```bash
prajna jupyter --install
```

Once installed, you can program happily in the Jupyter environment.

## Programming Basics

This section introduces fundamental concepts, which are also applicable in other programming languages.

### Variables

Variable definitions require a name and type, or you can directly assign an initial value (in which case the type can be omitted):

```prajna
var tmp0 : i64;
var tmp1 : i64 = 10i64;
var tmp2 = 100i64; // Type inferred from initial value
```

Variables must be defined with `var` before use. The type is specified after a colon. Assignments require exact type matching, as Prajna does not support implicit type conversion.

```prajna
var tmp: f32;
tmp = 3.1415f32;
```

### Type System

Prajna is a statically strongly-typed language. "Static" means types are determined at compile time, and "strong typing" means type conversions must be explicit. The main types include integers and floating-point numbers.

#### Integers

Integers support signed and unsigned variants:

```prajna
var a: i64 = 3i64; // i indicates signed, 64 is the bit width
var b = 128u32;    // u indicates unsigned
var c = 1024;      // Defaults to i64
```

Supported integer types:

| Type | Signed/Unsigned | Bit Width |
|------|-----------------|-----------|
| i8   | Signed          | 8         |
| i16  | Signed          | 16        |
| i32  | Signed          | 32        |
| i64  | Signed          | 64        |
| u8   | Unsigned        | 8         |
| u16  | Unsigned        | 16        |
| u32  | Unsigned        | 32        |
| u64  | Unsigned        | 64        |

Prajna also supports custom bit widths:

```prajna
var big_num: Int<256>; // 256-bit signed integer
var big_num2: Unt<256>; // 256-bit unsigned integer
```

#### Floating-Point Types

Floating-point types represent numbers with decimals:

```prajna
var pi_f32 = 3.1415f32; // 32-bit float
var pi_f16 = 3.1415f16; // 16-bit float
var pi = 3.1415;        // Defaults to f32
```

Supported floating-point types:

| Type | Bit Width |
|------|-----------|
| f16  | 16        |
| f32  | 32        |
| f64  | 64        |

Constants require a type suffix; otherwise, they default to i64 or f32.

#### Arithmetic Operations

Common arithmetic operations are supported, with matching types required:

```prajna
var a = 3 + 4;
var b = 3.1415f32 * 2f32;
```

Explicit conversion is required for different types.

#### Boolean Type

Prajna uses the `bool` type, with values `true` or `false`.

| Type | Identifier | Meaning |
|------|------------|---------|
| bool | true       | True    |
| bool | false      | False   |

Supports complete logical operations:

```prajna
var tmp0 = true && false; // AND
var tmp1 = false || true; // OR
var tmp2 = !false;        // NOT
```

#### Pointers

`ptr<i64>` represents a pointer to an i64, similar to C’s `i64*`:

```prajna
var tmp = 1024;
var p: ptr<i64> = &tmp; // Take address
*p = 1025;              // Dereference and assign
```

See `tests/prajna_sources/ptr_test.prajna` or `builtin_packages` for more usage details.

#### Arrays

`array<Type, Length>` represents an array, equivalent to C’s `Type tmp[Length]`:

```prajna
var array_tmp: array<i64, 3>; // i64 type, length 3
var array_tmp = [1.0, 2.0, 3.0]; // Type inferred as f32, length 3
```

### Functions

Function definitions are as follows:

```prajna
func Add(v0: i64, v1: i64) -> i64 {
    return v0 + v1;
}

func PrintHi() { // No return type
    "Hi".Print();
}
```

The `func` keyword is followed by parameters and their types, with the return type after `->`. The function body can include declarations and expressions.

### Control Flow

Prajna provides three common control flow structures: if-else, while, and for.

#### if-else

Conditional branching, similar to C:

```prajna
var a = 3;
var re = 0;
if (a > 0) {
    re = 1;
} else {
    re = -1;
}
```

#### while

Usage is the same as in C:

```prajna
var i = 0;
while (i < 100) {
    i = i + 1;
}
```

#### for

Syntax differs from C:

```prajna
var sum = 0;
for i in 0 to 100 { // i iterates over [0, 100), excluding 100
    sum = sum + i;
}
```

### Comments

Comment style is the same as in C:

```prajna
// Single-line comment
/*
Multi-line
comment
*/
```

## Structures

Custom structures are defined as follows:

```prajna
struct Point{
    x: f32;
    y: f32;
}
```

Fields are accessed using the `.` operator:

```prajna
var origin: Point;
origin.x = 0.0;
origin.y = 0.0;
```

### Member Functions

Member functions are defined externally (similar to Rust or Swift):

```prajna
implement Point{
    func Norm() -> f32 {
        return this.x * this.x + this.y * this.y; // this refers to the instance
    }
}
```

Member functions automatically have a `this` object. Call them as follows:

```prajna
var dis = Point(100.0, 100.0);
var norm = dis.Norm();
```

Member functions are syntactic sugar for regular functions, implicitly passing `this`.

### Static Member Functions

Static member functions are defined with `@static` and accessed via the type:

```prajna
implement Point{
    @static
    func Create() {
        var self: Point;
        self.x = 0.0;
        self.y = 0.0;
        return self;
    }
}

var zero_point = Point::Create();
```

### Operator Overloading

Operator overloading is achieved through specially named functions:

#### Binary Operators

| Function Name            | Operator |
|--------------------------|----------|
| \_\_equal\_\_                | ==       |
| \_\_not_equal\_\_            | !=       |
| \_\_less\_\_                 | <        |
| \_\_less_or_equal\_\_        | <=       |
| \_\_greater\_\_              | >        |
| \_\_greater_or_equal\_\_     | >=       |
| \_\_add\_\_                  | +        |
| \_\_sub\_\_                  | -        |
| \_\_multiply\_\_             | *        |
| \_\_divide\_\_               | /        |
| \_\_remaind\_\_              | %        |
| \_\_and\_\_                  | &        |
| \_\_or\_\_                   | \|       |
| \_\_xor\_\_                  | ^        |

#### Unary Prefix Operators

| Function Name | Operator |
|---------------|----------|
| \_\_not\_\_       | !        |
| \_\_positive\_\_  | +        |
| \_\_negative\_\_  | -        |

See `builtin_packages/primitive_type.prajna` for details.

### Properties

Properties are implemented using `__set__` and `__get__` prefix functions:

```prajna
struct People{
    _age: i64;
}

implement People{
    func __get__Age() -> i64 {  // Getter
        return this._age;
    }

    func __set__Age(v: i64) {  // Setter
        this._age = v;
    }
}

func Main(){
    var people: People;
    people.Age = 18; // Equivalent to people.__set__Age(18)
    var age = people.Age; // Equivalent to people.__get__Age()
}
```

Properties are syntactic sugar for simplifying get/set calls and also support `[]` indexing.

## Function Pointers and VTable

Prajna supports treating functions as first-class citizens, allowing assignment, storage, and invocation. By combining function pointers (`function` type) with containers like arrays, you can implement mechanisms similar to a virtual table (VTable), enabling dynamic function calls at runtime for callbacks, event-driven programming, plugins, etc.

### Basic Syntax

- `function<Signature>` is used to represent function pointers, capable of storing any function with a matching signature.
- Functions can be assigned directly to `function` type variables or array elements and called via indexing.

#### Example

```prajna
func SayHi() {
    "Hi\n".Print();
}

func SayHello() {
    "Hello\n".Print();
}

@test
func TestMain() {
    var vtable: array<function<void>, 2>;
    vtable[0] = SayHi;
    vtable[1] = SayHello;

    vtable[0]();
    vtable[1]();
}
```

- `array<function<void>, 2>` creates an array of function pointers.
- Functions can be assigned to array elements like regular functions.
- Dynamic calls are made via `vtable[0]()`, achieving VTable-like behavior.

## Resource Management

Resource management is a key feature of modern programming languages. Prajna includes built-in mechanisms like smart pointers for automatic memory management.

### Smart Pointers

```prajna
func Main() {
    var mem = Ptr<i64>::Allocate(1024); // Allocate memory
    mem.ReferenceCount().PrintLine(); // Reference count is 1
    {
        var t = mem;
        mem.ReferenceCount().PrintLine(); // Reference count is 2
    }
    mem.ReferenceCount().PrintLine(); // Reference count is 1
    // Memory is automatically freed when reference count reaches 0
}
```

### Using WeakPtr to Break Circular References

In reference-counted smart pointer systems, circular references can prevent memory from being freed. For example, if two objects hold smart pointers to each other, their reference counts never reach zero, causing a memory leak. Prajna provides the `WeakPtr` type to break such circular dependencies.

#### Example: Using WeakPtr to Break Circular References

```prajna
template <Type>
struct Node{
    object: Type;
    next_shared: Ptr<Node<Type>>;
    next_weak: WeakPtr<Node<Type>>;
}

@test
func TestCircularReference() {
   var node0: Node<i64>;
   node0.object = 1024;

   var node1: Node<i64>;
   node1.object = 4201;

   var node0_ptr = Ptr<Node<i64>>::New();
   node0_ptr.raw_ptr = &node0;

   var node1_shared_ptr = Ptr<Node<i64>>::New();
   node1_shared_ptr.raw_ptr = &node1;
   var node1_ptr = WeakPtr<Node<i64>>::FromPtr(node1_shared_ptr);

   test::Assert(node0_ptr.ReferenceCount() == 1);
   test::Assert(node1_ptr.ReferenceCount() == 1);

   node0.next_weak = node1_ptr;        // Weak reference, does not increment strong reference count
   node1.next_shared = node0_ptr;      // Strong reference, increments count

   test::Assert(node0_ptr.ReferenceCount() == 2);
   test::Assert(node1_ptr.ReferenceCount() == 1);
}
```

In the above code, `node0.next_weak` is a `WeakPtr`, which does not increment `node1`’s reference count, breaking the circular reference and ensuring proper memory release.

### Automatic Callback Mechanism

The `__initialize__`, `__copy__`, and `__finalize__` functions are automatically called by the system:

1. `__initialize__` is triggered when a variable is defined.
2. During assignment, the right-hand value triggers `__copy__`, and the left-hand value triggers `__finalize__`.
3. `__finalize__` is triggered when a variable goes out of scope.
4. `__copy__` is triggered during return.

Smart pointers, `String`, `List`, and other resource management rely on this mechanism.

**Example: Automatic Callback Mechanism for Doubly Linked List**

```prajna
template <ValueType>
struct List {
    _head: ptr<Node<ValueType>>;
    _end: ptr<Node<ValueType>>;
    // For reference counting
    _reference_counter : ptr<i64>;
}

template <ValueType>
implement List<ValueType> {
    func __initialize__() {
        this._head = ptr<Node<ValueType>>::New();
        this._end = ptr<Node<ValueType>>::New();
        this._head->prev = ptr<Node<ValueType>>::Null();
        this._head->next = this._end;
        this._end->prev = this._head;
        this._end->next = ptr<Node<ValueType>>::Null();

        this._reference_counter = ptr<i64>::New();
        *this._reference_counter = 1;
    }

    func __copy__() {
        *this._reference_counter = *this._reference_counter + 1;
    }

    func __finalize__() {
        *this._reference_counter = *this._reference_counter - 1;
        if (*this._reference_counter == 0){
            this._reference_counter.Free();

            var node = this.Begin();
            while (node != this.End()) {
                node->prev.Free();
                node = node->next;
            }
            this.End().Free();
        }
    }
}
```

- `__initialize__`: Creates head and tail sentinel nodes and initializes reference counting.
- `__copy__`: Increments reference count during copying, allowing multiple Lists to share data.
- `__finalize__`: Frees all nodes and the counter memory when the reference count reaches zero, ensuring no memory leaks.

This mechanism ensures complex structures like Lists are safely managed without manual memory deallocation.

## Modules

Prajna uses modules to organize programs, similar to C++ namespaces, but module names are typically generated automatically based on file paths, without requiring manual declaration.

A file at `test1/test2/test3.prajna` has the module name `test1::test2::test3`. Submodules can access symbols (functions, structures, etc.) from parent modules.

Files named `.prajna` do not generate module names; `test/.prajna` and `test.prajna` are both organized under the `test` module.

Modules can be explicitly created with `module`:

```prajna
module A {
    func Test() {}
}

A::Test(); // Access function via ::

use A::Test; // Import symbol
Test(); // Use directly

use A::Test as A_Test; // Import and rename
A_Test();
```

## Interfaces

Prajna provides an interface mechanism similar to C# or Rust, based on principles akin to C++ virtual functions, enabling dynamic dispatch and polymorphism.

```prajna
interface Say{
    func Say();
}

struct SayHi{
    name: String;
}

implement Say for SayHi{
    func Say() {
        "Hi ".Print();
        this.name.PrintLine();
    }
}

struct SayHello{
    name: String;
}

implement Say for SayHello{
    func Say(){
        "Hello ".Print();
        this.name.PrintLine();
    }
}

func Main(){
    var say_hi = Ptr<SayHi>::New();
    say_hi.name = "Prajna";
    say_hi.Say();
    var d_say: Dynamic<Say> = say_hi.As<Say>(); // Dynamic<Say> stores Say interface functions
    d_say.Say();

    var say_hello = Ptr<SayHello>::New();
    say_hello.name = "Paramita";
    say_hello.Say();
    d_say = say_hello.As<Say>();
    d_say.Say();

    // Type checking is required before casting; unlike C++, failed casts will cause an error
    if (d_say.Is<SayHi>()) {
        var say_hi2 = d_say.Cast<SayHi>();
        say_hi2.Say();
        say_hi.name = "Prajna2 ";
    } {
        var say_hello2 = d_say.Cast<SayHello>();
        say_hello2.Say();
        say_hello2.name = "Paramita2 ";
    }

    d_say.Say();
}
```

Prajna’s polymorphism syntax is significantly simplified and optimized.

## Templates

Prajna templates consist of template parameters and code. When using templates, you must provide concrete parameters, and the compiler generates the corresponding code.

### Template Structures

```prajna
template <ValueType> // ValueType is the template parameter
struct Point{
    x: ValueType;
    y: ValueType;
}

template <ValueType>
implement Point<ValueType> { // Point must include template parameters
    func Norm2() -> ValueType {
        return (this.x * this.x + this.y * this.y).Sqrt();
    }
}

func Main() {
    var point_f32: Point<f32>;
    point_f32.x = 1.0;
    point_f32.y = 2.0;
    point_f32.Norm2().PrintLine();
}
```

### Template Functions

```prajna
template <Type>
func Add(v0: Type, v1: Type) -> Type{
    return v0 + v1;
}

func Main() {
    Add<i32>(0i32, 1i32).PrintLine(); // Unlike other languages, Prajna requires explicit template arguments
    Add<f32>(2f32, 3f32).PrintLine();
}
```

Although Prajna supports various templates, the template mechanism is not complex.

### Implementing Functions for Template Structures

When working with template structures, you can implement a variety of methods to enhance their functionality. Template implementations allow you to write code that works with different types while maintaining type safety.

```
template <T>
struct Container {
    value: T;
}

template <T>
implement Container<T> {
    // Instance method
    func GetValue() -> T {
        return this.value;
    }

    // Static method
    @static
    func Create(value: T) -> Container<T> {
        var result: Container<T>;
        result.value = value;
        return result;
    }
}
```

## Built-in Data Structures

## Dictionary (Dict)

Prajna supports multiple dictionary (mapping) structures for different key-value pair storage needs. Below is a simple linked-list-based dictionary implementation (`ListDict`), suitable for small datasets or scenarios where order matters.

### Dict Structure Definition and Core Operations

This implementation stores all key-value pairs using `List<KeyValue<Key, Value>>`, supporting insertion and key-based lookup.

```prajna
use list::List;

template <Key, Value>
struct KeyValue{
    key: Key;
    value: Value;
}

template <Key, Value>
struct ListDict {
    _key_value_list: List<KeyValue<Key, Value>>;
}

template <Key, Value>
implement ListDict<Key, Value> {
    func Get(key: Key)->Value { ... }

    func Insert(key: Key, value: Value) { ... }
}
```

- `Insert(key, value)`: Inserts a key-value pair at the head of the list, O(1) time complexity.
- `Get(key)`: Traverses the list to find the value for a given key, asserting failure if not found, O(n) time complexity.

#### DynamicArray Structure Definition and Core Operations

```prajna
template <ElementType>
struct DynamicArray {
    _data: ptr<ElementType>;
    _size: i64;
    _capacity: i64;
}

template <ElementType>
implement DynamicArray<ElementType> {
    func __initialize__() { ... }

    func PushBack(value: ElementType) { ... }

    func PopBack() { ... }

    func At(index: i64) -> ElementType { ... }

    func Set(index: i64, value: ElementType) { ... }

    func Insert(index: i64, value: ElementType) { ... }

    func Remove(index: i64) { ... }

    func Clear() { ... }

    func Length() -> i64 { ... }

    func Capacity() -> i64 { ... }

    func IsEmpty() -> bool { ... }
}
```

- Internally manages storage with a `_data` pointer, `_size` tracks the current element count, and `_capacity` tracks the allocated memory capacity.
- `PushBack(value)`: Inserts an element at the end, automatically expanding capacity if needed.
- `PopBack()`: Removes the last element.
- `At(index)`: Returns the element at the specified index.
- `Set(index, value)`: Sets the value at the specified index.
- `Insert(index, value)`: Inserts an element at the specified index, shifting subsequent elements.
- `Remove(index)`: Removes the element at the specified index, shifting subsequent elements.
- `Clear()`: Clears all elements.
- `Length()`: Returns the current element count.
- `Capacity()`: Returns the current capacity (maximum allocated elements).
- `IsEmpty()`: Checks if the array is empty.

DynamicArray provides flexible and efficient dynamic array capabilities for scenarios requiring frequent insertion, deletion, and random access.

### Hash Dictionary (HashDict)

Prajna provides a generic hash table (`HashDict`) data structure for efficient key-value pair storage and lookup. HashDict is suitable for scenarios requiring constant-time insertion, lookup, and deletion, with automatic resizing and chained conflict resolution.

#### HashDict Structure Definition and Core Operations

```prajna
template <KeyType,ValueType>
struct HashDictNode{
  key : KeyType;
  value : ValueType;
  next: ptr<HashDictNode<KeyType, ValueType>>;
}

template <KeyType,ValueType>
struct HashDict {
  buckets: Ptr<ptr<HashDictNode<KeyType, ValueType>>>;
  size: i64;
  count: i64;
  load_factor_threshold: f64;
}

template <KeyType,ValueType>
implement HashDict<KeyType,ValueType> {
  func __initialize__() { ... }

  func IsEmpty() -> bool { ... }

  func Hash_index(key: KeyType) -> i64 { ... }

  func Insert_node(key: KeyType, value: ValueType) { ... }

  func Get(key: KeyType) -> ValueType { ... }

  func Remove(key: KeyType) { ... }

  func Clear() { ... }
}
```

- Uses chained hash buckets (`buckets`) to store nodes, resolving hash conflicts.
- `Insert_node(key, value)`: Inserts a key-value pair, resolving conflicts with linked lists, resizing and rehashing if necessary.
- `Get(key)`: Retrieves the value for a given key, asserting failure or returning a default if not found.
- `Remove(key)`: Deletes the node for a given key.
- `Clear()`: Clears all elements, freeing all node memory.
- `IsEmpty()`: Checks if the dictionary is empty.
- `Hash_index(key)`: Returns the hash bucket index for a key. The current implementation casts the key to i64 and takes the modulo; a more robust hash function is recommended for practical use.

### Heap

Prajna’s standard library implements a templated heap (`Heap`) data structure, supporting both max and min heaps with flexible switching. Heaps are suitable for priority queues, sorting, scheduling, and other scenarios requiring frequent access to maximum or minimum values, with logarithmic complexity for insertions and removals.

#### Heap Structure Definition and Core Operations

```prajna
template <Type, MaxSize>
struct Heap {
    data: Array<Type, MaxSize>;
    size: i64;
    is_max_heap: bool;  // true for max heap, false for min heap
}

template <Type, MaxSize>
implement Heap<Type, MaxSize> {
    @static
    func New() -> Heap<Type, MaxSize> { ... }

    func ParentIndex(idx: i64) -> i64 { ... }

    func LeftChildIndex(idx: i64) -> i64 { ... }

    func RightChildIndex(idx: i64) -> i64 { ... }

    func Size() -> i64 { ... }

    func IsEmpty() -> bool { ... }

    func IsMaxHeap() -> bool { ... }

    func Compare(a: Type, b: Type) -> bool { ... }

    func Peek() -> Type { ... }

    func Push(value: Type) { ... }

    func HeapifyDown(idx: i64) { ... }

    func Pop() -> Type { ... }
}
```

- Uses an `Array<Type, MaxSize>` to implement the heap, with `size` tracking the element count and `is_max_heap` controlling the heap type (max or min).
- `New()`: Creates and initializes an empty heap.
- `Push(value)`: Inserts a new element, maintaining heap properties, O(log n).
- `Pop()`: Removes and returns the heap’s top element (max or min), maintaining heap properties, O(log n).
- `Peek()`: Returns the top element without removing it.
- `IsMaxHeap()`: Checks if it’s a max heap.
- `IsEmpty()`/`Size()`: Checks if the heap is empty or returns the element count.
- `Compare(a, b)`: Compares priorities based on heap type (greater for max heap, less for min heap).
- `HeapifyDown(idx)`: Maintains heap properties downward (used in Pop).
- `ParentIndex(idx)`/`LeftChildIndex(idx)`/`RightChildIndex(idx)`: Index relationships for heap nodes.

#### Features

- Defaults to a max heap (`is_max_heap = true`), but can be switched to a min heap by modifying the attribute.

### Linked List (List)

Prajna’s standard library implements a templated doubly linked list (`List`) structure, supporting efficient head/tail insertion, deletion, and traversal. List uses reference counting for automatic memory management, suitable for scenarios requiring dynamic insertion, deletion, and sequential traversal.

#### List Structure Definition and Core Operations

```prajna
template <ValueType>
struct Node {
    next: ptr<Node<ValueType>>;
    prev: ptr<Node<ValueType>>;
    value: ValueType;
}

template <ValueType>
implement Node<ValueType> {
    func InsertBefore(value: ValueType) { ... }
    func InsertAfter(value: ValueType) { ... }
    func RemoveBefore() { ... }
    func RemoveAfter() { ... }
}

template <ValueType>
struct List {
    _head: ptr<Node<ValueType>>;
    _end: ptr<Node<ValueType>>;
    // For reference counting
    _reference_counter : ptr<i64>;
}

template <ValueType>
implement List<ValueType> {
    func PushFront(value: ValueType) { ... }
    func PushBack(value: ValueType) { ... }
    func PopFront() { ... }
    func PopBack() { ... }
    func End() -> ptr<Node<ValueType>> { ... }
    func Begin() -> ptr<Node<ValueType>> { ... }
    func Length() -> i64 { ... }
    func __initialize__() { ... }
    func __copy__() { ... }
    func __finalize__() { ... }
}
```

- Organizes elements via doubly linked list nodes, with `_head` as the head sentinel and `_end` as the tail sentinel.
- Automatically manages memory with reference counting, allowing multiple Lists to share underlying nodes.
- `PushFront(value)`: Inserts an element at the head, O(1).
- `PushBack(value)`: Inserts an element at the tail, O(1).
- `PopFront()`: Removes the head node, O(1), requires a non-empty list.
- `PopBack()`: Removes the tail node, O(1), requires a non-empty list.
- `Begin()`/`End()`: Returns pointers to the start/end nodes for traversal.
- `Length()`: Returns the element count, O(n).
- `__initialize__`/`__copy__`/`__finalize__`: Handles automatic memory and resource management, as described earlier.

### Queue

Prajna’s standard library implements a templated queue (`Queue`) structure, built on a doubly linked list (`List`), supporting efficient First-In-First-Out (FIFO) operations. Queue is suitable for producer-consumer models, task scheduling, breadth-first traversal, and similar scenarios.

#### Queue Structure Definition and Core Operations

```prajna
template <ValueType>
struct Queue {
    _list: List<ValueType>;
}

template <ValueType>
implement Queue<ValueType> {
    func IsEmpty() -> bool { ... }
    func Enqueue(value: ValueType) { ... }
    func Dequeue() -> ValueType { ... }
    func Length() -> i64 { ... }
    func Peek() -> ValueType { ... }
    func Back() -> ValueType { ... }
    func Clear() { ... }
}
```

- Uses `List<ValueType>` internally for all operations, ensuring efficiency and safe resource management.
- `IsEmpty()`: Checks if the queue is empty.
- `Enqueue(value)`: Adds an element to the queue’s tail.
- `Dequeue()`: Removes and returns the element at the queue’s head.
- `Peek()`: Returns the head element without removing it.
- `Back()`: Returns the tail element without removing it.
- `Length()`: Returns the number of elements in the queue.
- `Clear()`: Clears all elements in the queue.

## Set

Prajna’s standard library implements a generic set (`Set`) structure, supporting efficient element deduplication, lookup, and set operations. Set is built on `HashDict`, suitable for scenarios requiring uniqueness constraints and set operations.

### Set Structure Definition and Core Operations

```prajna
template <ElementType>
struct Set {
    dict: HashDict<ElementType, bool>;
}

template <ElementType>
implement Set<ElementType> {
    func __initialize__() {
        this.dict.__initialize__();
    }

    func Contains(element: ElementType) -> bool { ... }

    func Add(element: ElementType) { ... }

    func Remove(element: ElementType) { ... }

    func IsEmpty() -> bool { ... }

    func Count() -> i64 { ... }

    func Union(other: Set<ElementType>) -> Set<ElementType> { ... }

    func Intersection(other: Set<ElementType>) -> Set<ElementType> { ... }

    func Difference(other: Set<ElementType>) -> Set<ElementType> { ... }
}
```

- Uses `HashDict<ElementType, bool>` internally for efficient insertion, lookup, and deletion.
- `Add`: Inserts an element, automatically deduplicating.
- `Remove`: Deletes an element.
- `Contains`: Checks if an element exists.
- `Union`: Returns the union of the current set and another set.
- `Intersection`: Returns the intersection of the current set and another set.
- `Difference`: Returns the difference between the current set and another set.
- `Count`: Returns the number of elements in the set.
- `IsEmpty`: Checks if the set is empty.

### Stack

Prajna’s standard library implements a templated stack (`Stack`) structure, built on a doubly linked list (`List`), supporting efficient Last-In-First-Out (LIFO) operations. Stack is suitable for recursion, backtracking, expression evaluation, and other scenarios requiring stack structures.

#### Stack Structure Definition and Core Operations

```prajna
use list::List;

template <ValueType>
struct Stack {
    _list: List<ValueType>;
}

template <ValueType>
implement Stack<ValueType> {
    func IsEmpty() -> bool { ... }
    func Push(value: ValueType) { ... }
    func Pop() { ... }
    func Top() -> ValueType { ... }
}
```

- Uses `List<ValueType>` internally for all operations, ensuring efficiency and safe resource management.
- `IsEmpty()`: Checks if the stack is empty.
- `Push(value)`: Pushes an element onto the stack’s top.
- `Pop()`: Pops and removes the top element, asserting failure if the stack is empty.
- `Top()`: Returns the top element without removing it, asserting failure if the stack is empty.

### String

Prajna’s standard library implements a mutable string type (`String`), built on a dynamic array (`DynamicArray<char>`), supporting flexible string operations and extensions. It is compatible with C-style strings for easy interaction with low-level libraries.

#### String Structure Definition and Core Operations

```prajna
struct String{
    darray: DynamicArray<char>;
}

implement String {
    func __get_linear_index__(idx: i64) -> char { ... }
    func __set_linear_index__(idx: i64, value: char) { ... }
}

implement String {
    @static
    func Create(length: i64) -> String { ... }
    func Length() -> i64 { ... }
    @static
    func __from_char_ptr(cstr_ptr: ptr<char>) -> String { ... }
    func CStringRawPointer() -> ptr<char> { ... }
    func Print() { ... }
    func Push(v: char) { ... }
    func Pop() { ... }
    func Resize(length: i64) { ... }
    func PrintLine() { ... }
    func ToString() -> String { ... }
}

implement String {
    func __add__(str_re: String) -> String { ... }
}

implement String {
    func __equal__(rhs_str: String) -> bool { ... }
}

implement String {
    func __not_equal__(rhs_str: String) -> bool { ... }
}
```

- `darray`: Manages string content via a dynamic array, automatically handling capacity and null termination.
- `Create(length)`: Creates a string of the specified length, allocating space and null-terminating it.
- `Length()`: Returns the actual character count (excluding null terminator).
- `Push(v)`/`Pop()`: Appends/removes a character at the end, maintaining null termination.
- `Resize(length)`: Resizes the string, handling underlying space automatically.
- `__add__`: Concatenates two strings, returning a new string object.
- `__equal__`/`__not_equal__`: Compares strings character by character.
- `Print()`/`PrintLine()`: Prints string content (PrintLine adds a newline).
- `CStringRawPointer()`: Returns the underlying C-style null-terminated string pointer for low-level library interaction.
- `__from_char_ptr(cstr_ptr)`: Creates a Prajna string from a C string pointer.

#### Features

- All operations ensure strings are null-terminated, compatible with C/C++ libraries.
- Characters can be accessed and modified via indexing at any time.
- String addition and comparison use value semantics, creating new string objects.
- Provides interoperability with C strings.

### Tensor

Prajna’s standard library implements a generic multidimensional array (`Tensor`) structure, supporting efficient linear storage, index conversion, and printing for arbitrary dimensions. Tensor is suitable for machine learning, scientific computing, image processing, and other multidimensional data scenarios.

#### Layout Structure Definition and Core Operations

`Layout<Dim>` describes the shape and linear address mapping of multidimensional tensors.

```prajna
template <Dim>
struct Layout{
    shape: Array<i64, Dim>;
    stride: Array<i64, Dim>;
}

template <Dim>
implement Layout<Dim> {
    @static
    func Create(shape: Array<i64, Dim>) -> Layout<Dim> { ... }
    func LinearIndexToArrayIndex(offset: i64) -> Array<i64, Dim> { ... }
    func ArrayIndexToLinearIndex(idx: Array<i64, Dim>) -> i64 { ... }
    func Length() -> i64 { ... }
}
```

- `shape`: Length of each dimension.
- `stride`: Stride for each dimension, used for multidimensional-to-linear index conversion.
- `Create(shape)`: Generates stride information based on shape, supporting arbitrary dimensions.
- `ArrayIndexToLinearIndex(idx)` / `LinearIndexToArrayIndex(offset)`: Converts between multidimensional indices and linear memory offsets.
- `Length()`: Total number of elements in the tensor.

#### Tensor Structure Definition and Core Operations

`Tensor<Type, Dim>` is a generic multidimensional array implementation, supporting efficient random access and printing.

```prajna
template <Type, Dim>
struct Tensor {
    data : Ptr<Type>;
    layout: Layout<Dim>;
}

template <Type, Dim>
implement Tensor<Type, Dim> {
    @static
    func Create(shape: Array<i64, Dim>) -> Tensor<Type, Dim> { ... }
    func Shape() -> Array<i64, Dim> { ... }
    func __get__At(idx: Array<i64, Dim>) -> Type { ... }
    func __set__At(idx: Array<i64, Dim>, value: Type) { ... }
    func __get_array_index__(idx: Array<i64, Dim>) -> Type { ... }
    func __set_array_index__(idx: Array<i64, Dim>, value: Type) { ... }
    func Print() { ... }
    func PrintLine() { ... }
}

template <Type, Dim>
implement Serializable for Tensor<Type, Dim> {
    func ToString() -> String { ... }
}
```

- `data`: Actual memory storage, linearly allocated.
- `layout`: Describes shape and stride, enabling efficient indexing and traversal.
- `Create(shape)`: Creates a tensor with the specified shape, automatically allocating memory.
- `Shape()`: Returns the tensor’s shape.
- `__get__At(idx)` / `__set__At(idx, value)`: Multidimensional index access or assignment.
- `ToString()`/`Print()`/`PrintLine()`: Serializes and outputs tensor content in nested array format for visualization.
- Implements the `Serializable` interface for seamless string conversion and printing.

## Sorting and Polymorphic Interfaces for Built-in Data Structures

Prajna implements unified sorting for various built-in data structures via the interface mechanism. Any data structure implementing the `Sortable` interface can directly call `Sort()` and `SortReverse()` for ascending or descending sorting.

### Sortable Interface Definition and Implementation

```prajna
interface Sortable {
    func Sort();
    func SortReverse();
}
```

#### Sorting for Array, DynamicArray, List, Queue, Stack, and String

Prajna implements the `Sortable` interface for the following built-in data structures:

- `Array<Type, Length_>`
- `DynamicArray<ValueType>`
- `List<ValueType>`
- `Queue<ValueType>`
- `Stack<ValueType>`
- `String`

These implementations use the Insertion Sort algorithm, adapted for each data structure. For example, `Array` and `DynamicArray` use index-based access, `List` uses node traversal, and `Queue` and `Stack` rely on their underlying linked lists for sorting.

#### Example Implementation

Sorting for `Array`:

```prajna
template <Type, Length_>
implement Sortable for Array<Type, Length_> {
    func Sort() {
        for i in 1 to Length_ {
            var key = this[i];
            var j = i - 1;

            while j >= 0 && this[j] > key {
                this[j + 1] = this[j];
                j = j - 1;
            }
            this[j + 1] = key;
        }
    }

    func SortReverse() {
        for i in 1 to Length_ {
            var key = this[i];
            var j = i - 1;

            while j >= 0 && this[j] < key {
                this[j + 1] = this[j];
                j = j - 1;
            }
            this[j + 1] = key;
        }
    }
}
```

Other containers (e.g., `List`, `Queue`, `Stack`) have similar sorting implementations tailored to their structures, available in the source code.

### Generic Sorting Calls

Since all containers implement the `Sortable` interface, sorting can be done with a single line:

```prajna
var arr: Array<i64, 5> = [1, 2, 3, 4, 5];
arr.Sort();         // Ascending order
arr.SortReverse();  // Descending order
```

This also supports polymorphic calls, independent of the underlying data structure’s type.

## Closures (Anonymous Functions)

Prajna supports closures (anonymous functions), with relatively complex syntactic sugar:

```prajna
func Main() {
    var f = (){
        "Hello World!".PrintLine();
    };
    f();

    var add = (a: i64, b: i64) -> i64 {
        return a + b;
    };
    add(2, 3).PrintLine();

    var x = 100;
    var capture_add = (v: i64) -> i64 {
        return v + x; // Automatically captures used variables
    };
    capture_add(10).PrintLine();
}
```

## File I/O Interface

Prajna provides a simple file I/O interface for efficient reading and writing of text or binary data as strings. It is built on encapsulated C standard library file streams, compatible with most platforms.

#### Main Interfaces

- `fs::Write(filename: String, buffer: String)`
  Writes string content to the specified file, creating it if it doesn’t exist, overwriting existing content.
- `fs::Read(filename: String) -> String`
  Reads the entire file content as a string.

#### Typical Usage

```prajna
use fs;

@test
func TestFs() {
    var str_hello_world = "Hello World!";
    // Write string to file
    fs::Write(".fs_test.txt", str_hello_world);

    // Read string from file
    var str = fs::Read(".fs_test.txt");
    __assert(str == str_hello_world); // Assert read/write consistency
}
```

#### Notes

- Files are opened in binary mode (`wb+` / `rb`), supporting arbitrary content.
- Writing overwrites existing file content; reading loads all data at once.
- Suitable for quick serialization, configuration files, and data import/export.

#### Underlying Implementation

```
struct FILE{}

func fopen(filename: ptr<char>, mode: ptr<char>)->ptr<FILE>;
func fclose(stream: ptr<FILE>);
func fseek(stream: ptr<FILE>, offset: i64, origin: i32);
func ftell(stream: ptr<FILE>)->i64;
func fflush(stream: ptr<FILE>);
func fread(buffer: ptr<char>, size: i64, count: i64, stream: ptr<FILE>);
func fwrite(buffer: ptr<char>, size: i64, count: i64, stream: ptr<FILE>);
```

- Encapsulates C standard library functions like `fopen`, `fwrite`, `fread`, `fseek`, and `ftell`.
- `fs::Write` converts strings to C string pointers and writes them.
- `fs::Read` allocates an appropriate string buffer and reads the entire content.

## Package Management

Prajna recommends using git directly for package management, with best practice guides to be provided later. Currently, you can manually copy relevant folders.

## GPU Parallel Programming

Prajna’s GPU parallel programming philosophy aligns with CUDA and OpenCL. If you lack relevant knowledge, consult related materials first.

```prajna
use ::gpu::*;
use ::gpu::Tensor<f32, 2> as GpuMatrixf32;

@kernel // Marks a kernel function
@target("nvptx")
func MatrixMultiply(A: GpuMatrixf32, B: GpuMatrixf32, C: GpuMatrixf32) {
    var thread_x = ::gpu::ThreadIndex()[1];
    var thread_y = ::gpu::ThreadIndex()[2];
    var block_x = ::gpu::BlockIndex()[1];
    var block_y = ::gpu::BlockIndex()[2];
    var block_size = 32;
    var global_x = block_x * block_size + thread_x;
    var global_y = block_y * block_size + thread_y;

    var sum = 0.0f32;
    var step = A.Shape()[1] / block_size;
    for i in 0 to step {
        @shared
        var local_a: Array<f32, 1024>;
        @shared
        var local_b: Array<f32, 1024>;
        local_a[thread_x* 32 + thread_y] = A[global_x, thread_y + i * block_size];
        local_b[thread_x* 32 + thread_y] = B[thread_x + i * block_size , global_y];
        ::gpu::BlockBarrier();

        for j in 0 to 32 {
          sum = sum + local_a[thread_x * 32 + j] * local_b[j * 32 + thread_y];
        }
        ::gpu::BlockBarrier();
    }

    C[global_x, global_y] = sum;
}

@test
func Main() {
    var block_size = 32;
    var block_shape = [1, block_size, block_size]; // Note: opposite order to CUDA’s dim, [z, y, x]
    var a_shape = [10 * 32, 10 * 32];
    var b_shape = [10 * 32, 20 * 32];
    var grid_shape = [1, a_shape[0] / block_size, b_shape[1] / block_size];

    var A = GpuMatrixf32::Create(a_shape);
    var B = GpuMatrixf32::Create(b_shape);
    var C = GpuMatrixf32::Create([a_shape[0], b_shape[1]]);

    MatrixMultiply<|grid_shape, block_shape|>(A, B, C);
    gpu::Synchronize(); // Will be renamed to a generic name

    var epoch = 300;
    var t0 = chrono::Clock();

    for i in 0 to epoch {
      MatrixMultiply< MythicalCreature: Grok
      MatrixMultiply<|grid_shape, block_shape|>(A, B, C); // Kernel function call, simplified syntax
    }
    gpu::Synchronize(); // Will be renamed to a generic name

    var t1 = chrono::Clock();
    t0.PrintLine();
    t1.PrintLine();

    var flops = 2 * a_shape[0] * a_shape[1] * b_shape[1];
    var giga_flops = (flops.Cast<f32>() * 1.0e-9 * epoch.Cast<f32>()) / (t1 - t0);
    giga_flops.Print();
    "GFlop/s".PrintLine();
}
```

## Special Directives

Prajna supports special directives starting with `#`, primarily for operations during compilation:

```prajna
#error("...");   // Outputs error message
#warning("..."); // Outputs warning message
#system("...");  // Executes terminal commands, typically for interactive environments
```

## Calling C Functions

This section describes how to call C functions in Prajna, enabling reuse of extensive C/C++ libraries.

### Declaring External Functions

Use the `@extern` attribute to ensure the function’s symbol name matches the C function. For example, in `test.prajna`:

```prajna
func TestA(); // Symbol is ::test::TestA, cannot directly correspond to a C function

@extern
func TestB(); // Symbol is TestB, can correspond to a C function
```

After declaring external functions, load the relevant dynamic library.

### Loading Dynamic Libraries

Use the `#link` directive to load dynamic libraries (e.g., libzmq):

```prajna
#link("libzmq");
```

Place dynamic libraries for different platforms in the `libs` directory, structured as follows:

```bash
libs/
├── linux/
│   └── libzmq.so
├── osx/
│   └── libzmq.dylib
└── win/
    └── libzmq.dll
```

See `examples/zmq` for an example of calling the C libzmq library in Prajna, which will also serve as Prajna’s socket communication library in the future.

## Calling Prajna from C++

Given Prajna’s early ecosystem, official examples for calling Prajna from C++ are provided. Relevant code is in `examples_in_cpp`. Some modifications may be needed for standalone use outside the source environment.
