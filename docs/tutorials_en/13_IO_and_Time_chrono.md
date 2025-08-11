# IO and Time (chrono) (Beginner-Friendly)

Basics of printing, reading input, parsing numbers, timing, and sleeping.

## 1. Printing
```prajna
"Hello".Print();
"World".PrintLine();
```

## 2. Reading a line
```prajna
var s = io::Input(); s.PrintLine();
```
Parse to integer (example):
```prajna
func ParseI64(str: String)->i64 {
  var sign = 1i64; var i = 0i64; if (str.Length()>0 && str[0] == '-') { sign = -1i64; i = 1i64; }
  var v = 0i64; while (i < str.Length()) {
    var ch = str[i]; if (ch<'0' || ch>'9') { break; }
    v = v*10i64 + (cast<char, i64>(ch) - cast<char, i64>('0')); i = i + 1i64;
  }
  return sign * v;
}
```

## 3. Timing and sleep
```prajna
var t0 = chrono::Clock(); /* work */ var t1 = chrono::Clock(); (t1 - t0).PrintLine();
chrono::Sleep(0.5f32);
```

## 4. Mini stopwatch
```prajna
func Main(){
  "Press Enter to start...".PrintLine(); io::Input(); var t0 = chrono::Clock();
  "Press Enter to stop...".PrintLine(); io::Input(); var t1 = chrono::Clock();
  (t1 - t0).PrintLine();
}
```

Tips: average over multiple runs for benchmarking; extend parsing as needed.
