#ifndef _IDALLVM_LIBQEMU_H
#define _IDALLVM_LIBQEMU_H

#include "idallvm/ida_util.h"

#include "libqemu/qemu-lib-external.h"

#define SO_PREFIX "lib"
#define SO_SUFFIX ".so"

#define MAKE_LIBNAME(archname) SO_PREFIX "qemu-" archname SO_SUFFIX

#define LIBQEMU_LIBNAME_ARM MAKE_LIBNAME("arm")
#define LIBQEMU_LIBNAME_I386 MAKE_LIBNAME("i386")
#define LIBQEMU_LIBNAME_X86_64 MAKE_LIBNAME("x86_64")

extern libqemu_init_fn ida_libqemu_init;
extern libqemu_raise_error_fn ida_libqemu_raise_error;
extern libqemu_gen_intermediate_code_fn ida_libqemu_gen_intermediate_code;

int libqemu_load(Processor processor);

#endif /* _IDALLVM_LIBQEMU_H */

