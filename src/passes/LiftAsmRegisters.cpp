#include <list>
#include <map>
#include <sstream>

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Metadata.h>
#include <llvm/IR/Constants.h>

#include "idallvm/passes/IdentifyCalls.h"
#include "idallvm/passes/InlineOpcodeCalls.h"
#include "idallvm/passes/LiftAsmRegisters.h"
#include "idallvm/passes.h"
#include "idallvm/libqemu.h"

#include "libqemu/register_info.h"

using std::map;
using std::list;
using std::make_pair;

using llvm::Function;
using llvm::BasicBlock;
using llvm::BasicBlockPass;
using llvm::Instruction;
using llvm::Value;
using llvm::LoadInst;
using llvm::StoreInst;
using llvm::MDNode;
using llvm::ConstantInt;
using llvm::RegisterPass;
using llvm::dyn_cast_or_null;
using llvm::cast;
using llvm::GetElementPtrInst;
using llvm::Type;
using llvm::AllocaInst;
using llvm::makeArrayRef;
using llvm::StringRef;
using llvm::FunctionPass;

char LiftAsmRegisters::ID = 0;

class Registers
{
private:
    Function& function;
    std::map< unsigned, std::pair< Value*, GetElementPtrInst* > > registers;

public:
    Registers(Function& f) : function(f) {}
    //TODO: offset and gep are redundant
    Value* getPtr(unsigned offset, GetElementPtrInst* gep) {

        auto itr = registers.find(offset);
        if (itr == registers.end()) {
            //Get register name
            std::string name;
            if (RegisterInfo const* reg = Libqemu_GetRegisterInfoByOffset(offset)) {
                name = reg->name;
            }
            else {
                std::stringstream ss;
                ss << "reg_" << std::hex << offset;
                name = ss.str();
            }

            //Get register type
            Type* type = gep->getType()->getElementType();

            Instruction* insertionPoint = function.getEntryBlock().getFirstInsertionPt();
            AllocaInst* alloca = new AllocaInst(type, name, insertionPoint);
            std::vector< Value* > indices(gep->idx_begin(), gep->idx_end());
            GetElementPtrInst* newgep = GetElementPtrInst::CreateInBounds(function.arg_begin(), makeArrayRef(indices), "ptr_" + name, insertionPoint);
            LoadInst* load = new LoadInst(newgep, "val_" + name, insertionPoint);
            new StoreInst(load, alloca, insertionPoint);
            registers.insert(std::make_pair(offset, std::make_pair(alloca, gep)));
            itr = registers.find(offset);
        }

        return itr->second.first;
    }

    void commit(Instruction* inst) {
        for (auto reg : registers) {
            StringRef name = reg.second.first->getName();
            LoadInst* load = new LoadInst(reg.second.second, "val_" + name, inst);
            std::vector< Value* > indices(reg.second.second->idx_begin(), reg.second.second->idx_end());
            GetElementPtrInst* gep = GetElementPtrInst::CreateInBounds(function.arg_begin(), makeArrayRef(indices), "ptr_" + name, inst);
            new StoreInst(load, gep, inst);
        }
    }
};

LiftAsmRegisters::LiftAsmRegisters() : FunctionPass(ID) 
{
}

bool LiftAsmRegisters::runOnFunction(Function& f)
{
    list<Instruction*> eraseList;
    Registers registers(f);
    std::list< Instruction* > commitPoints;

    for (BasicBlock& bb : f) {
        for (Instruction& inst : bb) {
            switch (inst.getOpcode()) {
                case Instruction::Load: {
                    if (MDNode* md = inst.getMetadata("tcg-llvm.env_access.offset")) {
                        ConstantInt * ci = cast<ConstantInt>(md->getOperand(0));
                        uint64_t offset = ci->getZExtValue();
                        Value* ptr = registers.getPtr(offset, cast<GetElementPtrInst>(inst.getOperand(0)));        
                        assert(ptr);
                        LoadInst* newLoad = new LoadInst(ptr, "val_" + ptr->getName(), &inst);
                        inst.replaceAllUsesWith(newLoad);
                        eraseList.push_back(&inst);
                    }
                    break;
                }
                case Instruction::Store: {
                    if (inst.getMetadata("idallvm.value_propagation_barrier")) {
                        commitPoints.push_back(&inst);
                        //An indirect jump or call or return or whatsoever
                        //insert store for all registers before and load after
                    }
                    else if (MDNode* md = inst.getMetadata("tcg-llvm.env_access.offset")) {
                        ConstantInt * ci = cast<ConstantInt>(md->getOperand(0));
                        uint64_t offset = ci->getZExtValue();
                        Value* ptr = registers.getPtr(offset, cast<GetElementPtrInst>(inst.getOperand(1)));
                        assert(ptr);
                        new StoreInst(inst.getOperand(0), ptr, &inst);
                        eraseList.push_back(&inst);
                    }
                    break;
                }
                case Instruction::Call: {
                    if (inst.getMetadata("idallvm.asm_call")) {
                        commitPoints.push_back(&inst);
                    }
                    break;
                }
            }
        }
    }

    for (Instruction* inst : commitPoints) {
        registers.commit(inst);
    }

    for (Instruction* inst : eraseList) {
        inst->eraseFromParent();
    }

    return true;
}

//    map< BasicBlock*, map< uint64_t, LoadInst* > > incomingValues;
//    map< BasicBlock*, map< uint64_t, StoreInst* > > outgoingValues;
//
//    for (BasicBlock& bb : f) {
//        map< uint64_t, LoadInst* >& curIncomingValues = incomingValues[&bb];
//        map< uint64_t, StoreInst* >& curOutgoingValues = outgoingValues[&bb];
//
//        for (Instruction& inst : bb) {
//            bool breakLoop = false;
//            switch (inst.getOpcode()) {
//                case Instruction::Call: 
//                    if (inst.getMetadata("idallvm.asm_call")) {
//                        goto stop_processing_instructions;
//                        breakLoop = true;
//                        break;
//                    }
//                case Instruction::Load: {
//                    MDNode* md = inst.getMetadata("tcg-llvm.env_access.offset");
//                    ConstantInt * ci = dyn_cast_or_null<ConstantInt>(md ? md->getOperand(0) : NULL);
//                    if (!ci) {
//                        break;
//                    }
//                    uint64_t offset = ci->getZExtValue();
//                    if (curIncomingValues.find(offset) == curIncomingValues.end()) {
//                        curIncomingValues.insert(make_pair(offset, cast<LoadInst>(&inst)));
//                    }
//                    break;
//                }
//            }
//        }
//stop_processing_instructions:
//
//        for (Instruction& inst : bb) {
//            switch (inst.getOpcode()) {
//                case Instruction::Call: 
//                    if (inst.getMetadata("idallvm.asm_call")) {
//                        curOutgoingValues.clear();
//                    }
//                    break;
//                case Instruction::Store: {
//                    StoreInst* store = cast<StoreInst>(&inst);
//                    MDNode* md = inst.getMetadata("tcg-llvm.env_access.offset");
//                    ConstantInt * ci = dyn_cast_or_null<ConstantInt>(md ? md->getOperand(0) : NULL);
//                    if (!ci) {
//                        break;
//                    }
//                    uint64_t offset = ci->getZExtValue();
//                    curOutgoingValues[offset] = store;
//                    break;
//                }
//            }
//        }
//    }
//
//    //TODO: Propagate values between basic blocks in flow direction
//
//    return true;
//}

void LiftAsmRegisters::getAnalysisUsage(llvm::AnalysisUsage &AU) const
{
    AU.addRequired<InlineOpcodeCalls>();
    AU.addRequired<IdentifyCalls>();
    AU.setPreservesCFG();
    AU.addPreserved<InlineOpcodeCalls>();
    AU.addPreserved<IdentifyCalls>();
}

FunctionPass* createLiftAsmRegistersPass(void)
{
    return new LiftAsmRegisters();
}

static RegisterPass<LiftAsmRegisters> passInfo("LiftAsmRegisters", "Remove successive stores and loads to the cpu state structure if there is no call in between", false, false);
