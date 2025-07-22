# 般若编程语言

[![Remote Docker Test Workflow](https://github.com/ConvolutedDog/prajna/actions/workflows/Remote-Docker-Test.yml/badge.svg?branch=dev-remote-docker-test-workflow)](https://github.com/ConvolutedDog/prajna/actions/workflows/Remote-Docker-Test.yml)
[![Jenkins](http://dev.matazure.com:8080/job/prajna/job/main/badge/icon)](http://dev.matazure.com:8080/blue/organizations/jenkins/prajna/activity)

> [中文](README_ZH.md) | English

## 什么是般若？

<div>
    <img src="./assets/zmq_server.png" align="left" width="45%">
</div>

般若是一门静态类型的通用编程语言，由玄青矩阵开发。它语法简洁，运行高效，旨在结合 Python 的灵活性与 C++ 的性能优势。

般若支持即时编译（JIT），无需构建独立二进制即可跨平台运行。兼容 X86、ARM、RISC-V 等架构，并内建对 GPU 并行计算的支持。

般若具备强静态类型系统、模块与接口机制、自动资源管理（智能指针与弱引用），支持函数指针和模板系统，适合从高性能计算到系统工具的广泛应用开发。

<div>
    <img src="./assets/bumper.png" width="100%" alt="Bumper">
</div>

## 主要特性

### 简洁高效的语法设计

<div>
    <img src="./assets/stack.png" align="right" width="50%">
</div>

般若借鉴现代编程语言设计，去除冗余语法，保持语义清晰，易于学习和书写。支持函数指针、模板元编程等高级特性，适合系统级或高性能代码开发。

<div>
    <img src="./assets/bumper.png" width="100%" alt="Bumper">
</div>

### LLVM 后端支持

般若使用 LLVM 作为编译后端，生成高效本地代码。支持多种架构（x86、ARM、RISC-V 等），满足性能关键应用需求。

#### 性能基准对比

<div>
    <img src="./assets/perf.png" align="left" width="50%">
</div>

本程序通过计时添加两个包含 10 亿个 `i32` 元素的一维大数组的元素级加法，测量 GB/s 吞吐率。对应语言的基准代码见 [此处](./assets/code/)。

| 语言   | 吞吐率 (GB/s) |
|--------|---------------|
| 般若   | 0.8205        |
| Python | 0.5100        |
| C++    | 0.6016        |
| Rust   | 0.3300        |

<div>
    <img src="./assets/bumper.png" width="100%" alt="Bumper">
</div>

### 即时编译（JIT）

<div>
    <img src="./assets/jit.gif" align="right" width="50%">
</div>

般若支持运行时即时编译，允许源代码无需构建二进制即可执行，适用于交互开发、脚本执行和动态加载。

<div>
    <img src="./assets/bumper.png" width="100%" alt="Bumper">
</div>

### REPL 与调试支持

### 即时编译（JIT）

<div>
    <img src="./assets/repl.gif" align="left" width="50%">
</div>

语言提供交互式 REPL 环境，方便快速测试与调试。包含断言和错误堆栈信息，提高开发效率。

<div>
    <img src="./assets/bumper.png" width="100%" alt="Bumper">
</div>

### 模块与接口机制

般若内建 `module` 与 `interface` 机制，支持灵活的代码组织、封装与复用。接口支持静态多态，便于构建类型安全的泛型库。

<div>
    <img src="./assets/use.png" width="44%" style="float:left; margin-left: 3%; vertical-align: top;">
    <img src="./assets/module.png" width="50%" style="float:right; margin-right: 3%; vertical-align: top;">
    <img src="./assets/bumper.png" width="100%" alt="Bumper">
</div>

### 内置智能指针系统

<div>
    <img src="./assets/circular_reference.png" align="left" width="35%">
</div>

语言内置 `Ptr<T>` 智能指针和 `WeakPtr<T>` 弱引用，实现自动资源生命周期管理，避免手动释放导致的内存错误，同时支持显式释放机制，适合资源敏感场景。

<div>
    <img src="./assets/bumper.png" width="100%" alt="Bumper">
</div>

### GPU 编程抽象

般若支持使用 `@kernel` 和 `@target` 注解直接声明 GPU 核函数，统一用 `Tensor` 类型管理 GPU 上多维数组。通过 `::gpu::ThreadIndex()` 和 `::gpu::BlockIndex()` 访问线程与块索引，提供类似 CUDA 的并行结构。支持 `@shared` 共享内存变量和 `BlockSynchronize()` 线程同步。

<div>
    <img src="./assets/gpu1.png" width="44%" style="float:left; margin-left: 3%; vertical-align: top;">
    <img src="./assets/gpu2.png" width="50%" style="float:right; margin-right: 3%; vertical-align: top;">
    <img src="./assets/bumper.png" width="100%" alt="Bumper">
</div>

## 文档

更多内容请参考 [般若编程语言指南](./docs/Prajna%20Programming%20Language%20Guide.md)。

## 使用方法

已编译的二进制程序可在发布页面下载。

也可直接拉取带预装般若的 Docker 镜像体验：

```bash
docker pull matazure/prajna:0.1.0-cpu-ubuntu20.04
docker run -ti matazure/prajna:0.1.0-cpu-ubuntu20.04 prajna repl
