#pragma once

#include "common.hpp"
#include "tk_loot.hpp"
#include <map>
#include <string>
#include <unordered_map>

namespace tk
{
    struct LootEntry
    {
        Vector3 pos; // unused for inventory loot
        std::string id;
        std::string name;
        int value;
        int value_per_slot;
        bool unknown = false;
        bool highlighted = false;
        bool overriden = false;
        LootItem::Rarity rarity;
        std::string bundle_path;
    };

    struct Observer
    {
        int pid;
        uint8_t cid;

        enum ObserverType
        {
            Self,
            Player,
            Scav
        } type;

        std::string id;
        std::string group_id;
        std::string name;

        Vector3 last_pos;
        Vector3 pos;
        Vector3 rot;

        int level = 0;
        bool is_dead = false;
        bool is_npc = false;
        bool is_unspawned = false;

        // TODO: Loot refactor
        // This should be a vector of std::strings (object IDs) corresponding with loot entries in the loot table.
        std::vector<LootEntry> inventory;
    };

    // TODO: Loot refactor
    // This should not exist. Loot should already be in the loot list. Just move from obs to world.
    struct TemporaryLoot
    {
        int id; // hash of either template ID or object ID, C# GetHashCode()
        Vector3 pos;
    };

    class Map
    {
    public:
        Map(Vector3 min, Vector3 max);

        Vector3 bounds_min() const { return m_min; }
        Vector3 bounds_max() const { return m_max; }

        void create_observer(int cid, Observer&& obs);
        void destroy_observer(int cid);
        Observer* get_observer(int cid);
        std::vector<Observer*> get_observers();
        Observer* get_player();

        void add_loot_item(LootEntry&& entry);
        std::vector<LootEntry*> get_loot();
        void destroy_loot_item_by_id(std::string& id);
        LootEntry* get_loot_by_id(std::string& id);

        void add_static_corpse(Vector3 pos);
        std::vector<Vector3*> get_static_corpses();

        TemporaryLoot* get_or_create_temporary_loot(int id);
        std::vector<TemporaryLoot*> get_temporary_loots();

    private:

        Vector3 m_min;
        Vector3 m_max;
        std::unordered_map<uint8_t, Observer> m_observers;
        std::unordered_map<int, TemporaryLoot> m_temporary_loot;
        std::unordered_map<std::string, LootEntry> m_loot;
        std::vector<Vector3> m_static_corpses;

        // TODO: Loot refactor
        // This should be a vector of std::strings (object IDs) corresponding with loot entries in the loot table.
        // std::vector<std::string> m_world_loot;
    };
}
