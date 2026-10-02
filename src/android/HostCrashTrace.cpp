// Host validation diagnostics. This translation unit is never packaged in APKs.
#include <windows.h>
#include <dbghelp.h>
#include <cstdio>

namespace {
LONG CALLBACK traceException(EXCEPTION_POINTERS* exception) {
    if (exception->ExceptionRecord->ExceptionCode != EXCEPTION_ACCESS_VIOLATION) return EXCEPTION_CONTINUE_SEARCH;
    std::fprintf(stderr, "HOST access violation at %p\n", exception->ExceptionRecord->ExceptionAddress);
    void* frames[48];
    const auto count = CaptureStackBackTrace(0, 48, frames, nullptr);
    for (USHORT i = 0; i < count; ++i) {
        HMODULE module = nullptr;
        char path[MAX_PATH] = {};
        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            static_cast<const char*>(frames[i]), &module);
        GetModuleFileNameA(module, path, MAX_PATH);
        const auto offset = reinterpret_cast<ULONG_PTR>(frames[i]) - reinterpret_cast<ULONG_PTR>(module);
        char storage[sizeof(SYMBOL_INFO) + MAX_SYM_NAME] = {};
        auto* symbol = reinterpret_cast<SYMBOL_INFO*>(storage);
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = MAX_SYM_NAME;
        DWORD64 displacement = 0;
        const bool resolved = SymFromAddr(GetCurrentProcess(), reinterpret_cast<DWORD64>(frames[i]), &displacement, symbol);
        std::fprintf(stderr, "HOST frame %u %s + 0x%llx %s\n", i, path,
            static_cast<unsigned long long>(offset), resolved ? symbol->Name : "");
    }
    std::fflush(stderr);
    return EXCEPTION_CONTINUE_SEARCH;
}
}
void enableHostCrashTrace() {
    SymInitialize(GetCurrentProcess(), nullptr, TRUE);
    AddVectoredExceptionHandler(1, traceException);
}
