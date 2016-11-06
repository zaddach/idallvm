#include <list>
#include <map>

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Metadata.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/CFG.h>

#include "idallvm/passes/IdentifyCalls.h"
#include "idallvm/passes/InlineOpcodeCalls.h"
#include "idallvm/passes/FixBasicBlockEdges.h"
#include "idallvm/passes/LiftAsmRegisters.h"
#include "idallvm/passes.h"
#include "idallvm/libqemu.h"
#include "idallvm/msg.h"

using std::map;
using std::list;
using std::make_pair;

using llvm::Function;
using llvm::BasicBlock;
using llvm::FunctionPass;
using llvm::Instruction;
using llvm::Value;
using llvm::LoadInst;
using llvm::StoreInst;
using llvm::MDNode;
using llvm::ConstantInt;
using llvm::RegisterPass;
using llvm::dyn_cast;
using llvm::cast;
using llvm::SwitchInst;
using llvm::BranchInst;
using llvm::pred_begin;
using llvm::pred_end;

struct BasicBlockDataFlow {

};

char FixBasicBlockEdges::ID = 0;

FixBasicBlockEdges::FixBasicBlockEdges() : FunctionPass(ID) 
{
}

bool FixBasicBlockEdges::runOnFunction(Function& f)
{
    std::list< BasicBlock* > bbsTodo;
    std::map< BasicBlock*, std::list< std::pair< Value*, Instruction* > > > inValues;
    std::map< BasicBlock*, std::pair<  Value*, Instruction* > > outValue;
    RegisterInfo const* ri = Libqemu_GetRegisterInfoPc();
    std::list< Instruction* > eraseList;
    assert(ri && "Could not get PC register information");

    std::list< SwitchInst* > switchInsts;
    for (BasicBlock& bb : f) {
        if (SwitchInst* switchInst = dyn_cast< SwitchInst >(bb.getTerminator())) {
            LoadInst* load = cast< LoadInst >(switchInst->getCondition());
            MDNode* md = load->getMetadata("tcg-llvm.env_access.offset");
            assert(md->getNumOperands() == 1 && 
                   cast< ConstantInt >(md->getOperand(0))->getZExtValue() == ri->offset && 
                   "Load before switch is not for PC");
            switchInsts.push_back(switchInst);
        }
    }

    for (auto switchInst : switchInsts) {
        bool switchDone = false;
        //First try to find a PC store inst in the BB of the switch inst (before the switch)
        for (auto itr = BasicBlock::reverse_iterator(switchInst), end = switchInst->getParent()->rend(); itr != end; ++itr) {
            if (StoreInst* store = dyn_cast< StoreInst >(&*itr)) {
                MDNode* md = store->getMetadata("tcg-llvm.env_access.offset");
                if (md && cast< ConstantInt >(md->getOperand(0))->getZExtValue() == ri->offset) {
                    //TODO: Found store in same BB as switch, use this one
                    if (ConstantInt* value = dyn_cast< ConstantInt >(store->getValueOperand())) {
                        SwitchInst::CaseIt caseIt = switchInst->findCaseValue(value);
                        if (caseIt != switchInst->case_default()) {
                            BranchInst* branch = BranchInst::Create(caseIt.getCaseSuccessor(), switchInst);
                            eraseList.push_back(switchInst);
                        }
                        else {
                            outs() << "FixBasicBlockEdges - ERROR: Switch does not cover PC value " << value->getZExtValue() << '\n';
                            //TODO: Case does not cover the PC value
                        }
                    }
                    else {
                        outs() << "FixBasicBlockEdges - ERROR: Stored PC value in BB " << switchInst->getParent()->getName() << " is not constant" << '\n';
                        //TODO: stored PC value is not constant
                    }
                    //Exit the current loop and proceed to next switch 
                    switchDone = true;
                    break;
                }
            }
        }

        if (switchDone) {
            continue;
        }
        
        //No PC store in the same BB as the switch found, try predecessor BBs
        //Keep branch replacements in a list, because iterators get confused if we modify them directly
        std::list< std::pair< BranchInst*, BasicBlock* > > branchReplaces;
        for (auto itr = pred_begin(switchInst->getParent()), end = pred_end(switchInst->getParent()); itr != end; ++itr) {
            StoreInst* pcStore = nullptr;
            for (BasicBlock::reverse_iterator instItr = (*itr)->rbegin(), instEnd = (*itr)->rend(); instItr != instEnd; ++instItr) {
                if (StoreInst* store = dyn_cast< StoreInst >(&*instItr)) {
                    MDNode* md = store->getMetadata("tcg-llvm.env_access.offset");
                    if (md && cast< ConstantInt >(md->getOperand(0))->getZExtValue() == ri->offset) {
                        pcStore = store;
                        break;
                    }
                }
            }

            if (!pcStore) {
                outs() << "FixBasicBlockEdges - ERROR: No PC store found in BB " << (*itr)->getName() << '\n';
                //TODO: No PC store in this BB
                continue;
            }

            ConstantInt* pcValue = dyn_cast< ConstantInt >(pcStore->getValueOperand());
            if (pcValue) {
                SwitchInst::CaseIt foundCase = switchInst->findCaseValue(pcValue);
                if (foundCase != switchInst->case_default()) {
                    BasicBlock* nextBB = foundCase.getCaseSuccessor();
                    //TODO: Verify that between the point of setting the PC and the end of the BB in which the PC is set, 
                    //there is nothing major (for now, it should do to expect the store being followed directly by a br)
                    BranchInst* branch = dyn_cast< BranchInst >(pcStore->getParent()->getTerminator());
                    if (branch && branch->isUnconditional()) {
                        //ok, we found the branch whose target is to be replaced
                        assert(branch->getNumSuccessors() == 1 && "Unconditional branch should only have one successor");
                        branchReplaces.push_back(std::make_pair(branch, nextBB));
                    }
                    else {
                        outs() << "FixBasicBlockEdges - ERROR: Terminator instruction in BB where PC is assigned is not an unconditional branch: " << *pcStore->getParent()->getTerminator() << '\n';
                        //TODO: not ok, is not a branch inst or a conditional branch inst
                    }
                }
                else {
                    outs() << "FixBasicBlockEdges - ERROR: Value " << pcValue->getZExtValue() << " was not found in case values" << '\n';
                    //TODO: Case value was not found in the list of successors dispatched by the switch.
                }
            }
            else {
                outs() << "FixBasicBlockEdges - ERROR: PC value set at the end of the BB " << pcStore->getParent()->getName() << " is not constant" << '\n';
                //TODO: Last PC value set before end of BB is not constant
            }
        }
        for (auto branchReplace : branchReplaces) {
            branchReplace.first->setSuccessor(0, branchReplace.second);
        }
    }

    for (Instruction* inst : eraseList) {
        inst->eraseFromParent();
    }

    return true;
}

void FixBasicBlockEdges::getAnalysisUsage(llvm::AnalysisUsage &AU) const
{
    AU.addRequired<InlineOpcodeCalls>();
    AU.addRequired<IdentifyCalls>();
//    AU.addRequired<CpuStructToReg>();
    AU.setPreservesCFG();
    AU.addPreserved<InlineOpcodeCalls>();
    AU.addPreserved<IdentifyCalls>();
//    AU.addPreserved<CpuStructToReg>();
}

FunctionPass* createFixBasicBlockEdgesPass(void)
{
    return new FixBasicBlockEdges();
}

static RegisterPass<FixBasicBlockEdges> passInfo("FixBasicBlockEdges", "Replace switch statements at end of basic blocks with proper edges", false, false);
