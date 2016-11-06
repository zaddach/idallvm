#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Metadata.h>
#include <llvm/IR/Constants.h>

#include "idallvm/passes/LiftAsmReturn.h"
#include "idallvm/passes.h"


using llvm::Function;
using llvm::FunctionPass;
using llvm::RegisterPass;

char LiftAsmReturn::ID = 0;

LiftAsmReturn::LiftAsmReturn() : FunctionPass(ID) 
{
}

bool LiftAsmReturn::runOnFunction(Function& f)
{
    return true;
}


void LiftAsmReturn::getAnalysisUsage(llvm::AnalysisUsage &AU) const
{
    AU.setPreservesCFG();
}

FunctionPass* createLiftAsmReturnPass(void)
{
    return new LiftAsmReturn();
}

static RegisterPass<LiftAsmReturn> passInfo("LiftAsmReturn", "Transform PC assignments that represent function returns to proper returns", false, false);
