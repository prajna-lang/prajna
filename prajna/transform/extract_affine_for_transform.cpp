#include <algorithm>

#include "prajna/helper.hpp"
#include "prajna/ir/ir.hpp"
#include "prajna/transform/transform_pass.hpp"

namespace prajna::transform {

/// @brief 将变量及其
class ExtractAffineLoopPass : public FunctionPass {
   public:
    bool runOnFunction(std::shared_ptr<ir::Function> ir_function) override {
        for (auto block : ir_function->blocks) {
            auto for_block = cast<ir::For>(block);
            if (!for_block) continue;
            if (std::find(for_block->annotations.begin(), for_block->annotations.end(), "affine") !=
                for_block->annotations.end()) {
                continue;
            }

            // affine-for

            // 内部没有控制流

            // 捕获变量
            // 获取在index中不存在的非局部变量, 将其获取, 并标记其是什么类型的, tensor(
            // view)的要特别表示

            /*
            tensor view index操作  关于index的仿射函数

            TensorAccess 也是个Value?

            value是关于index的仿射函数

            暂时不用考虑if for等控制流

            每个TensorAccess写是一个statement
            */

            // 仿射表示
            // 将tensor表示出来
            //  是一个仿射访问

            // 注入hybird的IR
        }

        return false;
    }
};

}  // namespace prajna::transform
