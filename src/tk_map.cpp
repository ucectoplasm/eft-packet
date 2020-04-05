#include "tk_map.hpp"
#include "tk_net.hpp"

namespace tk
{
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

    void Map::add_loot_item(LootEntry&& entry)
    {
        m_loot[entry.id] = std::move(entry);
    }

    std::vector<LootEntry*> Map::get_loot()
    {
        std::vector<LootEntry*> ret;
        for (auto& [label, entry] : m_loot)
        {
            ret.emplace_back(&entry);
        }
        return ret;
    }

    void Map::destroy_loot_item_by_id(std::string& id)
    {
        m_loot.erase(id);
    }

    LootEntry* Map::get_loot_by_id(std::string& id)
    {
        auto iter = m_loot.find(id);
        return iter == std::end(m_loot) ? nullptr : &iter->second;
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

    TemporaryLoot* Map::get_or_create_temporary_loot(int id)
    {
        auto entry = m_temporary_loot.find(id);
        if (entry == std::end(m_temporary_loot))
        {
            m_temporary_loot[id] = { id, {0.0f, 0.0f, 0.0f } };
        }
        return &m_temporary_loot[id];
    }

    std::vector<TemporaryLoot*> Map::get_temporary_loots()
    {
        std::vector<TemporaryLoot*> ret;
        for (auto& [id, loot] : m_temporary_loot)
        {
            ret.push_back(&loot);
        }
        return ret;
    }
}
