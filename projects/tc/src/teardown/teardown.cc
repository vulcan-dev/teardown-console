#include "pch.h"
#include "console.h"
#include "teardown/containers/td_containers.h"
#include "teardown/td_script_core.h"
#include "teardown/lua_helpers.h"
#include "teardown/td_hook_present.h"
#include "teardown/types.h"
#include "teardown/teardown.h"
#include "memory/memory.h"
#include "shared/util/util.h"
#include "memory/hooks.h"
#include "offsets_generated.h"

using namespace tc;
using namespace teardown;

static td::script_core_t* g_devScript;

namespace {
    std::vector<td::td_string> gInputHistory;
    td::td_string input_history_str, history_indexes_str;
    int gInputHistoryIndex = 0;

    // Do not set td_string here as if it's heap allocated, we won't have the alloc function in time.
    td::td_string input_history_key;// = "savegame.mod.tc.input_history";
    td::td_string history_count_indexes_key;// = "savegame.mod.tc.input_history_sizes";
}

// Forward Functions
//------------------------------------------------------------------------
types::game_t* __fastcall h_teardown_initialize(types::game_t* magicShit, DWORD** a2, int64_t a3);
void h_script_core_registerLuaFunctions(td::script_core_t* scriptCore);
void __fastcall h_teardown_update(types::game_t* game, void* input);
lua_State* h_lua_newstate(lua_Alloc f, void* ud);

// Public Functions
//------------------------------------------------------------------------
void teardown::initialize() {
    console::setStatus("Setting up hooks");

    td::renderer::hookPresent();

    mem::hooks::addHook("teardown::ctor", tc::offsets::teardown::initialize, &h_teardown_initialize, &funcs::teardown::initialize);
    mem::hooks::addHook("teardown::update", tc::offsets::teardown::update, &h_teardown_update, &funcs::teardown::update);
    mem::hooks::addHook("luaL_newstate", tc::offsets::lua::lua_newstate, &h_lua_newstate, &funcs::lua::lua_newstate);
    mem::hooks::addHook("script_core::registerLuaFunctions", tc::offsets::script_core::registerLuaFunctions, &h_script_core_registerLuaFunctions, &funcs::script_core::registerLuaFunctions);

    // TODO: Figure out how to set a custom tag for the logging function
    funcs::game::log(types::log_level::debug, "Debug");
    funcs::game::log(types::log_level::info, "Info");
    funcs::game::log(types::log_level::warning, "Warning");
    funcs::game::log(types::log_level::error, "Error");
}

// Other Functions
//------------------------------------------------------------------------
void td::renderer::onRender() {
    static char luaInput[4096];
    static char errorMessage[1024] = "";

    static bool first = true;

    ImGui::SetNextWindowSize(ImVec2(300, 300), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Teardown Console")) {
        if (g_devScript) {
            ImGui::InputTextMultiline("Lua Input", luaInput, IM_ARRAYSIZE(luaInput), ImVec2(-1.0f, ImGui::GetTextLineHeight() * 8));

            if (ImGui::Button("Execute Lua")) {
                int luaInputLen = strlen(luaInput);
                int totalSize = luaInputLen + 1; // +1 for the comma

                // Add to history
                if (funcs::registry::hasKey((uint8_t**)game->registry, &input_history_key)) {
                    funcs::registry::getString(game->registry, &input_history_str, &input_history_key);
                }

                if (funcs::registry::hasKey((uint8_t**)game->registry, &history_count_indexes_key)) {
                    funcs::registry::getString(game->registry, &history_indexes_str, &history_count_indexes_key);
                    std::string indexes = std::string(history_indexes_str.c_str(), history_indexes_str.length());
                    int colonPos = indexes.find(":");
                    if (colonPos != std::string::npos) {
                        totalSize += std::stoi(indexes.substr(0, colonPos));
                        td::td_string without_size_indexes_str = indexes.substr(colonPos + 1).c_str();
                        history_indexes_str = td::td_string::fromInt(totalSize);
                        history_indexes_str += ":";
                        history_indexes_str += without_size_indexes_str;
                        history_indexes_str += ",";
                    } else {
                        // Bad, clear all history.
                    }
                } else {
                    history_indexes_str = td::td_string::fromInt(totalSize);
                    history_indexes_str += ":";
                }

                history_indexes_str += td::td_string::fromInt(luaInputLen);
                funcs::registry::setString(game->registry, &history_count_indexes_key, &history_indexes_str);

                input_history_str += luaInput;
                funcs::registry::setString(game->registry, &input_history_key, &input_history_str);

                int error = luaL_loadstring(g_devScript->innerCore.state_info->state, luaInput);
                if (error) {
                    snprintf(errorMessage, sizeof(errorMessage), "Error loading Lua: %s", lua_tostring(g_devScript->innerCore.state_info->state, -1));
                    lua_pop(g_devScript->innerCore.state_info->state, 1);
                } else {
                    error = lua_pcall(g_devScript->innerCore.state_info->state, 0, LUA_MULTRET, 0);
                    if (error) {
                        snprintf(errorMessage, sizeof(errorMessage), "Error executing Lua: %s", lua_tostring(g_devScript->innerCore.state_info->state, -1));
                        lua_pop(g_devScript->innerCore.state_info->state, 1);
                    } else {
                        snprintf(errorMessage, sizeof(errorMessage), "Lua code executed successfully.");
                    }
                }
            }

            ImGui::Separator();

            ImGui::Text("History: %s", input_history_str.c_str());
            ImGui::Text("Indexes: %s", history_indexes_str.c_str());
        } else {
            ImGui::TextWrapped("Please make sure you have `data/tc.lua` in your game directory.");
        }

        ImGui::TextWrapped("%s", errorMessage);
        ImGui::End();
    }
}

// Hooked Functions
//------------------------------------------------------------------------
lua_State* h_lua_newstate(lua_Alloc f, void* ud) {
    lua_State* L = funcs::lua::lua_newstate(f, ud);
    //console::writeln("Lua State: 0x{:X}", (uintptr_t)L);

    return L;
}

void h_script_core_registerLuaFunctions(td::script_core_t* scriptCore) {
    console::writeln("Registering functions for {}", scriptCore->path.c_str());
    lua_State* L = scriptCore->innerCore.state_info->state;

#if TC_DEBUG
    lua_pushboolean(L, true);
#else
    lua_pushboolean(L, false);
#endif
    lua_setglobal(L, "TC_DEBUG");

    lua_helpers::registerLuaFunction(scriptCore, td::td_string("TC_Test"), [](td::script_core_t* scriptCore, lua_State* L) -> int {
        td::td_string str;
        td::td_string name = "savegame.mod.tc.input_history";
        funcs::registry::getString(teardown::game->registry, &str, &name);
        lua_pushstring(L, str.c_str());
        return 1;
    });

    funcs::script_core::registerLuaFunctions(scriptCore);
}

teardown::types::game_t* h_teardown_initialize(teardown::types::game_t* game, DWORD** a2, int64_t a3) {
    teardown::game = funcs::teardown::initialize(game, a2, a3);
    console::writeln("Game: 0x{:X}", (uintptr_t)teardown::game);

    // We cannot use td_string before
    input_history_key = "savegame.mod.tc.input_history";
    history_count_indexes_key = "savegame.mod.tc.input_history_sizes";

    { // Test
        //void* mem = funcs::mem::alloc(0x128);
        //console::writeln("Allocated: 0x{:X}", (uintptr_t)mem);
        //delete mem; // not good
    }

    // TODO: Improve how strings are handled.

    { // Setup our script
        if (!std::filesystem::exists("data/tc.lua")) {
            console::writeln("Failed to find data/tc.lua");
            return teardown::game;
        }

        g_devScript = (td::script_core_t*)malloc(0x2278);
        funcs::script_core::ctor(g_devScript);

        funcs::script_core::loadScript(g_devScript, "data/tc.lua");
        luaL_openlibs(g_devScript->innerCore.state_info->state);

        { // Get input history
            td::td_string history_str;
            td::td_string sizes_str;

            if (funcs::registry::hasKey((uint8_t**)teardown::game->registry, &history_count_indexes_key)) {
                funcs::registry::getString(teardown::game->registry, &sizes_str, &history_count_indexes_key);
                history_indexes_str = sizes_str;

                if (funcs::registry::hasKey((uint8_t**)teardown::game->registry, &input_history_key)) {
                    funcs::registry::getString(teardown::game->registry, &history_str, &input_history_key);
                    input_history_str = history_str;

                    int startIndex = 0;
                    int allocSize = 0;
                    int currentPosition = 0;

                    std::string sizes_stdstr = std::string(sizes_str.c_str(), sizes_str.length());
                    std::string history_stdstr = std::string(history_str.c_str(), history_str.length());
                    for (size_t i = 0; i < sizes_str.length(); i++) {
                        if (allocSize == 0 && sizes_str[i] == ':') {
                            allocSize = std::stoi(sizes_stdstr.substr(startIndex, i - startIndex).c_str());
                            gInputHistory.reserve(allocSize);
                            startIndex = i + 1;
                        } else if (allocSize > 0 && (sizes_str[i] == ',' || i == sizes_str.length() - 1)) {
                            int endPos = i;
                            if (i == sizes_str.length() - 1) {
                                endPos = i + 1;
                            }

                            int size = std::stoi(sizes_stdstr.substr(startIndex, endPos - startIndex).c_str());

                            if (size > 0 && currentPosition + size <= history_str.length()) {
                                gInputHistory.push_back(history_stdstr.substr(currentPosition, size).c_str()); // This string conversion is beautiful, but right now I don't care
                                currentPosition += size;
                            }

                            startIndex = i + 1;
                        }
                    }
                }
            }
        }

    }

    return teardown::game;
}

void h_teardown_update(types::game_t* game, void* input) {
    funcs::teardown::update(teardown::game, input);

    static types::game_state state = types::game_state::none;
    static bool first = true;
}