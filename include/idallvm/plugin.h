#ifndef _IDALLVM_PLUGIN_H
#define _IDALLVM_PLUGIN_H

#if !defined(__cplusplus)
#error Needs to be compiled with c++
#endif /* !defined(__cplusplus) */

#include <pro.h>
#include <loader.hpp>

#define PLUGIN_NAME "IDA-LLVM"
#define PLUGIN_WANTED_NAME "LLVM translator"
#define PLUGIN_COMMENT "This plugin translates assembler language to LLVM intermediate language."
#define PLUGIN_HELP "A plugin to translate assembler instructions to LLVM intermediate language.\n" \
  "The plugin leverages Qemu to convert code from the assembler representation\n" \
  "into LLVM instructions, which then can be inspected through a Python interface."
#define PLUGIN_HOTKEY "F6"

int idaapi PLUGIN_init(void);
void idaapi PLUGIN_term(void);
void idaapi PLUGIN_run(int arg);

#endif /* _IDALLVM_PLUGIN_H */
