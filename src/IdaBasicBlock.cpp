#include "idallvm/IdaInstruction.h"
#include "idallvm/IdaBasicBlock.h"

IdaInstruction& IdaBasicBlock::getInstruction(ea_t ea)
{
    auto itr = instrCache.find(ea);
    if (itr != instrCache.end()) {
        return itr->second;
    }
    assert(ea != BADADDR && "Invalid address");
    return instrCache.insert(std::make_pair(ea, IdaInstruction(ea, *this))).first->second;

}
