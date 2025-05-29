#include "prajna/transform/transform.h"

#include "prajna/transform/extract_gpu_grid_pass.hpp"
#include "prajna/transform/transform_to_ssa_pass.hpp"

namespace prajna::transform {

std::shared_ptr<ir::Module> SperateModule(std::shared_ptr<ir::Module> ir_module) {
    for (auto iter_function = ir_module->functions.begin();
         iter_function != ir_module->functions.end();) {
        auto ir_function = *iter_function;
        if (std::count(RANGE(ir_function->annotation_dict["target"]), "nvptx")) {
            auto ir_nvptx_module = ir_module->modules[ir::Target::nvptx];
            ir_nvptx_module->AddFunction(ir_function);
            iter_function = ir_module->functions.erase(iter_function);
        } else {
            ++iter_function;
        }
    }
    return ir_module;
}

}  // namespace prajna::transform
