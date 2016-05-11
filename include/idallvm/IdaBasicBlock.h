#ifndef _IDALLVM_IDABASICBLOCK_H
#define _IDALLVM_IDABASICBLOCK_H

#include <pro.h>
#include <gdl.hpp>

#include "idallvm/IdaFlowChart.h"

class IdaBasicBlock;
class IdaFlowChart;
class IdaInstruction;

class IdaBasicBlock
{
public:
    class predsucc_iterator : public std::iterator<std::input_iterator_tag, IdaBasicBlock>
    {
    private:
        int idx;
        int size;
        IdaBasicBlock& bb;

    public:
        predsucc_iterator(IdaBasicBlock& bb, int idx, int size) : idx(idx), size(size), bb(bb) {}
        predsucc_iterator& operator++() {assert(idx < size); idx++; return *this;}
        bool operator==(const predsucc_iterator& other) const {return idx == other.idx;}
        bool operator!=(const predsucc_iterator& other) const {return idx != other.idx;}
        IdaBasicBlock& operator*(void) const {return bb.chart.getBasicBlock(idx);}

    };

    class PredecessorIterable
    {
    private:
        IdaBasicBlock& bb;

    public:
        PredecessorIterable(IdaBasicBlock& bb) : bb(bb) {}
        predsucc_iterator begin(void) {return predsucc_iterator(bb, 0, size());}
        predsucc_iterator end(void) {return predsucc_iterator(bb, size(), size());}
        size_t size(void) {return bb.chart.chart.npred(bb.id);}
    };

    class SuccessorIterable
    {
    private:
        IdaBasicBlock& bb;

    public:
        SuccessorIterable(IdaBasicBlock& bb) : bb(bb) {}
        predsucc_iterator begin(void) {return predsucc_iterator(bb, 0, size());}
        predsucc_iterator end(void) {return predsucc_iterator(bb, size(), size());}
        size_t size(void) {return bb.chart.chart.nsucc(bb.id);}
    };

    class iterator : public std::iterator<std::input_iterator_tag, IdaInstruction>
    {
    private:
        ea_t ea;
        IdaBasicBlock& bb;

    public:
        iterator(IdaBasicBlock& bb, ea_t ea) : ea(ea), bb(bb) {}
        iterator& operator++() {assert(ea != BADADDR); ea = next_head(ea, bb.getEndAddress()); return *this;}
        bool operator==(const iterator& other) const {return ea == other.ea;}
        bool operator!=(const iterator& other) const {return ea != other.ea;}
        IdaInstruction& operator*(void) const {return bb.getInstruction(ea);}
    };

private:
    int id;
    qbasic_block_t& bb;
    IdaFlowChart& chart;
    PredecessorIterable predecessors;
    SuccessorIterable successors;
    std::map<ea_t, IdaInstruction> instrCache;

public:
    IdaBasicBlock(int id, qbasic_block_t& bb, IdaFlowChart& chart) : 
        id(id), 
        bb(bb), 
        chart(chart), 
        predecessors(*this),
        successors(*this) {}
    ea_t getStartAddress(void) {return bb.startEA;}
    ea_t getEndAddress(void) {return bb.endEA;}
    PredecessorIterable& getPredecessors() {return predecessors;}
    SuccessorIterable& getSuccessors() {return successors;}
    iterator begin(void) {return iterator(*this, getStartAddress());}
    iterator end(void) {return iterator(*this, BADADDR);}
    IdaInstruction& getInstruction(ea_t ea);
};

#endif /* _IDALLVM_IDABASICBLOCK_H */
