#ifndef _IDALLVM_PASSES_CPUSTRUCTTOREG_H
#define _IDALLVM_PASSES_CPUSTRUCTTOREG_H

#include <llvm/Pass.h>
#include <llvm/IR/Function.h>

struct CpuStructToReg : public llvm::FunctionPass
{
    static char ID;
    CpuStructToReg();
    bool runOnFunction(llvm::Function& f) override;
    void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;
};

#endif /* _IDALLVM_PASSES_CPUSTRUCTTOREG_H */
