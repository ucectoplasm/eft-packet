#include "tk.hpp"
#include "tk_map.hpp"
#include "tk_net.hpp"

namespace tk
{
    int get_loot_instance_owner(LootInstance* loot)
    {
        TK_ASSERT(loot);
        while (!loot->parent_id.empty())
        {
            loot = tk::g_state->map->get_loot_instance_by_id(loot->parent_id);
            if (!loot) return LootInstanceOwnerInvalid;
        }

        TK_ASSERT(loot->owner != LootInstanceOwnerInvalid);
        return loot->owner;
    }

    bool is_loot_instance_inaccessible(LootInstance* loot)
    {
        TK_ASSERT(loot);
        while (!loot->parent_id.empty())
        {
            if (loot->inaccessible) return true;
            loot = tk::g_state->map->get_loot_instance_by_id(loot->parent_id);
            if (!loot) return true;
        }

        return loot->inaccessible;
    }

    Map::Map(Vector3 min, Vector3 max)
        : m_min(min), m_max(max)
    { }

    void Map::create_observer(int cid, Observer&& obs)
    {
        m_observers[cid] = std::move(obs);
    }

    void Map::destroy_observer(int cid)
    {
        m_observers.erase(cid);
        m_observers.erase(cid-1);
    }

    Observer* Map::get_observer(int cid)
    {
        auto entry = m_observers.find(cid);

        if (entry == std::end(m_observers))
        {
            entry = m_observers.find(cid - 1);
            if (entry == std::end(m_observers))
            {
                return nullptr;
            }
        }

        return &entry->second;
    }

    std::vector<Observer*> Map::get_observers()
    {
        std::vector<Observer*> ret;
        for (auto& [cid, obs] : m_observers)
        {
            ret.emplace_back(&obs);
        }
        return ret;
    }

    Observer* Map::get_player()
    {
        for (auto& [cid, obs] : m_observers)
        {
            if (obs.type == Observer::Self)
            {
                return &obs;
            }
        }

        return nullptr;
    }

    LootInstance* Map::add_loot_instance(LootInstance&& entry)
    {
        auto iter = m_loot.insert(std::make_pair(entry.id, std::move(entry)));
        return &iter.first->second;
    }

    void Map::destroy_loot_instance_by_id(const std::string& id)
    {
        m_loot.erase(id);
    }

    LootInstance* Map::get_loot_instance_by_id(const std::string& id)
    {
        auto iter = m_loot.find(id);
        return iter == std::end(m_loot) ? nullptr : &iter->second;
    }

    std::vector<LootInstance*> Map::get_loot()
    {
        std::vector<LootInstance*> ret;
        for (auto& [id, loot] : m_loot)
        {
            ret.emplace_back(&loot);
        }
        return ret;
    }

    void Map::add_static_corpse(Vector3 pos)
    {
        m_static_corpses.emplace_back(std::move(pos));
    }

    std::vector<Vector3*> Map::get_static_corpses()
    {
        std::vector<Vector3*> ret;
        for (auto& entry : m_static_corpses)
        {
            ret.emplace_back(&entry);
        }
        return ret;
    }
}
