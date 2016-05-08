#ifndef _IDALLVM_LLVMPASSES_H
#define _IDALLVM_LLVMPASSES_H

#include <llvm/IR/Function.h>

#include <pro.h>

llvm::Function* translate_function_to_llvm(ea_t ea);

#endif /* _IDALLVM_LLVMPASSES_H */
