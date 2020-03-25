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
        std::unique_ptr<LootDatabase> loot_db;
    };

    extern std::unique_ptr<GlobalState> g_state;
}
