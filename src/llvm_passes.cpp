#include <llvm/IR/Function.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Module.h>

#include <llvm/Support/raw_ostream.h>

#include "idallvm/IdaBasicBlock.h"
#include "idallvm/IdaFlowChart.h"
#include "idallvm/passes.h"
#include "idallvm/libqemu.h"

using llvm::Function;
using llvm::legacy::FunctionPassManager;
using llvm::unwrap;

Function* translate_function_to_llvm(ea_t ea) 
{
    static std::unique_ptr<FunctionPassManager> fpm;
    IdaFlowChart flowChart(ea);
    Function* function = generateOpcodeCallsFromIda(flowChart);
    if (!fpm) {
        fpm.reset(new FunctionPassManager(unwrap(Libqemu_GetModule())));
        fpm->add(createInlineOpcodeCallsPass());
        fpm->add(createIdentifyCallsPass());
        fpm->add(createCpuStructToRegPass());
    }

    if (function) {
        fpm->run(*function);
    }

    return function;
}
