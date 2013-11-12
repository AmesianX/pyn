#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "pin.H"

// TODO fix memory leaks introduced by strdup
#define strdup _strdup

#define ARRAYSIZE(arr) (sizeof(arr)/sizeof((arr)[0]))
#define CYCLIC(arr, idx) (&arr[idx++ % ARRAYSIZE(arr)])

namespace Py {
    #include "Python.h"
}

#define F(name) {#name, &name}
#define F2(name) {#name, &name##_detour}

static const char *IMG_Name_detour(IMG img);
static IMG IMG_Open_detour(const char *fname);
static const char *RTN_Name_detour(RTN rtn);
static const char *RTN_FindNameByAddress_detour(ADDRINT addr);
static RTN RTN_CreateAt_detour(ADDRINT addr, const char *name);
static const char *INS_Mnemonic_detour(INS ins);
static const char *INS_Disassemble_detour(INS ins);
static const char *SYM_Name_detour(SYM sym);
static const char *PIN_UndecorateSymbolName_detour(
    const char *symbol_name, UNDECORATION style);

static void *g_functions[][2] = {
    // IMG - Image Object
    F(IMG_Next),
    F(IMG_Prev),
    F(IMG_Invalid),
    F(IMG_Valid),
    F(IMG_SecHead),
    F(IMG_SecTail),
    F(IMG_RegsymHead),
    F(IMG_Entry),
    F2(IMG_Name),
    F(IMG_Gp),
    F(IMG_LoadOffset),
    F(IMG_LowAddress),
    F(IMG_HighAddress),
    F(IMG_StartAddress),
    F(IMG_SizeMapped),
    F(IMG_Type),
    F(IMG_IsMainExecutable),
    F(IMG_IsStaticExecutable),
    F(IMG_Id),
    F(IMG_FindImgById),
    F(IMG_FindByAddress),
    F(IMG_AddInstrumentFunction),
    F(IMG_AddUnloadFunction),
    F2(IMG_Open),
    F(IMG_Close),

    // APP
    F(APP_ImgHead),
    F(APP_ImgTail),

    // RTN - Routine Object
    F(RTN_Sec),
    F(RTN_Next),
    F(RTN_Prev),
    F(RTN_Invalid),
    F(RTN_Valid),
    F2(RTN_Name),
    F(RTN_Sym),
    F(RTN_Funptr),
    F(RTN_Id),
    F(RTN_AddInstrumentFunction),
    F(RTN_Range),
    F(RTN_Size),
    F2(RTN_FindNameByAddress),
    F(RTN_FindByAddress),
    F(RTN_FindByName),
    F(RTN_Open),
    F(RTN_Close),
    F(RTN_InsHead),
    F(RTN_InsHeadOnly),
    F(RTN_InsTail),
    F(RTN_NumIns),
    F(RTN_InsertCall),
    F(RTN_Address),
    F2(RTN_CreateAt),
    F(RTN_Replace),

    // TRACE - Single entrance, multiple exit
    F(TRACE_AddInstrumentFunction),
    F(TRACE_InsertCall),
    F(TRACE_BblHead),
    F(TRACE_BblTail),
    F(TRACE_Original),
    F(TRACE_Address),
    F(TRACE_Size),
    F(TRACE_Rtn),
    F(TRACE_HasFallThrough),
    F(TRACE_NumBbl),
    F(TRACE_NumIns),
    F(TRACE_StubSize),

    // BBL - Single entrance, single exit
    F(BBL_MoveAllAttributes),
    F(BBL_NumIns),
    F(BBL_InsHead),
    F(BBL_InsTail),
    F(BBL_Next),
    F(BBL_Prev),
    F(BBL_Valid),
    F(BBL_Original),
    F(BBL_Address),
    F(BBL_Size),
    F(BBL_InsertCall),
    F(BBL_HasFallThrough),

    // INS Instrumentation
    F(INS_AddInstrumentFunction),
    F(INS_InsertCall),

    // INS Generic Inspection
    F2(INS_Mnemonic),
    F(INS_IsOriginal),
    F2(INS_Disassemble),
    F(INS_Next),
    F(INS_Prev),
    F(INS_Invalid),
    F(INS_Valid),
    F(INS_Address),
    F(INS_Size),

    // INS Modification
    F(INS_InsertIndirectJump),
    F(INS_InsertDirectJump),
    F(INS_Delete),

    // SYM - Symbol Object
    F(SYM_Next),
    F(SYM_Prev),
    F2(SYM_Name),
    F(SYM_Invalid),
    F(SYM_Valid),
    F(SYM_Dynamic),
    F(SYM_IFunc),
    F(SYM_Value),
    F(SYM_Index),
    F(SYM_Address),
    F2(PIN_UndecorateSymbolName),
};

static const char *IMG_Name_detour(IMG img)
{
    return strdup(IMG_Name(img).c_str());
}

static IMG IMG_Open_detour(const char *fname)
{
    // IMG_Open takes a std::string as parameter,
    // hence the detour function
    return IMG_Open(fname);
}

static const char *RTN_Name_detour(RTN rtn)
{
    return strdup(RTN_Name(rtn).c_str());
}

static const char *RTN_FindNameByAddress_detour(ADDRINT addr)
{
    return strdup(RTN_FindNameByAddress(addr).c_str());
}

static RTN RTN_CreateAt_detour(ADDRINT addr, const char *name)
{
    // RTN_CreateAt takes a std::string as parameter,
    // hence the detour function
    return RTN_CreateAt(addr, name);
}

static const char *INS_Mnemonic_detour(INS ins)
{
    static char cyclic_strings[32][32]; static uint32_t cyclic_index;

    string s = INS_Mnemonic(ins);
    if(s.c_str() == NULL) return NULL;

    return strcpy(*CYCLIC(cyclic_strings, cyclic_index), s.c_str());
}

static const char *INS_Disassemble_detour(INS ins)
{
    static char cyclic_strings[32][64]; static uint32_t cyclic_index;

    string s = INS_Disassemble(ins);
    if(s.c_str() == NULL) return NULL;

    return strcpy(*CYCLIC(cyclic_strings, cyclic_index), s.c_str());
}

static const char *SYM_Name_detour(SYM sym)
{
    return strdup(SYM_Name(sym).c_str());
}

static const char *PIN_UndecorateSymbolName_detour(
    const char *symbol_name, UNDECORATION style)
{
    return strdup(PIN_UndecorateSymbolName(symbol_name, style).c_str());
}

static Py::PyObject *g_fini_callback;

static void fini_callback(int32_t code, void *v)
{
    Py::PyObject_CallFunction(g_fini_callback, "i", code);
}

static Py::PyObject *g_child_callback;

static BOOL child_callback(CHILD_PROCESS child_process, void *v)
{
    // TODO return the actual return value
    Py::PyObject_CallFunction(g_child_callback, "i", child_process);
    return TRUE;
}

int main(int argc, char *argv[])
{
    PIN_Init(argc, argv);
    PIN_InitSymbols();

    Py::Py_Initialize();

    Py::PyRun_SimpleString("_pin_function_addr = {}");

    char buf[256];
    for (uint32_t idx = 0; idx < ARRAYSIZE(g_functions); idx++) {
        sprintf(buf, "_pin_function_addr['%s'] = 0x%08lx",
            g_functions[idx][0], g_functions[idx][1]);
        Py::PyRun_SimpleString(buf);
    }

    // we want to execute pypin in the current namespace
    Py::PyRun_SimpleString("exec open('pypin.py', 'rb').read()");

    // manually parse argv, because KNOB - do you even parse?!
    for (int i = 0, tool_arg_start = -1; i < argc; i++) {
        // end of parameters to our pintool
        if(!strcmp(argv[i], "--")) break;

        // -t specifies our pintool, after that come args to our tool
        if(!strcmp(argv[i], "-t")) {
            tool_arg_start = i + 2;
            continue;
        }

        // check if we're already in the arguments to our tool
        if(tool_arg_start < 0 || i < tool_arg_start) continue;

        // python code injection!!1
        snprintf(buf, sizeof(buf), "exec open('%s', 'rb').read()", argv[i]);
        Py::PyRun_SimpleString(buf);
    }

    Py::PyObject *globals = NULL, *py_str;
    sprintf(buf, "import ctypes; ctypes.memmove(0x%08lx, "
            "ctypes.byref(ctypes.c_int(id(globals()))), 4)", &globals);
    Py::PyRun_SimpleString(buf);

#define CALLBACK_REGISTER(name, api) \
    py_str = Py::PyString_FromString(#name); \
    g_##name##_callback = Py::PyDict_GetItem(globals, py_str); \
    if(g_##name##_callback != NULL) { \
        api(&name##_callback, NULL); \
    }

    if(globals != NULL) {
        CALLBACK_REGISTER(fini, PIN_AddFiniFunction);
        CALLBACK_REGISTER(child, PIN_AddFollowChildProcessFunction);
    }

    PIN_StartProgram();
    Py::Py_Finalize();
    return 0;
}
