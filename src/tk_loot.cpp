#include "tk_loot.hpp"
#include "json11.hpp"
#include <cassert>

namespace tk
{
    LootDatabase::LootDatabase(const char* path)
    {
        FILE* file = fopen(path, "rb");

        if (file)
        {
            fseek(file, 0, SEEK_END);
            int len = ftell(file);
            fseek(file, 0, SEEK_SET);

            std::vector<char> data;
            data.resize(len + 1);
            fread(data.data(), 1, len, file);
            data[data.size() - 1] = '\0';

            fclose(file);

            std::string err;
            json11::Json json = json11::Json::parse(data.data(), err);

            for (auto data_entry : json["data"].object_items())
            {
                LootItem item;
                item.id = data_entry.first;

                auto props = data_entry.second["_props"];

                if (props["Name"].is_string())
                {
                    item.name = props["Name"].string_value();
                }
                else if (props["ShortName"].is_string())
                {
                    item.name = props["ShortName"].string_value();
                }
                else if (data_entry.second["_name"].is_string())
                {
                    item.name = data_entry.second["_name"].string_value();
                }

                item.value = props["CreditsPrice"].int_value();
                item.lootable = !props["Unlootable"].bool_value();
                item.bundle_path = props["Prefab"]["path"].string_value();

                item.rarity = LootItem::Common;

                if (props["Rarity"] == "Rare")
                {
                    item.rarity = LootItem::Rare;
                }
                else if (props["Rarity"] == "Superrare")
                {
                    item.rarity = LootItem::SuperRare;
                }
                else if (props["Rarity"] == "Not_exist")
                {
                    item.rarity = LootItem::NotExist;
                }

                m_db[item.id] = std::move(item);
            }
        }
    }

    LootItem* LootDatabase::query_loot(const std::string& id)
    {
        auto entry = m_db.find(id);
        return entry == std::end(m_db) ? nullptr : &entry->second;
    }

    std::unique_ptr<Polymorph> read_polymorph(CSharpByteStream* stream);

    std::vector<std::unique_ptr<Polymorph>> read_polymorphs(uint8_t* data, int size)
    {
        CSharpByteStream stream(data, size);

        std::vector<std::unique_ptr<Polymorph>> ret;
        int num = stream.ReadInt32();
        for (int i = 0; i < num; ++i)
        {
            ret.emplace_back(read_polymorph(&stream));
        }

        return ret;
    }

    template <typename T>
    std::unique_ptr<Polymorph> make_polymorph(CSharpByteStream* stream, Polymorph::Type type)
    {
        T* desc = new T();
        desc->type = type;
        desc->read(stream);
        return std::unique_ptr<Polymorph>(desc);
    }

    std::unique_ptr<Polymorph> read_polymorph(CSharpByteStream* stream)
    {
        Polymorph::Type type = (Polymorph::Type)stream->ReadByte();

        switch (type)
        {
            case Polymorph::FoodDrinkComponentDescriptor: return make_polymorph<FoodDrinkComponentDescriptor>(stream, type);
            case Polymorph::ResourceItemComponentDescriptor: return make_polymorph<ResourceItemComponentDescriptor>(stream, type);
            case Polymorph::LightComponentDescriptor: return make_polymorph<LightComponentDescriptor>(stream, type);
            case Polymorph::LockableComponentDescriptor: return make_polymorph<LockableComponentDescriptor>(stream, type);
            case Polymorph::MapComponentDescriptor: return make_polymorph< MapComponentDescriptor>(stream, type);
            case Polymorph::MedKitComponentDescriptor: return make_polymorph<MedKitComponentDescriptor>(stream, type);
            case Polymorph::RepairableComponentDescriptor: return make_polymorph<RepairableComponentDescriptor>(stream, type);
            case Polymorph::SightComponentDescriptor: return make_polymorph<SightComponentDescriptor>(stream, type);
            case Polymorph::TogglableComponentDescriptor: return make_polymorph<TogglableComponentDescriptor>(stream, type);
            case Polymorph::FaceShieldComponentDescriptor: return make_polymorph<FaceShieldComponentDescriptor>(stream, type);
            case Polymorph::FoldableComponentDescriptor: return make_polymorph<FoldableComponentDescriptor>(stream, type);
            case Polymorph::FireModeComponentDescriptor: return make_polymorph<FireModeComponentDescriptor>(stream, type);
            case Polymorph::DogTagComponentDescriptor: return make_polymorph<DogTagComponentDescriptor>(stream, type);
            case Polymorph::TagComponentDescriptor: return make_polymorph<TagComponentDescriptor>(stream, type);
            case Polymorph::KeyComponentDescriptor: return make_polymorph<KeyComponentDescriptor>(stream, type);
            case Polymorph::JsonLootItemDescriptor: return make_polymorph<JsonLootItemDescriptor>(stream, type);
            case Polymorph::JsonCorpseDescriptor: return make_polymorph<JsonCorpseDescriptor>(stream, type);
            case Polymorph::ApplyHealthOperationDescriptor: return make_polymorph<ApplyHealthOperationDescriptor>(stream, type);
            case Polymorph::OperateStationaryWeaponOperationDescription: return make_polymorph<OperateStationaryWeaponOperationDescription>(stream, type);
            default: __debugbreak(); assert(false); break;
        }

        return nullptr;
    }

    void FoodDrinkComponentDescriptor::read(CSharpByteStream* stream)
    {
        hp_percent = stream->ReadSingle();
    }

    void ResourceItemComponentDescriptor::read(CSharpByteStream* stream)
    {
        resource = stream->ReadSingle();
    }

    void LightComponentDescriptor::read(CSharpByteStream* stream)
    {
        active = stream->ReadBool();
        mode = stream->ReadInt32();
    }

    void LockableComponentDescriptor::read(CSharpByteStream* stream)
    {
        locked = stream->ReadBool();
    }

    void InventoryLogicMapMarker::read(CSharpByteStream* stream)
    {
        type = stream->ReadInt32();
        x = stream->ReadInt32();
        y = stream->ReadInt32();
        note = stream->ReadString();
    }

    void MapComponentDescriptor::read(CSharpByteStream* stream)
    {
        int num = stream->ReadInt32();
        for (int i = 0; i < num; ++i)
        {
            InventoryLogicMapMarker marker;
            marker.read(stream);
            markers.emplace_back(std::move(marker));
        }
    }

    void MedKitComponentDescriptor::read(CSharpByteStream* stream)
    {
        hp = stream->ReadSingle();
    }

    void RepairableComponentDescriptor::read(CSharpByteStream* stream)
    {
        durability = stream->ReadSingle();
        max_durability = stream->ReadSingle();
    }

    void SightComponentDescriptor::read(CSharpByteStream* stream)
    {
        sight_mode = stream->ReadInt32();
    }

    void TogglableComponentDescriptor::read(CSharpByteStream* stream)
    {
        on = stream->ReadBool();
    }

    void FaceShieldComponentDescriptor::read(CSharpByteStream* stream)
    {
        hits = stream->ReadByte();
        hit_seed = stream->ReadByte();
    }

    void FoldableComponentDescriptor::read(CSharpByteStream* stream)
    {
        folded = stream->ReadBool();
    }

    void FireModeComponentDescriptor::read(CSharpByteStream* stream)
    {
        fire_mode = stream->ReadInt32();
    }

    void DogTagComponentDescriptor::read(CSharpByteStream* stream)
    {
        name = stream->ReadString();
        side = stream->ReadInt32();
        level = stream->ReadInt32();
        time = stream->ReadDouble();
        status = stream->ReadString();
        killer_name = stream->ReadString();
        weapon_name = stream->ReadString();
    }

    void TagComponentDescriptor::read(CSharpByteStream* stream)
    {
        name = stream->ReadString();
        colour = stream->ReadInt32();
    }

    void KeyComponentDescriptor::read(CSharpByteStream* stream)
    {
        uses = stream->ReadInt32();
    }

    void SlotDescriptor::read(CSharpByteStream* stream)
    {
        id = stream->ReadString();
        contained_item = std::make_unique<ItemDescriptor>();
        contained_item->read(stream);
    }

    void LocationInGrid::read(CSharpByteStream* stream)
    {
        x = stream->ReadInt32();
        y = stream->ReadInt32();
        rotation = stream->ReadInt32();
        searched = stream->ReadBool();
    }

    void ItemInGridDescriptor::read(CSharpByteStream* stream)
    {
        location.read(stream);
        item = std::make_unique<ItemDescriptor>();
        item->read(stream);
    }

    void GridDescriptor::read(CSharpByteStream* stream)
    {
        id = stream->ReadString();
        int num = stream->ReadInt32();
        for (int i = 0; i < num; ++i)
        {
            ItemInGridDescriptor desc;
            desc.read(stream);
            items.emplace_back(std::move(desc));
        }
    }

    void StackSlotDescriptor::read(CSharpByteStream* stream)
    {
        id = stream->ReadString();
        int num = stream->ReadInt32();
        for (int i = 0; i < num; ++i)
        {
            std::unique_ptr<ItemDescriptor> item = std::make_unique<ItemDescriptor>();
            item->read(stream);
            items.emplace_back(std::move(item));
        }
    }

    void ItemDescriptor::read(CSharpByteStream* stream)
    {
        id = stream->ReadString();
        template_id = stream->ReadString();
        stack_count = stream->ReadInt32();
        spawned_in_session = stream->ReadBool();
        int num1 = stream->ReadInt32();
        for (int i = 0; i < num1; ++i)
        {
            components.emplace_back(read_polymorph(stream));
        }
        int num2 = stream->ReadInt32();
        for (int i = 0; i < num2; ++i)
        {
            SlotDescriptor desc;
            desc.read(stream);
            slots.emplace_back(std::move(desc));
        }
        int num3 = stream->ReadInt32();
        for (int i = 0; i < num3; ++i)
        {
            GridDescriptor desc;
            desc.read(stream);
            grids.emplace_back(std::move(desc));
        }
        int num4 = stream->ReadInt32();
        for (int i = 0; i < num4; ++i)
        {
            StackSlotDescriptor desc;
            desc.read(stream);
            stack_slots.emplace_back(std::move(desc));
        }
    }

    void JsonLootItemDescriptor::read(CSharpByteStream* stream)
    {
        if (stream->ReadBool())
        {
            id = stream->ReadString();
        }

        position = stream->ReadVector();
        rotation = stream->ReadVector();
        item.read(stream);

        if (stream->ReadBool())
        {
            int num = stream->ReadInt32();
            for (int i = 0; i < num; ++i)
            {
                profiles.emplace_back(stream->ReadString());
            }
        }

        is_static = stream->ReadBool();
        use_gravity = stream->ReadBool();
        random_rotation = stream->ReadBool();
        shift = stream->ReadVector();
        platform_id = stream->ReadInt16();
    }

    void ClassTransformSync::read(CSharpByteStream* stream)
    {
        position = stream->ReadVector();
        rotation = stream->ReadQuaternion();
    }

    void JsonCorpseDescriptor::read(CSharpByteStream* stream)
    {
        int num1 = stream->ReadInt32();
        for (int i = 0; i < num1; ++i)
        {
            int entry = stream->ReadInt32();
            std::string str = stream->ReadString();
            customization[entry] = std::move(str);
        }
        side = stream->ReadInt32();
        int num2 = stream->ReadInt32();
        for (int i = 0; i < num2; ++i)
        {
            ClassTransformSync bone;
            bone.read(stream);
            bones.emplace_back(std::move(bone));
        }

        if (stream->ReadBool())
        {
            id = stream->ReadString();
        }

        position = stream->ReadVector();
        rotation = stream->ReadVector();
        item.read(stream);

        if (stream->ReadBool())
        {
            int num3 = stream->ReadInt32();
            for (int i = 0; i < num3; ++i)
            {
                profiles.emplace_back(stream->ReadString());
            }
        }

        is_static = stream->ReadBool();
        use_gravity = stream->ReadBool();
        random_rotation = stream->ReadBool();
        shift = stream->ReadVector();
        platform_id = stream->ReadInt16();
    }

    void ApplyHealthOperationDescriptor::read(CSharpByteStream* stream)
    {
        item_id = stream->ReadString();
        body_part = stream->ReadInt32();
        count = stream->ReadInt32();
        operation_id = stream->ReadUInt16();
    }

    void OperateStationaryWeaponOperationDescription::read(CSharpByteStream* stream)
    {
        weapon_id = stream->ReadString();
        operation_id = stream->ReadUInt16();
    }
}
