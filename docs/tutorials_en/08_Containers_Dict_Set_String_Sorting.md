# Containers II: Dict, Set, String, and Sorting (Beginner-Friendly)

We introduce dictionary-like containers and sets, the `String` type, and sorting across containers.

## 1. ListDict<Key, Value>

Linked-list-backed dictionary; simple inserts; lookup by traversal.

```prajna
var d: ListDict<i64, i64>;
d.Insert(1i64, 10i64); d.Insert(2i64, 20i64);
d.Get(1i64).PrintLine(); // 10
```
Safe access and removal helpers:
```prajna
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
  var prev = dict->_key_value_list._head; var cur = prev->next;
  while (cur != dict->_key_value_list._end) {
    if (cur->value.key == key) { prev->next = cur->next; cur->next->prev = prev; cur.Free(); return true; }
    prev = cur; cur = cur->next;
  }
  return false;
}
```
Iterate all pairs:
```prajna
var node = d._key_value_list.Begin();
while (node != d._key_value_list.End()) { node->value.key.Print(); " => ".Print(); node->value.value.PrintLine(); node = node->next; }
```

## 2. HashDict<Key, Value>

Bucketed hash table (current hash maps integer keys to buckets).

```prajna
var h: HashDict<i64, String>; h.__initialize__();
h.Insert_node(100i64, String::__from_char_ptr("Alice\0")); (h.Get(100i64)).PrintLine();
h.Remove(100i64); (h.IsEmpty()).PrintLine();
```
Iterate buckets:
```prajna
for i in 0 to h.size {
  var node = h.buckets[i];
  while (!node.IsNull()) { (*node).key.Print(); " => ".Print(); (*node).value.PrintLine(); node = (*node).next; }
}
```
Safe get:
```prajna
func TryGet(h: HashDict<i64, String>, key: i64, out: ptr<String>)->bool {
  var index = h.Hash_index(key); var node = h.buckets[index];
  while (!node.IsNull()) { if ((*node).key == key) { *out = (*node).value; return true; } node = (*node).next; }
  return false;
}
```
Note: for non-integer keys, design a mapping to `i64` first.

## 3. Set<T>

Set built on `HashDict<T, bool>`.

```prajna
var s: Set<i64>; s.__initialize__(); s.Add(3i64);
s.Contains(3i64).PrintLine(); s.Remove(3i64); (s.IsEmpty()).PrintLine();

var a: Set<i64>; a.__initialize__(); a.Add(1i64); a.Add(2i64);
var b: Set<i64>; b.__initialize__(); b.Add(2i64); b.Add(3i64);
var u = a.Union(b); var inter = a.Intersection(b); var diff = a.Difference(b);
for i in 0 to u.dict.size { var node = u.dict.buckets[i]; while (!node.IsNull()) { (*node).key.PrintLine(); node = (*node).next; } }
```

## 4. String

Mutable string with trailing `\0` for C interop; `Length()` excludes the null terminator.

```prajna
var s = String::Create(2i64); s.Length().PrintLine();
var s1 = String::__from_char_ptr("Hi\0"); s1.Print(); s1.PrintLine();
var s2 = String::__from_char_ptr("Prajna\0"); var s3 = s1 + s2; s3.PrintLine();
(s1 == String::__from_char_ptr("Hi\0")).PrintLine(); (s1 != s2).PrintLine();

s1.Push('!'); s1[s1.Length() - 1].PrintLine(); s1.Pop(); s1.Resize(10i64);
var cptr = s1.CStringRawPointer();
```

## 5. Sorting (Sortable)

Insertion sort across arrays/dynamic arrays/lists:
```prajna
var arr: Array<i64, 5> = [3,1,4,1,5];
"before:".PrintLine(); for i in 0 to 5 { arr[i].PrintLine(); }
arr.Sort();
"after:".PrintLine(); for i in 0 to 5 { arr[i].PrintLine(); }
```

## 6. Pitfalls
- `Get` on missing key: use `TryGet` or provide default
- HashDict assumes integer keys by default: map other keys first
- Set ignores duplicates by design
- Char vs String confusion
- Verify after sorting

## 7. Exercises
1) Store three names in `ListDict<i64, String>`, demonstrate safe retrieval.
2) Use `Set<i64>` to show union/intersection/difference and print elements.
3) Implement `Join(words: DynamicArray<String>, sep: String)->String` and sort+print results.
