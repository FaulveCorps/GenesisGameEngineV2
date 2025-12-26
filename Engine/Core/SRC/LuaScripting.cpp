#include "engine/IScripting.h"
#include "engine/SubsystemRegistry.h"
#include <iostream>

#ifndef HAVE_LUA
// No-op stub when Lua isn't available
namespace Genesis::Engine { void RegisterLuaScriptingFactory() {} }
#else
#include <lua.hpp>

namespace Genesis::Engine {

class LuaScripting : public IScripting {
public:
    LuaScripting() : L(nullptr) {}
    bool Init() override {
        L = luaL_newstate();
        if (!L) return false;
        luaL_openlibs(L);
        std::cout << "LuaScripting: Init\n";
        return true;
    }
    void Update(double /*dt*/) override { /* no-op for now */ }
    void Shutdown() override {
        if (L) {
            lua_close(L);
            L = nullptr;
        }
        std::cout << "LuaScripting: Shutdown\n";
    }
    std::string Name() const override { return "lua"; }

    bool ExecuteString(const std::string& code) override {
        if (!L) return false;
        if (luaL_loadstring(L, code.c_str()) || lua_pcall(L, 0, LUA_MULTRET, 0)) {
            const char* err = lua_tostring(L, -1);
            std::cerr << "LuaScripting: ExecuteString error: " << (err ? err : "unknown") << std::endl;
            lua_pop(L, 1);
            return false;
        }
        return true;
    }

    bool ExecuteFile(const std::string& path) override {
        if (!L) return false;
        if (luaL_loadfile(L, path.c_str()) || lua_pcall(L, 0, LUA_MULTRET, 0)) {
            const char* err = lua_tostring(L, -1);
            std::cerr << "LuaScripting: ExecuteFile error: " << (err ? err : "unknown") << std::endl;
            lua_pop(L, 1);
            return false;
        }
        return true;
    }

private:
    lua_State* L;
};

static bool register_lua_scripting = []() {
    SubsystemRegistry::Instance().RegisterFactory("Scripting", "lua", []() {
        return std::make_unique<LuaScripting>();
    });
    return true;
}();

void RegisterLuaScriptingFactory() {
    SubsystemRegistry::Instance().RegisterFactory("Scripting", "lua", []() {
        return std::make_unique<LuaScripting>();
    });
}

} // namespace Genesis::Engine
#endif
