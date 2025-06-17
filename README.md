# 般若编程语言

[![Remote Docker Test Workflow](https://github.com/ConvolutedDog/prajna/actions/workflows/Remote-Docker-Test.yml/badge.svg?branch=dev-remote-docker-test-workflow)](https://github.com/ConvolutedDog/prajna/actions/workflows/Remote-Docker-Test.yml)
[![Jenkins](http://dev.matazure.com:8080/job/prajna/job/main/badge/icon)](http://dev.matazure.com:8080/blue/organizations/jenkins/prajna/activity)

般若编程语言是针对目前通用场景而研发的高级编程语言, 其会侧重算力基础领域

## 般若语言的一些特性

### 即时编译

般若采用即时编译方式,代码即程序, 无需事先编译为二进制可执行程序. 可以直接在X86, Arm和RiscV等各种指令集的芯片上直接运行. 采用LLVM作为后端, 所以会有着和C/C++一样的性能.

### GPU/异构编程

般若将同时提供对CPU, GPU等的编程支持. 目前已支持Nvidia GPU, 后期会加大对各种芯片的支持力度

### 语法改善

般若是属于类C语言, 借鉴了Rust的面向对象的设计, 移除不必要的语法特性, 例如引用等. 内存管理采用比较通用的引用计数.

### 友好交互

般若支持main函数, Repl和Jupyter等多种交互方式, 适合算法研发和部署等多种场景

### 更佳的性能

般若编译器的实现非常高效, 有着更好的编译性能和运行性能

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
