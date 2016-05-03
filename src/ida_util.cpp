#include <ida.hpp>
#include <idp.hpp>
#include <kernwin.hpp>
#include <srarea.hpp>

#include "idallvm/ida_util.h"
#include "idallvm/msg.h"
#include "idallvm/string.h"

bool ida_is_graphical_mode(void)
{
    return callui(ui_get_hwnd).vptr != NULL || is_idaq();
}

static int ida_arm_get_t_register_num(void)
{
    static int regnum = -1;

    if (regnum == -1) {
        regnum = str2reg("T");
    }

    return regnum;
}

bool ida_arm_is_thumb_code(ea_t ea)
{
    return get_segreg(ea, ida_arm_get_t_register_num());
}



ProcessorInformation ida_get_processor_information(void)
{
    char processor_name[sizeof(inf.procName) + 1];
    ProcessorInformation processor_info;

    processor_info.processor = PROCESSOR_UNKNOWN;

    inf.get_proc_name(processor_name); 
    for (char* p = processor_name; *p; ++p) *p = tolower(*p);

    if (startswith(processor_name, "arm")) {
        processor_info.processor = PROCESSOR_ARM;
    }
    else {
        MSG_ERROR( "Don't know processor architecture %s", processor_name);
    }

    return processor_info;
}

std::pair<ea_t, ea_t> ida_get_basic_block(ea_t ea)
{
    ea_t bb_start = ea;
    ea_t bb_end = ea;

    //First get BB start
    while (true) {
        ea_t prev_ea = get_first_cref_to(bb_start);
        if ((prev_ea == BADADDR) || (get_next_cref_to(bb_start, prev_ea) != BADADDR)) {
            break;
        }
        bb_start = prev_ea;
    }

    while (true) {
        ea_t next_ea = get_first_cref_from(bb_end);
        if ((next_ea == BADADDR) || (get_next_cref_from(bb_end, next_ea) != BADADDR)) {
            break;
        }
        bb_end = next_ea;
    }

    return std::make_pair(bb_start, bb_end);
}
