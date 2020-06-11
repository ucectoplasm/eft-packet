#pragma once

#include "common.hpp"
#include "tk_loot.hpp"
#include <map>
#include <string>
#include <unordered_map>

namespace tk
{
    static constexpr int LootInstanceOwnerInvalid = -2;
    static constexpr int LootInstanceOwnerWorld = -1;

    struct LootInstance
    {
        std::string id;
        std::string parent_id;
        int csharp_hash;
        int owner;
        LootTemplate* template_;
        Vector3 pos; // unused for inventory loot
        int stack_count;
        int value; // template_->value * stack_count
        int value_per_slot;
        bool highlighted = false;
        bool inaccessible = false; // is a secure container or scabbard
    };

    int get_loot_instance_owner(LootInstance* loot);
    bool is_loot_instance_inaccessible(LootInstance* loot);

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

        // all loot instances
        LootInstance* add_loot_instance(LootInstance&& instance);
        void destroy_loot_instance_by_id(const std::string& id);
        LootInstance* get_loot_instance_by_id(const std::string& id);
        std::vector<LootInstance*> get_loot();

        void add_static_corpse(Vector3 pos);
        std::vector<Vector3*> get_static_corpses();

    private:

        Vector3 m_min;
        Vector3 m_max;
        std::unordered_map<uint8_t, Observer> m_observers;
        std::unordered_map<std::string, LootInstance> m_loot;
        std::vector<Vector3> m_static_corpses;
    };
}
