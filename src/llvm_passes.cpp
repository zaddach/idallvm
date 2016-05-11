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
#include <llvm/Transforms/Utils/Cloning.h>

#include <llvm/Support/raw_ostream.h>

#include <pro.h>
#include <funcs.hpp>
#include <xref.hpp>

#include "libqemu/qemu-lib-external.h"

#include "idallvm/ida_util.h"
#include "idallvm/msg.h"
#include "idallvm/libqemu.h"
#include "idallvm/llvm_passes.h"
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

static ea_t get_function_entry_point(ea_t ea)
{
    func_t* ida_function = get_func(ea);

    if (!ida_function) {
        MSG_WARN("EA 0x%08x does not belong to a function!", ea);
        return BADADDR;
    }

    return ida_function->startEA;
}

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

    ida_libqemu_gen_intermediate_code(ea, code_flags, true, &llvm_function);
    return llvm::cast<llvm::Function>(llvm::unwrap(llvm_function));
}

/**
 * Get the number of code references from an instruction.
 * @param from Address of the instruction.
 * @return Number of code references from this instruction.
 */
static int get_num_crefs_from(ea_t from) 
{
    int num = 0;
    for (ea_t next_ea = get_first_cref_from(from); next_ea != BADADDR; next_ea = get_next_cref_from(from, next_ea)) {
        num += 1;
    }

    return num;
}

static std::vector<llvm::Value*>& get_pc_indices(llvm::LLVMContext& ctx) 
{
    static std::vector<llvm::Value*> indices;

    if (indices.empty()) {
        std::vector<unsigned>& tmpIndices = ida_libqemu_get_pc_indices();
        
        //LLVM always needs an initial 0 index to dereference the pointer array element
        indices.push_back(llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx), 0));
        for (unsigned idx : tmpIndices) {
            indices.push_back(llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx), idx));
        }
    }

    return indices;
}


static bool restoreCallInst(llvm::Function& f, ea_t ea)
{
    //TODO: implement
    return true;
}

static bool CpuStructToReg(llvm::Function& f)
{
    for (llvm::BasicBlock& bb : f) {
        std::list<llvm::Instruction*> eraseList;
        std::map< uint64_t, llvm::Value* > curValues;
        std::map< uint64_t, llvm::StoreInst* > latestStore;

        for (llvm::Instruction& inst : bb) {
            switch (inst.getOpcode()) {
                case llvm::Instruction::Call: {
                    if (inst.getMetadata("idallvm.asm_call")) {
                        latestStore.clear();
                        curValues.clear();
                    }
                    break;
                }
                case llvm::Instruction::Load: {
                    llvm::MDNode* md = inst.getMetadata("tcg-llvm.env_access.offset");
                    llvm::ConstantInt * ci = llvm::dyn_cast_or_null<llvm::ConstantInt>(md ? md->getOperand(0) : NULL);
                    if (!ci) {
                        break;
                    }
                    uint64_t offset = ci->getZExtValue();
                    std::map< uint64_t, llvm::Value* >::iterator prevValue = curValues.find(offset);
                    if (prevValue == curValues.end()) {
                        curValues.insert(std::make_pair(offset, &inst));
                    }
                    else {
                        inst.replaceAllUsesWith(prevValue->second);
                        eraseList.push_back(&inst);
                    }

                    break;
                }
                case llvm::Instruction::Store: {
                    llvm::StoreInst* store = llvm::cast<llvm::StoreInst>(&inst);
                    llvm::MDNode* md = inst.getMetadata("tcg-llvm.env_access.offset");
                    llvm::ConstantInt * ci = llvm::dyn_cast_or_null<llvm::ConstantInt>(md ? md->getOperand(0) : NULL);
                    if (!ci) {
                        break;
                    }
                    uint64_t offset = ci->getZExtValue();
                    curValues[offset] = store->getValueOperand();
                    auto lastStore = latestStore.find(offset);
                    if (lastStore != latestStore.end()) {
                        eraseList.push_back(lastStore->second);
                    }
                    latestStore[offset] = store;
                    break;
                }
            }
        }

        for (llvm::Instruction* inst : eraseList) {
            inst->eraseFromParent();
        }

    }

    std::map< llvm::BasicBlock*, std::map< uint64_t, llvm::LoadInst* > > incomingValues;
    std::map< llvm::BasicBlock*, std::map< uint64_t, llvm::StoreInst* > > outgoingValues;

    for (llvm::BasicBlock& bb : f) {
        std::map< uint64_t, llvm::LoadInst* >& curIncomingValues = incomingValues[&bb];
        std::map< uint64_t, llvm::StoreInst* >& curOutgoingValues = outgoingValues[&bb];

        for (llvm::Instruction& inst : bb) {
            bool breakLoop = false;
            switch (inst.getOpcode()) {
                case llvm::Instruction::Call: 
                    if (inst.getMetadata("idallvm.asm_call")) {
                        breakLoop = true;
                        break;
                    }
                case llvm::Instruction::Load: {
                    llvm::MDNode* md = inst.getMetadata("tcg-llvm.env_access.offset");
                    llvm::ConstantInt * ci = llvm::dyn_cast_or_null<llvm::ConstantInt>(md ? md->getOperand(0) : NULL);
                    if (!ci) {
                        break;
                    }
                    uint64_t offset = ci->getZExtValue();
                    if (curIncomingValues.find(offset) == curIncomingValues.end()) {
                        curIncomingValues.insert(std::make_pair(offset, llvm::cast<llvm::LoadInst>(&inst)));
                    }
                    break;
                }
            }
            if (breakLoop) {
                break;
            }
        }

        for (llvm::Instruction& inst : bb) {
            switch (inst.getOpcode()) {
                case llvm::Instruction::Call: 
                    if (inst.getMetadata("idallvm.asm_call")) {
                        curOutgoingValues.clear();
                        break;
                    }
                case llvm::Instruction::Store: {
                    llvm::StoreInst* store = llvm::cast<llvm::StoreInst>(&inst);
                    llvm::MDNode* md = inst.getMetadata("tcg-llvm.env_access.offset");
                    llvm::ConstantInt * ci = llvm::dyn_cast_or_null<llvm::ConstantInt>(md ? md->getOperand(0) : NULL);
                    if (!ci) {
                        break;
                    }
                    uint64_t offset = ci->getZExtValue();
                    curOutgoingValues[offset] = store;
                    break;
                }
            }
        }
    }

    //TODO: Propagate values between basic blocks in flow direction

    return true;
}

static bool inlineInstructionCalls(llvm::Function& f)
{
    std::list<llvm::CallInst*> instructionsToInline;

    for (llvm::BasicBlock& bb : f) {
        for (llvm::Instruction& inst : bb) {
            if (llvm::CallInst* callInst = llvm::dyn_cast<llvm::CallInst>(&inst)) {
                instructionsToInline.push_back(callInst);
            }
        }
    }

    for (llvm::CallInst* callInst : instructionsToInline) {
        llvm::errs() << *callInst << '\n';
        llvm::InlineFunctionInfo inlineFunctionInfo;
        llvm::InlineFunction(callInst, inlineFunctionInfo);
    }
    verifyFunction(f, &llvm::errs());

    return true;
}


static llvm::Function* generateOpcodeCallsFromIda(IdaFlowChart& flowChart)
{
    static std::map<ea_t, llvm::Function*> translationCache;

    auto translationCacheItr = translationCache.find(flowChart.getEntryBlock().getStartAddress());
    if (translationCacheItr != translationCache.end()) {
        return translationCacheItr->second;
    }

    //Create LLVM function
    llvm::Module* module = llvm::unwrap(ida_libqemu_get_module());
    llvm::Type* cpuStructPtrType = ida_libqemu_get_cpustruct_type();
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
        llvm::GetElementPtrInst* gepInst = llvm::GetElementPtrInst::CreateInBounds(function->arg_begin(), get_pc_indices(function->getContext()), "ptr_PC", bb->llvmBasicBlock);
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

llvm::Function* translate_function_to_llvm(ea_t ea) 
{
    IdaFlowChart flowChart(ea);
    llvm::Function* function = generateOpcodeCallsFromIda(flowChart);
    if (function) {
        inlineInstructionCalls(*function);
    }
//    llvm::Function* instFunction = translate_single_instruction(ea);
//    llvm::errs() << *instFunction << '\n';
    return function;
}
