#ifndef _IDALLVM_PASSES_H
#define _IDALLVM_PASSES_H

#include <llvm/IR/Function.h>
#include <llvm/Pass.h>

#include "idallvm/IdaBasicBlock.h"
#include "idallvm/IdaFlowChart.h"

llvm::Function* generateOpcodeCallsFromIda(IdaFlowChart& flowChart);
llvm::FunctionPass* createIdentifyCallsPass(void);
llvm::FunctionPass* createCpuStructToRegPass(void);
llvm::FunctionPass* createInlineOpcodeCallsPass(void);

#endif /* _IDALLVM_PASSES_H */
