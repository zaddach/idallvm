#ifndef _IDALLVM_IDAINSTRUCTION_H
#define _IDALLVM_IDAINSTRUCTION_H

#include <pro.h>

class IdaBasicBlock;

class IdaInstruction
{
private:
    ea_t ea;
    IdaBasicBlock& basicBlock;

public:
    IdaInstruction(ea_t ea, IdaBasicBlock& bb) : ea(ea), basicBlock(bb) {}
    ea_t getAddress(void) {return ea;}
};

#endif /* _IDALLVM_IDAINSTRUCTION_H */
