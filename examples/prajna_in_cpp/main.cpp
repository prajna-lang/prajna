#include <iostream>

#include "prajna/bindings/core.hpp"
#include "prajna/compiler/compiler.h"

int main() {
    auto compiler = prajna::Compiler::create();
    compiler->compileBuiltinSourceFiles("prajna/prajna_builtin_packages");
    compiler->compileProgram("examples/prajna_in_cpp/add.prajna", false);

    using MatrixF32 = prajna::Tensor<float, 2>;

    prajna::Array<int64_t, 2> shape(2, 3);
    auto ts = MatrixF32::create(shape);
    ts(0, 0) = 1;
    ts(0, 1) = 2;
    ts(0, 2) = 3;
    ts(1, 0) = 4;
    ts(1, 1) = 5;
    ts(1, 2) = 6;

    auto matrix_add_f32 = reinterpret_cast<MatrixF32 (*)(MatrixF32 *, MatrixF32 *, MatrixF32 *)>(
        compiler->getSymbolValue("::examples::prajna_in_cpp::add::matrixAddF32"));

    auto ts_re = MatrixF32::create(shape);

    // ts等的引用计数依然正常工作, 其可以在c++和Prajna交互时正常工作
    matrix_add_f32(&ts, &ts, &ts_re);

    for (int64_t i = 0; i < shape[0]; ++i) {
        for (int64_t j = 0; j < shape[1]; ++j) {
            std::cout << ts_re(i, j) << ", ";
        }
        std::cout << std::endl;
    }

    return 0;
}
