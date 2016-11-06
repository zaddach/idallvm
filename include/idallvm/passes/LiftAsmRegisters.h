#ifndef _IDALLVM_PASSES_LIFTASMREGISTERS_H
#define _IDALLVM_PASSES_LIFTASMREGISTERS_H

#include <llvm/Pass.h>
#include <llvm/IR/Function.h>

struct LiftAsmRegisters : public llvm::FunctionPass
{
    static char ID;
    LiftAsmRegisters();
    bool runOnFunction(llvm::Function& f) override;
    void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;
};

#endif /* _IDALLVM_PASSES_LIFTASMREGISTERS_H */
