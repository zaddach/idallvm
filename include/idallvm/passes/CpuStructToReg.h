#ifndef _IDALLVM_PASSES_CPUSTRUCTTOREG_H
#define _IDALLVM_PASSES_CPUSTRUCTTOREG_H

#include <llvm/Pass.h>
#include <llvm/IR/BasicBlock.h>

struct CpuStructToReg : public llvm::BasicBlockPass
{
    static char ID;
    CpuStructToReg();
    bool runOnBasicBlock(llvm::BasicBlock& bb) override;
    void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;
};

#endif /* _IDALLVM_PASSES_CPUSTRUCTTOREG_H */
