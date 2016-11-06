#ifndef _IDALLVM_PASSES_LIFTASMRETURN_H
#define _IDALLVM_PASSES_LIFTASMRETURN_H

#include <llvm/Pass.h>
#include <llvm/IR/Function.h>

struct LiftAsmReturn : public llvm::FunctionPass
{
    static char ID;
    LiftAsmReturn();
    bool runOnFunction(llvm::Function& f) override;
    void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;
};

#endif /* _IDALLVM_PASSES_LIFTASMRETURN_H */
