#include "pch.h"
#include "teardown/teardown.h"
#include "teardown/lua_helpers.h"
#include "teardown/td_hook_present.h"
#include "teardown/types.h"

#include "offsets_generated.h"
#include "shared/util/util.h"
#include "memory/hooks.h"

using namespace tc;
using namespace teardown;

static td::script_core_t* g_devScript;

// Forward Functions
//------------------------------------------------------------------------
types::game_t* h_teardown_initialize(types::game_t* magicShit, DWORD** a2, int64_t a3);
void h_script_core_registerLuaFunctions(td::script_core_t* scriptCore);
void h_teardown_update(types::game_t* game, int64_t input);
lua_State* h_lua_newstate(lua_Alloc f, void* ud);

void renderDebugMenu();

// Public Functions
//------------------------------------------------------------------------
void teardown::initialize() {
    console::setStatus("Setting up hooks");

    td::renderer::hookPresent(td::renderer::type::opengl);

    mem::hooks::addHook("teardown::ctor", tc::offsets::teardown::initialize, &h_teardown_initialize, &funcs::teardown::initialize);
    mem::hooks::addHook("teardown::update", tc::offsets::teardown::update, &h_teardown_update, &funcs::teardown::update);
    mem::hooks::addHook("luaL_newstate", tc::offsets::lua::lua_newstate, &h_lua_newstate, &funcs::lua::lua_newstate);
    mem::hooks::addHook("script_core::registerLuaFunctions", tc::offsets::script_core::registerLuaFunctions, &h_script_core_registerLuaFunctions, &funcs::script_core::registerLuaFunctions);

    td::renderer::addRenderCallback(&renderDebugMenu);

    // TODO: Figure out how to set a custom tag for the logging function
    funcs::game::log(types::log_level::debug, "Debug");
    funcs::game::log(types::log_level::info, "Info");
    funcs::game::log(types::log_level::warning, "Warning");
    funcs::game::log(types::log_level::error, "Error");
}

// Other Functions
//------------------------------------------------------------------------
void renderDebugMenu() {
    static char luaInput[4096];
    static char errorMessage[1024] = "";

    static bool first = true;

    if (first) {
        if (!teardown::game) {
            console::writeln("fuck!");
            return;
        }

        if (funcs::registry::hasKey(teardown::game->registry, "game.mp.lua_input")) {
            td::td_string str = funcs::registry::getString(teardown::game->registry, td::td_string("game.mp.lua_input"));
            ::memcpy(luaInput, str.c_str(), str.length());
        }

        first = false;
    }

    ImGui::SetNextWindowSize(ImVec2(300, 300), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Teardown Console")) {
        ImGui::InputTextMultiline("Lua Input", luaInput, IM_ARRAYSIZE(luaInput), ImVec2(-1.0f, ImGui::GetTextLineHeight() * 8));

        if (ImGui::Button("Execute Lua")) {
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

            funcs::registry::setString(game->registry, "game.mp.lua_input", luaInput);
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

    lua_helpers::registerLuaFunction(scriptCore, td::td_string("MP_GetVersion"), [](td::script_core_t* scriptCore, lua_State* L) -> int {
        lua_pushstring(L, "not-set");
        return 1;
    });

    funcs::script_core::registerLuaFunctions(scriptCore);
}

teardown::types::game_t* h_teardown_initialize(teardown::types::game_t* game, DWORD** a2, int64_t a3) {
    teardown::game = funcs::teardown::initialize(game, a2, a3);
    console::writeln("Game initialized");

    return teardown::game;
}

void h_teardown_update(types::game_t* game, int64_t input) {
    static types::game_state state = types::game_state::none;
    static bool first = true;

    // Exception thrown at 0x00007FF793BB57ED in teardown.exe.unpacked.exe: 0xC0000005: Access violation reading location 0x00000000000001A8.
    funcs::teardown::update(teardown::game, input);

    if (first) {
        first = false;

        //g_devScript = (td::script_core_t*)malloc(0x21A8);
        g_devScript = (td::script_core_t*)malloc(0x2278);
        funcs::script_core::ctor(g_devScript);

        funcs::script_core::loadScript(g_devScript, "data/tdmp_dev.lua");
        luaL_openlibs(g_devScript->innerCore.state_info->state);
    }

    //types::player_t* player = (types::player_t*)(teardown::game->player);
    //types::scene_t*  scene = (types::scene_t*)(teardown::game->scene);

    //if (state != game->state) {
    //    state = game->state;

    //    td::td_string stateSwitch = "Switched state to: ";
    //    switch (state) {
    //        case types::game_state::splash:       stateSwitch += "splash"; break;
    //        case types::game_state::menu:         stateSwitch += "menu"; break;
    //        case types::game_state::ui:           stateSwitch += "ui"; break;
    //        case types::game_state::loading:      stateSwitch += "loading"; break;
    //        case types::game_state::menu_loading: stateSwitch += "menu_loading"; break;
    //        case types::game_state::play:         stateSwitch += "play"; break;
    //        case types::game_state::edit:         stateSwitch += "edit"; break;
    //        case types::game_state::quit:         stateSwitch += "quit"; break;
    //        default:                              stateSwitch += "none"; break;
    //    }

    //    console::writeln_td(stateSwitch);
    //}

    //if (scene) {
    //    scene->firesystem->fires.reset();
    //    scene->projectiles.reset();
    //}

    //player->health = 1;

    //if (GetAsyncKeyState(VK_F1) & 0x8000) {
    //    //console::writeln("TEST: {}", *((float*)((char*)game->qwordB8 + 0x15c)));

    //    console::writeln("Health: {}\nSpeed: {}", player->health, player->walkingSpeed);
    //    console::writeln("Grabbed body & shape: {} {}",
    //                     (player->grabbedBody != nullptr ? player->grabbedBody->handle : 0),
    //                     (player->grabbedShape != nullptr ? player->grabbedShape->handle : 0)
    //    );

    //    console::writeln("Transform: ({} {} {}) ({} {} {} {})", player->pos.x, player->pos.y, player->pos.z,
    //                     player->rot.x,
    //                     player->rot.y,
    //                     player->rot.z,
    //                     player->rot.w
    //    );

    //    console::writeln("Velocity: {} {} {}", player->velocity.x, player->velocity.y, player->velocity.z);
    //    console::writeln("Grounded: {}", player->isGrounded != 0);
    //}
}