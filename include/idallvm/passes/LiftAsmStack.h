#ifndef _IDALLVM_PASSES_LIFTASMSTACK_H
#define _IDALLVM_PASSES_LIFTASMSTACK_H

#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"

#include <list>
#include <map>

namespace llvm {
	class CallInst;
}


  struct LiftAsmStack : public llvm::FunctionPass {
	struct AccessInfo  {
		enum LoadStore {LOAD, STORE};
		int64_t stackDepth;
        llvm::Type* type;
		LoadStore loadStore;
	};
    static char ID;

    LiftAsmStack();

    virtual bool runOnFunction(llvm::Function &f);
	virtual void getAnalysisUsage(llvm::AnalysisUsage& info) const;

	bool hasStaticStackFrame() {return !m_dynamicStackFrameSize;}
	bool hasStackFrameError() {return m_stackFrameError;}
    llvm::StructType* getStackFrameType();
	int64_t getMinStackDepth() {return m_minStackDepth;}
	int64_t getMaxStackDepth() {return m_maxStackDepth;}
	std::map<llvm::CallInst*, AccessInfo>::const_iterator stackAccesses_begin() {return m_stackDepth.begin();}
	std::map<llvm::CallInst*, AccessInfo>::const_iterator stackAccesses_end() {return m_stackDepth.end();}
	bool isStackAccess(llvm::Instruction* inst);
	const AccessInfo& getAccessInfo(llvm::Instruction* inst);

  private:
	std::map<llvm::CallInst*, AccessInfo> m_stackDepth;
	int64_t m_maxStackDepth;
	int64_t m_minStackDepth;
	bool m_dynamicStackFrameSize;
	bool m_stackFrameError;
  };
#endif /* _IDALLVM_PASSES_LIFTASMSTACK_H */
