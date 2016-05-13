#include "idallvm/IdaInstruction.h"
#include "idallvm/IdaFlowChart.h"
#include "idallvm/IdaBasicBlock.h"

IdaBasicBlock& IdaFlowChart::getBasicBlock(int id)
{
    auto itr = bbCache.find(id);
    if (itr != bbCache.end()) {
        return *itr->second.get();
    }
    assert(id < size() && "Invalid index");
    return *bbCache.insert(std::make_pair(id, std::unique_ptr<IdaBasicBlock>(new IdaBasicBlock(id, chart.blocks[id], *this)))).first->second.get();
}

std::string const& IdaFlowChart::getFunctionName(void)
{
    if (functionName.empty()) {
        qstring tmp;
        get_func_name2(&tmp, getStartAddress());
        functionName.assign(tmp.c_str());
    }

    return functionName;
}

