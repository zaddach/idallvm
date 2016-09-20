#include "idallvm/passes/LiftAsmStack.h"
#include "idallvm/libqemu.h"
#include "idallvm/msg.h"

#include "llvm/IR/Instructions.h"
#include "llvm/IR/Constants.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/CFG.h"
#include "llvm/ADT/DepthFirstIterator.h"

using llvm::Function;
using llvm::Instruction;
using llvm::CallInst;
using llvm::LoadInst;
using llvm::StoreInst;
using llvm::Value;
using llvm::MDNode;
using llvm::ConstantInt;
using llvm::BinaryOperator;
using llvm::CastInst;
using llvm::BasicBlock;
using llvm::AnalysisUsage;
using llvm::RegisterPass;
using llvm::cast;
using llvm::dyn_cast;
using llvm::df_begin;
using llvm::df_end;

char LiftAsmStack::ID = 0;

LiftAsmStack::LiftAsmStack() : llvm::FunctionPass(ID)  {
}

bool LiftAsmStack::isStackAccess(Instruction* inst) {
	CallInst* call = dyn_cast<CallInst>(inst);

	if (!call) {
		return false;
	}

	return m_stackDepth.find(call) != m_stackDepth.end();
}

const LiftAsmStack::AccessInfo& LiftAsmStack::getAccessInfo(Instruction* inst)  {
	CallInst* call = dyn_cast<CallInst>(inst);

	assert(call);
	assert(isStackAccess(call));

	return m_stackDepth[call];
}

struct BBStackState
{
    uint64_t entryStackDepth;
    uint64_t exitStackDepth;
};

std::map< unsigned, std::function< int64_t(int64_t, int64_t) > > OPERATIONS = {
    {Instruction::Add, [] (int64_t x, int64_t y)->int64_t {return x + y;}},
    {Instruction::Sub, [] (int64_t x, int64_t y)->int64_t {return x - y;}}};

static bool backtrackStackOffset(Value* value, uint64_t envSpOffset, std::map< LoadInst*, int64_t > const& stackDepth, int64_t& stackOffset)
{
    if (Instruction* inst = dyn_cast< Instruction >(value)) {
        switch (inst->getOpcode()) {
            case Instruction::Load: {
                MDNode* md = inst->getMetadata("tcg-llvm.env_access.offset");
                if (md && cast< ConstantInt >(md->getOperand(0))->getZExtValue() == envSpOffset) {
                    auto loadStackDepth = stackDepth.find(cast< LoadInst >(inst));
                    if (loadStackDepth != stackDepth.end()) {
                        stackOffset = loadStackDepth->second;
                        return true;
                    }
                    else {
                        outs() << "LiftAsmStack - ERROR: Load instruction " << *inst << " in BB " << inst->getParent()->getName() 
                               << " has not been labeled with stack depth" << '\n';
                        return false;
                    }
                }
                else {
                    outs() << "LiftAsmStack - ERROR: Load instruction not referring to SP while back-tracking SP: " << *inst << '\n';
                    return false;
                }
                break;
            }
            case Instruction::Add: 
            case Instruction::Sub: {
                BinaryOperator* binOp = cast< BinaryOperator >(inst);
                ConstantInt* constOperand;
                Value* spOperand;
                
                if ((constOperand = dyn_cast< ConstantInt >(inst->getOperand(0)))) {
                    spOperand = inst->getOperand(1);
                }
                else if ((constOperand = dyn_cast< ConstantInt >(inst->getOperand(1)))) {
                    spOperand = inst->getOperand(0);
                }
                else {
                    outs() << "LiftAsmStack - ERROR: Found binary operation in BB " << inst->getParent()->getName() 
                           << " where second operand is not constant: " << *inst << '\n';
                    return false;
                }

                auto operation = OPERATIONS.find(inst->getOpcode());
                assert(operation != OPERATIONS.end() && "Operation not found");
                if (backtrackStackOffset(spOperand, envSpOffset, stackDepth, stackOffset)) {
                    stackOffset = operation->second(stackOffset, constOperand->getSExtValue());
                    return true;
                }
                else {
                    return false;
                }
                break;
            }
            default: {
                outs() << "LiftAsmStack - ERROR: Backtracking not implemented for instruction " << *inst << " in BB " << inst->getParent()->getName() << '\n';
                return false;
            }
        }
    }
    else {
        outs() << "Unknown value " << *value << "; cannot backtrack." << '\n';
        return false;
    }
}

static bool isStackRelative(Value* value, uint64_t envSpOffset) {
    if (BinaryOperator* binOp = dyn_cast< BinaryOperator >(value)) {
        return isStackRelative(binOp->getOperand(0), envSpOffset) || isStackRelative(binOp->getOperand(1), envSpOffset);
    }
    else if (LoadInst* load = dyn_cast< LoadInst >(value)) {
        MDNode* md = load->getMetadata("tcg-llvm.env_access.offset");
        if (md && cast< ConstantInt >(md->getOperand(0))->getZExtValue() == envSpOffset) {
            return true;
        }
        else {
            return false;
        }
    }
    else if (CastInst* cast = dyn_cast< CastInst >(value)) {
        return isStackRelative(cast->getOperand(0), envSpOffset);
    }
    else if (ConstantInt* constant = dyn_cast< ConstantInt >(value)) {
        return false;
    }
    else {
        outs() << "LiftAsmStack - ERROR: Cannot tell anything about Value " << *value << " in isStackRelative" << '\n';
        return false;
    }
}


//Start at entry BB by finding load of stack pointer
//Assign value 0 to max stack depth
//Follow all uses of stack pointer value
//If use is a phi node, spawn a new state
//If 
bool LiftAsmStack::runOnFunction(Function &f) {
    RegisterInfo const* ri = Libqemu_GetRegisterInfoSp();
    assert(ri && "Cannot get SP register info");

    ///Stack depth at the beginning (before first inst) and end (after last inst) for each BB
    std::map< BasicBlock*, BBStackState > bbStackDepth;
    ///Stack depth at every load instruction for the SP register
    std::map< LoadInst*, int64_t > loadStackDepth;
    //In theory this is not necessary, as entryStackDepth should have been initialized to 0
    bbStackDepth[&f.getEntryBlock()].entryStackDepth = 0;
    ///List of accesses to stack fields
    std::list< std::pair< CallInst*, int64_t > > stackAccesses;

    for (auto bbItr = df_begin(&f.getEntryBlock()), bbEnd = df_end(&f.getEntryBlock()); bbItr != bbEnd; ++bbItr) {
        BBStackState& stackState = bbStackDepth[*bbItr];
        int64_t curStackDepth = stackState.entryStackDepth;
        for (Instruction& inst : **bbItr) {
            switch (inst.getOpcode()) {
                case Instruction::Load: {
                    //Load instructions for the SP register need to be labeled with the current stack height
                    MDNode* md = inst.getMetadata("tcg-llvm.env_access.offset");
                    if (md && cast< ConstantInt >(md->getOperand(0))->getZExtValue() == ri->offset) {
                        loadStackDepth[cast< LoadInst >(&inst)] = curStackDepth;
                    }
                    break;
                }
                case Instruction::Store: {
                    //Store instructions need to be traced back to SP load instructions to calculate stack height changes
                    StoreInst* store = cast< StoreInst >(&inst);
                    MDNode* md = inst.getMetadata("tcg-llvm.env_access.offset");
                    if (md && cast< ConstantInt >(md->getOperand(0))->getZExtValue() == ri->offset) {
                        if (backtrackStackOffset(store->getValueOperand(), ri->offset, loadStackDepth, curStackDepth)) {
                        }
                        else {
                            outs() << "LiftAsmStack - ERROR: Cannot backtrack stack offset at store inst " << inst <<  " in BB " << inst.getParent()->getName() 
                                   << ". Aborting analysis of function " << f.getName() << '\n';
                            return true;
                        }
                    }
                    break;
                }
                //TODO: Think about scenarios how a pointer to the stack frame can escape (function calls, store with variable index, ...)
                //Any stack value that has its address taken needs to be handled very carefully (could be variable, array, printf magic, ...)
                case Instruction::Call: {
                    //Calls to helper_libqemu_st/helper_libqemu_ld with negative offset relative to SP at function beginning (stack growing downwards) 
                    //need to be redirected to the newly generated stack frame
                    //Calls with positive offsets need to be converted into function arguments (respecting the fn's calling convention)
                    CallInst* call = cast< CallInst >(&inst);
                    if (call->getCalledFunction() && call->getCalledFunction()->hasName() && 
                        (call->getCalledFunction()->getName() == "helper_libqemu_st" || call->getCalledFunction()->getName() == "helper_libqemu_ld") && 
                        isStackRelative(call->getArgOperand(1), ri->offset)) 
                    {
                        int64_t frameOffset = 0;
                        if (backtrackStackOffset(call->getArgOperand(0), ri->offset, loadStackDepth, frameOffset)) {
                            stackAccesses.push_back(std::make_pair(call, frameOffset));
                        }
                        else {
                            outs() << "LiftAsmStack - ERROR: " << inst 
                                   << " is accessing a stack-relative value, but backtracking cannot determine a constant offset in BB "
                                   << inst.getParent()->getName() << '\n';
                            continue;
                        }
                    }
                    break;
                }
            }
        }
        //TODO: For calculating min and max stack depth in this function, accesses to stack memory needs to be taken into account,
        //but also stack register values passed to called functions

        for (unsigned i = 0, e = (*bbItr)->getTerminator()->getNumSuccessors(); i < e; ++i) {
            auto bbStackDepthItr = bbStackDepth.find((*bbItr)->getTerminator()->getSuccessor(i));
            if (bbStackDepthItr != bbStackDepth.end()) {
                if (bbStackDepthItr->second.entryStackDepth != curStackDepth) {
                    outs() << "Mismatch between stack depth at end of BB " << (*bbItr)->getName()
                           << " (" << curStackDepth << ") and entry of BB " << bbStackDepthItr->first->getName()
                           << " (" << bbStackDepthItr->second.entryStackDepth << ")" << '\n';
                    outs() << "Aborting analysis." << '\n';
                    return true;
                }
            }
        }
    }

//	m_maxStackDepth = 0;
//	m_minStackDepth = 0;
//	m_dynamicStackFrameSize = false;
//	m_stackFrameError = false;
//    m_stackDepth.clear();
//
//	while (!valuesToVisit.empty())  {
//		Value* curValue = valuesToVisit.front().first;
//		int64_t stackDepth = valuesToVisit.front().second;
//		valuesToVisit.pop_front();
//
//		if (stackDepth < m_maxStackDepth)  {
//			m_maxStackDepth = stackDepth;
//		}
//		if (stackDepth > m_minStackDepth) {
//			m_minStackDepth = stackDepth;
//		}
//
//		for (Value::use_iterator useItr = curValue->use_begin(), useEnd = curValue->use_end(); useItr != useEnd; useItr++)  {
//			PHINode* phi = dyn_cast<PHINode>(*useItr);
//			if (phi)  {
//				if (visitedPHINodes.find(phi) == visitedPHINodes.end())  {
//					visitedPHINodes.insert(std::make_pair(phi, stackDepth));
//					valuesToVisit.push_back(std::make_pair(phi, stackDepth));
//				}
//				else {
//					if (visitedPHINodes[phi] != stackDepth)  {
//						m_dynamicStackFrameSize = true;	
//					}
//				}
//				continue;
//			}
//
//			BinaryOperator* op = dyn_cast<BinaryOperator>(*useItr);
//			if (op)  {
//				Value* otherOperand;
//				assert(op->getNumOperands() == 2);
//				if (op->getOperand(0) == curValue)  {
//					otherOperand = op->getOperand(1);
//				} else  {
//					assert(op->getOperand(1) == curValue);
//					otherOperand = op->getOperand(0);
//				}
//
//				switch (op->getOpcode()) {
//					case Instruction::Add:
//					{
//						ConstantInt* constantInt = dyn_cast<ConstantInt>(otherOperand);
//						if (constantInt)  {
//							int64_t addedVal = constantInt->getSExtValue();
//							valuesToVisit.push_back(std::make_pair(op, stackDepth + addedVal));
//						}
//						break;
//					}
//					default:  {
//						errs() << "[LiftAsmStack] WARNING: Unhandled stack operation " << *op << " in BB " << op->getParent()->getName() << ", function " << f.getName() << '\n';
//						break;
//					}
//				}
//				continue;
//			}
//
//			StoreInst* store = dyn_cast<StoreInst>(*useItr);
//			if (store && store->getMetadata("trans.keep_store"))  {
//				//This should be just before the return
//				if (stackDepth != 0)  {
//					m_stackFrameError = true;	
//					errs() << "[LiftAsmStack] WARNING: Stack depth at function " << f.getName() << " exit is not 0: " << stackDepth << '\n';
//				}
//				continue;
//			}
//
//			CallInst* call = dyn_cast<CallInst>(*useItr);
//			if (call)  {
//                Type* accessType;
//				AccessInfo::LoadStore loadStore = AccessInfo::LOAD;
//				if (call->getCalledFunction()->getName() == "__stl_mmu")  {
//					accessType = call->getArgOperand(1)->getType();
//					loadStore = AccessInfo::STORE;
//				}
//				else if (call->getCalledFunction()->getName() == "__sts_mmu")  {
//					accessType = call->getArgOperand(1)->getType();
//					loadStore = AccessInfo::STORE;
//				}
//				else if (call->getCalledFunction()->getName() == "__stb_mmu")  {
//					accessType = call->getArgOperand(1)->getType();
//					loadStore = AccessInfo::STORE;
//				}
//				else if (call->getCalledFunction()->getName() == "__ldl_mmu")  {
//                    accessType = call->getType();
//					loadStore = AccessInfo::LOAD;
//				}
//				else if (call->getCalledFunction()->getName() == "__lds_mmu")  {
//                    accessType = call->getType();
//					loadStore = AccessInfo::LOAD;
//				}
//				else if (call->getCalledFunction()->getName() == "__ldb_mmu")  {
//                    accessType = call->getType();
//					loadStore = AccessInfo::LOAD;
//				}
//				else {
//					m_stackFrameError = true;
//					errs() << "[LiftAsmStack] WARNING: Unknown function call with argument derived from stack pointer: '" << call->getCalledFunction()->getName() << "'" << '\n';
//				}
//
//				if (accessType)  {
//					AccessInfo& accessInfo = m_stackDepth[call];
//					accessInfo.stackDepth = stackDepth;
//					accessInfo.type = accessType;
//					accessInfo.loadStore = loadStore;
//
//                    errs() << "[LiftAsmStack] DEBUG: " << (loadStore == AccessInfo::STORE ? "Store to" : "Load from")  
//                           << " stack at "  << stackDepth << " with type " << *accessType
//                           << '\n';
//				}
//				continue;
//			}
//
//			Instruction* inst = dyn_cast<Instruction>(*useItr);
//			if (inst)  {
//				m_stackFrameError = true;
//				errs() << "[LiftAsmStack] WARNING: Unhandled stack instruction " << *inst << " in BB " << inst->getParent()->getName() << ", function " << f.getName() << '\n';
//			}
//			else  {
//				m_stackFrameError = true;
//				errs() << "[LiftAsmStack] WARNING: Unhandled stack value " << **useItr << " in function " << f.getName() << '\n';
//			}
//		}
//	}


	return false;
}

void LiftAsmStack::getAnalysisUsage(AnalysisUsage& usage) const {
}

static RegisterPass<LiftAsmStack> X("LiftAsmStack", "Convert accesses to memory relative to the stack to alloca'd variables", true, true);

