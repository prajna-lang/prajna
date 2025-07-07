# 般若编程语言

[![Remote Docker Test Workflow](https://github.com/ConvolutedDog/prajna/actions/workflows/Remote-Docker-Test.yml/badge.svg?branch=dev-remote-docker-test-workflow)](https://github.com/ConvolutedDog/prajna/actions/workflows/Remote-Docker-Test.yml)
[![Jenkins](http://dev.matazure.com:8080/job/prajna/job/main/badge/icon)](http://dev.matazure.com:8080/blue/organizations/jenkins/prajna/activity)

> 中文 | [English](README.md)

般若编程语言是针对目前通用场景而研发的高级编程语言, 其会侧重算力基础领域

## 为什么是Prajna

一门通用编程语言应该“简单高效”, “简单高效”就是Prajna的核心理念

### Prajna VS C++

* Prajna没有C++编译慢的问题. 我们的module引用是直接使用编译好的函数, 不会从头编译一次
* Prajna更加注重内存安全
* Prajna的编程特性更加精简
* Prajna的模板就是单纯的模板, 而非C++那种模板元编程

### Prajna VS Python

* Prajna一样具备Interpreter Terminal窗口
* Prajna也会支持Jupyter可视化系统
* Prajna是JIT模式, 编译为本机代码执行, 性能上有绝对优势
* Prajna是明确的静态强类型编程语言, 不会像Python一样模棱两可

### Prajna VS Rust

* Prajna借鉴了Rust的traits这种隔离式的接口实现
* Prajna增加了C#中的属性功能, 去除了“引用”这种非常糟糕的特性
* Prajna会直接使用“值拷贝” + “引用计数”来作为默认的内存安全机制, 而不是Owership机制

### Prajna VS Cuda C++

* Prajna的编译速度会比CUDA快很多
* 会有更精准的报错机制
* 不会过分的拓展语法
* 会和host共享绝大部分代码
* 支持Nvida GPU, 会支持AMD GPU. 容易拓展支持各种GPU

Prajna并非通过做加法来实现上述的目的, 我们通过减法来达到上述目的

## 简单例子

```prjana
func Main(){
    "Hello World\n".PrintLine();
}
```

## 文档

可以查阅[般若编程语言指南](docs/般若编程语言指南.md)来进一步了解.

## 使用

release页面有编译好的二进制程序.

还可以直接下载已经安转Prajna的docker来直接体验.

```bash
docker pull matazure/prajna:0.1.0-cpu-ubuntu20.04
docker run -ti matazure/prajna:0.1.0-cpu-ubuntu20.04 prajna repl
```
