#ifdef HAVE_WASM3
#include "engine/WasmHostBindings.h"
#include <iostream>
#include <fstream>
#include "wasm3.h"
#include "engine/WasmRuntime.h"

#include <unordered_map>
#include <mutex>
#include <memory>
#include <functional>
#include <algorithm>
#include <sstream>
#include <thread>
#include <vector>
#include <cstdint>
#include <windows.h>
#ifdef _WIN32
#include <dbghelp.h>
#pragma comment(lib, "Dbghelp.lib")
#endif

#ifdef _DEBUG
#include <crtdbg.h>
struct CrtDbgInitializer {
    CrtDbgInitializer() {
        int flags = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);
        flags |= _CRTDBG_CHECK_ALWAYS_DF | _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF;
        _CrtSetDbgFlag(flags);
    }
};
static CrtDbgInitializer g_crtDbgInit;
#endif

// Lightweight logging helper that avoids C++ stream facilities doing heavy locking
// Limit entries to avoid DoS via enormous logs
static void LogToFile(const std::string& s) {
    constexpr size_t kMaxLogEntry = 1024; // keep log entries reasonably small
    std::string out = s;
    if (out.size() > kMaxLogEntry) {
        out.resize(kMaxLogEntry);
        out += "...(truncated)";
    }
    HANDLE h = CreateFileA("wasm_hb_log.txt", FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return;
    SetFilePointer(h, 0, NULL, FILE_END);
    DWORD written = 0;
    WriteFile(h, out.c_str(), (DWORD)out.size(), &written, NULL);
    WriteFile(h, "\r\n", 2, &written, NULL);
    CloseHandle(h);
}

namespace Genesis::Engine {

// Stub used when unregistering a host function — keeps the import present but traps if called.
m3ApiRawFunction(host_unregistered_stub) {
    // Trap with a descriptive string to help debugging if an unregistered import is invoked
    m3ApiTrap("host function unregistered");
}

// Callback wrapper types stored in a global map keyed by "namespace:name".
// Forward-declare ImportUserdata because CallbackBase keeps a shared_ptr to it.
struct ImportUserdata;
struct CallbackBase {
    virtual ~CallbackBase() = default;
    bool active = true;
    IM3Module module = nullptr;
    size_t maxStringLength = 1024 * 1024; // copied from HostBindings default
    // Keep a per-registration ImportUserdata alive so we never mutate a shared global
    std::shared_ptr<ImportUserdata> udptr;
    // Optional pointer to the WasmModule's timed_out flag to allow lock-free checks in trampolines
    std::atomic<bool>* timed_out_ptr = nullptr;
};

struct CallbackI32I32 : CallbackBase {
    std::function<int32_t(int32_t)> cb;
};

struct CallbackVoidString : CallbackBase {
    std::function<void(const std::string&)> cb;
};

struct CallbackRaw : CallbackBase {
    M3RawCall cb;
};

static std::mutex g_callbacksMutex;
static std::unordered_map<std::string, std::shared_ptr<CallbackBase>> g_callbacks;
// Stable key userdata pointers so we can pass a long-lived pointer into wasm3 that
// remains valid even if the callback holder is erased from g_callbacks.
static std::unordered_map<std::string, std::shared_ptr<std::string>> g_callback_keys;

// ImportUserdata: small POD we pass as userdata into m3_LinkRawFunctionEx. The trampoline
// will interpret this and either use the pointer as a key string pointer (kind=1) or a
// direct holder pointer (kind=2). We keep the ImportUserdata instances alive via
// g_import_userdata so the pointers remain valid for the lifetime of the registration.
struct ImportUserdata { int kind; void* ptr; };
static std::unordered_map<std::string, std::shared_ptr<ImportUserdata>> g_import_userdata;

// Small helper to build a short, safe preview of an untrusted string for logs.
// This avoids writing arbitrarily large or binary data into log files.
static std::string SanitizePreview(const std::string& s, size_t max_preview = 256) {
    std::string out;
    size_t n = s.size() < max_preview ? s.size() : max_preview;
    out.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (c >= 32 && c <= 126) out.push_back(static_cast<char>(c));
        else out.push_back('?');
    }
    if (s.size() > max_preview) out += "...";
    out += " (len=" + std::to_string(s.size()) + ")";
    return out;
}

static void WriteMiniDumpForException(EXCEPTION_POINTERS* ep, const std::string& context) {
    SYSTEMTIME st; GetLocalTime(&st);
    char filename[256];
    sprintf_s(filename, "token_crash_%04d%02d%02d_%02d%02d%02d.dmp", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    std::string dir = "Tools\\external\\procdump\\dumps\\";
    CreateDirectoryA(dir.c_str(), NULL);
    std::string path = dir + filename;
    HANDLE hFile = CreateFileA(path.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return;
    MINIDUMP_EXCEPTION_INFORMATION mei;
    mei.ThreadId = GetCurrentThreadId();
    mei.ExceptionPointers = ep;
    mei.ClientPointers = FALSE;
    MINIDUMP_TYPE dumpType = static_cast<MINIDUMP_TYPE>(MiniDumpWithFullMemory | MiniDumpWithHandleData | MiniDumpWithThreadInfo | MiniDumpWithUnloadedModules);
    MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hFile, dumpType, &mei, NULL, NULL);
    CloseHandle(hFile);
    std::ostringstream oss; oss << "WriteMiniDumpForException: wrote dump " << path << " context: " << context;
    LogToFile(oss.str());
} 

static void WriteMiniDumpSnapshot(const std::string& context) {
    SYSTEMTIME st; GetLocalTime(&st);
    char filename[256];
    sprintf_s(filename, "token_snapshot_%04d%02d%02d_%02d%02d%02d.dmp", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    std::string dir = "Tools\\external\\procdump\\dumps\\";
    CreateDirectoryA(dir.c_str(), NULL);
    std::string path = dir + filename;
    HANDLE hFile = CreateFileA(path.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        LogToFile("WriteMiniDumpSnapshot: CreateFileA failed");
        return;
    }
    MINIDUMP_TYPE dumpType = static_cast<MINIDUMP_TYPE>(MiniDumpWithFullMemory | MiniDumpWithHandleData | MiniDumpWithThreadInfo | MiniDumpWithUnloadedModules);
    BOOL rv = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hFile, dumpType, NULL, NULL, NULL);
    CloseHandle(hFile);
    std::ostringstream oss; oss << "WriteMiniDumpSnapshot: wrote dump " << path << " context: " << context;
    LogToFile(oss.str());
} 

// Vectored exception handler that writes a dump when an access violation occurs.
// We register this with AddVectoredExceptionHandler during sensitive map operations so we
// can capture an exception context even if the process is about to crash.
static LONG CALLBACK VectoredDumpHandler(PEXCEPTION_POINTERS ep) {
    try {
        WriteMiniDumpForException(ep, "VectoredDumpHandler");
    } catch (...) {
        LogToFile("VectoredDumpHandler: WriteMiniDumpForException threw");
    }
    // Do not swallow the exception; allow normal crash handling to proceed after we've written a dump.
    return EXCEPTION_CONTINUE_SEARCH;
}

// RAII helper to register/unregister vectored exception handler
struct ScopedVectoredDump {
    void* handler;
    ScopedVectoredDump() : handler(nullptr) {
        handler = AddVectoredExceptionHandler(1, VectoredDumpHandler);
        std::ostringstream oss; oss << "ScopedVectoredDump: handler=" << handler;
        LogToFile(oss.str());
    }
    ~ScopedVectoredDump() {
        if (handler) {
            RemoveVectoredExceptionHandler(handler);
            std::ostringstream oss; oss << "ScopedVectoredDump: removed handler=" << handler;
            LogToFile(oss.str());
            handler = nullptr;
        }
    }
};

static void SafeUnregisterCallback_NoThrow(const std::string& key, bool module_managed) {
    // Register a vectored exception handler so we can capture an AV during map access.
    ScopedVectoredDump svd;

    // Conservative, non-throwing instrumentation: inspect pointer with VirtualQuery,
    // log memory region and a small preview, write a snapshot dump, but avoid dereferencing
    // or destroying potentially-corrupted holder pointer (unsafe to touch or delete).
    std::lock_guard<std::mutex> lk(g_callbacksMutex);
    auto it = g_callbacks.find(key);
    if (it == g_callbacks.end()) {
        std::ostringstream oss; oss << "SafeUnregisterCallback_NoThrow: no holder found for key='" << key << "'";
        LogToFile(oss.str());
        return;
    }

    void* rawptr = it->second ? static_cast<void*>(it->second.get()) : nullptr;
    std::ostringstream oss; oss << "SafeUnregisterCallback_NoThrow: holder rawptr=" << rawptr << " module_managed=" << (module_managed ? "true" : "false");
    LogToFile(oss.str());

    if (!rawptr) {
        LogToFile("SafeUnregisterCallback_NoThrow: rawptr is null, nothing to probe");
        return;
    }

    MEMORY_BASIC_INFORMATION mbi;
    SIZE_T q = VirtualQuery(rawptr, &mbi, sizeof(mbi));
    if (q == 0) {
        std::ostringstream os2; os2 << "SafeUnregisterCallback_NoThrow: VirtualQuery failed for ptr=" << rawptr;
        LogToFile(os2.str());
        WriteMiniDumpSnapshot(key);
        return;
    }

    std::ostringstream os3;
    os3 << "SafeUnregisterCallback_NoThrow: region Base=" << mbi.BaseAddress << " RegionSize=" << mbi.RegionSize << " State=" << mbi.State << " Protect=" << mbi.Protect;
    LogToFile(os3.str());

    // Only preview bytes if memory is committed
    if (mbi.State == MEM_COMMIT) {
        uintptr_t base = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
        uintptr_t p = reinterpret_cast<uintptr_t>(rawptr);
        size_t avail = static_cast<size_t>(mbi.RegionSize - (p - base));
        size_t preview = avail < 256 ? avail : 256;
        if (preview > 0) {
            // safe to read within the region
            std::string hex;
            hex.reserve(preview * 3 + 32);
            unsigned char* bytes = reinterpret_cast<unsigned char*>(rawptr);
            for (size_t i = 0; i < preview; ++i) {
                char buf[8]; sprintf_s(buf, "%02X", bytes[i]);
                if (i) hex.push_back(' ');
                hex += buf;
            }
            std::ostringstream os4; os4 << "SafeUnregisterCallback_NoThrow: memory preview (first " << preview << " bytes): " << hex;
            LogToFile(os4.str());
        }
    } else {
        std::ostringstream os5; os5 << "SafeUnregisterCallback_NoThrow: not committed (State=" << mbi.State << ") - skipping preview";
        LogToFile(os5.str());
    }

    // Write a snapshot dump so we can analyze registers/stack and module memory later
    WriteMiniDumpSnapshot(key);

    // Avoid modifying or erasing the map entry here: modifying may cause the shared_ptr
    // destructor to run on a potentially-corrupted pointer and cause an immediate crash.
    // We'll analyze the snapshot to decide a safe remediation (e.g., change teardown order
    // so Token destructors run only after global callback cleanup).
}


// Diagnostic helper: copy g_callbacks under lock and safely probe holder memory for
// each entry (small preview). This avoids holding the lock while performing memory reads.
static void DumpCallbacksSnapshot(const std::string& context) {
    try {
        std::vector<std::pair<std::string, void*>> entries;
        {
            std::lock_guard<std::mutex> lk(g_callbacksMutex);
            entries.reserve(g_callbacks.size());
            for (const auto &p : g_callbacks) {
                void* ptr = p.second ? static_cast<void*>(p.second.get()) : nullptr;
                entries.emplace_back(p.first, ptr);
            }
        }
        std::ostringstream oss; oss << "DumpCallbacksSnapshot: context=" << context << " entries=" << entries.size();
        LogToFile(oss.str());
        for (const auto &e : entries) {
            std::ostringstream o2; o2 << "DumpCallbacksSnapshot: key=" << e.first << " ptr=" << e.second;
            LogToFile(o2.str());
            if (e.second) {
                MEMORY_BASIC_INFORMATION mbi;
                SIZE_T q = VirtualQuery(e.second, &mbi, sizeof(mbi));
                if (q != 0 && mbi.State == MEM_COMMIT) {
                    DWORD prot = mbi.Protect;
                    bool skipRead = false;
                    if ((prot & PAGE_GUARD) != 0) skipRead = true;
                    if (prot == PAGE_NOACCESS) skipRead = true;
                    if (skipRead) {
                        std::ostringstream osr; osr << "DumpCallbacksSnapshot: key=" << e.first << " memory not readable (Protect=" << prot << ")";
                        LogToFile(osr.str());
                    } else {
                        uintptr_t base = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
                        uintptr_t p = reinterpret_cast<uintptr_t>(e.second);
                        size_t avail = static_cast<size_t>(mbi.RegionSize - (p - base));
                        size_t preview = avail < 64 ? avail : 64;
                        if (preview > 0) {
                            std::string hex;
                            hex.reserve(preview * 3 + 32);
                            unsigned char* bytes = reinterpret_cast<unsigned char*>(e.second);
                            {
                                SIZE_T bytesRead = 0;
                                std::vector<unsigned char> bufVec(preview);
                                if (ReadProcessMemory(GetCurrentProcess(), e.second, bufVec.data(), preview, &bytesRead) && bytesRead > 0) {
                                    for (SIZE_T i = 0; i < bytesRead; ++i) {
                                        char bbuf[8]; sprintf_s(bbuf, "%02X", bufVec[i]);
                                        if (i) hex.push_back(' ');
                                        hex += bbuf;
                                    }
                                } else {
                                    std::ostringstream err; err << "DumpCallbacksSnapshot: ReadProcessMemory failed for key=" << e.first << " err=" << GetLastError();
                                    LogToFile(err.str());
                                    hex = "<READ_FAILED>";
                                }
                            }
                            std::ostringstream o3; o3 << "DumpCallbacksSnapshot: key=" << e.first << " memory preview:" << hex;
                            LogToFile(o3.str());
                        }
                    }
                } else {
                    std::ostringstream o4; o4 << "DumpCallbacksSnapshot: key=" << e.first << " memory region not committed or VirtualQuery failed";
                    LogToFile(o4.str());
                }
            } else {
                LogToFile(std::string("DumpCallbacksSnapshot: ptr is null for key=") + e.first);
            }
        }
    } catch (...) {
        LogToFile("DumpCallbacksSnapshot: exception while generating snapshot");
    }
}

// Deferred unregistration queue to avoid touching g_callbacks in potentially-unsafe teardown moments
static std::mutex g_deferredUnregMutex;
static std::vector<std::pair<std::string, bool>> g_deferred_unregs;

static void DeferUnregisterCallback(const std::string& key, bool module_managed) {
    try {
        std::lock_guard<std::mutex> lk(g_deferredUnregMutex);
        g_deferred_unregs.emplace_back(key, module_managed);
        std::ostringstream oss; oss << "DeferUnregisterCallback: queued key=" << key << " module_managed=" << (module_managed ? "true" : "false");
        LogToFile(oss.str());
    } catch (...) {
        LogToFile("DeferUnregisterCallback: exception while queuing");
    }
}

static void ProcessDeferredUnregistrations_Internal() {
    std::vector<std::pair<std::string, bool>> todo;
    {
        std::lock_guard<std::mutex> lk(g_deferredUnregMutex);
        todo.swap(g_deferred_unregs);
    }
    for (auto &p : todo) {
        std::ostringstream oss; oss << "ProcessDeferredUnregistrations: processing key=" << p.first << " module_managed=" << (p.second ? "true" : "false");
        LogToFile(oss.str());
        SafeUnregisterCallback_NoThrow(p.first, p.second);
    }
}


// Trampolines: small adapters that extract userdata (the Callback holder) and invoke
// the C++ callable. These are minimal and must be robust (trap on unregistered callbacks).

m3ApiRawFunction(host_trampoline_i32_i32) {
    m3ApiReturnType(uint32_t);
    m3ApiGetArg(int32_t, in0);
#ifdef _DEBUG
    std::cerr << "host_trampoline_i32_i32 entered: in0=" << in0 << std::endl;
    std::ostringstream ossdbg; ossdbg << "trampoline_i32: in0=" << in0;
    LogToFile(ossdbg.str());
#endif
    void* ud = _ctx ? _ctx->userdata : nullptr;
    if (!ud) m3ApiTrap("host userdata missing");
    auto udt = reinterpret_cast<ImportUserdata*>(ud);
    if (!udt) m3ApiTrap("host userdata parse failed");
    std::shared_ptr<CallbackBase> base;
    if (udt->kind == 2) {
        auto hptr = reinterpret_cast<CallbackI32I32*>(udt->ptr);
        if (!hptr || !hptr->active) m3ApiTrap("host function unregistered");
        // If the module has been marked timed-out/quarantined, reject the call immediately
        if (hptr->timed_out_ptr && hptr->timed_out_ptr->load()) m3ApiTrap("module timed out");
        int32_t out = 0;
        try {
            out = hptr->cb(in0);
        } catch (...) {
            m3ApiTrap("host callback threw exception");
        }
#ifdef _DEBUG
        if (!_CrtCheckMemory()) {
            LogToFile("trampoline_i32: _CrtCheckMemory FAILED (heap corruption detected)");
        } else {
            LogToFile("trampoline_i32: _CrtCheckMemory OK");
        }
        // Skipping CaptureStackBackTrace here to avoid SEH crashes during repro; use minidumps for symbolized stacks.
#endif
        m3ApiReturn((uint32_t)out);
    } else if (udt->kind == 1) {
        auto keyPtr = reinterpret_cast<const std::string*>(udt->ptr);
        if (!keyPtr) m3ApiTrap("host userdata missing");
        std::lock_guard<std::mutex> lk(g_callbacksMutex);
        auto it = g_callbacks.find(*keyPtr);
        if (it == g_callbacks.end() || !it->second || !it->second->active) m3ApiTrap("host function unregistered");
        base = it->second;
    } else {
        m3ApiTrap("host userdata kind unknown");
    }
    auto h = std::static_pointer_cast<CallbackI32I32>(base);
    // If module was quarantined, reject quickly
    if (h->timed_out_ptr && h->timed_out_ptr->load()) m3ApiTrap("module timed out");
    int32_t out = 0;
    try {
        out = h->cb(in0);
    } catch (...) {
        m3ApiTrap("host callback threw exception");
    }
    m3ApiReturn((uint32_t)out);
}

m3ApiRawFunction(host_trampoline_v_ptr_len) {
    m3ApiGetArg(int32_t, ptr);
    m3ApiGetArg(int32_t, len);
#ifdef _DEBUG
    std::ostringstream ossdbg;
    ossdbg << "trampoline_v: ptr=" << ptr << " len=" << len;
    LogToFile(ossdbg.str());
#endif
    void* ud = _ctx ? _ctx->userdata : nullptr;
    if (!ud) m3ApiTrap("host userdata missing");
    auto udt = reinterpret_cast<ImportUserdata*>(ud);
    if (!udt) m3ApiTrap("host userdata parse failed");

    // Two cases: direct holder pointer (kind==2) or key-based lookup (kind==1)
    if (udt->kind == 2) {
        auto hptr = reinterpret_cast<CallbackVoidString*>(udt->ptr);
        if (!hptr || !hptr->active) m3ApiTrap("host function unregistered");
        // Reject calls if module was previously timed out/quarantined
        if (hptr->timed_out_ptr && hptr->timed_out_ptr->load()) m3ApiTrap("module timed out");

        IM3Runtime modRuntime = m3_GetModuleRuntime(hptr->module);
#ifdef _DEBUG
        {
            std::ostringstream oss; oss << "trampoline_v: modRuntime=" << modRuntime;
            LogToFile(oss.str());
        }
#endif
        uint32_t memSz = 0;
        uint8_t* mem = m3_GetMemory(modRuntime, &memSz, 0);
#ifdef _DEBUG
        {
            std::ostringstream oss; oss << "trampoline_v: mem=" << (void*)mem << " memSz=" << memSz;
            LogToFile(oss.str());
        }
#endif
        if (!mem) m3ApiTrap("host memory not available");
        if (ptr < 0) m3ApiTrap("invalid pointer");
        if (len < 0) m3ApiTrap("invalid length");
        uint32_t uptr = static_cast<uint32_t>(ptr);
        uint32_t ulen = static_cast<uint32_t>(len);
        if (ulen > hptr->maxStringLength) m3ApiTrap("string length exceeds limit");
        uint64_t end = static_cast<uint64_t>(uptr) + static_cast<uint64_t>(ulen);
        if (end > memSz) m3ApiTrap("string access out-of-bounds");

        std::string s(reinterpret_cast<char*>(mem + uptr), static_cast<size_t>(ulen));
        {
            std::ostringstream oss; oss << "trampoline_v: about to call callback with s_preview='" << SanitizePreview(s) << "'";
            LogToFile(oss.str());
        }
        try {
            hptr->cb(s);
        } catch (...) {
            m3ApiTrap("host callback threw exception");
        }
        {
            std::ostringstream oss; oss << "trampoline_v: callback returned";
            LogToFile(oss.str());
        }
        LogToFile("trampoline_v: about to call m3ApiSuccess()");
#ifdef _DEBUG
        if (!_CrtCheckMemory()) {
            LogToFile("trampoline_v: _CrtCheckMemory FAILED (heap corruption detected)");
        } else {
            LogToFile("trampoline_v: _CrtCheckMemory OK");
        }
        // Skipping CaptureStackBackTrace here to avoid SEH crashes during repro; use minidumps for symbolized stacks.
#endif
        m3ApiSuccess();
    } else if (udt->kind == 1) {
        auto keyPtr = reinterpret_cast<const std::string*>(udt->ptr);
        if (!keyPtr) m3ApiTrap("host userdata missing");
        std::shared_ptr<CallbackBase> base;
        {
            std::lock_guard<std::mutex> lk(g_callbacksMutex);
            auto it = g_callbacks.find(*keyPtr);
            if (it == g_callbacks.end() || !it->second || !it->second->active) m3ApiTrap("host function unregistered");
            base = it->second;
        }
        auto h = std::static_pointer_cast<CallbackVoidString>(base);
        // If module was quarantined, reject quickly
        if (h->timed_out_ptr && h->timed_out_ptr->load()) m3ApiTrap("module timed out");

        IM3Runtime modRuntime = m3_GetModuleRuntime(h->module);
#ifdef _DEBUG
        {
            std::ostringstream oss; oss << "trampoline_v: modRuntime=" << modRuntime;
            LogToFile(oss.str());
        }
#endif
        uint32_t memSz = 0;
        uint8_t* mem = m3_GetMemory(modRuntime, &memSz, 0);
#ifdef _DEBUG
        {
            std::ostringstream oss; oss << "trampoline_v: mem=" << (void*)mem << " memSz=" << memSz;
            LogToFile(oss.str());
        }
#endif
        if (!mem) m3ApiTrap("host memory not available");
        if (ptr < 0) m3ApiTrap("invalid pointer");
        if (len < 0) m3ApiTrap("invalid length");
        uint32_t uptr = static_cast<uint32_t>(ptr);
        uint32_t ulen = static_cast<uint32_t>(len);
        if (ulen > h->maxStringLength) m3ApiTrap("string length exceeds limit");
        uint64_t end = static_cast<uint64_t>(uptr) + static_cast<uint64_t>(ulen);
        if (end > memSz) m3ApiTrap("string access out-of-bounds");

        std::string s(reinterpret_cast<char*>(mem + uptr), static_cast<size_t>(ulen));
        {
            std::ostringstream oss; oss << "trampoline_v: about to call callback with s_preview='" << SanitizePreview(s) << "'";
            LogToFile(oss.str());
        }
        try {
            h->cb(s);
        } catch (...) {
            m3ApiTrap("host callback threw exception");
        }
        {
            std::ostringstream oss; oss << "trampoline_v: callback returned";
            LogToFile(oss.str());
        }
        LogToFile("trampoline_v: about to call m3ApiSuccess()");
        m3ApiSuccess();
    } else {
        m3ApiTrap("host userdata kind unknown");
    }
}

// Helper to build map key
static std::string KeyFor(const char* ns, const char* name) { return std::string(ns) + ":" + std::string(name); }

// Register a raw host function (namespace, name, signature, callback). Returns a Token
// that will unregister on destruction. The callback must conform to wasm3's M3RawCall.
m3ApiRawFunction(host_trampoline_raw) {
    void* ud = _ctx ? _ctx->userdata : nullptr;
    if (!ud) m3ApiTrap("host userdata missing");
    auto udt = reinterpret_cast<ImportUserdata*>(ud);
    if (!udt) m3ApiTrap("host userdata parse failed");
    if (udt->kind == 2) {
        auto hptr = reinterpret_cast<CallbackRaw*>(udt->ptr);
        if (!hptr || !hptr->active) m3ApiTrap("host function unregistered");
        if (hptr->timed_out_ptr && hptr->timed_out_ptr->load()) m3ApiTrap("module timed out");
        IM3Runtime modRuntime = m3_GetModuleRuntime(hptr->module);
        {
            std::ostringstream oss; oss << "host_trampoline_raw: invoking raw cb holder=" << hptr << " module=" << hptr->module << " runtime=" << modRuntime;
            LogToFile(oss.str());
        }
        try {
            auto r = hptr->cb(modRuntime, _ctx, _sp, _mem);
            std::ostringstream oss; oss << "host_trampoline_raw: callback returned ptr=" << (void*)r;
            LogToFile(oss.str());
            return r;
        } catch (...) {
            LogToFile("host_trampoline_raw: callback threw exception");
            m3ApiTrap("host callback threw exception");
        }
    } else if (udt->kind == 1) {
    } else if (udt->kind == 1) {
        auto keyPtr = reinterpret_cast<const std::string*>(udt->ptr);
        if (!keyPtr) m3ApiTrap("host userdata missing");
        std::shared_ptr<CallbackBase> base;
        {
            std::lock_guard<std::mutex> lk(g_callbacksMutex);
            auto it = g_callbacks.find(*keyPtr);
            if (it == g_callbacks.end() || !it->second || !it->second->active) m3ApiTrap("host function unregistered");
            base = it->second;
        }
        auto h = std::static_pointer_cast<CallbackRaw>(base);
        if (h->timed_out_ptr && h->timed_out_ptr->load()) m3ApiTrap("module timed out");
        IM3Runtime modRuntime = m3_GetModuleRuntime(h->module);
        try {
            return h->cb(modRuntime, _ctx, _sp, _mem);
        } catch (...) {
            m3ApiTrap("host callback threw exception");
        }
    } else {
        m3ApiTrap("host userdata kind unknown");
    }
}

HostBindings::Token HostBindings::RegisterRaw(const char* ns, const char* name, const char* sig, M3RawCall cb, std::atomic<bool>* module_timed_out_ptr) {
    if (!module_) return {};
    IM3Runtime modRuntime = m3_GetModuleRuntime(module_);
    const char* modName = m3_GetModuleName(module_);
    std::cerr << "RegisterRaw: creating holder for ns='" << ns << "' name='" << name << "' module=" << module_ << " runtime=" << modRuntime << " thread=" << std::this_thread::get_id() << std::endl;

    try {
        // Create holder for raw callback
        auto holder = std::make_shared<CallbackRaw>();
        holder->cb = cb;
        holder->module = module_;
        holder->maxStringLength = maxStringLength_;

        std::string key = KeyFor(ns, name);
        std::shared_ptr<std::string> keyPtr;
        {
            std::lock_guard<std::mutex> lk(g_callbacksMutex);
            g_callbacks[key] = holder;
            auto itKey = g_callback_keys.find(key);
            if (itKey == g_callback_keys.end()) {
                keyPtr = std::make_shared<std::string>(key);
                g_callback_keys[key] = keyPtr;
            } else {
                keyPtr = itKey->second;
            }
        }
        std::cerr << "RegisterRaw: inserted holder=" << holder.get() << " keyPtr=" << (void*)keyPtr.get() << std::endl;

        auto udptr_local = std::make_shared<ImportUserdata>();
        IM3Runtime modRuntimeLocal = m3_GetModuleRuntime(module_);
        // Prefer key-based userdata so the userdata pointer stored in wasm3 is a stable pointer
        // to a string that we hold in g_callback_keys. This avoids dangling userdata if we
        // erase our internal holder maps before the module is fully torn down.
        udptr_local->kind = 1;
        udptr_local->ptr = keyPtr.get();
        if (modRuntimeLocal) {
            // If caller provided a timed_out pointer (e.g., LinkHostFunctions already holds g_wasmMutex
            // and can pass &wm->timed_out), use it to avoid re-locking g_wasmMutex inside
            // WasmRuntime::GetModuleTimedOutPtr which would deadlock.
            if (module_timed_out_ptr) {
                holder->timed_out_ptr = module_timed_out_ptr;
            } else {
                try {
                    holder->timed_out_ptr = WasmRuntime::GetModuleTimedOutPtr(module_);
                } catch (const std::system_error &se) {
                    std::cerr << "RegisterRaw: GetModuleTimedOutPtr threw system_error: " << se.what() << " code=" << se.code().value() << "\n";
                    holder->timed_out_ptr = nullptr;
                }
            }
        }

        holder->udptr = udptr_local;
        {
            std::lock_guard<std::mutex> lk2(g_callbacksMutex);
            g_import_userdata[key] = udptr_local;
        }

        std::cerr << "RegisterRaw: prepared import userdata, about to call m3_LinkRawFunctionEx" << std::endl;

        const char* thisSig = sig ? sig : "";
        try {
            // Prefer passing the key-based ImportUserdata pointer so module-stored userdata remains stable
            M3Result r = m3_LinkRawFunctionEx(module_, ns, name, thisSig, (M3RawCall)host_trampoline_raw, udptr_local.get());
            if (r) {
                std::cerr << "HostBindings: m3_LinkRawFunctionEx failed for '" << ns << "'.'" << name << "' sig='" << thisSig << "': " << r << std::endl;
                // Fallback: try the older holder-pointer based path
                M3Result rf = m3_LinkRawFunctionEx(module_, ns, name, thisSig, (M3RawCall)host_trampoline_raw, holder.get());
                if (rf) {
                    std::cerr << "HostBindings: fallback m3_LinkRawFunction failed: " << rf << std::endl;
                    std::lock_guard<std::mutex> lk(g_callbacksMutex);
                    auto it = g_callbacks.find(key);
                    if (it != g_callbacks.end() && it->second == holder) g_callbacks.erase(it);
                    g_import_userdata.erase(key);
                    return {};
                }
            }
        } catch (const std::system_error &se) {
            std::cerr << "RegisterRaw: m3_LinkRawFunctionEx threw system_error: " << se.what() << " code=" << se.code().value() << std::endl;
#ifdef _WIN32
            void* addrs[64];
            USHORT frames = CaptureStackBackTrace(0, 64, addrs, nullptr);
            std::cerr << "Stack frames captured: " << frames << std::endl;
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
                for (USHORT i=0;i<frames;i++) std::cerr << addrs[i] << std::endl;
            }
#endif
            // Fallback: try raw link path as a last resort
            std::cerr << "RegisterRaw: attempting fallback to m3_LinkRawFunction" << std::endl;
            M3Result rf = m3_LinkRawFunction(module_, ns, name, thisSig, cb);
            if (rf) {
                std::cerr << "RegisterRaw: fallback m3_LinkRawFunction failed: " << rf << std::endl;
                std::lock_guard<std::mutex> lk(g_callbacksMutex);
                auto it = g_callbacks.find(key);
                if (it != g_callbacks.end() && it->second == holder) g_callbacks.erase(it);
                g_import_userdata.erase(key);
                return {};
            }
            std::cerr << "RegisterRaw: fallback m3_LinkRawFunction succeeded" << std::endl;
        } catch (const std::exception &ex) {
            std::cerr << "RegisterRaw: exception while linking: " << ex.what() << std::endl;
            std::lock_guard<std::mutex> lk(g_callbacksMutex);
            auto it = g_callbacks.find(key);
            if (it != g_callbacks.end() && it->second == holder) g_callbacks.erase(it);
            g_import_userdata.erase(key);
            return {};
        }

        std::cerr << "RegisterRaw: linked ok, holder=" << holder.get() << " key=" << key << std::endl;
        Token tok(module_, this, std::string(ns), std::string(name), std::string(sig));
        if (module_timed_out_ptr) {
            tok.module_managed_ = true;
        } else {
            try {
                tok.module_managed_ = (WasmRuntime::GetModuleTimedOutPtr(module_) != nullptr);
            } catch (const std::system_error &se) {
                std::cerr << "RegisterRaw: GetModuleTimedOutPtr threw when setting module_managed_: " << se.what() << " code=" << se.code().value() << std::endl;
                tok.module_managed_ = false;
            }
        }
        return tok;
    } catch (const std::system_error &se) {
        std::cerr << "RegisterRaw: caught system_error: " << se.what() << " code=" << se.code().value() << " thread=" << std::this_thread::get_id() << std::endl;
#ifdef _WIN32
        void* addrs[64];
        USHORT frames = CaptureStackBackTrace(0, 64, addrs, nullptr);
        std::cerr << "Stack frames captured: " << frames << std::endl;
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
            for (USHORT i=0;i<frames;i++) std::cerr << addrs[i] << std::endl;
        }
#endif
        // Try to cleanup any partial registration
        try {
            std::lock_guard<std::mutex> lk(g_callbacksMutex);
            auto it = g_callbacks.find(KeyFor(ns, name));
            if (it != g_callbacks.end()) g_callbacks.erase(it);
            g_import_userdata.erase(KeyFor(ns, name));
        } catch (...) {}
        return {};
    } catch (const std::exception &ex) {
        std::cerr << "RegisterRaw: exception: " << ex.what() << std::endl;
        return {};
    } catch (...) {
        std::cerr << "RegisterRaw: unknown exception" << std::endl;
        return {};
    }
}

// Register a typed (i32->i32) host function
HostBindings::Token HostBindings::RegisterI32I32(const char* ns, const char* name, std::function<int32_t(int32_t)> cb) {
    if (!module_) return {};
    auto holder = std::make_shared<CallbackI32I32>();
    IM3Runtime modRuntime = m3_GetModuleRuntime(module_);
    const char* modName = m3_GetModuleName(module_);
    std::cerr << "RegisterI32I32: creating holder=" << holder.get() << " module=" << module_ << " name='" << (modName ? modName : "(null)") << "' runtime=" << modRuntime << std::endl;
    holder->cb = std::move(cb);
    holder->module = module_;
    holder->maxStringLength = maxStringLength_;

    std::string key = KeyFor(ns, name);
    std::shared_ptr<std::string> keyPtr;
    {
        std::lock_guard<std::mutex> lk(g_callbacksMutex);
        g_callbacks[key] = holder;
        auto itKey = g_callback_keys.find(key);
        if (itKey == g_callback_keys.end()) {
            keyPtr = std::make_shared<std::string>(key);
            g_callback_keys[key] = keyPtr;
        } else {
            keyPtr = itKey->second;
        }
    }

    // Create a per-registration ImportUserdata to avoid shared-state races.
    auto udptr_local = std::make_shared<ImportUserdata>();
    // Prefer key-based userdata (kind==1) so the userdata pointer stored in the module
    // is a stable string pointer that does not dangle if holder/udptr are destroyed
    // before the module is freed. This avoids use-after-free when the module runtime
    // frees internals that may reference userdata pointers.
    udptr_local->kind = 1;
    udptr_local->ptr = keyPtr.get();
    if (modRuntime) {
        // If the module runtime exists, capture a pointer to its timed_out flag
        holder->timed_out_ptr = WasmRuntime::GetModuleTimedOutPtr(module_);
    }
    holder->udptr = udptr_local;
    {
        // Keep a stable reference to the ImportUserdata so the pointer we pass into wasm3 cannot dangle
        // even if the holder is later removed from g_callbacks.
        std::lock_guard<std::mutex> lk2(g_callbacksMutex);
        g_import_userdata[key] = udptr_local;
    }

    const char* sig = "i(i)";
    // Prefer the key-based userdata path
    M3Result r = m3_LinkRawFunctionEx(module_, ns, name, sig, (M3RawCall)host_trampoline_i32_i32, udptr_local.get());
    if (r) {
        std::cerr << "HostBindings: m3_LinkRawFunctionEx failed for '" << ns << "'.'" << name << "' sig='" << sig << "': " << r << std::endl;
        // Fallback: try holder pointer if key-based userdata is not accepted
        M3Result r2 = m3_LinkRawFunctionEx(module_, ns, name, sig, (M3RawCall)host_trampoline_i32_i32, holder.get());
        if (!r2) {
            std::cerr << "HostBindings: m3_LinkRawFunctionEx(holder) succeeded as fallback" << std::endl;
            udptr_local->kind = 2;
            udptr_local->ptr = holder.get();
            holder->timed_out_ptr = WasmRuntime::GetModuleTimedOutPtr(module_);
            holder->udptr = udptr_local;
            std::lock_guard<std::mutex> lk2(g_callbacksMutex);
            g_import_userdata[key] = udptr_local;
        } else {
            std::lock_guard<std::mutex> lk(g_callbacksMutex);
            auto it = g_callbacks.find(key);
            if (it != g_callbacks.end() && it->second == holder) g_callbacks.erase(it);
            // Clean up the import userdata entry we created above
            g_import_userdata.erase(key);
            return {};
        }
    }
    std::cerr << "RegisterI32I32: linked ok, holder=" << holder.get() << " key=" << key << std::endl;
    Token tok(module_, this, std::string(ns), std::string(name), std::string(sig));
    tok.module_managed_ = (WasmRuntime::GetModuleTimedOutPtr(module_) != nullptr);
    return tok;
}

// Register a host function that receives (ptr, len) and decodes a string
HostBindings::Token HostBindings::RegisterVoidString(const char* ns, const char* name, std::function<void(const std::string&)> cb) {
    if (!module_) return {};
    auto holder = std::make_shared<CallbackVoidString>();
    IM3Runtime modRuntime = m3_GetModuleRuntime(module_);
    const char* modName = m3_GetModuleName(module_);
    std::cerr << "RegisterVoidString: creating holder=" << holder.get() << " module=" << module_ << " name='" << (modName ? modName : "(null)") << "' runtime=" << modRuntime << std::endl;
    {
        std::ostringstream oss; oss << "RegisterVoidString: holder=" << holder.get() << " module=" << module_ << " key=" << ns << ":" << name << " runtime=" << modRuntime;
        LogToFile(oss.str());
    }
    holder->cb = std::move(cb);
    holder->module = module_;
    holder->maxStringLength = maxStringLength_;

    std::string key = KeyFor(ns, name);
    std::shared_ptr<std::string> keyPtr;
    {
        std::lock_guard<std::mutex> lk(g_callbacksMutex);
        g_callbacks[key] = holder;
        auto itKey = g_callback_keys.find(key);
        if (itKey == g_callback_keys.end()) {
            keyPtr = std::make_shared<std::string>(key);
            g_callback_keys[key] = keyPtr;
        } else {
            keyPtr = itKey->second;
        }
    }

    // Create a per-registration ImportUserdata to avoid shared-state races.
    auto udptr_local = std::make_shared<ImportUserdata>();
    // Prefer key-based userdata (kind==1) so the userdata pointer stored in the module
    // is a stable string pointer that does not dangle if holder/udptr are destroyed
    // before the module is freed. This avoids use-after-free when the module runtime
    // frees internals that may reference userdata pointers.
    udptr_local->kind = 1;
    udptr_local->ptr = keyPtr.get();
    if (modRuntime) {
        // If the module runtime exists, record a pointer to the module's timed_out flag
        holder->timed_out_ptr = WasmRuntime::GetModuleTimedOutPtr(module_);
    }
    holder->udptr = udptr_local;
    {
        // Keep a stable reference to the ImportUserdata so the pointer we pass into wasm3 cannot dangle.
        std::lock_guard<std::mutex> lk2(g_callbacksMutex);
        g_import_userdata[key] = udptr_local;
    }

    const char* sig = "v(ii)"; // void(ptr,len)
    // Prefer the key-based userdata path (udptr_local)
    M3Result r = m3_LinkRawFunctionEx(module_, ns, name, sig, (M3RawCall)host_trampoline_v_ptr_len, udptr_local.get());
    if (r) {
        std::cerr << "HostBindings: m3_LinkRawFunctionEx(key) failed for '" << ns << "'.'" << name << "' sig='" << sig << "': " << r << std::endl;
        // Fallback: if the wasm3 variant doesn't accept our userdata layout, try passing holder.get()
        M3Result r2 = m3_LinkRawFunctionEx(module_, ns, name, sig, (M3RawCall)host_trampoline_v_ptr_len, holder.get());
        if (!r2) {
            std::cerr << "HostBindings: m3_LinkRawFunctionEx(holder) succeeded as fallback" << std::endl;
            // If we fall back to the holder representation, update the ImportUserdata to reflect that
            udptr_local->kind = 2;
            udptr_local->ptr = holder.get();
            holder->timed_out_ptr = WasmRuntime::GetModuleTimedOutPtr(module_);
            holder->udptr = udptr_local;
            std::lock_guard<std::mutex> lk2(g_callbacksMutex);
            g_import_userdata[key] = udptr_local;
        } else {
            std::cerr << "HostBindings: m3_LinkRawFunctionEx(holder) fallback also failed: " << r2 << std::endl;
            std::lock_guard<std::mutex> lk(g_callbacksMutex);
            auto it = g_callbacks.find(key);
            if (it != g_callbacks.end() && it->second == holder) g_callbacks.erase(it);
            // Clean up any import userdata we might have registered
            g_import_userdata.erase(key);
            return {};
        }
    }
    std::cerr << "RegisterVoidString: linked ok, holder=" << holder.get() << " key=" << key << std::endl;
    {
        std::ostringstream oss; oss << "RegisterVoidString: keyPtrAddr=" << (void*)keyPtr.get();
        LogToFile(oss.str());
    }
    Token tok(module_, this, std::string(ns), std::string(name), std::string(sig));
    tok.module_managed_ = (WasmRuntime::GetModuleTimedOutPtr(module_) != nullptr);
    return tok;
}

std::string HostBindings::read_string(uint32_t ptr, uint32_t len) const {
    if (!module_) return {};
    if (len > maxStringLength_) return {};
    IM3Runtime runtime = m3_GetModuleRuntime(module_);
    uint32_t memSz = 0;
    uint8_t* mem = m3_GetMemory(runtime, &memSz, 0);
    if (!mem) return {};
    if (ptr > memSz) return {};
    uint64_t end = static_cast<uint64_t>(ptr) + static_cast<uint64_t>(len);
    if (end > memSz) return {};
    return std::string(reinterpret_cast<char*>(mem + ptr), static_cast<size_t>(len));
}

uint32_t HostBindings::alloc_in_module(const std::string& s) {
    if (!module_) return 0;
    if (s.size() > maxStringLength_) {
        std::cerr << "HostBindings: alloc_in_module rejected allocation size " << s.size() << " > maxStringLength " << maxStringLength_ << std::endl;
        return 0;
    }
    IM3Runtime runtime = m3_GetModuleRuntime(module_);
    IM3Function f = nullptr;
    M3Result r = m3_FindFunction(&f, runtime, "alloc");
    if (r) {
        r = m3_FindFunction(&f, runtime, "__alloc");
        if (r) {
            // No alloc export available; best-effort: return 0
            return 0;
        }
    }

    std::string arg = std::to_string(s.size());
    const char* argv[] = { arg.c_str() };
    M3Result r2 = m3_CallArgv(f, 1, argv);
    if (r2) {
        std::cerr << "HostBindings: alloc call failed: " << r2 << std::endl;
        return 0;
    }

    // Try to read the result (best-effort). If the API differs in your wasm3 version, this
    // may need adjustment (TODO: robust result extraction).
    uint32_t out = 0;
    const void* retPtrs[1] = { &out };
    M3Result r3 = m3_GetResults(f, 1, retPtrs);
    if (r3) {
        std::cerr << "HostBindings: m3_GetResults failed after alloc: " << r3 << std::endl;
        return 0;
    }

    uint32_t memSz = 0;
    uint8_t* mem = m3_GetMemory(runtime, &memSz, 0);
    if (!mem) {
        std::cerr << "HostBindings: alloc_in_module couldn't access memory to validate alloc result" << std::endl;
        return 0;
    }
    uint64_t end = static_cast<uint64_t>(out) + static_cast<uint64_t>(s.size());
    if (out >= memSz || end > memSz || end < out) {
        std::cerr << "HostBindings: alloc_in_module returned invalid pointer or range (out=" << out << " size=" << s.size() << " memSz=" << memSz << ")" << std::endl;
        return 0;
    }
    return out;
}

HostBindings::Token::~Token() noexcept {
    if (!module_) return;
    std::cerr << "Token::~Token: unregistering host '" << ns_ << "'.'" << name_ << "' sig='" << sig_ << "' module=" << module_ << " thread=" << std::this_thread::get_id() << std::endl;
    {
        std::ostringstream oss; oss << "Token::~Token: entering ns=" << ns_ << " name=" << name_ << " sig=" << sig_ << " module=" << module_ << " thread=" << std::this_thread::get_id();
        LogToFile(oss.str());
    }

    std::string key = KeyFor(ns_.c_str(), name_.c_str());
    std::cerr << "Token::~Token: about to acquire g_callbacksMutex for key='" << key << "'" << std::endl;
    // Register a vectored exception handler so we capture any AV during cleanup
    ScopedVectoredDump svd;
    {
        std::ostringstream oss2; oss2 << "Token::~Token: g_callbacks addr=" << &g_callbacks << " g_callback_keys addr=" << &g_callback_keys << " g_import_userdata addr=" << &g_import_userdata;
        LogToFile(oss2.str());
    }
    try {
        std::ostringstream osz; osz << "Token::~Token: sizes: g_callbacks=" << g_callbacks.size() << " g_callback_keys=" << g_callback_keys.size() << " g_import_userdata=" << g_import_userdata.size();
        LogToFile(osz.str());
    } catch (...) {
        LogToFile("Token::~Token: exception reading container sizes");
    }

    // Probe the callback entry for this key with minimal map access and safe memory reads
    // to avoid iterating potentially-corrupted container internals.
    // ProbeCallbackForKey only performs a find() under lock and then uses ReadProcessMemory
    // and VirtualQuery to safely get a small preview without risking iterator derefs.
    static auto ProbeCallbackForKey = [](const std::string &key) {
        try {
            void* ptr = nullptr;
            {
                std::lock_guard<std::mutex> lk(g_callbacksMutex);
                auto it = g_callbacks.find(key);
                bool found = (it != g_callbacks.end());
                if (found && it->second) ptr = static_cast<void*>(it->second.get());
                std::ostringstream os; os << "ProbeCallbackForKey: key=" << key << " found=" << found << " ptr=" << ptr;
                LogToFile(os.str());
                // Also probe g_callback_keys and g_import_userdata presence
                auto itk = g_callback_keys.find(key);
                if (itk != g_callback_keys.end()) {
                    std::ostringstream os2; os2 << "ProbeCallbackForKey: g_callback_keys keyPtr=" << (void*)itk->second.get();
                    LogToFile(os2.str());
                } else {
                    LogToFile(std::string("ProbeCallbackForKey: g_callback_keys missing for key=") + key);
                }
                auto itu = g_import_userdata.find(key);
                if (itu != g_import_userdata.end() && itu->second) {
                    std::ostringstream os3; os3 << "ProbeCallbackForKey: g_import_userdata kind=" << itu->second->kind << " ptr=" << itu->second->ptr;
                    LogToFile(os3.str());
                } else {
                    LogToFile(std::string("ProbeCallbackForKey: g_import_userdata missing for key=") + key);
                }
            }
            if (!ptr) return;
            MEMORY_BASIC_INFORMATION mbi;
            SIZE_T q = VirtualQuery(ptr, &mbi, sizeof(mbi));
            if (q == 0) {
                std::ostringstream os; os << "ProbeCallbackForKey: VirtualQuery failed for ptr=" << ptr;
                LogToFile(os.str());
                WriteMiniDumpSnapshot(key);
                return;
            }
            std::ostringstream os4; os4 << "ProbeCallbackForKey: Base=" << mbi.BaseAddress << " RegionSize=" << mbi.RegionSize << " State=" << mbi.State << " Protect=" << mbi.Protect;
            LogToFile(os4.str());
            if (mbi.State == MEM_COMMIT && ((mbi.Protect & PAGE_GUARD) == 0) && mbi.Protect != PAGE_NOACCESS) {
                uintptr_t base = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
                uintptr_t p = reinterpret_cast<uintptr_t>(ptr);
                size_t avail = static_cast<size_t>(mbi.RegionSize - (p - base));
                size_t preview = avail < 64 ? avail : 64;
                if (preview > 0) {
                    SIZE_T bytesRead = 0;
                    std::vector<unsigned char> bufVec(preview);
                    if (ReadProcessMemory(GetCurrentProcess(), ptr, bufVec.data(), preview, &bytesRead) && bytesRead > 0) {
                        std::ostringstream oh; oh << "ProbeCallbackForKey: memory preview:";
                        for (SIZE_T i = 0; i < bytesRead; ++i) {
                            char bbuf[8]; sprintf_s(bbuf, "%02X", bufVec[i]);
                            if (i) oh << " ";
                            oh << bbuf;
                        }
                        LogToFile(oh.str());
                    } else {
                        std::ostringstream err; err << "ProbeCallbackForKey: ReadProcessMemory failed err=" << GetLastError();
                        LogToFile(err.str());
                    }
                }
            } else {
                std::ostringstream os5; os5 << "ProbeCallbackForKey: not readable or not committed (State=" << mbi.State << " Protect=" << mbi.Protect << ")";
                LogToFile(os5.str());
            }
        } catch (...) {
            LogToFile("ProbeCallbackForKey: exception during probe");
        }
    };

    // Defer unregistration to avoid touching g_callbacks during potentially unsafe teardown windows.
    {
        std::ostringstream osd; osd << "Token::~Token: deferring unregister for key='" << key << "' module_managed=" << (module_managed_ ? "true" : "false");
        LogToFile(osd.str());
    }
    DeferUnregisterCallback(key, module_managed_);

    std::cerr << "Token::~Token: releasing lock and clearing module_" << std::endl;
    module_ = nullptr;
}


void HostBindings::CleanupModuleCallbacks(IM3Module module) {
    std::lock_guard<std::mutex> lk(g_callbacksMutex);
    for (auto it = g_callbacks.begin(); it != g_callbacks.end();) {
        if (it->second && it->second->module == module) {
            std::ostringstream oss; oss << "CleanupModuleCallbacks: erasing key=" << it->first << " holder=" << it->second.get();
            LogToFile(oss.str());
            g_import_userdata.erase(it->first);
            it = g_callbacks.erase(it);
        } else {
            ++it;
        }
    }
}

void HostBindings::ProcessDeferredUnregistrations() {
    ProcessDeferredUnregistrations_Internal();
}

#ifdef _DEBUG
size_t HostBindings::DebugGetCallbacksCount() {
    std::lock_guard<std::mutex> lk(g_callbacksMutex);
    return g_callbacks.size();
}
size_t HostBindings::DebugGetDeferredCount() {
    std::lock_guard<std::mutex> lk(g_deferredUnregMutex);
    return g_deferred_unregs.size();
}
#endif

} // namespace Genesis::Engine
#endif // HAVE_WASM3
