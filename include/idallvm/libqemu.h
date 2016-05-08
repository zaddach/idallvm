#ifndef _IDALLVM_LIBQEMU_H
#define _IDALLVM_LIBQEMU_H

#include "idallvm/ida_util.h"

#include "libqemu/qemu-lib-external.h"

#include <llvm/IR/Type.h>

#define SO_PREFIX "lib"
#define SO_SUFFIX ".so"

#define MAKE_LIBNAME(archname) SO_PREFIX "qemu-" archname SO_SUFFIX

#define LIBQEMU_LIBNAME_ARM MAKE_LIBNAME("arm")
#define LIBQEMU_LIBNAME_I386 MAKE_LIBNAME("i386")
#define LIBQEMU_LIBNAME_X86_64 MAKE_LIBNAME("x86_64")

extern libqemu_init_fn ida_libqemu_init;
extern libqemu_raise_error_fn ida_libqemu_raise_error;
extern libqemu_gen_intermediate_code_fn ida_libqemu_gen_intermediate_code;
extern libqemu_get_module_fn ida_libqemu_get_module;
extern libqemu_get_target_name_fn ida_libqemu_get_target_name;

llvm::Type* ida_libqemu_get_cpustruct_type(void);
std::vector<unsigned>& ida_libqemu_get_pc_indices(void);

int libqemu_load(Processor processor);
void libqemu_unload(void);

#endif /* _IDALLVM_LIBQEMU_H */

