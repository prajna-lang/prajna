# IO 与时间（chrono）（给初学者）

本篇介绍最常用的输入输出与计时：如何打印文本、读取一行输入、把输入解析为数字，如何测量一段代码的耗时、以及让程序暂停一会儿。

你将学到：
- 打印与换行：`Print` / `PrintLine`
- 读取输入：`io::Input()` 返回一行 `String`
- 基础解析：把 `String` 转为数字（思路示例）
- 计时与睡眠：`chrono::Clock()`、`chrono::Sleep(sec)`
- 小练习：简易秒表

## 1. 打印文本

```prajna
"Hello".Print();
"World".PrintLine();  // 自动带换行
```

## 2. 读取一行输入

- `io::Input()` 从标准输入读取一行，返回 `String`
```prajna
var s = io::Input();
s.PrintLine();
```

- 将输入解析为整数（示例思路：逐字符累加；也可以在项目中封装通用解析函数）
```prajna
func ParseI64(str: String)->i64 {
    var sign = 1i64;
    var i = 0i64;
    if (str.Length()>0 && str[0] == '-') { sign = -1i64; i = 1i64; }

    var v = 0i64;
    while (i < str.Length()) {
        var ch = str[i];
        if (ch<'0' || ch>'9') { break; }
        v = v*10i64 + (cast<char, i64>(ch) - cast<char, i64>('0'));
        i = i + 1i64;
    }
    return sign * v;
}
```

## 3. 计时与睡眠

- `chrono::Clock()` 返回当前时间戳（`f32`，单位秒）；可两次取差来度量耗时
```prajna
var t0 = chrono::Clock();
// ... 执行任务 ...
var t1 = chrono::Clock();
(t1 - t0).PrintLine();
```

- `chrono::Sleep(t)` 让程序睡眠 `t` 秒（`f32`）
```prajna
chrono::Sleep(0.5f32); // 睡 0.5 秒
```

## 4. 小练习：简易秒表

```prajna
func Main(){
    "Press Enter to start...".PrintLine();
    io::Input();
    var t0 = chrono::Clock();

    "Press Enter to stop...".PrintLine();
    io::Input();
    var t1 = chrono::Clock();

    (t1 - t0).PrintLine();
}
```

提示：
- 打印用于交互与调试；性能测试请多次测量取平均值
- 输入解析可根据需要扩展浮点、带空格的列表等
