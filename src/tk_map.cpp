#include "tk_map.hpp"
#include "tk_net.hpp"

namespace tk
{
    Map::Map(Vector3 min, Vector3 max)
        : m_min(min), m_max(max)
    {
    }

    void Map::create_observer(int cid, Observer&& obs)
    {
        std::lock_guard<std::mutex> lock(m_lock);
        m_observers[cid] = std::move(obs);
    }

    void Map::destroy_observer(int cid)
    {
        std::lock_guard<std::mutex> lock(m_lock);

        m_observers.erase(cid);
        m_observers.erase(cid-1);
    }

    Observer* Map::get_observer_manual_lock(int cid)
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

    std::vector<Observer*> Map::get_observers_manual_lock()
    {
        std::vector<Observer*> ret;
        for (auto& [cid, obs] : m_observers)
        {
            ret.emplace_back(&obs);
        }
        return ret;
    }

    Observer* Map::get_player_manual_lock()
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
        std::lock_guard<std::mutex> lock(m_lock);
        m_loot.emplace_back(std::move(entry));
    }

    std::vector<LootEntry*> Map::get_loot_manual_lock()
    {
        std::vector<LootEntry*> ret;
        for (auto& entry : m_loot)
        {
            ret.emplace_back(&entry);
        }
        return ret;
    }

    void Map::add_static_corpse(Vector3 pos)
    {
        std::lock_guard<std::mutex> lock(m_lock);
        m_static_corpses.emplace_back(std::move(pos));
    }

    std::vector<Vector3*> Map::get_static_corpses_manual_lock()
    {
        std::vector<Vector3*> ret;
        for (auto& entry : m_static_corpses)
        {
            ret.emplace_back(&entry);
        }
        return ret;
    }

    TemporaryLoot* Map::get_or_create_temporary_loot_manual_lock(int id)
    {
        auto entry = m_temporary_loot.find(id);
        if (entry == std::end(m_temporary_loot))
        {
            m_temporary_loot[id] = { id, {0.0f, 0.0f, 0.0f } };
        }
        return &m_temporary_loot[id];
    }

    std::vector<TemporaryLoot*> Map::get_temporary_loots_manual_lock()
    {
        std::vector<TemporaryLoot*> ret;
        for (auto& [id, loot] : m_temporary_loot)
        {
            ret.push_back(&loot);
        }
        return ret;
    }

    void Map::lock()
    {
        m_lock.lock();
    }

    void Map::unlock()
    {
        m_lock.unlock();
    }
}
