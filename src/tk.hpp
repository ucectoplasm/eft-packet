#pragma once

#include "common.hpp"
#include "tk_loot.hpp"
#include "tk_map.hpp"

#include <memory>
#include <string>

namespace tk
{
    struct GlobalState
    {
        std::string server_ip;
        std::unique_ptr<Map> map;
    };

    extern std::unique_ptr<LootDatabase> g_loot_db;
    extern std::unique_ptr<GlobalState> g_state;
}

#define TK_ASSERT(x) do { if (!(x)) __debugbreak(); } while (0)
