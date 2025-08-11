# 容器 II：字典、Set、字符串与排序

本篇介绍字典与集合两类关联容器，以及字符串 `String` 的常用操作，并演示如何对多种容器进行排序。

你将学到：
- `ListDict<Key, Value>`：基于链表的字典，易实现、适合小规模数据
- `HashDict<Key, Value>` 与 `Set<T>`：基于哈希桶，插入/查询更高效（当前适用于整数键）
- `String`：构造、长度、拼接、打印、与 C 字符串互转
- `Sortable`：为数组、动态数组、链表等提供排序能力
- 常见坑与练习

## 1. 字典：ListDict<Key, Value>

定义：使用链表存储键值对，按需插入到链表头，查找通过遍历链表完成。适合条目较少、实现简单明确的场景。

- 插入 / 获取 / 更新 / 删除（手动）
```prajna
var d: ListDict<i64, i64>;

d.Insert(1i64, 10i64);
d.Insert(2i64, 20i64);

// 获取（键不存在会报错）
d.Get(1i64).PrintLine(); // 10

// “更新”可用再次插入 + 自定义去重策略，或先删除再插入
// 这里演示安全获取与安全删除：

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
    var prev = dict->_key_value_list._head;
    var cur = prev->next;
    while (cur != dict->_key_value_list._end) {
        if (cur->value.key == key) {
            // 链表移除当前节点
            prev->next = cur->next; cur->next->prev = prev; cur.Free();
            return true;
        }
        prev = cur; cur = cur->next;
    }
    return false;
}
```

- 遍历全部键值对
```prajna
var node = d._key_value_list.Begin();
while (node != d._key_value_list.End()) {
    node->value.key.Print(); " => ".Print(); node->value.value.PrintLine();
    node = node->next;
}
```

时间复杂度（近似）：查找 O(n)，插入头部 O(1)。

## 2. 哈希字典：HashDict<Key, Value>

定义：使用“多个桶 + 每个桶一条链”的方式存储。查找流程：先对键做哈希得到桶索引，再在该桶的链表中查找。当前实现将键转为 `i64` 后取模，因此适合“整数键”。

- 初始化 / 插入 / 获取 / 删除 / 判空
```prajna
var h: HashDict<i64, String>;
h.__initialize__();           // 必须初始化

h.Insert_node(100i64, String::__from_char_ptr("Alice\0"));
(h.Get(100i64)).PrintLine();

h.Remove(100i64);
(h.IsEmpty()).PrintLine();
```

- 遍历全部键值对（按桶扫描）
```prajna
for i in 0 to h.size {
    var node = h.buckets[i];
    while (!node.IsNull()) {
        (*node).key.Print(); " => ".Print(); (*node).value.PrintLine();
        node = (*node).next;
    }
}
```

- “安全获取”示例
```prajna
func TryGet(h: HashDict<i64, String>, key: i64, out: ptr<String>)->bool {
    var index = h.Hash_index(key);
    var node = h.buckets[index];
    while (!node.IsNull()) {
        if ((*node).key == key) { *out = (*node).value; return true; }
        node = (*node).next;
    }
    return false;
}
```

提示：若需要非整数键（如 `String`、结构体）作为 key，需要自行定义“把 key 映射到 `i64` 的规则”，再将其作为 HashDict 的 KeyType；或在上层封装一层 `DictString`，内部用你定义的哈希把字符串映射到 `i64` 再转委到 `HashDict<i64, V>`。

时间复杂度（平均）：插入/查找 O(1)，最坏 O(n)。当前实现包含装载因子阈值字段但未启用自动扩容，数据规模大时建议手动迁移到更大的 `size`。

## 3. 集合：Set<T>

定义：在哈希字典之上，仅存储“元素是否存在”。提供是否包含、并集/交集/差集等操作。

- 基本操作
```prajna
var s: Set<i64>;
s.__initialize__();

s.Add(3i64);
s.Contains(3i64).PrintLine(); // true
s.Remove(3i64);
(s.IsEmpty()).PrintLine();
```

- 集合运算（并、交、差）与遍历
```prajna
var a: Set<i64>; a.__initialize__(); a.Add(1i64); a.Add(2i64);
var b: Set<i64>; b.__initialize__(); b.Add(2i64); b.Add(3i64);

var u = a.Union(b);            // {1,2,3}
var inter = a.Intersection(b); // {2}
var diff = a.Difference(b);    // {1}

// 遍历集合（按底层 buckets 扫描）
for i in 0 to u.dict.size {
    var node = u.dict.buckets[i];
    while (!node.IsNull()) { (*node).key.PrintLine(); node = (*node).next; }
}
```

注意：重复插入同一元素不会产生重复项。

## 4. 字符串：String

定义：可变长度字符串，内部以字符数组存储，并在末尾维护一个 `\0` 以便与 C 接口互通。`Length()` 返回的是“可视字符数”，不包含末尾 `\0`。

- 构造与长度
```prajna
var s = String::Create(2i64); // 预留长度 2（实际会额外维护结尾 '\0'）
s.Length().PrintLine();
```

- 从 C 字符串转换、打印
```prajna
var s1 = String::__from_char_ptr("Hi\0");
s1.Print(); s1.PrintLine();
```

- 拼接、相等/不等比较
```prajna
var s2 = String::__from_char_ptr("Prajna\0");
var s3 = s1 + s2;   // 连接
s3.PrintLine();
(s1 == String::__from_char_ptr("Hi\0")).PrintLine(); // true
(s1 != s2).PrintLine();                                  // true
```

- 读写字符、Push/Pop 与调整大小
```prajna
s1.Push('!');
s1[s1.Length() - 1].PrintLine(); // 打印最后一个字符
s1.Pop();

s1.Resize(10i64);  // 扩容后末尾仍保持 '\0'
```

- 传递给 C 接口（确保以 `\0` 结尾）
```prajna
var cptr = s1.CStringRawPointer(); // 调用前会确保末尾 '\0'
```

## 5. 排序（Sortable 接口 + 插入排序）

`Sortable` 接口为多种容器提供 `Sort/SortReverse`。内置实现采用插入排序（平均 O(n^2)；对近乎有序数据表现较好；通常是稳定排序）。

- 数组、动态数组、链表
```prajna
var arr: Array<i64, 5> = [3,1,4,1,5];
"before:".PrintLine();
for i in 0 to 5 { arr[i].PrintLine(); }

arr.Sort();
"after:".PrintLine();
for i in 0 to 5 { arr[i].PrintLine(); }

var d = DynamicArray<i64>::Create(0);
d.Push(3i64); d.Push(1i64); d.Push(2i64);
d.SortReverse();

var l: List<i64>;
l.PushBack(3i64); l.PushBack(1i64); l.PushBack(2i64);
l.Sort();
```

提示：海量数据请根据场景实现更高效的排序策略（如快速排序、归并排序），并可仿照 `Sortable` 的接口风格进行扩展。

## 6. 常见坑与修复

- ListDict 的 `Get` 在键不存在时会报错
  - 修复：先安全检查（如上 `TryGet`），或封装带默认值的获取
- HashDict 适用于整数键；非整数键需要映射到整数
  - 修复：为自定义键设计哈希到 `i64` 的规则；或改用 ListDict
- Set 的语义是“存在与否”，重复插入不会增加计数
  - 修复：插入前可先 `Contains` 检查
- String 与 `char` 混用
  - 修复：`'a'` 是 `char`，`"a"` 是 `String`；必要时自行转换
- 排序后未验证
  - 修复：排序完遍历打印或断言，确保顺序正确

## 7. 练习

1) 使用 `ListDict<i64, String>` 存三个人名，演示插入与安全获取（不存在时不崩溃）。
2) 用 `Set<i64>` 演示 `Union/Intersection/Difference`，并打印结果集合的元素。
3) 写一个函数 `Join(words: DynamicArray<String>, sep: String)->String`，把若干字符串用分隔符连接（可用 `+` 拼接），并在 `Main` 中对结果排序后打印。


