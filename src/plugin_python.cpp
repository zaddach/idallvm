#include <Python.h>

#include <pro.h>
#include <kernwin.hpp>
#include <loader.hpp>

#include "idallvm/msg.h"

static PyObject* plgpy_hello_world(PyObject* self, PyObject* args)
{
    msg("Hello world\n");
    Py_RETURN_NONE;
}

static PyMethodDef plugin_python_methods[] =
{
    {"hello_world", plgpy_hello_world, METH_VARARGS, ""},
    {NULL, NULL, 0, NULL}
};

void plugin_init_python(void)
{
    if (!Py_IsInitialized()) {
        MSG_ERROR("Python plugin not loaded or initialized, failing");
        return;
    }
    msg("Now trying ot add python package\n");
    PyGILState_STATE state = PyGILState_Ensure(); // Acquire the GIL
    Py_InitModule("idallvm", plugin_python_methods);
    PyGILState_Release(state); // Release the GIL
}

void plugin_unload_python(void)
{
}
