#include "idallvm/IdaInstruction.h"
#include "idallvm/IdaBasicBlock.h"

IdaInstruction& IdaBasicBlock::getInstruction(ea_t ea)
{
    auto itr = instrCache.find(ea);
    if (itr != instrCache.end()) {
        return *itr->second.get();
    }
    assert(ea != BADADDR && "Invalid address");
    return *instrCache.insert(std::make_pair(ea, std::unique_ptr<IdaInstruction>(new IdaInstruction(ea, *this)))).first->second.get();

}
