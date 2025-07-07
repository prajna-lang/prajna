# Prajna Programming Language

[![Remote Docker Test Workflow](https://github.com/ConvolutedDog/prajna/actions/workflows/Remote-Docker-Test.yml/badge.svg?branch=dev-remote-docker-test-workflow)](https://github.com/ConvolutedDog/prajna/actions/workflows/Remote-Docker-Test.yml)
[![Jenkins](http://dev.matazure.com:8080/job/prajna/job/main/badge/icon)](http://dev.matazure.com:8080/blue/organizations/jenkins/prajna/activity)

> [中文](README_ZH.md) | English

Prajna is a high-level programming language developed for general-purpose scenarios, with a focus on computational foundation domains.

## Why Prajna

A general-purpose programming language should be "simple and efficient," which is the core philosophy of Prajna.

### Prajna VS C++

* Prajna doesn't have C++'s slow compilation issues. Our module references directly use compiled functions without recompiling from scratch
* Prajna places greater emphasis on memory safety
* Prajna has more streamlined programming features
* Prajna templates are purely templates, not template metaprogramming like in C++

### Prajna VS Python

* Prajna also provides an Interpreter Terminal window
* Prajna will support Jupyter visualization systems
* Prajna uses JIT mode, compiling to native code for execution, offering absolute performance advantages
* Prajna is explicitly a statically, strongly typed programming language, unlike Python's ambiguity

### Prajna VS Rust

* Prajna borrows Rust's traits as an isolated interface implementation
* Prajna adds C#'s property functionality and removes the "reference" feature which is problematic
* Prajna directly uses "value copying" + "reference counting" as the default memory safety mechanism, rather than the Ownership mechanism

### Prajna VS Cuda C++

* Prajna's compilation speed is much faster than CUDA
* It has a more precise error reporting mechanism
* It doesn't excessively extend syntax
* It shares most code with the host
* Supports Nvidia GPUs, will support AMD GPUs, and can be easily extended to support various GPUs

Prajna achieves these goals not by adding features, but by subtraction.

## Simple Example

```prajna
func Main(){
    "Hello World\n".PrintLine();
}
```

## Documentation

You can refer to the [Prajna Programming Language Guide](./docs/Prajna%20Programming%20Language%20Guide.md) to learn more.

## Usage

Compiled binary programs are available on the release page.

You can also directly download a Docker image with Prajna pre-installed to experience it:
```bash
docker pull matazure/prajna:0.1.0-cpu-ubuntu20.04
docker run -ti matazure/prajna:0.1.0-cpu-ubuntu20.04 prajna repl
```
