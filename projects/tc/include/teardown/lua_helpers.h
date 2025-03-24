#ifndef TC_LUA_HELPERS_H
#define TC_LUA_HELPERS_H

#include "pch.h"
#include "teardown/types.h"

namespace tc::lua_helpers {

    void registerLuaFunction(td::script_core_t* scriptCore, td::td_string name, void* func);

}

#endif // TC_LUA_HELPERS_H