#ifndef _IDALLVM_LIBQEMU_H
#define _IDALLVM_LIBQEMU_H

#include "idallvm/ida_util.h"

#include "libqemu/qemu-lib-external.h"
#include "libqemu/register_info.h"

#include <llvm/IR/Type.h>

#define SO_PREFIX "lib"
#define SO_SUFFIX ".so"

#define MAKE_LIBNAME(archname) SO_PREFIX "qemu-" archname SO_SUFFIX

#define LIBQEMU_LIBNAME_ARM MAKE_LIBNAME("arm")
#define LIBQEMU_LIBNAME_I386 MAKE_LIBNAME("i386")
#define LIBQEMU_LIBNAME_X86_64 MAKE_LIBNAME("x86_64")

extern decltype(libqemu_init)* Libqemu_Init;
extern decltype(libqemu_raise_error)* Libqemu_RaiseError;
extern decltype(libqemu_gen_intermediate_code)* Libqemu_GenIntermediateCode;
extern decltype(libqemu_get_module)* Libqemu_GetModule;
extern decltype(libqemu_get_target_name)* Libqemu_GetTargetName;
extern decltype(libqemu_get_register_info_by_name)* Libqemu_GetRegisterInfoByName;
extern decltype(libqemu_get_register_info_by_offset)* Libqemu_GetRegisterInfoByOffset;
extern decltype(libqemu_get_register_info_by_indices)* Libqemu_GetRegisterInfoByIndices;
extern decltype(libqemu_get_register_info_pc)* Libqemu_GetRegisterInfoPc;
extern decltype(libqemu_get_register_info_sp)* Libqemu_GetRegisterInfoSp;

llvm::Type* Libqemu_GetCpustateType(void);

int Libqemu_Load(Processor processor);
void Libqemu_Unload(void);

#endif /* _IDALLVM_LIBQEMU_H */

