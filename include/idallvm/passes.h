#ifndef _IDALLVM_PASSES_H
#define _IDALLVM_PASSES_H

#include <llvm/IR/Function.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/Pass.h>

#include "idallvm/IdaBasicBlock.h"
#include "idallvm/IdaFlowChart.h"

llvm::Function* generateOpcodeCallsFromIda(IdaFlowChart& flowChart);
llvm::FunctionPass* createIdentifyCallsPass(void);
llvm::BasicBlockPass* createCpuStructToRegPass(void);
llvm::FunctionPass* createInlineOpcodeCallsPass(void);
llvm::FunctionPass* createFixBasicBlockEdgesPass(void);
llvm::FunctionPass* createLiftAsmStackPass(void);

#endif /* _IDALLVM_PASSES_H */
