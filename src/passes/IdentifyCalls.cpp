#include <list>

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Metadata.h>
#include <llvm/IR/Constants.h>

#include <pro.h>

#include "idallvm/passes/IdentifyCalls.h"
#include "idallvm/passes/InlineOpcodeCalls.h"
#include "idallvm/passes.h"
#include "idallvm/msg.h"

using std::list;

using llvm::Function;
using llvm::FunctionPass;
using llvm::Instruction;
using llvm::PointerType;
using llvm::BasicBlock;
using llvm::StoreInst;
using llvm::ConstantInt;
using llvm::CallInst;
using llvm::CastInst;
using llvm::MDNode;
using llvm::MDString;
using llvm::Value;
using llvm::SmallVector;
using llvm::RegisterPass;
using llvm::cast;
using llvm::dyn_cast;

char IdentifyCalls::ID = 0;

IdentifyCalls::IdentifyCalls() : FunctionPass(ID) 
{
}

bool IdentifyCalls::runOnFunction(Function& f)
{
    //TODO: Decide implementation based on DB processor architecture
    return runOnFunction_Arm(f);
}

bool IdentifyCalls::runOnFunction_Arm(Function& f)
{
    std::list<Instruction*> eraseList;
    //Function pointer of called assembler functions is of the same type as this function itself
    PointerType* functionPointerTy = f.getType();

    for (BasicBlock& bb : f) {
        ConstantInt* lastPcValue = nullptr;
        ConstantInt* lastLrValue = nullptr;
        StoreInst* lastPcStore = nullptr;
        StoreInst* lastLrStore = nullptr;
        bool firstInstructionInBB = true;
        for (Instruction& inst : bb) {
            switch (inst.getOpcode()) {
                case Instruction::Call: {
                    CallInst* call = cast<CallInst>(&inst);
                    if (!call->getCalledFunction() || !call->getCalledFunction()->hasName() || (call->getCalledFunction()->getName() != "tcg-llvm.opcode_start"))
                        break;
                    MDNode* md = call->getMetadata("tcg-llvm.pc");
                    assert(md && "PC metadata needs to be present");
                    assert(md->getNumOperands() > 0 && "Metadata needs to have at least one operand");
                    ConstantInt* ci = dyn_cast<ConstantInt>(md->getOperand(0));
                    assert(ci && "Instruction address needs to be set");
                    if (!firstInstructionInBB && (!lastPcValue ||  (lastPcValue->getZExtValue() != ci->getZExtValue()))) {
                        if (lastLrValue && (lastLrValue->getZExtValue() == ci->getZExtValue())) {
                            //This is dead sure a call (well, if the called object is a proper function)
                            Value* functionPointer = CastInst::Create(CastInst::IntToPtr, lastPcStore->getValueOperand(), functionPointerTy, "func_ptr", lastPcStore);
                            SmallVector<Value*, 1> args;
                            args.push_back(f.arg_begin());
                            CallInst* asmCall = CallInst::Create(
                                    functionPointer,
                                    args,
                                    "",
                                    lastPcStore);
                            asmCall->setMetadata("idallvm.asm_call", MDNode::get(f.getContext(), std::vector<Value*>()));
                            eraseList.push_back(lastPcStore);
                            eraseList.push_back(lastLrStore);
                        }
                        else {
                            //Not sure what this is: indirect jump?
                            MSG_WARN("Found indirect jump before 0x%08" PRIx64 ": Does not have return address in LR", ci->getZExtValue());
                            lastPcStore->setMetadata("idallvm.value_propagation_barrier", MDNode::get(f.getContext(), std::vector<Value*>()));
                        }


                        //There was another control flow in between, this is a barrier for
                        //value propagation
                    }
                    lastPcValue = nullptr;
                    firstInstructionInBB = false;
                    break;
                }
                case Instruction::Store: {
                    StoreInst* store = cast<StoreInst>(&inst);
                    MDNode* md = inst.getMetadata("tcg-llvm.env_access.register_name");
                    MDString* regName = (md && md->getNumOperands() > 0) ? dyn_cast<MDString>(md->getOperand(0)) : nullptr;
                    if (regName) {
                        if (regName->getString() == "pc") {
                            lastPcValue = dyn_cast<ConstantInt>(store->getValueOperand());
                            lastPcStore = store;
                        }
                        else if (regName->getString() == "lr") {
                            lastLrValue = dyn_cast<ConstantInt>(store->getValueOperand());
                            lastLrStore = store;
                        }
                    }
                    break;
                }
            }
        }
    }

    for (Instruction* inst : eraseList) {
        inst->eraseFromParent();
    }
    return true;
}

void IdentifyCalls::getAnalysisUsage(llvm::AnalysisUsage &AU) const
{
    AU.addRequired<InlineOpcodeCalls>();
    AU.setPreservesCFG();
    AU.addPreserved<InlineOpcodeCalls>();
}

FunctionPass* createIdentifyCallsPass(void)
{
    return new IdentifyCalls();
}



static RegisterPass<IdentifyCalls> passInfo("IdentifyCalls", "Identify calls from stores to the PC and replace the store instructions with annotated call instructions", false, false);
