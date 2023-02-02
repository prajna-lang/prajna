# 般若编程指南

## 与C++的互操作

### 在C++中调用Prajna函数

#### 参数传递

Prajna函数的参数传递使用的是值传递, 不存在引用方式, 并且传递的类型和声明的类型是一致的. C++与Prajna有所不同, C++会将复杂的结构的值传递隐式转换为指针再进行传递. 故为了避免导致错误, 建议结构体直接采用指针来传递. 具体可参阅[System V Application Binary Interface AMD64 Architecture Processor Supplement](https://www.intel.com/content/dam/develop/external/us/en/documents/mpx-linux64-abi.pdf)关于参数传递的部分.
