#include "tk_net.hpp"

#include "tk.hpp"
#include "tk_loot.hpp"
#include "tk_map.hpp"
#include "json11.hpp"

#include <unordered_map>

namespace tk
{
    static bool s_encrypted = false;

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
                if (!s_encrypted && packet == PacketCode::GameUpdate)
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
                case PacketCode::GameUpdate: if (!s_encrypted) process_game_update(stream, channel); break;
                default: break; // unhandled
                }
            }

            head += len + 4;
            stream->seek(head);
        }
    }

    void process_server_init(ByteStream* stream)
    {
        s_encrypted = stream->ReadBool(); // encrypt
        auto encrypted2 = stream->ReadBool(); // decrypt

        printf("SESSION ENCRYPTION: %d %d\n", !!s_encrypted, !!encrypted2);

        auto realDateTime = stream->ReadBool() ? 0 : stream->ReadInt64();
        auto gameDateTime = stream->ReadInt64();
        auto timeFactor = stream->ReadSingle();
        stream->ReadBytesAndSize();
        stream->ReadBytesAndSize();
        stream->ReadBytesAndSize();
        stream->ReadBool();
        auto member_type = stream->ReadInt32(); // see EMemberCategory
        stream->ReadSingle(); // dt?
        stream->ReadBytesAndSize();
        stream->ReadBytesAndSize();

        {
            // GClass806.SetupPositionQuantizer(@class.response.bounds_0);
            auto bound_min = stream->ReadVector3();
            auto bound_max = stream->ReadVector3();

            std::lock_guard<std::mutex> lock(g_world_lock);
            g_state->map = std::make_unique<Map>(bound_min, bound_max);
        }

        stream->ReadUInt16();
        stream->ReadByte();
    }

    void process_world_spawn(ByteStream* stream) { }
    void process_world_unspawn(ByteStream* stream) { }

    int csharp_get_hash_code(const char* str)
    {
        int hash1 = 5381;
        int hash2 = hash1;

        int c;
        const char *s = str;
        while ((c = s[0]) != 0)
        {
            hash1 = ((hash1 << 5) + hash1) ^ c;
            c = s[1];
            if (c == 0) break;
            hash2 = ((hash2 << 5) + hash2) ^ c;
            s += 2;
        }
        return hash1 + (hash2 * 1566083941);
    }

    LootInstance* recursive_add_items(ItemDescriptor* desc, std::vector<std::string>* entries, Vector3 pos, std::string& parent, int owner = LootInstanceOwnerInvalid, bool adding_to_player = false)
    {
        LootTemplate* template_ = g_loot_db->query_template(desc->template_id);

        LootInstance instance;
        instance.id = desc->id;
        instance.parent_id = parent;
        instance.csharp_hash = csharp_get_hash_code(desc->id.c_str());
        instance.owner = owner;
        instance.template_ = template_;
        instance.pos = pos;
        instance.stack_count = desc->stack_count;
        instance.value = template_->value * instance.stack_count;
        instance.value_per_slot = instance.value / (template_->width * template_->height);
        entries->emplace_back(instance.id);

        LootInstance* instance_ptr;

        {
            std::lock_guard<std::mutex> lock(g_world_lock);
            instance_ptr = g_state->map->add_loot_instance(std::move(instance));
        }

        for (const auto& grid : desc->grids)
        {
            for (const auto& item_in_grid : grid.items)
            {
                recursive_add_items(item_in_grid.item.get(), entries, pos, desc->id);
            }
        }

        for (const auto& slot : desc->slots)
        {
            LootInstance* added_slot = recursive_add_items(slot.contained_item.get(), entries, pos, desc->id);
            if (slot.id == "SecuredContainer" || (adding_to_player && slot.id == "Scabbard"))
            {
                added_slot->inaccessible = true;
            }
        }

        for (const auto& stack_slot : desc->stack_slots)
        {
            for (const auto& item_in_stack_slot : stack_slot.items)
            {
                recursive_add_items(item_in_stack_slot.get(), entries, pos, desc->id);
            }
        }

        return instance_ptr;
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

                std::vector<std::string> free_loot;
                recursive_add_items(&desc->item, &free_loot, desc->position, std::string(), LootInstanceOwnerWorld);
            }
            else if (morph->type == Polymorph::Type::JsonCorpseDescriptor)
            {
                JsonCorpseDescriptor* desc = (JsonCorpseDescriptor*)morph.get();

                std::vector<std::string> corpse_loot;
                recursive_add_items(&desc->item, &corpse_loot, desc->position, std::string(), LootInstanceOwnerWorld);

                std::lock_guard<std::mutex> lock(g_world_lock);
                g_state->map->add_static_corpse(desc->position);
            }
        }
    }

    Observer deserialize_initial_state(ByteStream* stream, int pid, int cid)
    {
        // now see async Task method_92(NetworkReader reader)
        auto unk2 = stream->ReadByte();
        auto unk3 = stream->ReadBool();
        auto pos = stream->ReadVector3();
        auto rot = stream->ReadQuaternion();
        auto in_prone = stream->ReadBool();
        auto pose_level = stream->ReadSingle();

        Observer obs;
        obs.pid = pid;
        obs.cid = cid;

        ItemDescriptor equipment;

        { // equipment
            auto inventory_data = stream->ReadBytesAndSize();
            CSharpByteStream cstream(inventory_data.data(), (int)inventory_data.size());
            equipment.read(&cstream);
        }

        { // profile
            auto profile_zip = stream->ReadBytesAndSize();
            auto profile_zip_data = decompress_zlib(profile_zip.data(), (int)profile_zip.size());

            std::string err;
            auto json = json11::Json::parse((char*)profile_zip_data.data(), err);

            auto id = json["_id"].string_value();
            auto aid = json["aid"].string_value();
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
            obs.is_npc = is_scav && aid == "0";
            obs.id = id;
            obs.group_id = info_group_id;
            obs.name = info_name;
            obs.level = info_level;
            obs.pos = pos;
            obs.rot = to_euler(rot);
        }

        std::vector<std::string> inventory_loot;
        recursive_add_items(&equipment, &inventory_loot, obs.pos, std::string(), obs.cid, !obs.is_npc);

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

        Observer obs = deserialize_initial_state(stream, pid, cid);
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

        Observer obs = deserialize_initial_state(stream, pid, cid);

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

        TK_ASSERT(!isnan(new_pos.x) && !isnan(new_pos.y) && !isnan(new_pos.z));
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
        bstream.ReadBool();

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

        if (bstream.ReadBool()) // MOVEMENT_DIRECTION_QUANTIZER
        {
            Vector2Quantizer quant(-1.0f, 1.0f, 0.03125f, -1.0f, 1.0f, 0.03125f);
            bstream.ReadQuantizedFloat(&quant._xFloatQuantizer);
            bstream.ReadQuantizedFloat(&quant._yFloatQuantizer);
        }

        if (bstream.ReadBool()) // PoseLevel, POSE_RANGE
        {
            bstream.ReadLimitedFloat(0.0f, 1.0f, 0.0078125f);
        }

        if (bstream.ReadBool()) // CharacterMovementSpeed
        {
            bstream.ReadLimitedFloat(0.0f, 1.0f, 0.0078125f);
        }

        if (bstream.ReadBool()) // Tilt, ILT_RANGE
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

        bstream.ReadCheck();

        bstream.ReadLimitedInt32(-1, 1); // BlindFire
        bstream.ReadBool(); // SoftSurface

        if (!bstream.ReadBool()) // HeadRotation
        {
            bstream.ReadLimitedFloat(-50.0f, 50.0f, 0.0625f);
            bstream.ReadLimitedFloat(-50.0f, 50.0f, 0.0625f);
        }

        bstream.ReadBool(); // StaminaExhausted
        bstream.ReadBool(); // OxygenExhausted
        bstream.ReadBool(); // HandsExhausted

        bstream.ReadCheck();

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

        bstream.ReadCheck();

        if (!bstream.ReadBool()) // handsChangePacket
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
                    if (LootInstance* instance = g_state->map->get_loot_instance_by_id(desc->item_id))
                    {
                        auto get_container_id = [](Polymorph* morph) -> const std::string&
                        {
                            if (morph->type == Polymorph::Type::InventoryGridItemAddressDescriptor)
                            {
                                InventoryGridItemAddressDescriptor* desc = (InventoryGridItemAddressDescriptor*)morph;
                                return desc->container.parent_id;
                            }
                            else if (morph->type == Polymorph::Type::InventoryOwnerItselfDescriptor)
                            {
                                InventoryOwnerItselfDescriptor* desc = (InventoryOwnerItselfDescriptor*)morph;
                                return desc->container.parent_id;
                            }
                            else if (morph->type == Polymorph::Type::InventorySlotItemAddressDescriptor)
                            {
                                InventorySlotItemAddressDescriptor* desc = (InventorySlotItemAddressDescriptor*)morph;
                                return desc->container.parent_id;
                            }
                            else if (morph->type == Polymorph::Type::InventoryStackSlotItemAddress)
                            {
                                InventoryStackSlotItemAddress* desc = (InventoryStackSlotItemAddress*)morph;
                                return desc->container.parent_id;
                            }

                            TK_ASSERT(false);
                            static std::string s_empty_str = "";
                            return s_empty_str;
                        };

                        const std::string& from = get_container_id(desc->from.get());
                        const std::string& to = get_container_id(desc->to.get());

                        instance->parent_id = to;
                        instance->owner = LootInstanceOwnerInvalid;
                    }
                }
                else if (polymorph->type == Polymorph::Type::InventoryThrowOperationDescriptor)
                {
                    // Move from player to world
                    InventoryThrowOperationDescriptor* desc = (InventoryThrowOperationDescriptor*)polymorph.get();
                    if (LootInstance* instance = g_state->map->get_loot_instance_by_id(desc->item_id))
                    {
                        instance->parent_id = "";
                        instance->owner = LootInstanceOwnerWorld;
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

            bstream.ReadCheck(299);

            if (!bstream.ReadBool())
            {
                g_state->map->get_observer(channel)->is_dead = true;
            }
            else
            {
                bstream.ReadCheck(304); // DeserializeDiffUsing(IBitReaderStream reader, ref GStruct118 current, GStruct118 prevFrame)
                bstream.ReadCheck(); // inside DeserializeDiffUsing(IBitReaderStream reader, ref GStruct90 movementInfoPacket, GStruct90 prevFrame, GStruct89 context)

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

                LootInstance* instance = nullptr;

                {
                    std::lock_guard<std::mutex> lock(g_world_lock);
                    for (LootInstance* loot : g_state->map->get_loot())
                    {
                        if (loot->csharp_hash == id)
                        {
                            instance = loot;
                            break;
                        }
                    }
                }

                Vector3 pos;

                if (instance)
                {
                    pos = instance->pos;
                }

                if (bstream.ReadBool())
                {
                    Vector3Quantizer quant(-10.0f, 10.0f, 0.001953125f, -10.0f, 10.0f, 0.0009765625f, -10.0f, 10.0f, 0.001953125f, true);
                    pos.x += bstream.ReadQuantizedFloat(&quant._xFloatQuantizer);
                    pos.y += bstream.ReadQuantizedFloat(&quant._yFloatQuantizer);
                    pos.z += bstream.ReadQuantizedFloat(&quant._zFloatQuantizer);
                }
                else
                {
                    Vector3Quantizer quant(
                        g_state->map->bounds_min().x, g_state->map->bounds_max().x, 0.001953125f,
                        g_state->map->bounds_min().y, g_state->map->bounds_max().y, 0.0009765625f,
                        g_state->map->bounds_min().z, g_state->map->bounds_max().z, 0.001953125f,
                        true);

                    pos.x = bstream.ReadQuantizedFloat(&quant._xFloatQuantizer);
                    pos.y = bstream.ReadQuantizedFloat(&quant._yFloatQuantizer);
                    pos.z = bstream.ReadQuantizedFloat(&quant._zFloatQuantizer);
                }

                if (instance)
                {
                    instance->pos = pos;
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

            bstream.ReadCheck(); // SerializeDiffUsing(IBitWriterStream writer, ref GStruct90 current, GStruct90 prevFrame)
            update_position(bstream, player);
            update_rotation(bstream, player);
            skip_misc_stuff(bstream);
            update_loot(bstream, player, true);
        }
    }
}
