#ifndef _IDALLVM_IDABASICBLOCK_H
#define _IDALLVM_IDABASICBLOCK_H

#include <memory>

#include <pro.h>
#include <gdl.hpp>

#include "idallvm/IdaFlowChart.h"
#include "idallvm/IdaInstruction.h"

class IdaBasicBlock;
class IdaFlowChart;
class IdaInstruction;

class IdaBasicBlock
{
public:
    class predecessor_iterator : public std::iterator<std::input_iterator_tag, IdaBasicBlock>
    {
    private:
        int idx;
        int size;
        IdaBasicBlock& bb;

    public:
        predecessor_iterator(IdaBasicBlock& bb, int idx) : idx(idx), size(bb.chart.chart.npred(bb.id)), bb(bb) {}
        predecessor_iterator& operator++() {assert(idx < size); idx++; return *this;}
        bool operator==(const predecessor_iterator& other) const {return idx == other.idx;}
        bool operator!=(const predecessor_iterator& other) const {return idx != other.idx;}
        IdaBasicBlock& operator*(void) const {return bb.chart.getBasicBlock(bb.chart.chart.pred(bb.id, idx));}

    };

    class successor_iterator : public std::iterator<std::input_iterator_tag, IdaBasicBlock>
    {
    private:
        int idx;
        int size;
        IdaBasicBlock& bb;

    public:
        successor_iterator(IdaBasicBlock& bb, int idx) : idx(idx), size(bb.chart.chart.nsucc(bb.id)), bb(bb) {}
        successor_iterator& operator++() {assert(idx < size); idx++; return *this;}
        bool operator==(const successor_iterator& other) const {return idx == other.idx;}
        bool operator!=(const successor_iterator& other) const {return idx != other.idx;}
        IdaBasicBlock& operator*(void) const {return bb.chart.getBasicBlock(bb.chart.chart.succ(bb.id, idx));}

    };

    class PredecessorIterable
    {
    private:
        IdaBasicBlock& bb;

    public:
        PredecessorIterable(IdaBasicBlock& bb) : bb(bb) {}
        predecessor_iterator begin(void) {return predecessor_iterator(bb, 0);}
        predecessor_iterator end(void) {return predecessor_iterator(bb, size());}
        size_t size(void) {return bb.chart.chart.npred(bb.id);}
    };

    class SuccessorIterable
    {
    private:
        IdaBasicBlock& bb;

    public:
        SuccessorIterable(IdaBasicBlock& bb) : bb(bb) {}
        successor_iterator begin(void) {return successor_iterator(bb, 0);}
        successor_iterator end(void) {return successor_iterator(bb, size());}
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
    std::map<ea_t, std::unique_ptr<IdaInstruction> > instrCache;

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
