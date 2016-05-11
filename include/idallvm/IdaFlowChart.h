#ifndef _IDALLVM_IDAFLOWCHART_H
#define _IDALLVM_IDAFLOWCHART_H

#include <map>

#include <pro.h>
#include <gdl.hpp>

class IdaBasicBlock;
class IdaFlowChart;
class IdaInstruction;

class IdaFlowChart
{
    friend class IdaBasicBlock;
    friend class IdaIinstruction;
private:
    qflow_chart_t chart;
    std::map<unsigned, IdaBasicBlock> bbCache;
    std::string functionName;

public:
    class iterator : public std::iterator<std::input_iterator_tag, IdaBasicBlock>
    {
    private:
         IdaFlowChart& chart;
         int idx;

    public:
        iterator(IdaFlowChart& chart, int idx) : chart(chart), idx(idx) {}
        iterator& operator++() {assert(idx < chart.size()); idx++; return *this;}
        bool operator==(const iterator& other) const {return idx == other.idx;}
        bool operator!=(const iterator& other) const {return idx != other.idx;}
        IdaBasicBlock& operator*(void) const {return chart.getBasicBlock(idx);}
    };

    IdaFlowChart(ea_t ea) : chart("", get_func(ea), BADADDR, BADADDR, FC_PREDS) {}
    IdaFlowChart(ea_t start, ea_t end) : chart("", NULL, start, end, FC_PREDS) {}
    int size(void) {return chart.size();}
    iterator begin(void) {return iterator(*this, 0);}
    iterator end(void) {return iterator(*this, size());}
    IdaBasicBlock& getBasicBlock(int id);
    IdaBasicBlock& getEntryBlock(void) {return getBasicBlock(0);}
    std::string const& getFunctionName(void);
    ea_t getStartAddress(void) {return chart.pfn->startEA;}
    ea_t getEndAddress(void) {return chart.pfn->endEA;}
};

#endif /* _IDALLVM_IDAFLOWCHART_H */
