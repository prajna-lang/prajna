# Containers I: Array, DynamicArray, List (Beginner-Friendly)

We cover fixed-size arrays `Array<T, N>`, growable arrays `DynamicArray<T>`, and doubly-linked lists `List<T>`.

## 1. Array<T, N>

```prajna
var a: Array<i64, 4> = [1,2,3,4]; a[2].PrintLine(); a[2] = 99i64;
for i in 0 to 4 { a[i].PrintLine(); }
```
Notes: `N` is compile-time; iterate with `for i in 0 to N`.

## 2. DynamicArray<T>

```prajna
var d = DynamicArray<i64>::Create(0); d.Push(10i64); d.Push(20i64); d.Length().PrintLine();
for i in 0 to d.Length() { d[i].PrintLine(); }
d.Resize(5i64); d[4] = 42i64;
```
Use for variable-length sequences; respect index bounds and `Resize` semantics.

## 3. List<T>

```prajna
var l: List<i64>; l.PushBack(1i64); l.PushFront(0i64); l.PushBack(2i64);
l.Length().PrintLine();
l.PopFront(); l.PopBack();
var node = l.Begin(); while (node != l.End()) { node->value.PrintLine(); node = node->next; }
```
Use for frequent end insertions/removals; random access is slow.

## 4. Traversal
- Array/DynamicArray: index loops
- List: walk nodes from `Begin()` to `End()` and advance each iteration

## 5. Sorting (Sortable)

```prajna
var arr: Array<i64, 5> = [3,1,4,1,5]; arr.Sort(); arr.SortReverse();
var d = DynamicArray<i64>::Create(0); d.Push(3i64); d.Push(1i64); d.Push(2i64); d.Sort();
```
Insertion sort is provided for several containers; suitable for small datasets.

## 6. Pitfalls
- Index out of bounds: ensure valid ranges; `Resize` before writing
- List traversal: don’t forget `node = node->next`
- `Length()` is length, not capacity
- Verify sorted order after sort

## 7. Exercises
1) Implement `Sum(a: Array<i64, 4>)->i64` and `SumD(d: DynamicArray<i64>)->i64`.
2) Create `List<i64>`, print, `Sort()`, then print again.
3) Collect `DynamicArray<String>`, sort, print lines.
