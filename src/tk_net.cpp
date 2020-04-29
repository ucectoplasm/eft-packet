#include "tk_net.hpp"

#include "tk.hpp"
#include "tk_loot.hpp"
#include "tk_map.hpp"
#include "json11.hpp"

#include <unordered_map>

namespace tk
{
    void process_server_init(ByteStream* stream);
    void process_world_spawn(ByteStream* stream);
    void process_world_unspawn(ByteStream* stream);
    void process_subworld_spawn(ByteStream* stream);
    void process_subworld_unspawn(ByteStream* stream);
    void process_player_spawn(ByteStream* stream);
    void process_player_unspawn(ByteStream* stream);
    void process_observer_spawn(ByteStream* stream);
    void process_observer_unspawn(ByteStream* stream);
    void process_game_update(ByteStream* stream, int channel);
    void process_game_update_outbound(ByteStream* stream, int channel);

    void process_packet(ByteStream* stream, uint8_t channel, bool outbound)
    {
        uint16_t head = 0;

        while (head < stream->len())
        {
            uint16_t len = stream->ReadUInt16();
            PacketCode packet = (PacketCode)stream->ReadInt16();

            if (outbound)
            {
                // Only care about 170 for outbound local player updates.
                if (packet == PacketCode::GameUpdate)
                {
                    process_game_update_outbound(stream, channel);
                }
            }
            else
            {
                switch (packet)
                {
                case PacketCode::ServerInit: process_server_init(stream); break;
                case PacketCode::WorldSpawn: process_world_spawn(stream); break;
                case PacketCode::WorldUnspawn: process_world_unspawn(stream); break;
                case PacketCode::SubworldSpawn: process_subworld_spawn(stream); break;
                case PacketCode::SubworldUnspawn: process_subworld_unspawn(stream); break;
                case PacketCode::PlayerSpawn: process_player_spawn(stream); break;
                case PacketCode::PlayerUnspawn: process_player_unspawn(stream); break;
                case PacketCode::ObserverSpawn: process_observer_spawn(stream); break;
                case PacketCode::ObserverUnspawn: process_observer_unspawn(stream); break;
                case PacketCode::BattleEye: break; // ignored
                case PacketCode::GameUpdate: process_game_update(stream, channel); break;
                default: break; // unhandled
                }
            }

            head += len + 4;
            stream->seek(head);
        }
    }

    void process_server_init(ByteStream* stream)
    {
        auto unk0 = stream->ReadByte();
        auto realDateTime = stream->ReadBool() ? 0 : stream->ReadInt64();
        auto gameDateTime = stream->ReadInt64();
        auto timeFactor = stream->ReadSingle();

        {
            // ASSET BUNDLES TO LOAD?
            // json
            auto unk1 = stream->ReadBytesAndSize();
        }

        {
            // WEATHER?
            // json
            auto unk2 = stream->ReadBytesAndSize();
        }
        
        {
            // json
            auto unk3 = stream->ReadBytesAndSize();
        }

        auto unk4 = stream->ReadBool();
        auto member_type = stream->ReadInt32(); // see EMemberCategory
        auto unk5 = stream->ReadSingle(); // dt?

        {
            // List of lootables? (no locations yet)
            // json
            auto unk6 = stream->ReadBytesAndSize();
        }

        auto unk7 = stream->ReadBytesAndSize();

        {
            // GClass806.SetupPositionQuantizer(@class.response.bounds_0);
            auto bound_min = stream->ReadVector3();
            auto bound_max = stream->ReadVector3();

            std::lock_guard<std::mutex> lock(g_world_lock);
            g_state->map = std::make_unique<Map>(bound_min, bound_max);
        }

        auto unk8 = stream->ReadUInt16();
        auto unk9 = stream->ReadByte();
    }

    void process_world_spawn(ByteStream* stream) { }
    void process_world_unspawn(ByteStream* stream) { }

    void recursive_add_items(ItemDescriptor* desc, std::vector<LootEntry>* entries)
    {
        LootItem* data = g_state->loot_db->query_loot(desc->template_id);

        LootEntry entry;
        entry.id = desc->id;
        entry.name = data ? data->name : desc->template_id;
        entry.value = data ? (data->value * desc->stack_count) : 0;
        entry.value_per_slot = data ? (entry.value / (data->width * data->height)) : 0;
        entry.rarity = data ? data->rarity : LootItem::Rarity::NotExist;
        entry.bundle_path = data ? data->bundle_path : "";
        entry.overriden = data ? data->overriden : false;
        entry.unknown = data == nullptr;
        entries->emplace_back(std::move(entry));

        for (const auto& grid : desc->grids)
        {
            for (const auto& item_in_grid : grid.items)
            {
                recursive_add_items(item_in_grid.item.get(), entries);
            }
        }
    }

    void process_subworld_spawn(ByteStream* stream)
    {
        if (!stream->ReadBool())
        {
            return;
        }

        auto loot_json_comp = stream->ReadBytesAndSize();
        auto loot_info_comp = stream->ReadBytesAndSize();
        auto loot_not_json = decompress_zlib(loot_json_comp.data(), (int)loot_json_comp.size());

        std::vector<std::unique_ptr<Polymorph>> morphs = read_polymorphs(loot_not_json.data(), (int)loot_not_json.size());

        for (std::unique_ptr<Polymorph>& morph : morphs)
        {
            if (morph->type == Polymorph::Type::JsonLootItemDescriptor)
            {
                JsonLootItemDescriptor* desc = (JsonLootItemDescriptor*)morph.get();

                std::vector<LootEntry> free_loot;
                recursive_add_items(&desc->item, &free_loot);

                std::lock_guard<std::mutex> lock(g_world_lock);

                for (LootEntry& entry : free_loot)
                {
                    entry.pos = desc->position;
                    g_state->map->add_loot_item(std::move(entry));
                }
            }
            else if (morph->type == Polymorph::Type::JsonCorpseDescriptor)
            {
                std::lock_guard<std::mutex> lock(g_world_lock);
                JsonCorpseDescriptor* desc = (JsonCorpseDescriptor*)morph.get();
                g_state->map->add_static_corpse(desc->position);
            }
        }
    }

    Observer deserialize_initial_state(ByteStream* stream)
    {
        // now see async Task method_92(NetworkReader reader)
        auto unk2 = stream->ReadByte();
        auto unk3 = stream->ReadBool();
        auto pos = stream->ReadVector3();
        auto rot = stream->ReadQuaternion();
        auto in_prone = stream->ReadBool();
        auto pose_level = stream->ReadSingle();

        Observer obs;

        {
            auto inventory_data = stream->ReadBytesAndSize();
            CSharpByteStream cstream(inventory_data.data(), (int)inventory_data.size());

            ItemDescriptor equipment;
            equipment.read(&cstream);

            for (SlotDescriptor& slot : equipment.slots)
            {
                // TODO: Should add scabbard if this is a scav. Some NPCs can drop RRs.
                if (slot.id != "Scabbard" && slot.id != "SecuredContainer")
                {
                    recursive_add_items(slot.contained_item.get(), &obs.inventory);
                }
            }
        }

        {
            auto profile_zip = stream->ReadBytesAndSize();
            auto profile_zip_data = decompress_zlib(profile_zip.data(), (int)profile_zip.size());

            std::string err;
            auto json = json11::Json::parse((char*)profile_zip_data.data(), err);

            auto id = json["_id"].string_value();
            auto info_name = json["Info"]["Nickname"].string_value();
            auto info_level = json["Info"]["Level"].int_value();
            auto info_side = json["Info"]["Side"].string_value();
            auto info_group_id = json["Info"]["GroupId"].string_value();
            auto info_role = json["Info"]["Settings"]["Role"].string_value();

            bool is_scav = info_side == "Savage";

            if (is_scav)
            {
                info_name = "Scav";
                info_name += info_role;
            }

            obs.type = is_scav ? Observer::Scav : Observer::Player;
            obs.is_npc = is_scav && info_level == 1;
            obs.id = id;
            obs.group_id = info_group_id;
            obs.name = info_name;
            obs.level = info_level;
            obs.pos = pos;
            obs.rot = to_euler(rot);
        }

        {
            // can find out what gear is in their pockets (I think)
            // json
            auto search_info = stream->ReadBytesAndSize();
        }

        // more stuff depending on flags to determine weapons, quickslots, etc...
        return obs;
    }

    void process_subworld_unspawn(ByteStream* stream) { }

    void process_player_spawn(ByteStream* stream)
    {
        auto pid = stream->ReadInt32();
        auto cid = stream->ReadByte();
        auto pos = stream->ReadVector3();

        Observer obs = deserialize_initial_state(stream);
        obs.pid = pid;
        obs.cid = cid;
        obs.type = Observer::Self;

        std::lock_guard<std::mutex> lock(g_world_lock);
        g_state->map->create_observer(obs.cid, std::move(obs));
    }

    void process_player_unspawn(ByteStream* stream) { }

    void process_observer_spawn(ByteStream* stream)
    {
        auto pid = stream->ReadInt32();
        auto cid = stream->ReadByte();
        auto pos = stream->ReadVector3();

        Observer obs = deserialize_initial_state(stream);
        obs.pid = pid;
        obs.cid = cid;

        std::lock_guard<std::mutex> lock(g_world_lock);
        g_state->map->create_observer(obs.cid, std::move(obs));
    }

    void process_observer_unspawn(ByteStream* stream)
    {
        auto pid = stream->ReadInt32();
        auto cid = stream->ReadByte();

        std::lock_guard<std::mutex> lock(g_world_lock);
        g_state->map->get_observer(cid)->is_unspawned = true;
    }

    void update_position(BitStream& bstream, tk::Observer* obs)
    {
        // static Vector3 smethod_3(IBitReaderStream reader, Vector3 prevPosition)
        obs->last_pos = obs->pos;

        Vector3 new_pos = obs->pos;

        bool read = bstream.ReadBool();
        bool partial_read;

        if (read) // vector.sqrMagnitude > 9.536743E-07f;
        {
            partial_read = bstream.ReadBool();
            if (partial_read) // vector.AllAbsComponentsLessThen(1f);
            {
                Vector3Quantizer quant(-1.0f, 1.f, 0.001953125f, -1.f, 1.f, 0.0009765625f, -1.f, 1.f, 0.001953125f, false);

                new_pos.x += bstream.ReadQuantizedFloat(&quant._xFloatQuantizer);
                new_pos.y += bstream.ReadQuantizedFloat(&quant._yFloatQuantizer);
                new_pos.z += bstream.ReadQuantizedFloat(&quant._zFloatQuantizer);
            }
            else
            {
                Vector3Quantizer quant(
                    g_state->map->bounds_min().x, g_state->map->bounds_max().x, 0.001953125f,
                    g_state->map->bounds_min().y, g_state->map->bounds_max().y, 0.0009765625f,
                    g_state->map->bounds_min().z, g_state->map->bounds_max().z, 0.001953125f,
                    true);

                new_pos.x = bstream.ReadQuantizedFloat(&quant._xFloatQuantizer);
                new_pos.y = bstream.ReadQuantizedFloat(&quant._yFloatQuantizer);
                new_pos.z = bstream.ReadQuantizedFloat(&quant._zFloatQuantizer);
            }
        }

        obs->pos = new_pos;
    }

    void update_rotation(BitStream& bstream, tk::Observer* obs)
    {
        if (bstream.ReadBool())
        {
            Vector2Quantizer quant(0.0f, 360.0f, 0.015625f, -90.0f, 90.0f, 0.015625f);
            obs->rot.x = bstream.ReadQuantizedFloat(&quant._xFloatQuantizer);
            obs->rot.y = bstream.ReadQuantizedFloat(&quant._yFloatQuantizer);
        }
    }

    void skip_misc_stuff(BitStream& bstream)
    {
        if (bstream.ReadBool())
        {
            bstream.ReadUInt8();
        }

        if (bstream.ReadBool())
        {
            bstream.ReadLimitedInt32(0, 31);
        }

        if (bstream.ReadBool())
        {
            bstream.ReadLimitedInt32(0, 63);
        }

        if (bstream.ReadBool())
        {
            Vector2Quantizer quant(-1.0f, 1.0f, 0.03125f, -1.0f, 1.0f, 0.03125f);
            bstream.ReadQuantizedFloat(&quant._xFloatQuantizer);
            bstream.ReadQuantizedFloat(&quant._yFloatQuantizer);
        }

        if (bstream.ReadBool())
        {
            bstream.ReadLimitedFloat(0.0f, 1.0f, 0.0078125f);
        }

        if (bstream.ReadBool())
        {
            bstream.ReadLimitedFloat(0.0f, 1.0f, 0.0078125f);
        }

        if (bstream.ReadBool())
        {
            bstream.ReadLimitedFloat(-5.0f, 5.0f, 0.0078125f);
        }

        if (bstream.ReadBool())
        {
            if (!bstream.ReadBool())
            {
                bstream.ReadBool();
            }
        }

        bstream.ReadLimitedInt32(-1, 1);
        bstream.ReadBool();

        if (!bstream.ReadBool())
        {
            bstream.ReadLimitedFloat(-50.0f, 50.0f, 0.0625f);
            bstream.ReadLimitedFloat(-50.0f, 50.0f, 0.0625f);
        }

        bstream.ReadBool();
        bstream.ReadBool();
        bstream.ReadBool();

        if (bstream.ReadBool())
        {
            if (bstream.ReadBool()) // InteractWithDoorPacket
            {
                if (bstream.ReadBool())
                {
                    auto type = bstream.ReadLimitedInt32(0, 4);
                    bstream.ReadString(1350);
                    bstream.ReadLimitedInt32(0, 2);
                    if (type == 2)
                    {
                        bstream.ReadLimitedString(L' ', L'z');
                    }
                }
            }
            if (bstream.ReadBool()) // LootInteractionPacket
            {
                if (bstream.ReadBool())
                {
                    std::wstring loot_id = bstream.ReadString(1350);
                    uint32_t callback_id = bstream.ReadUInt32();
                }
            }
            if (bstream.ReadBool()) // StationaryWeaponPacket
            {
                if (bstream.ReadBool())
                {
                    auto type = bstream.ReadUInt8();
                    if (type == 0)
                    {
                        bstream.ReadString(1350);
                    }
                }
            }
            if (bstream.ReadBool()) // PlantItemPacket
            {
                if (bstream.ReadBool())
                {
                    bstream.ReadString(1350);
                    bstream.ReadString(1350);
                }
            }
        }

        if (!bstream.ReadBool())
        {
            int optype = bstream.ReadLimitedInt32(0, 10);
            bstream.ReadLimitedString(L' ', L'z');
            bstream.ReadLimitedInt32(0, 2047);
            if (optype == 6) // CreateMeds
            {
                bstream.ReadLimitedInt32(0, 7);
                bstream.ReadFloat();
            }
            bstream.ReadLimitedInt32(-1, 3);
        }
    }

    void update_loot(BitStream& bstream, tk::Observer* obs, bool outbound)
    {
        auto read_operation = [&]()
        {
            if (bstream.ReadBool())
            {
                auto data = bstream.ReadBytesAlloc();
                int callback = bstream.ReadLimitedInt32(0, 2047);
                int hash = bstream.ReadInt32();

                CSharpByteStream cstream(data.data(), (int)data.size());
                std::unique_ptr<Polymorph> polymorph = read_polymorph(&cstream);

                if (polymorph->type == Polymorph::Type::InventoryMoveOperationDescriptor)
                {
                    InventoryMoveOperationDescriptor* desc = (InventoryMoveOperationDescriptor*)polymorph.get();
                    // TODO: Loot refactor
                    // This should move the object around, not destroy it. Destroying it allows us to hide it in the short term.
                    //if (desc->from->type == Polymorph::Type::InventoryOwnerItselfDescriptor)
                    {
                        tk::g_state->map->destroy_loot_item_by_id(desc->item_id);
                    }
                }
            }
        };

        int num = bstream.ReadUInt8();
        for (int i = 0; i < num; ++i)
        {
            if (outbound)
            {
                read_operation();
            }
            else
            {
                int tag = bstream.ReadUInt8();
                if (tag == 1) // command
                {
                    read_operation();
                    continue;
                }

                auto id = bstream.ReadUInt16();
                int status = bstream.ReadLimitedInt32(0, 3);
                if (status == 2)
                {
                    bstream.ReadLimitedString(L' ', L'\u007f');
                }
                if (bstream.ReadBool())
                {
                    bstream.ReadInt32();
                    bstream.ReadBool();
                }
            }

        }
    }

    void update_network_player(BitStream& bstream, int channel)
    {
        std::lock_guard<std::mutex> lock(g_world_lock);

        Observer* obs = g_state->map->get_observer(channel);

        if (!obs)
        {
            // I suspect this is caused by receiving game updates for observers before we receive the spawn message.
            // This leads to "phantom observers".
            // Fixing this will require us to understand unet acks in more detail and deliver messages on ordered channels strictly in-order.
            Observer new_obs;
            new_obs.pid = -1;
            new_obs.cid = channel;
            new_obs.type = Observer::ObserverType::Player;
            new_obs.name = "UNKNOWN?!";
            g_state->map->create_observer(channel, std::move(new_obs));
            obs = g_state->map->get_observer(channel);
        }

        if (obs->type != Observer::Self)
        {
            // ObservedPlayer::OnDeserializeFromServer(byte channelId, IBitReaderStream reader, int rtt)
            // void ObservedPlayer.GInterface89.Receive(IBitReaderStream reader, GClass656 destinationFramesInfoQueue)

            auto num = bstream.ReadBool() ? bstream.ReadLimitedInt32(1, 5) : bstream.ReadLimitedInt32(0, 2097151);
            auto time = bstream.ReadFloat();
            auto is_disconnected = bstream.ReadBool();

            if (!bstream.ReadBool())
            {
                g_state->map->get_observer(channel)->is_dead = true;
            }
            else
            {
                update_position(bstream, obs);
                update_rotation(bstream, obs);
                skip_misc_stuff(bstream);
                update_loot(bstream, obs, false);
            }
        }
    }

    void update_network_world(BitStream& bstream, int channel)
    {
        if (bstream.ReadBool()) // DeserializeInteractiveObjectsStatusPacket
        {
            return;
        }

        if (bstream.ReadBool()) // DeserializeSpawnQuestLootPacket
        {
            return;
        }

        if (bstream.ReadBool()) // DeserializeUpdateExfiltrationPointPacket
        {
            return;
        }

        if (bstream.ReadBool()) // DeserializeLampChangeStatePacket
        {
            return;
        }

        if (bstream.ReadBool()) // DeserializeLootSyncPackets
        {
            auto num = bstream.ReadLimitedInt32(1, 64);
            for (int i = 0; i < num; ++i)
            {
                auto id = bstream.ReadInt32();

                std::lock_guard<std::mutex> lock(g_world_lock);
                TemporaryLoot* loot = g_state->map->get_or_create_temporary_loot(id);

                if (bstream.ReadBool())
                {
                    Vector3Quantizer quant(-10.0f, 10.0f, 0.001953125f, -10.0f, 10.0f, 0.0009765625f, -10.0f, 10.0f, 0.001953125f, true);

                    loot->pos.x += bstream.ReadQuantizedFloat(&quant._xFloatQuantizer);
                    loot->pos.y += bstream.ReadQuantizedFloat(&quant._yFloatQuantizer);
                    loot->pos.z += bstream.ReadQuantizedFloat(&quant._zFloatQuantizer);
                }
                else
                {
                    Vector3Quantizer quant(
                        g_state->map->bounds_min().x, g_state->map->bounds_max().x, 0.001953125f,
                        g_state->map->bounds_min().y, g_state->map->bounds_max().y, 0.0009765625f,
                        g_state->map->bounds_min().z, g_state->map->bounds_max().z, 0.001953125f,
                        true);

                    loot->pos.x = bstream.ReadQuantizedFloat(&quant._xFloatQuantizer);
                    loot->pos.y = bstream.ReadQuantizedFloat(&quant._yFloatQuantizer);
                    loot->pos.z = bstream.ReadQuantizedFloat(&quant._zFloatQuantizer);
                }
            }
        }
    }

    void process_game_update(ByteStream* stream, int channel)
    {
        // see internal void method_7(byte channelId, NetworkReader reader)
        auto data = stream->ReadBytesAndSize();

        // see now bool method_6(byte channelId, IBitReaderStream reader, int rtt)
        BitStream bstream(data.data(), (int)data.size());

        if (bstream.ReadLimitedInt32(0, 1) == 1)
        {
            update_network_player(bstream, channel);
        }
        else
        {
            update_network_world(bstream, channel);
        }
    }

    void process_game_update_outbound(ByteStream* stream, int channel)
    {
        auto data = stream->ReadBytesAndSize();
        BitStream bstream(data.data(), (int)data.size());

        int num = bstream.ReadLimitedInt32(0, 127);
        for (int i = 0; i < num; ++i)
        {
            auto rtt = bstream.ReadBool() ? bstream.ReadUInt16() : 0;
            auto dt = bstream.ReadLimitedFloat(0, 1, 0.0009765625);
            auto frame = bstream.ReadLimitedInt32(0, 2097151);
            auto frame_2 = bstream.ReadBool()
                ? bstream.ReadLimitedInt32(0, 2097151)
                : bstream.ReadLimitedInt32(0, 15);

            std::lock_guard<std::mutex> lock(g_world_lock);
            tk::Observer* player = g_state->map->get_observer(channel);
            update_position(bstream, player);
            update_rotation(bstream, player);
            skip_misc_stuff(bstream);
            update_loot(bstream, player, true);
        }
    }
}
