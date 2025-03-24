#ifndef TC_GENERATED_OFFSETS_PREREQUISITE_H
#define TC_GENERATED_OFFSETS_PREREQUISITE_H

#include "pch.h"
#include "console.h"
#include "memory/memory.h"
#include "teardown/types.h"

#define SET_AND_CHECK_OFFSET(addr, offName) \
    do { \
        addr = tc::mem::baseAddress + addr; \
        console::writeln("  {} = 0x{:X}", offName, addr); \
        console::setStatus("Updating offset {}", offName); \
        if (!tc::mem::isAddressExecutable(addr)) { \
            failedOffsets.push_back(offName); \
        } \
    } while (0)

#define ASSIGN_FUNCTION(ns, func) \
    funcs::##ns::##func = (funcs::types::##ns::t##func)(tc::offsets::##ns::##func); \
    console::writeln("Assigning function {}::{} to {:x}", #ns, #func, tc::offsets::##ns::##func); \
    console::setStatus("Assigning function {}::{} to {:x}", #ns, #func, tc::offsets::##ns::##func);

#define DECLARE_GAME_LUA_FUNCTION(name, offset) \
    inline tc::mem::game_function name##(#name, offset)

#define UPDATE_GAME_LUA_FUNCTION(name) \
    lua_funcs::name##.address += tc::mem::baseAddress; \
    console::writeln("Updating {}: {:#x}", #name, lua_funcs::name##.address);


#endif // TC_GENERATED_OFFSETS_PREREQUISITE_H