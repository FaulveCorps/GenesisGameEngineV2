#include "engine/IScripting.h"
#include "engine/SubsystemRegistry.h"
#include "engine/Engine.h"
#include "engine/IPhysics.h"
#include <iostream>

#ifndef HAVE_LUA
// No-op stub when Lua isn't available
namespace Genesis::Engine { void RegisterLuaScriptingFactory() {} }
#else
#include <lua.hpp>

namespace Genesis::Engine {

class LuaScripting : public IScripting {
public:
    LuaScripting() : L(nullptr), m_contactBeginRef(LUA_REFNIL), m_contactEndRef(LUA_REFNIL) {}
    bool Init() override {
        L = luaL_newstate();
        if (!L) return false;
        luaL_openlibs(L);

        // Expose a small 'engine' table with helpers
        lua_newtable(L); // engine table
        lua_pushlightuserdata(L, this);
        lua_pushcclosure(L, &Lua_CreatePhysicsBackend, 1);
        lua_setfield(L, -2, "create_physics_backend");
        lua_pushlightuserdata(L, this);
        lua_pushcclosure(L, &Lua_CreateBody, 1);
        lua_setfield(L, -2, "create_body");
        lua_pushlightuserdata(L, this);
        lua_pushcclosure(L, &Lua_DestroyBody, 1);
        lua_setfield(L, -2, "destroy_body");
        lua_pushlightuserdata(L, this);
        lua_pushcclosure(L, &Lua_CreateDistanceJoint, 1);
        lua_setfield(L, -2, "create_distance_joint");
        lua_pushlightuserdata(L, this);
        lua_pushcclosure(L, &Lua_ApplyImpulse, 1);
        lua_setfield(L, -2, "apply_impulse");
        lua_pushlightuserdata(L, this);
        lua_pushcclosure(L, &Lua_RegisterContactBegin, 1);
        lua_setfield(L, -2, "register_contact_begin");
        lua_pushlightuserdata(L, this);
        lua_pushcclosure(L, &Lua_RegisterContactEnd, 1);
        lua_setfield(L, -2, "register_contact_end");
        lua_setglobal(L, "engine");

        std::cout << "LuaScripting: Init\n";
        return true;
    }
    void Update(double /*dt*/) override { /* no-op for now */ }
    void Shutdown() override {
        if (L) {
            // release any refs
            if (m_contactBeginRef != LUA_REFNIL) luaL_unref(L, LUA_REGISTRYINDEX, m_contactBeginRef);
            if (m_contactEndRef != LUA_REFNIL) luaL_unref(L, LUA_REGISTRYINDEX, m_contactEndRef);
            lua_close(L);
            L = nullptr;
            m_contactBeginRef = LUA_REFNIL;
            m_contactEndRef = LUA_REFNIL;
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
    int m_contactBeginRef;
    int m_contactEndRef;

    static int Lua_CreatePhysicsBackend(lua_State* L) {
        LuaScripting* self = reinterpret_cast<LuaScripting*>(lua_touserdata(L, lua_upvalueindex(1)));
        if (!self) return luaL_error(L, "internal error");
        const char* name = luaL_checkstring(L, 1);
        bool ok = Genesis::Engine::CreatePhysicsSubsystem(name);
        lua_pushboolean(L, ok);
        return 1;
    }

    static int Lua_CreateBody(lua_State* L) {
        LuaScripting* self = reinterpret_cast<LuaScripting*>(lua_touserdata(L, lua_upvalueindex(1)));
        if (!self) return luaL_error(L, "internal error");
        double mass = luaL_checknumber(L, 1);
        double x = luaL_checknumber(L, 2);
        double y = luaL_checknumber(L, 3);
        double sx = luaL_checknumber(L, 4);
        double sy = luaL_checknumber(L, 5);
        auto ph = Genesis::Engine::GetPhysicsSubsystem();
        if (!ph) { lua_pushnil(L); return 1; }
        auto h = ph->CreateBoxRigidBody(static_cast<float>(mass), static_cast<float>(x), static_cast<float>(y), 0.0f, static_cast<float>(sx), static_cast<float>(sy), 0.0f);
        if (h == 0) lua_pushnil(L); else lua_pushinteger(L, static_cast<lua_Integer>(h));
        return 1;
    }

    static int Lua_DestroyBody(lua_State* L) {
        LuaScripting* self = reinterpret_cast<LuaScripting*>(lua_touserdata(L, lua_upvalueindex(1)));
        if (!self) return luaL_error(L, "internal error");
        lua_Integer h = luaL_checkinteger(L, 1);
        auto ph = Genesis::Engine::GetPhysicsSubsystem();
        if (!ph) { lua_pushboolean(L, 0); return 1; }
        ph->DestroyRigidBody(static_cast<IPhysics::BodyHandle>(h));
        lua_pushboolean(L, 1);
        return 1;
    }

    static int Lua_CreateDistanceJoint(lua_State* L) {
        LuaScripting* self = reinterpret_cast<LuaScripting*>(lua_touserdata(L, lua_upvalueindex(1)));
        if (!self) return luaL_error(L, "internal error");
        lua_Integer a = luaL_checkinteger(L, 1);
        lua_Integer b = luaL_checkinteger(L, 2);
        double ax = luaL_checknumber(L, 3);
        double ay = luaL_checknumber(L, 4);
        double bx = luaL_checknumber(L, 5);
        double by = luaL_checknumber(L, 6);
        auto ph = Genesis::Engine::GetPhysicsSubsystem();
        if (!ph) { lua_pushnil(L); return 1; }
        auto j = ph->CreateDistanceJoint(static_cast<IPhysics::BodyHandle>(a), static_cast<IPhysics::BodyHandle>(b), static_cast<float>(ax), static_cast<float>(ay), static_cast<float>(bx), static_cast<float>(by));
        if (j == 0) lua_pushnil(L); else lua_pushinteger(L, static_cast<lua_Integer>(j));
        return 1;
    }

    static int Lua_ApplyImpulse(lua_State* L) {
        LuaScripting* self = reinterpret_cast<LuaScripting*>(lua_touserdata(L, lua_upvalueindex(1)));
        if (!self) return luaL_error(L, "internal error");
        lua_Integer h = luaL_checkinteger(L, 1);
        double ix = luaL_checknumber(L, 2);
        double iy = luaL_checknumber(L, 3);
        auto ph = Genesis::Engine::GetPhysicsSubsystem();
        if (!ph) { lua_pushboolean(L, 0); return 1; }
        ph->ApplyCentralImpulse(static_cast<IPhysics::BodyHandle>(h), static_cast<float>(ix), static_cast<float>(iy), 0.0f);
        lua_pushboolean(L, 1);
        return 1;
    }

    static int Lua_RegisterContactBegin(lua_State* L) {
        // upvalue 1 = LuaScripting* instance
        LuaScripting* self = reinterpret_cast<LuaScripting*>(lua_touserdata(L, lua_upvalueindex(1)));
        if (!self) return luaL_error(L, "internal error");

        // clear existing
        if (self->m_contactBeginRef != LUA_REFNIL) luaL_unref(self->L, LUA_REGISTRYINDEX, self->m_contactBeginRef);
        self->m_contactBeginRef = LUA_REFNIL;

        if (lua_isfunction(L, 1)) {
            lua_pushvalue(L, 1);
            self->m_contactBeginRef = luaL_ref(L, LUA_REGISTRYINDEX);
        }

        // Update physics subsystem callbacks
        auto ph = Genesis::Engine::GetPhysicsSubsystem();
        IPhysics::ContactCallback onBegin = [self](IPhysics::BodyHandle a, IPhysics::BodyHandle b){
            if (self->m_contactBeginRef == LUA_REFNIL) return;
            lua_rawgeti(self->L, LUA_REGISTRYINDEX, self->m_contactBeginRef);
            lua_pushinteger(self->L, static_cast<lua_Integer>(a));
            lua_pushinteger(self->L, static_cast<lua_Integer>(b));
            if (lua_pcall(self->L, 2, 0, 0) != LUA_OK) {
                const char* err = lua_tostring(self->L, -1);
                std::cerr << "Lua contact begin callback error: " << (err ? err : "unknown") << std::endl;
                lua_pop(self->L, 1);
            }
        };
        IPhysics::ContactCallback onEnd = [self](IPhysics::BodyHandle a, IPhysics::BodyHandle b){
            if (self->m_contactEndRef == LUA_REFNIL) return;
            lua_rawgeti(self->L, LUA_REGISTRYINDEX, self->m_contactEndRef);
            lua_pushinteger(self->L, static_cast<lua_Integer>(a));
            lua_pushinteger(self->L, static_cast<lua_Integer>(b));
            if (lua_pcall(self->L, 2, 0, 0) != LUA_OK) {
                const char* err = lua_tostring(self->L, -1);
                std::cerr << "Lua contact end callback error: " << (err ? err : "unknown") << std::endl;
                lua_pop(self->L, 1);
            }
        };
        if (ph) ph->SetContactCallbacks(onBegin, onEnd);
        return 0;
    }

    static int Lua_RegisterContactEnd(lua_State* L) {
        LuaScripting* self = reinterpret_cast<LuaScripting*>(lua_touserdata(L, lua_upvalueindex(1)));
        if (!self) return luaL_error(L, "internal error");

        if (self->m_contactEndRef != LUA_REFNIL) luaL_unref(self->L, LUA_REGISTRYINDEX, self->m_contactEndRef);
        self->m_contactEndRef = LUA_REFNIL;

        if (lua_isfunction(L, 1)) {
            lua_pushvalue(L, 1);
            self->m_contactEndRef = luaL_ref(L, LUA_REGISTRYINDEX);
        }

        // Update physics subsystem callbacks by reusing current begin ref
        auto ph = Genesis::Engine::GetPhysicsSubsystem();
        IPhysics::ContactCallback onBegin = [self](IPhysics::BodyHandle a, IPhysics::BodyHandle b){
            if (self->m_contactBeginRef == LUA_REFNIL) return;
            lua_rawgeti(self->L, LUA_REGISTRYINDEX, self->m_contactBeginRef);
            lua_pushinteger(self->L, static_cast<lua_Integer>(a));
            lua_pushinteger(self->L, static_cast<lua_Integer>(b));
            if (lua_pcall(self->L, 2, 0, 0) != LUA_OK) {
                const char* err = lua_tostring(self->L, -1);
                std::cerr << "Lua contact begin callback error: " << (err ? err : "unknown") << std::endl;
                lua_pop(self->L, 1);
            }
        };
        IPhysics::ContactCallback onEnd = [self](IPhysics::BodyHandle a, IPhysics::BodyHandle b){
            if (self->m_contactEndRef == LUA_REFNIL) return;
            lua_rawgeti(self->L, LUA_REGISTRYINDEX, self->m_contactEndRef);
            lua_pushinteger(self->L, static_cast<lua_Integer>(a));
            lua_pushinteger(self->L, static_cast<lua_Integer>(b));
            if (lua_pcall(self->L, 2, 0, 0) != LUA_OK) {
                const char* err = lua_tostring(self->L, -1);
                std::cerr << "Lua contact end callback error: " << (err ? err : "unknown") << std::endl;
                lua_pop(self->L, 1);
            }
        };
        if (ph) ph->SetContactCallbacks(onBegin, onEnd);
        return 0;
    }
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
