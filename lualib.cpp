#define LUA_LIB

#include "lua.h"

#include "lauxlib.h"
#include "lualib.h"

static const luaL_Reg lualibs[] = {{"", luaopen_base},
                                   {LUA_LOADLIBNAME, luaopen_package},
                                   {LUA_TABLIBNAME, luaopen_table},
                                   {LUA_IOLIBNAME, luaopen_io},
                                   {LUA_OSLIBNAME, luaopen_os},
                                   {LUA_STRLIBNAME, luaopen_string},
                                   {LUA_MATHLIBNAME, luaopen_math},
                                   {LUA_DBLIBNAME, luaopen_debug},
                                   {nullptr, nullptr}};

LUALIB_API void luaL_openlibs(lua_State *L) {
    const luaL_Reg *lib = lualibs;
    for (; lib->func; lib++) {
        lua_pushcfunction(L, lib->func);
        lua_pushstring(L, lib->name);
        lua_call(L, 1, 0);
    }
}
