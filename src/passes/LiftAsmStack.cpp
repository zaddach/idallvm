#include "idallvm/passes/LiftAsmStack.h"
#include "idallvm/libqemu.h"
#include "idallvm/msg.h"

#include "llvm/IR/Instructions.h"
#include "llvm/IR/Constants.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/CFG.h"
#include "llvm/ADT/DepthFirstIterator.h"

#include <sstream>

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
using llvm::LLVMContext;
using llvm::IntegerType;
using llvm::Type;
using llvm::StructType;
using llvm::AllocaInst;
using llvm::GetElementPtrInst;
using llvm::SmallVector;
using llvm::makeArrayRef;
using llvm::Twine;


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
            case Instruction::ZExt: {
                return backtrackStackOffset(inst->getOperand(0), envSpOffset, stackDepth, stackOffset);
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

static std::string hex(int64_t x)
{
    std::stringstream ss;

    if (x < 0) {
        ss << "-0x" << (-x);
    } else {
        ss << "0x" << std::hex << x;
    }
    return ss.str();
}


//Start at entry BB by finding load of stack pointer
//Assign value 0 to max stack depth
//Follow all uses of stack pointer value
//If use is a phi node, spawn a new state
//If 
bool LiftAsmStack::runOnFunction(Function &f) {
    LLVMContext& ctx = f.getContext();
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
                        if (backtrackStackOffset(call->getArgOperand(1), ri->offset, loadStackDepth, frameOffset)) {
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

    if (stackAccesses.empty()) {
        //If there are no stack accesses, we're done
        return 0;
    }

    //Get stack element sizes
    std::map< int64_t, uint64_t > stackElementSizes;
    std::map< uint64_t, unsigned > const SHIFT_TO_SIZE = {{0, 1}, {1, 2}, {2, 4}, {3, 8}};
    for (auto const& elem : stackAccesses) {
        uint64_t shift = cast<ConstantInt>(elem.first->getArgOperand(2))->getZExtValue();
        assert(SHIFT_TO_SIZE.find(shift) != SHIFT_TO_SIZE.end());
        unsigned size = SHIFT_TO_SIZE.find(shift)->second;
        if (stackElementSizes[elem.second] < size) {
            stackElementSizes[elem.second] = size;
        }
    }

    //NOTE: Stack frame reconstruction is not super clean.
    //Right now, everything _should_ work, even if the address of a
    //stack value is taken. The only case where this heuristics will fail is
    //if stack references are not cleanly passed to called functions (i.e.,
    //a called function accesses a stack value from another stack frame).
    //Stack arrays are not recovered cleanly, but the space occupied by them
    //is allocated correctly in the stack frame (possibly not with the correct type).
    std::vector< Type* > stackElementTypes;
    std::map< int64_t, unsigned > indices;
    int64_t prevOffset = INT64_MIN;
    //std::map is ordered by key
    for (auto const& elemSize : stackElementSizes) {
        if (prevOffset != INT64_MIN) {
            int64_t align = elemSize.first - prevOffset;
            while (align > 0) {
                if (align >= 8) {
                    stackElementTypes.push_back(IntegerType::get(ctx, 8 * 8));
                    align -= 8;
                }
                else if (align >= 4) {
                    stackElementTypes.push_back(IntegerType::get(ctx, 4 * 8));
                    align -= 4;
                }
                else if (align >= 2) {
                    stackElementTypes.push_back(IntegerType::get(ctx, 2 * 8));
                    align -= 2;
                }
                else {
                    stackElementTypes.push_back(IntegerType::get(ctx, 1 * 8));
                    align -= 1;
                }
            }
        }

        indices[elemSize.first] = stackElementTypes.size();
        stackElementTypes.push_back(IntegerType::get(ctx, elemSize.second * 8));
    }

    StructType* stackFrameType = StructType::create(ctx, stackElementTypes, ("stackframe_type_" + f.getName()).str(), true);

    //Allocate stack frame first thing in the function
    AllocaInst* frameAlloca = new AllocaInst(stackFrameType, ("stackframe_" + f.getName()).str(), f.getEntryBlock().getFirstInsertionPt());
    std::list< CallInst* > eraseList;
    for (auto const& elem : stackAccesses) {
        unsigned index = indices[elem.second];
        SmallVector<Value*, 2> vec;
        vec.push_back(ConstantInt::get(Type::getInt32Ty(ctx), 0));
        vec.push_back(ConstantInt::get(Type::getInt32Ty(ctx), index));
        GetElementPtrInst* gep = GetElementPtrInst::CreateInBounds(frameAlloca, makeArrayRef(vec), "stackframe_" + hex(elem.second), elem.first);
        if (elem.first->getCalledFunction()->getName() == "helper_libqemu_st") {
            CastInst* cast = CastInst::CreateIntegerCast(elem.first->getArgOperand(4), gep->getType()->getElementType(), false, "cast_stackframe_" + hex(elem.second), elem.first); 
            new StoreInst(cast, gep, elem.first);
            eraseList.push_back(elem.first);
        }
        else if (elem.first->getCalledFunction()->getName() == "helper_libqemu_ld") {
            LoadInst* load = new LoadInst(gep, "stackframe_", elem.first);
            CastInst* cast = CastInst::CreateZExtOrBitCast(load, elem.first->getType(), "cast_stackframe_" + hex(elem.second), elem.first); 
            elem.first->replaceAllUsesWith(cast);
            eraseList.push_back(elem.first);
        }
    }

    for (auto inst : eraseList) {
        inst->eraseFromParent();
    }

	return false;
}

void LiftAsmStack::getAnalysisUsage(AnalysisUsage& usage) const {
}

LiftAsmStack* createLiftAsmStackPass(void) {
    return new LiftAsmStack();
}

static RegisterPass<LiftAsmStack> X("LiftAsmStack", "Convert accesses to memory relative to the stack to alloca'd variables", true, true);

