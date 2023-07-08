# 如何在般若中使用C函数

本文介绍般若编程语言中如何集成C语言的函数, 目前般若仅支持在编译器内部直接集成C函数, 后期我们会推出加载动态库的集成方式(类型dlopen的方式).
这里我们以文件读写库的添加为例来介绍各个步骤.

## 绑定C函数地址到执行引擎中去

我们可以在文件"prajna/jit/execution_engine.cpp"的bindBuiltinFunction函数里把所需的函数地址绑定, 这样我们后面就可以通过名字去索引了.
下面我们绑定了操作文件所需的C函数.

```c++
    this->BindCFunction(reinterpret_cast<void *>(fopen), "::fs::_c::fopen");
    this->BindCFunction(reinterpret_cast<void*>(fclose), "::fs::_c::fclose");
    this->BindCFunction(reinterpret_cast<void *>(fseek), "::fs::_c::fseek");
    this->BindCFunction(reinterpret_cast<void *>(ftell), "::fs::_c::ftell");
    this->BindCFunction(reinterpret_cast<void*>(fflush), "::fs::_c::fflush");
    this->BindCFunction(reinterpret_cast<void *>(fread), "::fs::_c::fread");
    this->BindCFunction(reinterpret_cast<void *>(fwrite), "::fs::_c::fwrite");
```

## 在Prajna里声明函数

我们在prajna_builtin_packages/fs/_c.prajna里声明上面所绑定的函数.

```prajna
struct FILE{}

func fopen(filename: __ptr<char>, mode: __ptr<char>)->__ptr<FILE>;
func fclose(stream: __ptr<FILE>);
func fseek(stream: __ptr<FILE>, offset: i64, origin: i32);
func ftell(stream: __ptr<FILE>)->i64;
func fflush(stream: __ptr<FILE>);
func fread(buffer: __ptr<char>, size: i64, count: i64, stream: __ptr<FILE>);
func fwrite(buffer: __ptr<char>, size: i64, count: i64, stream: __ptr<FILE>);
```

prajna_builtin_pakcages是比较特殊的文件夹, 其名字空间就是"::", fs对应"fs", _c.prajna对应"_c", 合到一块就是::fs::_c.
这里可以看到我们声明的函数全名必须和绑定时所使用的名字一致, 这样执行引擎才能找到.
i32相当于int32, __ptr\<FILE>相当于FILE *, 我们可以从字面理解Prajna和C的对应类型.

## 封装成合理的接口

我们所绑定的函数大部分是不推荐直接使用的, 因为"指针"(__ptr)在Prajna里是不安全的.
"\__"开头表示其是不安全的, 使用时要特别关注其内存生存期间和其他安全性问题.
我们可以在prajna_builtin_packages/fs/.prajna里看到般若对文件读写的封装. (".prajna"不会增加名字空间, 其相当于fs模块的入口文件)

```prajna
use _c;

func Write(filename: String , buffer: String) {
        var mode = "wb+";
        mode.Push('\0');
        filename.Push('\0');
        var stream = _c::fopen(filename.CStringRawPointer() , mode.CStringRawPointer());
        Assert(stream.IsValid());
        _c::fwrite(buffer.CStringRawPointer(), 1, buffer.Length(), stream);
        _c::fclose(stream);
}

func Read(filename: String)->String {
        var mode = "rb";
        mode.Push('\0');
        filename.Push('\0');
        var stream = _c::fopen(filename.CStringRawPointer() , mode.CStringRawPointer());
        filename.Pop();
        Assert(stream.IsValid());

        _c::fseek(stream, 0, 2i32);
        var length = _c::ftell(stream);
        _c::fseek(stream, 0, 0i32);
        var buffer = String::Create(length);
        _c::fread(buffer.CStringRawPointer(), 1, buffer.Length(), stream);
        _c::fclose(stream);
        return buffer;
}

```

我们可以看到Prajna里的文件库就两个函数Write和Read. 很多时候我们的系统库过度设计了, 这在C++, C#和Java里都存在类似的问题, 搞得花里胡哨, 把简单的问题搞复杂了, 然后引入了一堆隐患. 大部分时候越简单才是越好的, 软件本身就是一个产品, 产品设计的核心原则应该是"简单有用".

## 测试函数

我们再tests/compiler/prajna_sources/fs_test.prajna增加了对fs::Read/Write函数的测试.

```prajna
use fs;

@test
func TestFs() {
    var str_hello_world = "Hello World!";
    {
        fs::Write("fs_test.txt", str_hello_world);
    }
    {
        var str = fs::Read("fs_test.txt");
        debug::Assert(str == str_hello_world);
    }
}
```

上述我们看到般若的fs模块就两个函数Read和Write, 使用起来干净利落. 后面还会视情况添加一些.
