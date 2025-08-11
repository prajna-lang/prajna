# 性能优化、调试与 FAQ（给初学者）

本篇提供一套能立即上手的“性能 + 调试”清单，并收录常见问题。建议先写对、再写快；性能优化以数据结构选择和算法为先。

你将学到：
- CPU 端常用优化要点
- 内存/分配与数据布局
- 用 `chrono::Clock()` 做简单基准测试
- GPU 优化入门（索引/共享内存/拷贝）
- 调试工作流与断言
- 常见问题排查

## 1) CPU 端性能要点

- 选对数据结构：
  - 小且固定长度：`Array<T,N>`
  - 追加/删减较多：`DynamicArray<T>`、`List<T>`（随机访问少）
  - 关联查找：小规模用 `ListDict`，大规模用 `HashDict`
- 避免不必要复制：按需传指针/智能指针，或在循环外预分配缓冲区并复用
- 热路径少用动态分发：在高频循环中优先静态调用，必要时再用 `Dynamic<Interface>`
- 小函数可 `@inline`：
  ```prajna
  @inline
  func Square(x: i64)->i64 { return x * x; }
  ```
- 简化分支与数据依赖：计算顺序尽量线性、连续

## 2) 内存与数据布局

- 连续内存更友好：`Array`/`Tensor` 的连续布局通常优于链表
- 预分配与复用：
  ```prajna
  var d = DynamicArray<i64>::Create(0);
  // 批量使用时，可以先 Resize 到目标长度再写入，减少多次 Push 的扩容成本
  d.Resize(1000i64);
  ```
- 指针选择：
  - `ptr<T>` 轻量但需手工 `Free()`
  - `Ptr<T>` 自动管理生命周期，易于跨函数传递

## 3) 用 chrono 做基准测试（简易）

- 多轮测试、丢弃热身，统计平均耗时：
```prajna
func Benchmark(iter: i64){
    // 热身
    for i in 0 to 10 { /* call your function */ }

    var rounds = iter;
    var t0 = chrono::Clock();
    for i in 0 to rounds { /* call your function */ }
    var t1 = chrono::Clock();

    var avg = (t1 - t0) / rounds.Cast<f32>();
    avg.PrintLine();
}
```
提示：打印不是性能路径，基准中尽量少打印。

## 4) GPU 优化入门

- 维度顺序固定为 `[z, y, x]`，常用 `x` 维做线性索引：
  ```prajna
  var i = BlockIndex()[2]*BlockShape()[2] + ThreadIndex()[2];
  ```
- 合理的块大小：128~256/512 常见，按设备调参；保证 `grid*block` 覆盖全部元素
- 尽量合并全局访问（coalesced）：邻近线程访问邻近地址
- 共享内存复用：按 tile 加载数据，块内 `BlockSynchronize()` 后再计算
- 减少 Host↔Device 拷贝：把拷贝放到循环外，批量执行多个 kernel 后再回传
- 每次核函数后适时 `Synchronize()`；在读回数据或测时前一定要同步

## 5) 调试工作流

- 最小复现：把问题缩成几十行；能在 REPL/小文件重现
- 断言防线：
  ```prajna
  test::Assert(condition);
  test::AssertWithMessage(cond, "bad input");
  ```
- 分治定位：逐步注释代码块/打印中间变量，二分定位问题点
- 检查边界：数组/张量 
  - 索引必须满足 `0 <= idx < shape`
  - 维度顺序是否写反（`[z,y,x]`）
- 指针/资源：
  - `ptr<T>` 是否已 `Free()` 再使用（悬垂）
  - 动态库/文件路径是否在搜索路径

## 6) FAQ（常见问题）

- Q: 动态库找不到？
  - A: 把库目录加入环境变量（Windows: `PATH`，Linux: `LD_LIBRARY_PATH`，macOS: `DYLD_LIBRARY_PATH`），或放到可执行同目录；`#link("libname")` 写库名而非文件名
- Q: `@extern` 调用崩溃？
  - A: 签名必须与 C 函数 ABI 完全一致；指针有效且传址；位宽（32/64）与平台匹配
- Q: GPU 不工作/结果不变？
  - A: 检查 `@target("nvptx"|"amdgpu")`、驱动与设备；确保 `Synchronize()`；索引越界保护 `if (i<...)`
- Q: Host↔Device 拷贝频繁影响性能？
  - A: 移到循环外，批量计算后一次性 `ToHost()`
- Q: 输出乱码或字符串异常？
  - A: C 字符串需以 `\0` 结尾；注意终端编码
- Q: 为什么用了动态分发变慢？
  - A: 热路径优先静态调用；必要时才用 `Dynamic<Interface>`

---
小结：优先选好数据结构与算法；用简单基准量化收益；GPU 侧关注索引、共享内存与拷贝；调试时用断言与最小复现快速定位。
