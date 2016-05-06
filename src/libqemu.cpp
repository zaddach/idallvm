#include <QtCore/QLibrary>

#include <pro.h>

#include "idallvm/msg.h"
#include "idallvm/libqemu.h"

static QLibrary* lib = NULL;

libqemu_init_fn ida_libqemu_init = NULL;
libqemu_raise_error_fn ida_libqemu_raise_error = NULL;
libqemu_gen_intermediate_code_fn ida_libqemu_gen_intermediate_code = NULL;

static int is_searched_plugin(const char* file, void* ud)
{
    return 1;
}

static int get_plugin_dir(char* path, size_t path_size)
{
    const extlang_t* extlang = NULL;
    int err = enum_plugins(is_searched_plugin,
            NULL,
            path,
            path_size,
            &extlang);
    if (*path) {
        qdirname(path, path_size, path);
    }

    return 0;
}




int libqemu_load(Processor processor)
{
    const char* libname = NULL;
    char plugins_dir[256];
    switch(processor) {
        case PROCESSOR_ARM:
            libname = LIBQEMU_LIBNAME_ARM;
            break;
    }

    get_plugin_dir(plugins_dir, sizeof(plugins_dir));
    qmakepath(plugins_dir, sizeof(plugins_dir), plugins_dir, libname);

    lib = new QLibrary(plugins_dir);
    lib->setLoadHints(QLibrary::ResolveAllSymbolsHint);
    if (!lib->load()) {
        MSG_ERROR("Cannot open libqemu library %s: %s", plugins_dir, lib->errorString().toLatin1().data());
        return -1;
    }

    ida_libqemu_init = reinterpret_cast<libqemu_init_fn>(lib->resolve("libqemu_init"));
    if (!ida_libqemu_init) {
        MSG_ERROR("Cannot get function pointer for %s", "libqemu_init");
        lib->unload();
        return -1;
    }

    ida_libqemu_gen_intermediate_code = reinterpret_cast<libqemu_gen_intermediate_code_fn>(lib->resolve("libqemu_gen_intermediate_code"));
    if (!ida_libqemu_gen_intermediate_code) {
        MSG_ERROR("Cannot get function pointer for %s", "libqemu_gen_intermediate_code");
        lib->unload();
        return -1;
    }

    ida_libqemu_raise_error = reinterpret_cast<libqemu_raise_error_fn>(lib->resolve("libqemu_raise_error"));
    if (!ida_libqemu_raise_error) {
        MSG_ERROR("Cannot get function pointer for %s", "libqemu_raise_error");
        lib->unload();
        return -1;
    }
}

void libqemu_unload(void)
{
    if (lib) {
        lib->unload();
        lib = NULL;
    }

    ida_libqemu_init = NULL;
    ida_libqemu_raise_error = NULL;
    ida_libqemu_gen_intermediate_code = NULL;
}

