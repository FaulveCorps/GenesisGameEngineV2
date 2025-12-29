#ifdef _WIN32
#include <windows.h>
#include <dbghelp.h>
#include <eh.h>
#include <iostream>

static void seh_translator(unsigned int code, _EXCEPTION_POINTERS* ep) {
    std::cerr << "SEH translator invoked: code=0x" << std::hex << code << std::dec << std::endl;
    void* addrs[64];
    USHORT frames = CaptureStackBackTrace(0, 64, addrs, nullptr);
    std::cerr << "SEH: Stack frames captured: " << frames << std::endl;
    HANDLE hProc = GetCurrentProcess();
    SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_UNDNAME);
    if (SymInitialize(hProc, NULL, TRUE)) {
        for (USHORT i = 0; i < frames; ++i) {
            DWORD64 addr = (DWORD64)(addrs[i]);
            char buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)];
            PSYMBOL_INFO pSymbol = (PSYMBOL_INFO)buffer;
            pSymbol->SizeOfStruct = sizeof(SYMBOL_INFO);
            pSymbol->MaxNameLen = MAX_SYM_NAME;
            DWORD64 displacement = 0;
            if (SymFromAddr(hProc, addr, &displacement, pSymbol)) {
                std::cerr << std::hex << pSymbol->Address << " " << pSymbol->Name << std::dec << " +0x" << displacement << std::endl;
            } else {
                std::cerr << std::hex << addr << std::dec << std::endl;
            }
        }
    } else {
        for (USHORT i = 0; i < frames; ++i) {
            std::cerr << addrs[i] << std::endl;
        }
    }
    // Re-throw as a C++ exception so the test harness can report it with the stack above
    throw std::runtime_error("SEH exception caught (see preceding stack)");
}

// Register translator at dynamic initialization time
struct SEHRegistrar {
    SEHRegistrar() {
        _set_se_translator(seh_translator);
    }
};
static SEHRegistrar g_seh_registrar;
#endif
