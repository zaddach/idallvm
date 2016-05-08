#include <map>
#include <list>
#include <sstream>
#include <iomanip>

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Constants.h>

#include <llvm/Support/raw_ostream.h>

#include <pro.h>
#include <funcs.hpp>
#include <xref.hpp>

#include "libqemu/qemu-lib-external.h"

#include "idallvm/ida_util.h"
#include "idallvm/msg.h"
#include "idallvm/libqemu.h"
#include "idallvm/llvm_passes.h"

struct TranslateBasicBlock;

struct TranslateBasicBlock
{
    bool completed;
    llvm::BasicBlock* llvmBasicBlock;
    uint64_t startAddress;
    uint64_t endAddress; //< Address of last instruction belonging to the BB
    std::list<TranslateBasicBlock*> successors;
    std::list<TranslateBasicBlock*> predecessors;

public:
    TranslateBasicBlock(llvm::Function* function, ea_t start) 
            : completed(false),
              startAddress(start) {
        std::stringstream ss;
        std::pair<ea_t, ea_t> bb = ida_get_basic_block(start);
        msg("start = 0x%08x, bb.first = 0x%08x\n", start, bb.first);
        assert(bb.first == start);
        endAddress = bb.second;
        ss << "0x" << std::setfill('0') << std::setw(8) << std::hex << start; 
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

llvm::Function* translate_function_to_llvm(ea_t ea)
{
    static std::map<ea_t, llvm::Function*> translationCache;
    ea = get_function_entry_point(ea);

    if (ea == BADADDR) {
        return NULL;
    }

    if (translationCache.find(ea) != translationCache.end()) {
        return translationCache.find(ea)->second;
    }

    //Create LLVM function
    llvm::Module* module = llvm::unwrap(ida_libqemu_get_module());
    llvm::Type* cpuStructPtrType = ida_libqemu_get_cpustruct_type();
    qstring ida_function_name;
    get_func_name2(&ida_function_name, ea);
    std::stringstream function_name;
    function_name << "idallvm_tmp_" << ida_function_name.c_str() << "_0x" << std::hex << ea;
    llvm::Function* function = llvm::cast<llvm::Function>(
        module->getOrInsertFunction(function_name.str(), llvm::Type::getVoidTy(module->getContext()), cpuStructPtrType, NULL));

    std::map<uint64_t, TranslateBasicBlock*> basicBlocks;
    std::list<TranslateBasicBlock*> blocksTodo;

    blocksTodo.push_back(new TranslateBasicBlock(function, ea));
    basicBlocks.insert(std::make_pair(ea, blocksTodo.front()));
    llvm::BasicBlock* unknownJumpTargetSink = llvm::BasicBlock::Create(function->getContext(), "unknownJumpTargetSink", function);
    while (!blocksTodo.empty()) {
        TranslateBasicBlock* bb = blocksTodo.front();
        blocksTodo.pop_front();

        //Translate all instructions inside the basic block and add calls to their functions
        for (ea_t cur_ea = bb->startAddress; cur_ea != BADADDR; cur_ea = next_head(cur_ea, bb->endAddress + 1)) {
            llvm::SmallVector<llvm::Value*, 1> args;
            llvm::Function* instFunction = translate_single_instruction(cur_ea);
            assert(instFunction && "LLVM function for assembler instruction should not be NULL");

            args.push_back(function->arg_begin());
            llvm::CallInst::Create(instFunction, args, "", bb->llvmBasicBlock);

            llvm::errs() << *bb->llvmBasicBlock << '\n';
            ;
        }

        //Generate case statement at end of basic block jumping to successors
        const int num_successors = get_num_crefs_from(bb->endAddress);
        llvm::GetElementPtrInst* gepInst = llvm::GetElementPtrInst::CreateInBounds(function->arg_begin(), get_pc_indices(function->getContext()), "ptr_PC", bb->llvmBasicBlock);
        llvm::errs() << *gepInst << '\n';
        llvm::LoadInst* loadInst = new llvm::LoadInst(gepInst, "PC", bb->llvmBasicBlock);
        llvm::errs() << *loadInst << '\n';
        llvm::SwitchInst* switchInst = llvm::SwitchInst::Create(loadInst, unknownJumpTargetSink, num_successors, bb->llvmBasicBlock);
        llvm::errs() << *switchInst << '\n';

        for (ea_t next_ea = get_first_cref_from(bb->endAddress); next_ea != BADADDR; next_ea = get_next_cref_from(bb->endAddress, next_ea)) {

            if (basicBlocks.find(next_ea) == basicBlocks.end()) {
                TranslateBasicBlock* next_bb = new TranslateBasicBlock(function, next_ea);
                basicBlocks.insert(std::make_pair(next_ea, next_bb));
                blocksTodo.push_back(next_bb);
            }
            TranslateBasicBlock* next_bb = basicBlocks.find(next_ea)->second;

            llvm::errs() << "Switch case type: " << *loadInst->getType() << ", next_bb: " << next_bb->llvmBasicBlock->getName() << '\n';
            switchInst->addCase(llvm::ConstantInt::get(llvm::cast<llvm::IntegerType>(loadInst->getType()), next_ea), next_bb->llvmBasicBlock);
            bb->successors.push_back(next_bb);
            next_bb->predecessors.push_back(bb); 
        }
        llvm::errs() << *switchInst << '\n';

        bb->completed = true;
    }

    return function;
}
