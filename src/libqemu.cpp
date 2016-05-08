#include <QtCore/QLibrary>

#include <pro.h>

#include <llvm/IR/Module.h>

#include "idallvm/msg.h"
#include "idallvm/libqemu.h"

static QLibrary* lib = NULL;

libqemu_init_fn ida_libqemu_init = NULL;
libqemu_raise_error_fn ida_libqemu_raise_error = NULL;
libqemu_gen_intermediate_code_fn ida_libqemu_gen_intermediate_code = NULL;
libqemu_get_module_fn ida_libqemu_get_module = NULL;
libqemu_get_target_name_fn ida_libqemu_get_target_name = NULL;

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


llvm::Type* ida_libqemu_get_cpustruct_type(void) 
{
    llvm::Module* module = llvm::unwrap(ida_libqemu_get_module());
    assert(module && "Cannot get libqemu LLVM module");
    llvm::GlobalVariable* cpu_type_anchor = module->getGlobalVariable("cpu_type_anchor", false);
    assert(cpu_type_anchor && "Cannot get cpu_type_anchor variable in LLVM module");
    llvm::PointerType* ptr_type = llvm::cast<llvm::PointerType>(cpu_type_anchor->getType());
    assert(ptr_type && "Cannot get pointer type of cpu_type_anchor variable");
    llvm::Type* type = ptr_type->getElementType();
    assert(type && "Cannot get type of cpu_type_anchor variable");
    return type;
}

std::vector<unsigned>& ida_libqemu_get_pc_indices(void)
{
    static std::vector<unsigned> indices;

    if (indices.empty()) {
        size_t size = 20;
        unsigned tmp_indices[20];

        libqemu_get_pc_indices_fn get_pc_indices = reinterpret_cast<libqemu_get_pc_indices_fn>(lib->resolve("libqemu_get_pc_indices"));
        assert(get_pc_indices);
        if (!get_pc_indices || get_pc_indices(tmp_indices, &size)) {
            //TODO: Error
        }
        for (unsigned i = 0; i < size; ++i) {
            indices.push_back(tmp_indices[i]);
        }
    }

    return indices;
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

    ida_libqemu_get_module = reinterpret_cast<libqemu_get_module_fn>(lib->resolve("libqemu_get_module"));
    if (!ida_libqemu_get_module) {
        MSG_ERROR("Cannot get function pointer for %s", "libqemu_get_module");
        lib->unload();
        return -1;
    }

    ida_libqemu_get_target_name = reinterpret_cast<libqemu_get_target_name_fn>(lib->resolve("libqemu_get_target_name"));
    if (!ida_libqemu_get_target_name) {
        MSG_ERROR("Cannot get function pointer for %s", "libqemu_get_target_name");
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
    ida_libqemu_get_module = NULL;
    ida_libqemu_get_target_name = NULL;
}

