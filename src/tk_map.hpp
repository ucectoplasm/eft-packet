#pragma once

#include "common.hpp"
#include "tk_loot.hpp"
#include <string>
#include <unordered_map>
#include <mutex>

namespace tk
{
    struct LootEntry
    {
        Vector3 pos;
        std::string name;
        int value;
        bool container = false;
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

        std::vector<LootEntry> inventory;
    };

    struct TemporaryLoot
    {
        int id; // Hash of the loot template ID. Could label dropped loot if we implement it.
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
        Observer* get_observer_manual_lock(int cid);
        std::vector<Observer*> get_observers_manual_lock();
        Observer* get_player_manual_lock();

        void add_loot_item(LootEntry&& entry);
        std::vector<LootEntry*> get_loot_manual_lock();

        void add_static_corpse(Vector3 pos);
        std::vector<Vector3*> get_static_corpses_manual_lock();

        TemporaryLoot* get_or_create_temporary_loot_manual_lock(int id);
        std::vector<TemporaryLoot*> get_temporary_loots_manual_lock();

        void lock();
        void unlock();

    private:

        Vector3 m_min;
        Vector3 m_max;
        std::unordered_map<uint8_t, Observer> m_observers;
        std::unordered_map<int, TemporaryLoot> m_temporary_loot;
        std::vector<LootEntry> m_loot;
        std::vector<Vector3> m_static_corpses;
        std::mutex m_lock;
    };
}
