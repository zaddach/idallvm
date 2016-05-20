/**
 * This module provides the function generateOpcodeCallsFromIda, which translates a function from IDA
 * into an LLVM function. The control flow structure of the IDA function is preserved, and instructions
 * are inserted as calls to a translation of the instruction to an LLVM function.
 * 
 * Control flow is preserved through switch statements at the end of every basic block wich either branch
 * to other basic blocks for known successors, or to a "catch-all" basic block who represents the unknown 
 * successor.
 */
#include "idallvm/passes.h"

#include <map>
#include <list>
#include <sstream>
#include <iomanip>

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Verifier.h>

#include <llvm/Support/raw_ostream.h>

#include <pro.h>
#include <funcs.hpp>

#include "libqemu/qemu-lib-external.h"

#include "idallvm/ida_util.h"
#include "idallvm/msg.h"
#include "idallvm/libqemu.h"
#include "idallvm/IdaBasicBlock.h"
#include "idallvm/IdaFlowChart.h"
#include "idallvm/IdaInstruction.h"

struct TranslateBasicBlock;

struct TranslateBasicBlock
{
    bool completed;
    llvm::BasicBlock* llvmBasicBlock;
    IdaBasicBlock& idaBasicBlock;
    std::list<TranslateBasicBlock*> successors;
    std::list<TranslateBasicBlock*> predecessors;

public:
    TranslateBasicBlock(llvm::Function* function, IdaBasicBlock& bb) 
            : completed(false),
              idaBasicBlock(bb) {
        std::stringstream ss;
        ss << "0x" << std::setfill('0') << std::setw(8) << std::hex << bb.getStartAddress(); 
        llvmBasicBlock = llvm::BasicBlock::Create(function->getContext(), ss.str(), function);
    }
};

/**
 * Translate a single instruction at address @ea to LLVM.
 */
static llvm::Function* translate_single_instruction(ea_t ea) 
{
    static ProcessorInformation processor_info = ida_get_processor_information();
    LLVMValueRef llvm_function;
    CodeFlags code_flags = {0};

    assert(isCode(getFlags(ea)) && "Needs to be a code head");

    switch (processor_info.processor) {
        case PROCESSOR_ARM:
            code_flags.arm.thumb = ida_arm_is_thumb_code(ea);
            break;
        default:
            MSG_WARN("Don't know how to set code flags for this processor.");
    }

    Libqemu_GenIntermediateCode(ea, code_flags, true, &llvm_function);
    return llvm::cast<llvm::Function>(llvm::unwrap(llvm_function));
}

static std::vector<llvm::Value*>& getPcIndices(llvm::LLVMContext& ctx)
{
    static std::vector<llvm::Value*> indices;

    if (indices.empty()) {
        RegisterInfo const* ri = Libqemu_GetRegisterInfoPc();
        indices.push_back(llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx), 0)); //Initial 0 needed for GEP
        for (size_t i = 0; i < ri->indices.count; ++i) {
            indices.push_back(llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx), ri->indices.indices[i]));
        }
    }

    return indices;
}

llvm::Function* generateOpcodeCallsFromIda(IdaFlowChart& flowChart)
{
    static std::map<ea_t, llvm::Function*> translationCache;

    auto translationCacheItr = translationCache.find(flowChart.getEntryBlock().getStartAddress());
    if (translationCacheItr != translationCache.end()) {
        return translationCacheItr->second;
    }

    //Create LLVM function
    llvm::Module* module = llvm::unwrap(Libqemu_GetModule());
    llvm::Type* cpuStructPtrType = Libqemu_GetCpustateType();
    std::stringstream function_name;
    function_name << "idallvm_tmp_" << flowChart.getFunctionName() << "_0x" << std::hex << flowChart.getStartAddress();
    llvm::Function* function = llvm::cast<llvm::Function>(
        module->getOrInsertFunction(function_name.str(), llvm::Type::getVoidTy(module->getContext()), cpuStructPtrType, NULL));

    std::map<uint64_t, TranslateBasicBlock*> basicBlocks;
    std::list<TranslateBasicBlock*> blocksTodo;

    blocksTodo.push_back(new TranslateBasicBlock(function, flowChart.getEntryBlock()));
    basicBlocks.insert(std::make_pair(flowChart.getEntryBlock().getStartAddress(), blocksTodo.front()));
    llvm::BasicBlock* unknownJumpTargetSink = llvm::BasicBlock::Create(function->getContext(), "unknownJumpTargetSink", function);
    llvm::ReturnInst::Create(function->getContext(), unknownJumpTargetSink);
    while (!blocksTodo.empty()) {
        TranslateBasicBlock* bb = blocksTodo.front();
        blocksTodo.pop_front();

        //Translate all instructions inside the basic block and add calls to their functions
        for (IdaInstruction& idaInst : bb->idaBasicBlock) {
            llvm::SmallVector<llvm::Value*, 1> args;
            llvm::Function* instFunction = translate_single_instruction(idaInst.getAddress());
            assert(instFunction && "LLVM function for assembler instruction should not be NULL");

            args.push_back(function->arg_begin());
            llvm::CallInst::Create(instFunction, args, "", bb->llvmBasicBlock);
        }

        //Generate case statement at end of basic block jumping to successors
        const int num_successors = bb->idaBasicBlock.getSuccessors().size();
        llvm::GetElementPtrInst* gepInst = llvm::GetElementPtrInst::CreateInBounds(function->arg_begin(), getPcIndices(function->getContext()), "ptr_PC", bb->llvmBasicBlock);
        llvm::LoadInst* loadInst = new llvm::LoadInst(gepInst, "PC", bb->llvmBasicBlock);
        llvm::SwitchInst* switchInst = llvm::SwitchInst::Create(loadInst, unknownJumpTargetSink, num_successors, bb->llvmBasicBlock);

        for (IdaBasicBlock& successor : bb->idaBasicBlock.getSuccessors()) {
            auto bbItr = basicBlocks.find(successor.getStartAddress());
            if (bbItr == basicBlocks.end()) {
                TranslateBasicBlock* next_bb = new TranslateBasicBlock(function, successor);
                bbItr = basicBlocks.insert(std::make_pair(successor.getStartAddress(), next_bb)).first;
                blocksTodo.push_back(next_bb);
            }

            switchInst->addCase(llvm::ConstantInt::get(llvm::cast<llvm::IntegerType>(loadInst->getType()), bbItr->second->idaBasicBlock.getStartAddress()), bbItr->second->llvmBasicBlock);
            bb->successors.push_back(bbItr->second);
            bbItr->second->predecessors.push_back(bb); 
        }


        bb->completed = true;
    }

    verifyFunction(*function, &llvm::errs());
    return function;
}
