#include <ida.hpp>
#include <idp.hpp>
#include <kernwin.hpp>

#include "idallvm/ida_util.h"
#include "idallvm/msg.h"
#include "idallvm/string.h"

bool ida_is_graphical_mode(void)
{
    return callui(ui_get_hwnd).vptr != NULL || is_idaq();
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
