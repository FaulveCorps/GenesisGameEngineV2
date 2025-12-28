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
#include <windows.h>

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
HostBindings::Token HostBindings::RegisterRaw(const char* ns, const char* name, const char* sig, M3RawCall cb) {
    if (!module_) return {};
    IM3Runtime modRuntime = m3_GetModuleRuntime(module_);
    const char* modName = m3_GetModuleName(module_);
    std::cerr << "HostBindings: RegisterRaw module=" << module_ << " name='" << (modName ? modName : "(null)") << "' runtime=" << modRuntime << " ns='" << ns << "' name='" << name << "' sig='" << sig << "'" << std::endl;
    M3Result r = m3_LinkRawFunction(module_, ns, name, sig, cb);
    if (r) {
        std::cerr << "HostBindings: m3_LinkRawFunction failed for '" << ns << "'.'" << name << "' sig='" << sig << "': " << r << std::endl;
        return {};
    }
    Token tok(module_, this, std::string(ns), std::string(name), std::string(sig));
    tok.module_managed_ = (WasmRuntime::GetModuleTimedOutPtr(module_) != nullptr);
    return tok;
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
    if (modRuntime) {
        udptr_local->kind = 2;
        udptr_local->ptr = holder.get();
        // If this module is managed by WasmRuntime, capture a pointer to its timed_out flag
        holder->timed_out_ptr = WasmRuntime::GetModuleTimedOutPtr(module_);
    } else {
        udptr_local->kind = 1;
        udptr_local->ptr = keyPtr.get();
    }
    holder->udptr = udptr_local;
    {
        // Keep a stable reference to the ImportUserdata so the pointer we pass into wasm3 cannot dangle
        // even if the holder is later removed from g_callbacks.
        std::lock_guard<std::mutex> lk2(g_callbacksMutex);
        g_import_userdata[key] = udptr_local;
    }

    const char* sig = "i(i)";
    M3Result r = m3_LinkRawFunctionEx(module_, ns, name, sig, (M3RawCall)host_trampoline_i32_i32, udptr_local.get());
    if (r) {
        std::cerr << "HostBindings: m3_LinkRawFunctionEx failed for '" << ns << "'.'" << name << "' sig='" << sig << "': " << r << std::endl;
        std::lock_guard<std::mutex> lk(g_callbacksMutex);
        auto it = g_callbacks.find(key);
        if (it != g_callbacks.end() && it->second == holder) g_callbacks.erase(it);
        // Clean up the import userdata entry we created above
        g_import_userdata.erase(key);
        return {};
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
    if (modRuntime) {
        udptr_local->kind = 2;
        udptr_local->ptr = holder.get();
        holder->timed_out_ptr = WasmRuntime::GetModuleTimedOutPtr(module_);
    } else {
        udptr_local->kind = 1;
        udptr_local->ptr = keyPtr.get();
    }
    holder->udptr = udptr_local;
    {
        // Keep a stable reference to the ImportUserdata so the pointer we pass into wasm3 cannot dangle.
        std::lock_guard<std::mutex> lk2(g_callbacksMutex);
        g_import_userdata[key] = udptr_local;
    }

    const char* sig = "v(ii)"; // void(ptr,len)
    M3Result r = m3_LinkRawFunctionEx(module_, ns, name, sig, (M3RawCall)host_trampoline_v_ptr_len, udptr_local.get());
    if (r) {
        std::cerr << "HostBindings: m3_LinkRawFunctionEx(key) failed for '" << ns << "'.'" << name << "' sig='" << sig << "': " << r << std::endl;
        // Fallback: try the previous behavior (pass holder.get()) to detect whether userdata type matters.
        M3Result r2 = m3_LinkRawFunctionEx(module_, ns, name, sig, (M3RawCall)host_trampoline_v_ptr_len, holder.get());
        if (!r2) {
            std::cerr << "HostBindings: m3_LinkRawFunctionEx(holder) succeeded as fallback" << std::endl;
            // Ensure holder carries a consistent userdata representation so the holder can be safely observed
            udptr_local->kind = 2;
            udptr_local->ptr = holder.get();
            holder->timed_out_ptr = WasmRuntime::GetModuleTimedOutPtr(module_);
            holder->udptr = udptr_local;
            // Record the userdata in the global map so it remains valid while linked
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
    std::cerr << "Token::~Token: unregistering host '" << ns_ << "'.'" << name_ << "' sig='" << sig_ << "' module=" << module_ << std::endl;
    {
        std::ostringstream oss; oss << "Token::~Token: entering ns=" << ns_ << " name=" << name_ << " sig=" << sig_ << " module=" << module_;
        LogToFile(oss.str());
    }

    // Conservative strategy:
    // - If this token was created for an unmanaged module (not under WasmRuntime), we avoid calling
    //   into wasm3 here (the module may already be freed) and instead remove the holder from g_callbacks.
    // - If the module is managed by WasmRuntime, we prefer to defer relinking/unregistration until
    //   module unload (WasmModule destructor calls CleanupModuleCallbacks). In that case mark the holder
    //   inactive and leave it in g_callbacks until module unload.

    std::string key = KeyFor(ns_.c_str(), name_.c_str());
    {
        std::lock_guard<std::mutex> lk(g_callbacksMutex);
        auto it = g_callbacks.find(key);
        if (it != g_callbacks.end()) {
            if (it->second) it->second->active = false;
            if (!module_managed_) {
                g_callbacks.erase(it);
                // Remove the associated ImportUserdata to avoid leaking it
                g_import_userdata.erase(key);
                std::ostringstream oss; oss << "Token::~Token: erased holder (unmanaged module) and cleared import userdata";
                LogToFile(oss.str());
            } else {
                std::ostringstream oss; oss << "Token::~Token: module managed; left holder in g_callbacks until module unload";
                LogToFile(oss.str());
            }
        } else {
            std::ostringstream oss; oss << "Token::~Token: no holder found in g_callbacks for key='" << key << "'";
            LogToFile(oss.str());
        }
    }

    // We won't call m3_LinkRawFunction or other wasm3 APIs here because module_ may already be freed
    // (callers that manage module lifetimes must ensure safe ordering). The WasmRuntime-managed path
    // will clean up remaining holders at module unload via CleanupModuleCallbacks.

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

} // namespace Genesis::Engine
#endif // HAVE_WASM3
