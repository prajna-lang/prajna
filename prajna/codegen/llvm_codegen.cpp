#include "prajna/codegen/llvm_codegen.h"

#include "llvm/ExecutionEngine/Orc/JITTargetMachineBuilder.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constant.h"
#include "llvm/IR/ConstantFolder.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/DebugLoc.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Operator.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/ValueHandle.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Linker/Linker.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/Debug.h"
#include "prajna/global_config.hpp"
#include "prajna/helper.hpp"
#include "prajna/ir/ir.hpp"
#include "prajna/ir/target.hpp"
#include "prajna/ir/visitor.hpp"
#include "third_party/llvm-project/llvm/include/llvm-c/Target.h"
#include "third_party/llvm-project/llvm/include/llvm/Analysis/AliasAnalysis.h"
#include "third_party/llvm-project/llvm/include/llvm/IR/AutoUpgrade.h"

namespace llvm {
class Module;
class LLVMContext;
}  // namespace llvm

namespace prajna::codegen {

/// llvm的全局context, 若放在局部不是特别容易管理, 故直接搞成全局变量
static llvm::LLVMContext static_llvm_context;

class LlvmCodegen : public prajna::ir::Visitor {
   protected:
    LlvmCodegen() {}

   public:
    static std::shared_ptr<LlvmCodegen> Create(prajna::ir::Target ir_target) {
        std::shared_ptr<LlvmCodegen> self(new LlvmCodegen);
        self->ir_target = ir_target;
        return self;
    }

    void EmitType(std::shared_ptr<ir::Type> ir_type) {
        if (ir_type->llvm_type) return;

        // 对于不完备的类型, codegen时选择跳过
        if (auto ir_real_number_type = Cast<ir::RealNumberType>(ir_type)) {
            if (ir_real_number_type->bits <= 0) {
                return;
            }
        }

        if (auto ir_bool_type = Cast<ir::BoolType>(ir_type)) {
            ir_bool_type->llvm_type = llvm::Type::getInt1Ty(static_llvm_context);
            return;
        }
        if (auto ir_char_type = Cast<ir::CharType>(ir_type)) {
            ir_char_type->llvm_type = llvm::Type::getInt8Ty(static_llvm_context);
            return;
        }
        if (auto ir_int_type = Cast<ir::IntType>(ir_type)) {
            ir_int_type->llvm_type = llvm::Type::getIntNTy(static_llvm_context, ir_int_type->bits);
            return;
        }
        if (auto ir_float_type = Cast<ir::FloatType>(ir_type)) {
            switch (ir_float_type->bits) {
                case 16:
                    ir_float_type->llvm_type = llvm::Type::getHalfTy(static_llvm_context);
                    return;
                case 32:
                    ir_float_type->llvm_type = llvm::Type::getFloatTy(static_llvm_context);
                    return;
                case 64:
                    ir_float_type->llvm_type = llvm::Type::getDoubleTy(static_llvm_context);
                    return;
                case 128:
                    ir_float_type->llvm_type = llvm::Type::getFP128Ty(static_llvm_context);
                    return;
                default:
                    PRAJNA_UNREACHABLE;
            }

            return;
        }
        if (auto ir_undef_type = Cast<ir::UndefType>(ir_type)) {
            ir_undef_type->llvm_type = llvm::Type::getInt8Ty(static_llvm_context);
            return;
        }
        if (auto ir_void_type = Cast<ir::VoidType>(ir_type)) {
            ir_void_type->llvm_type = llvm::Type::getVoidTy(static_llvm_context);
            return;
        }
        if (auto ir_function_type = Cast<ir::FunctionType>(ir_type)) {
            std::vector<llvm::Type *> llvm_argument_types(ir_function_type->parameter_types.size());
            std::ranges::transform(ir_function_type->parameter_types, llvm_argument_types.begin(),
                                   [this](std::shared_ptr<ir::Type> ir_type) {
                                       this->EmitType(ir_type);
                                       PRAJNA_ASSERT(ir_type->llvm_type);
                                       return ir_type->llvm_type;
                                   });
            this->EmitType(ir_function_type->return_type);
            ir_function_type->llvm_type = llvm::FunctionType::get(
                ir_function_type->return_type->llvm_type, llvm_argument_types, false);
            return;
        }
        if (auto ir_pointer_type = Cast<ir::PointerType>(ir_type)) {
            this->EmitType(ir_pointer_type->value_type);
            ir_pointer_type->llvm_type =
                llvm::PointerType::get(ir_pointer_type->value_type->llvm_type, 0);
            return;
        }
        if (auto ir_array_type = Cast<ir::ArrayType>(ir_type)) {
            this->EmitType(ir_array_type->value_type);
            ir_array_type->llvm_type =
                llvm::ArrayType::get(ir_array_type->value_type->llvm_type, ir_array_type->size);
            return;
        }
        if (auto ir_vector_type = Cast<ir::VectorType>(ir_type)) {
            this->EmitType(ir_vector_type->value_type);
            ir_vector_type->llvm_type =
                llvm::VectorType::get(ir_vector_type->value_type->llvm_type,
                                      llvm::ElementCount::getFixed(ir_vector_type->size));
            return;
        }
        if (auto ir_struct_type = Cast<ir::StructType>(ir_type)) {
            auto llvm_struct_type =
                llvm::StructType::create(static_llvm_context, ir_struct_type->fullname);
            ir_struct_type->llvm_type = llvm_struct_type;
            std::vector<llvm::Type *> llvm_types(ir_struct_type->fields.size());
            std::ranges::transform(ir_struct_type->fields, llvm_types.begin(),
                                   [this](std::shared_ptr<ir::Field> field) {
                                       this->EmitType(field->type);
                                       return field->type->llvm_type;
                                   });
            llvm_struct_type->setBody(llvm_types, true);
            return;
        }
        if (auto ir_simd_type = Cast<ir::SimdType>(ir_type)) {
            this->EmitType(ir_simd_type->value_type);
            ir_simd_type->llvm_type =
                llvm::VectorType::get(ir_simd_type->value_type->llvm_type,
                                      llvm::ElementCount::getFixed(ir_simd_type->size));
            return;
        }

        PRAJNA_TODO;
    }

    void Visit(std::shared_ptr<ir::Module> ir_module) override {
        PRAJNA_ASSERT(ir::Verify(ir_module));

        PRAJNA_ASSERT(!ir_module->llvm_module);
        ir_module->llvm_module = new llvm::Module(ir_module->name, static_llvm_context);

        for (auto ir_global_alloca : ir_module->global_allocas) {
            // this->EmitGlobalAlloca(ir_global_alloca);
            ir_global_alloca->ApplyVisitor(this->shared_from_this());
        }

        for (std::shared_ptr<ir::Function> ir_function : ir_module->functions) {
            this->EmitFunctionDeclaration(ir_function, this->ir_target);
            if (ir_function->annotation_dict.count("kernel")) {
                std::string metadata;
                if (ir_target == prajna::ir::Target::nvptx) {
                    metadata = "nvvm.annotations";
                } else if (ir_target == prajna::ir::Target::amdgpu) {
                    metadata = "amdhsa.kernels";
                }
                auto md_node = llvm::MDNode::get(
                    static_llvm_context, {llvm::ValueAsMetadata::get(ir_function->llvm_value),
                                          llvm::MDString::get(static_llvm_context, "kernel"),
                                          llvm::ValueAsMetadata::get(llvm::ConstantInt::get(
                                              llvm::Type::getInt32Ty(static_llvm_context), 1))});
                auto nvvm_annotations_md =
                    ir_module->llvm_module->getOrInsertNamedMetadata(metadata);
                nvvm_annotations_md->addOperand(md_node);
            }
        }

        for (std::shared_ptr<ir::Function> ir_function : ir_module->functions) {
            if (!ir_function->IsDeclaration()) {
                // this->EmitFunction(ir_function);
                ir_function->ApplyVisitor(this->shared_from_this());
            }
        }
    }

    void EmitFunctionDeclaration(std::shared_ptr<ir::Function> ir_function,
                                 prajna::ir::Target ir_target) {
        std::string function_fullname;
        if (ir_function->fullname.starts_with("__ocml_")) {  // 临时调试添加
            function_fullname = ir_function->fullname;
        } else if (ir_target == prajna::ir::Target::nvptx) {
            function_fullname = MangleNvvmName(ir_function->fullname);
        } else if (ir_target == prajna::ir::Target::amdgpu) {
            function_fullname = MangleHipName(ir_function->fullname);
        } else {
            function_fullname = ir_function->fullname;
        }

        PRAJNA_ASSERT(ir_function->function_type->llvm_type);
        llvm::FunctionType *llvm_fun_type =
            static_cast<llvm::FunctionType *>(ir_function->function_type->llvm_type);
        llvm::Function *llvm_fun =
            llvm::Function::Create(llvm_fun_type, llvm::Function::ExternalLinkage,
                                   function_fullname, ir_function->GetParentModule()->llvm_module);
        // 设置为 AMDGPU kernel calling convention
        if (ir_target == prajna::ir::Target::amdgpu &&
            ir_function->annotation_dict.count("kernel")) {
            llvm_fun->setCallingConv(llvm::CallingConv::AMDGPU_KERNEL);
        }
        ir_function->llvm_value = llvm_fun;
    }

    void Visit(std::shared_ptr<ir::Function> ir_function) override {
        llvm::Function *llvm_fun = static_cast<llvm::Function *>(ir_function->llvm_value);
        PRAJNA_ASSERT(llvm_fun);
        PRAJNA_ASSERT(ir_function->parameters.size() == llvm_fun->arg_size());
        int64_t i = 0;
        auto iter_parameter = ir_function->parameters.begin();
        for (auto llvm_arg = llvm_fun->arg_begin(); llvm_arg != llvm_fun->arg_end();
             ++llvm_arg, ++iter_parameter) {
            auto ir_parameter = *iter_parameter;
            if (ir_parameter->no_alias) {
                llvm_arg->addAttr(llvm::Attribute::NoAlias);
            }
            // if (ir_parameter->no_capture) {
            //     llvm_arg->addAttr(llvm::Attribute::NoCapture);
            // }
            if (ir_parameter->no_undef) {
                llvm_arg->addAttr(llvm::Attribute::NoUndef);
            }
            if (ir_parameter->readonly) {
                llvm_arg->addAttr(llvm::Attribute::ReadOnly);
            }
            ir_parameter->llvm_value = llvm_arg;
        }

        for (auto block : ir_function->blocks) {
            // EmitBlock(block);
            block->ApplyVisitor(this->shared_from_this());
        }
    }

    void Visit(std::shared_ptr<ir::Block> ir_block) override {
        auto ir_parent_function = ir_block->GetParentFunction();
        PRAJNA_ASSERT(ir_block && ir_parent_function);

        if (ir_block->llvm_value != nullptr) {
            return;
        }

        PRAJNA_ASSERT(ir_parent_function->llvm_value);
        ir_block->llvm_value = llvm::BasicBlock::Create(
            static_llvm_context, "", static_cast<llvm::Function *>(ir_parent_function->llvm_value),
            nullptr);

        for (auto ir_value : *ir_block) {
            // EmitValue(ir_value, ir_target);
            if (Is<ir::Function>(ir_value)) {
                continue;
            }
            ir_value->ApplyVisitor(this->shared_from_this());
        }
    }

    void Visit(std::shared_ptr<ir::Parameter> value) override {
        // Do nothing
    }
    void Visit(std::shared_ptr<ir::VoidValue> value) override { value->llvm_value = nullptr; }

    void Visit(std::shared_ptr<ir::ConstantChar> ir_constant) override {
        PRAJNA_ASSERT(ir_constant->type->llvm_type);
        ir_constant->llvm_value =
            llvm::ConstantInt::get(ir_constant->type->llvm_type, ir_constant->value);
    }

    void Visit(std::shared_ptr<ir::ConstantBool> ir_constant_bool) override {
        PRAJNA_ASSERT(ir_constant_bool->type->llvm_type);
        ir_constant_bool->llvm_value =
            llvm::ConstantInt::get(ir_constant_bool->type->llvm_type, ir_constant_bool->value);
    }

    void Visit(std::shared_ptr<ir::ConstantInt> ir_constant_int) override {
        PRAJNA_ASSERT(ir_constant_int->type->llvm_type);
        ir_constant_int->llvm_value =
            llvm::ConstantInt::get(ir_constant_int->type->llvm_type, ir_constant_int->value,
                                   Cast<ir::IntType>(ir_constant_int->type)->is_signed);
    }

    void Visit(std::shared_ptr<ir::ConstantFloat> ir_constant_float) override {
        PRAJNA_ASSERT(ir_constant_float->type->llvm_type);
        // 提前返回float的最大数或最小数
        llvm::APFloat::Semantics semantics;
        switch (ir_constant_float->type->bytes) {
            case 2:
                semantics = llvm::APFloat::Semantics::S_IEEEhalf;
                break;
            case 4:
                semantics = llvm::APFloat::Semantics::S_IEEEsingle;
                break;
            case 8:
                semantics = llvm::APFloat::Semantics::S_IEEEdouble;
                break;
            case 16:
                semantics = llvm::APFloat::Semantics::S_IEEEquad;
                break;
            default:
                PRAJNA_UNREACHABLE;
        }

        switch (ir_constant_float->special_value) {
            case ir::ConstantFloat::None:
                ir_constant_float->llvm_value = llvm::ConstantFP::get(
                    ir_constant_float->type->llvm_type, ir_constant_float->value);
                return;
            case ir::ConstantFloat::Smallest:
                ir_constant_float->llvm_value = llvm::ConstantFP::get(
                    ir_constant_float->type->llvm_type,
                    llvm::APFloat::getSmallest(llvm::APFloat::EnumToSemantics(semantics),
                                               ir_constant_float->is_negative));
                return;
            case ir::ConstantFloat::Largest:
                ir_constant_float->llvm_value = llvm::ConstantFP::get(
                    ir_constant_float->type->llvm_type,
                    llvm::APFloat::getLargest(llvm::APFloat::EnumToSemantics(semantics),
                                              ir_constant_float->is_negative));
                return;
            case ir::ConstantFloat::NaN:
                ir_constant_float->llvm_value = llvm::ConstantFP::get(
                    ir_constant_float->type->llvm_type,
                    llvm::APFloat::getNaN(llvm::APFloat::EnumToSemantics(semantics),
                                          ir_constant_float->is_negative));
                return;
            case ir::ConstantFloat::Inf:
                ir_constant_float->llvm_value = llvm::ConstantFP::get(
                    ir_constant_float->type->llvm_type,
                    llvm::APFloat::getInf(llvm::APFloat::EnumToSemantics(semantics),
                                          ir_constant_float->is_negative));
                return;
        }
    }

    void Visit(std::shared_ptr<ir::ConstantArray> ir_constant_array) override {
        // Ensure all constituent elements have their llvm_value set
        for (auto ir_init : ir_constant_array->initialize_constants) {
            if (!ir_init->llvm_value) {
                ir_init->ApplyVisitor(this->shared_from_this());
            }
        }

        std::vector<llvm::Constant *> llvm_contants(ir_constant_array->initialize_constants.size());
        std::ranges::transform(
            ir_constant_array->initialize_constants, llvm_contants.begin(), [=](auto ir_init) {
                auto llvm_constant = static_cast<llvm::Constant *>(ir_init->llvm_value);
                PRAJNA_ASSERT(llvm_constant);
                return llvm_constant;
            });
        PRAJNA_ASSERT(ir_constant_array->type->llvm_type);
        ir_constant_array->llvm_value = llvm::ConstantArray::get(
            static_cast<llvm::ArrayType *>(ir_constant_array->type->llvm_type), llvm_contants);
    }

    void Visit(std::shared_ptr<ir::ConstantVector> ir_constant_vector) override {
        // Ensure all constituent elements have their llvm_value set
        for (auto ir_init : ir_constant_vector->initialize_constants) {
            if (!ir_init->llvm_value) {
                ir_init->ApplyVisitor(this->shared_from_this());
            }
        }

        std::vector<llvm::Constant *> llvm_contants(
            ir_constant_vector->initialize_constants.size());
        std::ranges::transform(
            ir_constant_vector->initialize_constants, llvm_contants.begin(), [=](auto ir_init) {
                auto llvm_constant = static_cast<llvm::Constant *>(ir_init->llvm_value);
                PRAJNA_ASSERT(llvm_constant);
                return llvm_constant;
            });
        PRAJNA_ASSERT(ir_constant_vector->type->llvm_type);
        ir_constant_vector->llvm_value = llvm::ConstantVector::get(llvm_contants);
    }

    void Visit(std::shared_ptr<ir::Constant> ir_constant) override {
        if (ir_constant->llvm_value) {
            return;
        }
        ir_constant->ApplyVisitor(this->shared_from_this());
    }

    void Visit(std::shared_ptr<ir::GlobalAlloca> ir_global_alloca) override {
        auto ir_value_type = Cast<ir::PointerType>(ir_global_alloca->type)->value_type;
        PRAJNA_ASSERT(ir_value_type && ir_value_type->llvm_type);
        PRAJNA_ASSERT(ir_global_alloca->GetParentModule()->llvm_module);

        // 事实上llvm::GlobalVariable其实获取一个个指针
        auto llvm_global_variable = new llvm::GlobalVariable(
            *(ir_global_alloca->GetParentModule()->llvm_module), ir_value_type->llvm_type, false,
            llvm::GlobalValue::LinkageTypes::ExternalLinkage, nullptr, ir_global_alloca->fullname,
            nullptr, llvm::GlobalValue::NotThreadLocal, ir_global_alloca->address_space, false);

        if (!ir_global_alloca->is_external) {
            // @note 需要初始化, 否则符号会找到不到. 也就是说如果不初始化, 则其为external
            llvm_global_variable->setInitializer(llvm::UndefValue::get(ir_value_type->llvm_type));
        }
        ir_global_alloca->llvm_value = llvm_global_variable;
    }

    void Visit(std::shared_ptr<ir::Call> ir_call) override {
        auto llvm_basic_block = GetLlvmBasicBlock(ir_call);
        auto ir_function_type = ir_call->Function()->GetFunctionType();
        auto llvm_arguments = To<std::vector<llvm::Value *>>(
            (ir_call->Arguments() | std::ranges::views::transform([this](auto ir_argument) {
                 return ir_argument->llvm_value;
             })));
        PRAJNA_ASSERT(ir_call->Function()->GetFunctionType()->llvm_type);
        PRAJNA_ASSERT(ir_call->Function()->llvm_value);
        ir_call->llvm_value = llvm::CallInst::Create(
            static_cast<llvm::FunctionType *>(ir_call->Function()->GetFunctionType()->llvm_type),
            ir_call->Function()->llvm_value, llvm_arguments, "", llvm_basic_block);
    }

    void Visit(std::shared_ptr<ir::Select> ir_select) override {
        auto llvm_basic_block = GetLlvmBasicBlock(ir_select);
        PRAJNA_ASSERT(ir_select->Condition()->llvm_value);
        PRAJNA_ASSERT(ir_select->TrueValue()->llvm_value);
        PRAJNA_ASSERT(ir_select->FalseValue()->llvm_value);
        PRAJNA_ASSERT(ir_select->type->llvm_type);

        ir_select->llvm_value = llvm::SelectInst::Create(
            ir_select->Condition()->llvm_value, ir_select->TrueValue()->llvm_value,
            ir_select->FalseValue()->llvm_value, "", llvm_basic_block);
    }

    void Visit(std::shared_ptr<ir::Return> ir_return) override {
        auto llvm_basic_block = GetLlvmBasicBlock(ir_return);
        auto llvm_return = llvm::ReturnInst::Create(
            static_llvm_context, ir_return->Value()->llvm_value, llvm_basic_block);
    }

    void Visit(std::shared_ptr<ir::Alloca> ir_alloca) override {
        auto llvm_basic_block = GetLlvmBasicBlock(ir_alloca);
        auto ir_alloca_type = Cast<ir::PointerType>(ir_alloca->type);
        PRAJNA_ASSERT(ir_alloca_type && ir_alloca_type->value_type->llvm_type);
        if (ir_alloca->alignment > 0) {
            ir_alloca->llvm_value = new llvm::AllocaInst(
                ir_alloca_type->value_type->llvm_type, 0, ir_alloca->Length()->llvm_value,
                llvm::Align(ir_alloca->alignment), ir_alloca->name, llvm_basic_block);
        } else {
            // llvm会使用当前平台的默认alignment
            ir_alloca->llvm_value = new llvm::AllocaInst(ir_alloca_type->value_type->llvm_type, 0,
                                                         ir_alloca->Length()->llvm_value,
                                                         ir_alloca->name, llvm_basic_block);
        }
    }

    void Visit(std::shared_ptr<ir::LoadPointer> ir_load_pointer) override {
        auto llvm_basic_block = GetLlvmBasicBlock(ir_load_pointer);
        PRAJNA_ASSERT(ir_load_pointer->type->llvm_type);
        PRAJNA_ASSERT(ir_load_pointer->Pointer()->llvm_value);
        auto llvm_load_ptr =
            new llvm::LoadInst(ir_load_pointer->type->llvm_type,
                               ir_load_pointer->Pointer()->llvm_value, "", llvm_basic_block);
        ir_load_pointer->llvm_value = llvm_load_ptr;
    }

    void Visit(std::shared_ptr<ir::StorePointer> ir_store_pointer) override {
        auto llvm_basic_block = GetLlvmBasicBlock(ir_store_pointer);
        PRAJNA_ASSERT(ir_store_pointer->Value()->llvm_value);
        PRAJNA_ASSERT(ir_store_pointer->Pointer()->llvm_value);

        auto llvm_store_ptr =
            new llvm::StoreInst(ir_store_pointer->Value()->llvm_value,
                                ir_store_pointer->Pointer()->llvm_value, false, llvm_basic_block);
        ir_store_pointer->llvm_value = llvm_store_ptr;
    }

    void Visit(std::shared_ptr<ir::ConditionBranch> ir_condition_branch) override {
        auto llvm_basic_block = GetLlvmBasicBlock(ir_condition_branch);
        // 需要处理, 因为true/falseBlock在ir_condition_branch的后面
        ir_condition_branch->TrueBlock()->ApplyVisitor(this->shared_from_this());

        ir_condition_branch->FalseBlock()->ApplyVisitor(this->shared_from_this());

        PRAJNA_ASSERT(ir_condition_branch->TrueBlock()->llvm_value);
        PRAJNA_ASSERT(ir_condition_branch->FalseBlock()->llvm_value);
        PRAJNA_ASSERT(ir_condition_branch->Condition()->llvm_value);
        ir_condition_branch->llvm_value = llvm::BranchInst::Create(
            static_cast<llvm::BasicBlock *>(ir_condition_branch->TrueBlock()->llvm_value),
            static_cast<llvm::BasicBlock *>(ir_condition_branch->FalseBlock()->llvm_value),
            ir_condition_branch->Condition()->llvm_value, llvm_basic_block);
    }

    void Visit(std::shared_ptr<ir::JumpBranch> ir_jump_branch) override {
        auto llvm_basic_block = GetLlvmBasicBlock(ir_jump_branch);
        PRAJNA_ASSERT(Lock(ir_jump_branch->GetParentBlock()->parent) ==
                      Lock(ir_jump_branch->NextBlock()->parent));

        ir_jump_branch->NextBlock()->ApplyVisitor(this->shared_from_this());

        PRAJNA_ASSERT(ir_jump_branch->NextBlock()->llvm_value);
        ir_jump_branch->llvm_value = llvm::BranchInst::Create(
            static_cast<llvm::BasicBlock *>(ir_jump_branch->NextBlock()->llvm_value),
            llvm_basic_block);
    }

    void Visit(
        std::shared_ptr<ir::GetStructElementPointer> ir_get_struct_element_pointer) override {
        auto llvm_basic_block = GetLlvmBasicBlock(ir_get_struct_element_pointer);
        std::vector<llvm::Value *> llvm_idx_list(2);
        llvm_idx_list[0] = llvm::ConstantInt::get(llvm::Type::getInt64Ty(static_llvm_context), 0);
        // 结构体的偏移下标必须使用32位整型
        llvm_idx_list[1] = llvm::ConstantInt::get(llvm::Type::getInt32Ty(static_llvm_context),
                                                  ir_get_struct_element_pointer->field->index);

        auto ir_pointer_type =
            Cast<ir::PointerType>(ir_get_struct_element_pointer->Pointer()->type);
        PRAJNA_ASSERT(ir_pointer_type && ir_pointer_type->value_type->llvm_type);
        PRAJNA_ASSERT(ir_get_struct_element_pointer->Pointer()->llvm_value);
        ir_get_struct_element_pointer->llvm_value =
            llvm::GetElementPtrInst::Create(ir_pointer_type->value_type->llvm_type,
                                            ir_get_struct_element_pointer->Pointer()->llvm_value,
                                            llvm_idx_list, "", llvm_basic_block);
    }

    void Visit(std::shared_ptr<ir::GetArrayElementPointer> ir_get_array_element_pointer) override {
        auto llvm_basic_block = GetLlvmBasicBlock(ir_get_array_element_pointer);
        std::vector<llvm::Value *> llvm_idx_list(2);
        llvm_idx_list[0] = llvm::ConstantInt::get(llvm::Type::getInt64Ty(static_llvm_context), 0);
        llvm_idx_list[1] = ir_get_array_element_pointer->IndexVariable()->llvm_value;
        auto ir_pointer_type = Cast<ir::PointerType>(ir_get_array_element_pointer->Pointer()->type);
        PRAJNA_ASSERT(ir_pointer_type && ir_pointer_type->value_type->llvm_type);
        PRAJNA_ASSERT(ir_get_array_element_pointer->Pointer()->llvm_value);
        ir_get_array_element_pointer->llvm_value =
            llvm::GetElementPtrInst::Create(ir_pointer_type->value_type->llvm_type,
                                            ir_get_array_element_pointer->Pointer()->llvm_value,
                                            llvm_idx_list, "", llvm_basic_block);
    }

    void Visit(
        std::shared_ptr<ir::GetPointerElementPointer> ir_get_pointer_element_pointer) override {
        auto llvm_basic_block = GetLlvmBasicBlock(ir_get_pointer_element_pointer);
        std::vector<llvm::Value *> llvm_idx_list(1);
        llvm_idx_list[0] = ir_get_pointer_element_pointer->IndexVariable()->llvm_value;
        PRAJNA_ASSERT(ir_get_pointer_element_pointer->type->llvm_type);
        PRAJNA_ASSERT(ir_get_pointer_element_pointer->Pointer()->llvm_value);
        auto llvm_pointer = new llvm::LoadInst(
            ir_get_pointer_element_pointer->type->llvm_type,
            ir_get_pointer_element_pointer->Pointer()->llvm_value, "", llvm_basic_block);
        auto ir_pointer_type = Cast<ir::PointerType>(ir_get_pointer_element_pointer->type);
        PRAJNA_ASSERT(ir_pointer_type && ir_pointer_type->value_type->llvm_type);
        ir_get_pointer_element_pointer->llvm_value =
            llvm::GetElementPtrInst::Create(ir_pointer_type->value_type->llvm_type, llvm_pointer,
                                            llvm_idx_list, "", llvm_basic_block);
    }

    void Visit(std::shared_ptr<ir::BitCast> ir_bit_cast) override {
        auto llvm_basic_block = GetLlvmBasicBlock(ir_bit_cast);
        PRAJNA_ASSERT(ir_bit_cast->Value()->llvm_value);
        PRAJNA_ASSERT(ir_bit_cast->type->llvm_type);
        ir_bit_cast->llvm_value = new llvm::BitCastInst(
            ir_bit_cast->Value()->llvm_value, ir_bit_cast->type->llvm_type, "", llvm_basic_block);
    }

    void Visit(std::shared_ptr<ir::CastInstruction> ir_cast_instruction) override {
        auto llvm_basic_block = GetLlvmBasicBlock(ir_cast_instruction);
        std::unordered_map<ir::CastInstruction::Operation, llvm::CastInst::CastOps>
            cast_operator_dict = {
                {ir::CastInstruction::Operation::Trunc, llvm::CastInst::Trunc},
                {ir::CastInstruction::Operation::ZExt, llvm::CastInst::ZExt},
                {ir::CastInstruction::Operation::SExt, llvm::CastInst::SExt},
                {ir::CastInstruction::Operation::FPToUI, llvm::CastInst::FPToUI},
                {ir::CastInstruction::Operation::FPToSI, llvm::CastInst::FPToSI},
                {ir::CastInstruction::Operation::UIToFP, llvm::CastInst::UIToFP},
                {ir::CastInstruction::Operation::SIToFP, llvm::CastInst::SIToFP},
                {ir::CastInstruction::Operation::FPTrunc, llvm::CastInst::FPTrunc},
                {ir::CastInstruction::Operation::FPExt, llvm::CastInst::FPExt},
                {ir::CastInstruction::Operation::PtrToInt, llvm::CastInst::PtrToInt},
                {ir::CastInstruction::Operation::IntToPtr, llvm::CastInst::IntToPtr},
                {ir::CastInstruction::Operation::BitCast, llvm::CastInst::BitCast},
                {ir::CastInstruction::Operation::AddrSpaceCast, llvm::CastInst::AddrSpaceCast},
            };
        PRAJNA_ASSERT(cast_operator_dict.count(ir_cast_instruction->operation));
        auto cast_op = cast_operator_dict[ir_cast_instruction->operation];
        auto operand_llvm_value = ir_cast_instruction->GetOperand(0)->llvm_value;
        if (!operand_llvm_value) {
            ir_cast_instruction->GetOperand(0)->ApplyVisitor(this->shared_from_this());
        }
        ir_cast_instruction->llvm_value =
            llvm::CastInst::Create(cast_op, ir_cast_instruction->GetOperand(0)->llvm_value,
                                   ir_cast_instruction->type->llvm_type, "", llvm_basic_block);
    }

    void Visit(std::shared_ptr<ir::CompareInstruction> ir_compare_instruction) override {
        auto llvm_basic_block = GetLlvmBasicBlock(ir_compare_instruction);
        std::unordered_map<std::string, llvm::CmpInst::OtherOps> cmp_inst_other_ops = {
            {"ICmp", llvm::CmpInst::OtherOps::ICmp}, {"FCmp", llvm::CmpInst::OtherOps::FCmp}};

        std::unordered_map<ir::CompareInstruction::Operation, llvm::CmpInst::OtherOps>
            llvm_compare_other_ops_dict = {
                {ir::CompareInstruction::Operation::FCMP_FALSE, llvm::CmpInst::OtherOps::FCmp},
                {ir::CompareInstruction::Operation::FCMP_OEQ, llvm::CmpInst::OtherOps::FCmp},
                {ir::CompareInstruction::Operation::FCMP_OGT, llvm::CmpInst::OtherOps::FCmp},
                {ir::CompareInstruction::Operation::FCMP_OGE, llvm::CmpInst::OtherOps::FCmp},
                {ir::CompareInstruction::Operation::FCMP_OLT, llvm::CmpInst::OtherOps::FCmp},
                {ir::CompareInstruction::Operation::FCMP_OLE, llvm::CmpInst::OtherOps::FCmp},
                {ir::CompareInstruction::Operation::FCMP_ONE, llvm::CmpInst::OtherOps::FCmp},
                {ir::CompareInstruction::Operation::FCMP_ORD, llvm::CmpInst::OtherOps::FCmp},
                {ir::CompareInstruction::Operation::FCMP_UNO, llvm::CmpInst::OtherOps::FCmp},
                {ir::CompareInstruction::Operation::FCMP_UEQ, llvm::CmpInst::OtherOps::FCmp},
                {ir::CompareInstruction::Operation::FCMP_UGT, llvm::CmpInst::OtherOps::FCmp},
                {ir::CompareInstruction::Operation::FCMP_UGE, llvm::CmpInst::OtherOps::FCmp},
                {ir::CompareInstruction::Operation::FCMP_ULT, llvm::CmpInst::OtherOps::FCmp},
                {ir::CompareInstruction::Operation::FCMP_ULE, llvm::CmpInst::OtherOps::FCmp},
                {ir::CompareInstruction::Operation::FCMP_UNE, llvm::CmpInst::OtherOps::FCmp},
                {ir::CompareInstruction::Operation::FCMP_TRUE, llvm::CmpInst::OtherOps::FCmp},
                {ir::CompareInstruction::Operation::ICMP_EQ, llvm::CmpInst::OtherOps::ICmp},
                {ir::CompareInstruction::Operation::ICMP_NE, llvm::CmpInst::OtherOps::ICmp},
                {ir::CompareInstruction::Operation::ICMP_UGT, llvm::CmpInst::OtherOps::ICmp},
                {ir::CompareInstruction::Operation::ICMP_UGE, llvm::CmpInst::OtherOps::ICmp},
                {ir::CompareInstruction::Operation::ICMP_ULT, llvm::CmpInst::OtherOps::ICmp},
                {ir::CompareInstruction::Operation::ICMP_ULE, llvm::CmpInst::OtherOps::ICmp},
                {ir::CompareInstruction::Operation::ICMP_SGT, llvm::CmpInst::OtherOps::ICmp},
                {ir::CompareInstruction::Operation::ICMP_SGE, llvm::CmpInst::OtherOps::ICmp},
                {ir::CompareInstruction::Operation::ICMP_SLT, llvm::CmpInst::OtherOps::ICmp},
                {ir::CompareInstruction::Operation::ICMP_SLE, llvm::CmpInst::OtherOps::ICmp},
            };

        // clang-format off
        std::unordered_map<ir::CompareInstruction::Operation, llvm::ICmpInst::Predicate>
            llvm_compare_predicator_dict = {
                {ir::CompareInstruction::Operation::FCMP_FALSE, llvm::CmpInst::Predicate::FCMP_FALSE},
                {ir::CompareInstruction::Operation::FCMP_OEQ, llvm::CmpInst::Predicate::FCMP_OEQ},
                {ir::CompareInstruction::Operation::FCMP_OGT, llvm::CmpInst::Predicate::FCMP_OGT},
                {ir::CompareInstruction::Operation::FCMP_OGE, llvm::CmpInst::Predicate::FCMP_OGE},
                {ir::CompareInstruction::Operation::FCMP_OLT, llvm::CmpInst::Predicate::FCMP_OLT},
                {ir::CompareInstruction::Operation::FCMP_OLE, llvm::CmpInst::Predicate::FCMP_OLE},
                {ir::CompareInstruction::Operation::FCMP_ONE, llvm::CmpInst::Predicate::FCMP_ONE},
                {ir::CompareInstruction::Operation::FCMP_ORD, llvm::CmpInst::Predicate::FCMP_ORD},
                {ir::CompareInstruction::Operation::FCMP_UNO, llvm::CmpInst::Predicate::FCMP_UNO},
                {ir::CompareInstruction::Operation::FCMP_UEQ, llvm::CmpInst::Predicate::FCMP_UEQ},
                {ir::CompareInstruction::Operation::FCMP_UGT, llvm::CmpInst::Predicate::FCMP_UGT},
                {ir::CompareInstruction::Operation::FCMP_UGE, llvm::CmpInst::Predicate::FCMP_UGE},
                {ir::CompareInstruction::Operation::FCMP_ULT, llvm::CmpInst::Predicate::FCMP_ULT},
                {ir::CompareInstruction::Operation::FCMP_ULE, llvm::CmpInst::Predicate::FCMP_ULE},
                {ir::CompareInstruction::Operation::FCMP_UNE, llvm::CmpInst::Predicate::FCMP_UNE},
                {ir::CompareInstruction::Operation::FCMP_TRUE, llvm::CmpInst::Predicate::FCMP_TRUE},
                {ir::CompareInstruction::Operation::ICMP_EQ, llvm::CmpInst::Predicate::ICMP_EQ},
                {ir::CompareInstruction::Operation::ICMP_NE, llvm::CmpInst::Predicate::ICMP_NE},
                {ir::CompareInstruction::Operation::ICMP_UGT, llvm::CmpInst::Predicate::ICMP_UGT},
                {ir::CompareInstruction::Operation::ICMP_UGE, llvm::CmpInst::Predicate::ICMP_UGE},
                {ir::CompareInstruction::Operation::ICMP_ULT, llvm::CmpInst::Predicate::ICMP_ULT},
                {ir::CompareInstruction::Operation::ICMP_ULE, llvm::CmpInst::Predicate::ICMP_ULE},
                {ir::CompareInstruction::Operation::ICMP_SGT, llvm::CmpInst::Predicate::ICMP_SGT},
                {ir::CompareInstruction::Operation::ICMP_SGE, llvm::CmpInst::Predicate::ICMP_SGE},
                {ir::CompareInstruction::Operation::ICMP_SLT, llvm::CmpInst::Predicate::ICMP_SLT},
                {ir::CompareInstruction::Operation::ICMP_SLE, llvm::CmpInst::Predicate::ICMP_SLE},
            };
        // clang-format on
        PRAJNA_ASSERT(llvm_compare_other_ops_dict.count(ir_compare_instruction->operation));
        PRAJNA_ASSERT(llvm_compare_predicator_dict.count(ir_compare_instruction->operation));
        auto llvm_compare_other_ops =
            llvm_compare_other_ops_dict[ir_compare_instruction->operation];
        auto llvm_compare_predicator =
            llvm_compare_predicator_dict[ir_compare_instruction->operation];
        ir_compare_instruction->llvm_value = llvm::CmpInst::Create(
            llvm_compare_other_ops, llvm_compare_predicator,
            ir_compare_instruction->GetOperand(0)->llvm_value,
            ir_compare_instruction->GetOperand(1)->llvm_value, "", llvm_basic_block);
    }

    void Visit(std::shared_ptr<ir::BinaryOperator> ir_binary_operator) override {
        auto llvm_basic_block = GetLlvmBasicBlock(ir_binary_operator);
        std::unordered_map<ir::BinaryOperator::Operation, llvm::BinaryOperator::BinaryOps>
            binary_operator_dict = {
                {ir::BinaryOperator::Operation::Add, llvm::BinaryOperator::BinaryOps::Add},
                {ir::BinaryOperator::Operation::Sub, llvm::BinaryOperator::BinaryOps::Sub},
                {ir::BinaryOperator::Operation::Mul, llvm::BinaryOperator::BinaryOps::Mul},
                {ir::BinaryOperator::Operation::SDiv, llvm::BinaryOperator::BinaryOps::SDiv},
                {ir::BinaryOperator::Operation::UDiv, llvm::BinaryOperator::BinaryOps::UDiv},
                {ir::BinaryOperator::Operation::SRem, llvm::BinaryOperator::BinaryOps::SRem},
                {ir::BinaryOperator::Operation::URem, llvm::BinaryOperator::BinaryOps::URem},
                {ir::BinaryOperator::Operation::And, llvm::BinaryOperator::BinaryOps::And},
                {ir::BinaryOperator::Operation::Or, llvm::BinaryOperator::BinaryOps::Or},
                {ir::BinaryOperator::Operation::Shl, llvm::BinaryOperator::BinaryOps::Shl},
                {ir::BinaryOperator::Operation::LShr, llvm::BinaryOperator::BinaryOps::LShr},
                {ir::BinaryOperator::Operation::AShr, llvm::BinaryOperator::BinaryOps::AShr},
                {ir::BinaryOperator::Operation::Xor, llvm::BinaryOperator::BinaryOps::Xor},
                {ir::BinaryOperator::Operation::Xor, llvm::BinaryOperator::BinaryOps::Xor},
                {ir::BinaryOperator::Operation::Xor, llvm::BinaryOperator::BinaryOps::Xor},
                {ir::BinaryOperator::Operation::FAdd, llvm::BinaryOperator::BinaryOps::FAdd},
                {ir::BinaryOperator::Operation::FSub, llvm::BinaryOperator::BinaryOps::FSub},
                {ir::BinaryOperator::Operation::FMul, llvm::BinaryOperator::BinaryOps::FMul},
                {ir::BinaryOperator::Operation::FDiv, llvm::BinaryOperator::BinaryOps::FDiv},
                {ir::BinaryOperator::Operation::FRem, llvm::BinaryOperator::BinaryOps::FRem},
            };

        PRAJNA_ASSERT(binary_operator_dict.count(ir_binary_operator->operation));
        auto llvm_binary_operator_operation = binary_operator_dict[ir_binary_operator->operation];
        ir_binary_operator->llvm_value = llvm::BinaryOperator::Create(
            llvm_binary_operator_operation, ir_binary_operator->GetOperand(0)->llvm_value,
            ir_binary_operator->GetOperand(1)->llvm_value, "", llvm_basic_block);
    }

    void Visit(std::shared_ptr<ir::ShuffleVector> ir_shuffle_vector) override {
        auto llvm_basic_block = GetLlvmBasicBlock(ir_shuffle_vector);
        ir_shuffle_vector->llvm_value = new llvm::ShuffleVectorInst(
            ir_shuffle_vector->Value()->llvm_value, ir_shuffle_vector->Mask()->llvm_value, "",
            llvm_basic_block);
    }

    llvm::BasicBlock *GetLlvmBasicBlock(std::shared_ptr<ir::Instruction> ir_instruction) {
        auto llvm_basic_block =
            static_cast<llvm::BasicBlock *>(ir_instruction->GetParentBlock()->llvm_value);
        PRAJNA_ASSERT(llvm_basic_block);
        return llvm_basic_block;
    }

    void Visit(std::shared_ptr<ir::Instruction> ir_instruction) override {
        if (auto ir_global_alloca = Cast<ir::GlobalAlloca>(ir_instruction)) {
            PRAJNA_UNREACHABLE;
        }
        ir_instruction->ApplyVisitor(this->shared_from_this());
        PRAJNA_ASSERT(false, ir_instruction->tag);
    }

    void Visit(std::shared_ptr<ir::InlineAsm> ir_inline_asm) override {
        ir_inline_asm->llvm_value =
            llvm::InlineAsm::get(static_cast<llvm::FunctionType *>(ir_inline_asm->type->llvm_type),
                                 ir_inline_asm->str_asm, ir_inline_asm->str_constrains,
                                 ir_inline_asm->has_side_effects, ir_inline_asm->is_align_stack);
        ;
    }

   private:
    prajna::ir::Target ir_target;
};

std::shared_ptr<ir::Module> LlvmCodegen(std::shared_ptr<ir::Module> ir_module,
                                        prajna::ir::Target ir_target) {
    auto llvm_codegen = LlvmCodegen::Create(ir_target);

    // emit type
    for (auto type : ir::global_context.created_types) {
        llvm_codegen->EmitType(type);
    }
    ir_module->ApplyVisitor(llvm_codegen);

    for (auto [ir_target, ir_sub_module] : ir_module->modules) {
        if (ir_sub_module == nullptr) continue;

        // 如果没有核函数, 则不生成. 因为不会被使用, gpu会把所用的的ir都拷贝过去
        if (std::ranges::none_of(ir_sub_module->functions,
                                 [](std::shared_ptr<ir::Function> ir_function) {
                                     return ir_function->annotation_dict.count("kernel");
                                 })) {
            continue;
        }

        LlvmCodegen(ir_sub_module, ir_target);
    }

    return ir_module;
}

inline void LinkLibdeviceBitcodeFiles(llvm::Module &llvm_module, llvm::LLVMContext &context,
                                      const std::string &isa_version = "906") {
    llvm::SMDiagnostic err;
    llvm::Linker linker(llvm_module);

    auto link_bc = [&](const std::string &path) {
        auto bc = llvm::parseIRFile(path, err, context);
        PRAJNA_ASSERT(bc,
                      "Failed to parse bitcode: " + path + ", error: " + err.getMessage().str());
        bool failed = linker.linkInModule(std::move(bc));
        PRAJNA_ASSERT(!failed, "Failed to link bitcode: " + path);
    };

    // 基础 math 函数库
    link_bc("/opt/rocm/amdgcn/bitcode/ocml.bc");

    // 编译选项配置符号
    link_bc("/opt/rocm/amdgcn/bitcode/oclc_finite_only_off.bc");
    link_bc("/opt/rocm/amdgcn/bitcode/oclc_daz_opt_off.bc");

    // ISA 版本控制（根据目标 GPU）
    link_bc("/opt/rocm/amdgcn/bitcode/oclc_isa_version_" + isa_version + ".bc");
}

std::shared_ptr<ir::Module> LlvmPass(std::shared_ptr<ir::Module> ir_module) {
    LLVMInitializeNativeTarget();
    LLVMInitializeNativeAsmPrinter();

    auto JTMB = llvm::orc::JITTargetMachineBuilder::detectHost();
    PRAJNA_VERIFY(JTMB);
    JTMB->setCPU("");
    JTMB->setRelocationModel(std::nullopt);
    JTMB->setCodeModel(std::nullopt);
    // JTMB->setCodeGenOptLevel(llvm::CodeGenOpt::);
    JTMB->addFeatures(std::vector<std::string>());
    auto TM = JTMB->createTargetMachine();
    PRAJNA_VERIFY(TM && TM.get());

    // Create the analysis managers.    llvm::LoopAnalysisManager LAM;
    llvm::FunctionAnalysisManager FAM;
    llvm::CGSCCAnalysisManager CGAM;
    llvm::ModuleAnalysisManager MAM;
    llvm::LoopAnalysisManager LAM;
    llvm::PassBuilder PB(TM.get().get());
    PB.registerModuleAnalyses(MAM);
    PB.registerCGSCCAnalyses(CGAM);
    PB.registerFunctionAnalyses(FAM);
    PB.registerLoopAnalyses(LAM);
    PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);

    llvm::ModulePassManager MPM = PB.buildPerModuleDefaultPipeline(llvm::OptimizationLevel::O2);

    MPM.run(*ir_module->llvm_module, MAM);

    if (GlobalConfig::Instance().get<bool>("prajna.dump_llvm_ir", false)) {
        ir_module->llvm_module->dump();
    }

    // TODO: 存在未知错误
    // auto verify_result = llvm::verifyModule(*ir_module->llvm_module, &llvm::errs());
    // PRAJNA_ASSERT(verify_result);

    auto ir_nvptx_module = ir_module->modules[prajna::ir::Target::nvptx];
    if (ir_nvptx_module && ir_nvptx_module->llvm_module) {
        llvm::Linker linker(*ir_nvptx_module->llvm_module);
        llvm::SMDiagnostic err;
#ifdef _WIN32
        // TODO: 后期增加cuda版本配置的功能, 目前使用最新版本
        auto cuda_path = std::filesystem::path(
            "C:\\Program Files\\NVIDIA GPU Computing "
            "Toolkit\\CUDA");
        auto libdevice_bc_path_postfix = std::filesystem::path("nvvm\\libdevice\\libdevice.10.bc");
        PRAJNA_ASSERT(std::filesystem::is_directory(cuda_path),
                      "Can't find cuda in " + cuda_path.string());
        auto cuda_version_dir_iter = std::filesystem::directory_iterator(cuda_path);
        auto dir_iter_end = std::filesystem::directory_iterator();
        // 使用最新那版本的cuda
        auto cuda_version_path = (*std::max_element(cuda_version_dir_iter, dir_iter_end)).path();
        auto libdevice_bc_path = cuda_version_path / libdevice_bc_path_postfix;
        auto uq_llvm_libdevice_module =
            llvm::parseIRFile(libdevice_bc_path.string(), err, static_llvm_context);
        PRAJNA_ASSERT(uq_llvm_libdevice_module, "Failed to parse " + libdevice_bc_path.string());
#else
        auto uq_llvm_libdevice_module = llvm::parseIRFile(
            "/usr/local/cuda/nvvm/libdevice/libdevice.10.bc", err, static_llvm_context);
        PRAJNA_ASSERT(uq_llvm_libdevice_module,
                      "\"/usr/local/cuda/nvvm/libdevice/libdevice.10.bc\" is not found");
#endif
        linker.linkInModule(std::move(uq_llvm_libdevice_module));

        prajna::codegen::LlvmPass(ir_nvptx_module);
    }

    auto ir_amdgpu_module = ir_module->modules[prajna::ir::Target::amdgpu];
    if (ir_amdgpu_module && ir_amdgpu_module->llvm_module) {
#ifdef _WIN32
        // ToDo
#else

        LinkLibdeviceBitcodeFiles(*ir_amdgpu_module->llvm_module, static_llvm_context);

#endif

        prajna::codegen::LlvmPass(ir_amdgpu_module);
    }

    return ir_module;
}

}  // namespace prajna::codegen
