#include <list>
#include <map>

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Metadata.h>
#include <llvm/IR/Constants.h>

#include "idallvm/passes/IdentifyCalls.h"
#include "idallvm/passes/InlineOpcodeCalls.h"
#include "idallvm/passes/CpuStructToReg.h"
#include "idallvm/passes.h"

using std::map;
using std::list;
using std::make_pair;

using llvm::Function;
using llvm::BasicBlock;
using llvm::FunctionPass;
using llvm::Instruction;
using llvm::Value;
using llvm::LoadInst;
using llvm::StoreInst;
using llvm::MDNode;
using llvm::ConstantInt;
using llvm::RegisterPass;
using llvm::dyn_cast_or_null;
using llvm::cast;

char CpuStructToReg::ID = 0;

CpuStructToReg::CpuStructToReg() : FunctionPass(ID) 
{
}

bool CpuStructToReg::runOnFunction(Function& f)
{
    for (BasicBlock& bb : f) {
        list<Instruction*> eraseList;
        map< uint64_t, Value* > curValues;
        map< uint64_t, StoreInst* > latestStore;

        for (Instruction& inst : bb) {
            switch (inst.getOpcode()) {
                case Instruction::Call: {
                    if (inst.getMetadata("idallvm.asm_call")) {
                        latestStore.clear();
                        curValues.clear();
                    }
                    break;
                }
                case Instruction::Load: {
                    MDNode* md = inst.getMetadata("tcg-llvm.env_access.offset");
                    ConstantInt * ci = dyn_cast_or_null<ConstantInt>(md ? md->getOperand(0) : NULL);
                    if (!ci) {
                        break;
                    }
                    uint64_t offset = ci->getZExtValue();
                    map< uint64_t, Value* >::iterator prevValue = curValues.find(offset);
                    if (prevValue == curValues.end()) {
                        curValues.insert(make_pair(offset, &inst));
                    }
                    else {
                        inst.replaceAllUsesWith(prevValue->second);
                        eraseList.push_back(&inst);
                    }

                    break;
                }
                case Instruction::Store: {
                    if (inst.getMetadata("idallvm.value_propagation_barrier")) {
                        latestStore.clear();
                        curValues.clear();
                    }
                    else {
                        StoreInst* store = cast<StoreInst>(&inst);
                        MDNode* md = inst.getMetadata("tcg-llvm.env_access.offset");
                        ConstantInt * ci = dyn_cast_or_null<ConstantInt>(md ? md->getOperand(0) : NULL);
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
        }

        for (Instruction* inst : eraseList) {
            inst->eraseFromParent();
        }

    }

    map< BasicBlock*, map< uint64_t, LoadInst* > > incomingValues;
    map< BasicBlock*, map< uint64_t, StoreInst* > > outgoingValues;

    for (BasicBlock& bb : f) {
        map< uint64_t, LoadInst* >& curIncomingValues = incomingValues[&bb];
        map< uint64_t, StoreInst* >& curOutgoingValues = outgoingValues[&bb];

        for (Instruction& inst : bb) {
            bool breakLoop = false;
            switch (inst.getOpcode()) {
                case Instruction::Call: 
                    if (inst.getMetadata("idallvm.asm_call")) {
                        breakLoop = true;
                        break;
                    }
                case Instruction::Load: {
                    MDNode* md = inst.getMetadata("tcg-llvm.env_access.offset");
                    ConstantInt * ci = dyn_cast_or_null<ConstantInt>(md ? md->getOperand(0) : NULL);
                    if (!ci) {
                        break;
                    }
                    uint64_t offset = ci->getZExtValue();
                    if (curIncomingValues.find(offset) == curIncomingValues.end()) {
                        curIncomingValues.insert(make_pair(offset, cast<LoadInst>(&inst)));
                    }
                    break;
                }
            }
            if (breakLoop) {
                break;
            }
        }

        for (Instruction& inst : bb) {
            switch (inst.getOpcode()) {
                case Instruction::Call: 
                    if (inst.getMetadata("idallvm.asm_call")) {
                        curOutgoingValues.clear();
                    }
                    break;
                case Instruction::Store: {
                    StoreInst* store = cast<StoreInst>(&inst);
                    MDNode* md = inst.getMetadata("tcg-llvm.env_access.offset");
                    ConstantInt * ci = dyn_cast_or_null<ConstantInt>(md ? md->getOperand(0) : NULL);
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

void CpuStructToReg::getAnalysisUsage(llvm::AnalysisUsage &AU) const
{
    AU.addRequired<InlineOpcodeCalls>();
    AU.addRequired<IdentifyCalls>();
    AU.setPreservesCFG();
    AU.addPreserved<InlineOpcodeCalls>();
    AU.addPreserved<IdentifyCalls>();
}

FunctionPass* createCpuStructToRegPass(void)
{
    return new CpuStructToReg();
}

static RegisterPass<CpuStructToReg> passInfo("CpuStructToReg", "Remove successive stores and loads to the cpu state structure if there is no call in between", false, false);
