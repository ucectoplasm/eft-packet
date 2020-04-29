#include "tk_loot.hpp"
#include "json11.hpp"
#include <cassert>

namespace tk
{
    LootDatabase::LootDatabase()
    {
        auto load_file_as_json = [](const char* path) -> json11::Json
        {
            FILE* file = fopen(path, "rb");

            json11::Json json;

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
                json = json11::Json::parse(data.data(), err, json11::COMMENTS);
            }

            return json;
        };

        { // First pass - open item templates.
            auto templates = load_file_as_json("db_templates.json");

            for (auto& data_entry : templates.object_items())
            {
                LootItem item;
                item.id = data_entry.first;

                auto props = data_entry.second["_props"];

                item.name = props["Name"].string_value();
                item.value = props["CreditsPrice"].int_value();
                item.lootable = !props["Unlootable"].bool_value();
                item.bundle_path = props["Prefab"]["path"].string_value();
                item.width = props["Width"].int_value();
                item.height = props["Height"].int_value();

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

                item.overriden = false;

                m_db[item.id] = std::move(item);
            }
        }

        { // Second pass - correct name with localization.
            auto locale = load_file_as_json("db_locale.json");
            for (auto& data_entry : locale["templates"].object_items())
            {
                auto iter = m_db.find(data_entry.first);
                if (iter != std::end(m_db))
                {
                    iter->second.name = data_entry.second["Name"].string_value();
                }
            }

        }

        { // Third pass - correct price.
            auto prices = load_file_as_json("db_prices.json");
            for (auto& data_array_entry : prices.array_items())
            {
                auto iter = m_db.find(data_array_entry["template"].string_value());
                if (iter != std::end(m_db))
                {
                    iter->second.value = data_array_entry["price"].int_value();
                }
            }
        }

        { // Fourth pass - manual prices
            auto prices = load_file_as_json("db_manualprices.json");
            for (auto data_entry : prices.object_items())
            {
                auto iter = m_db.find(data_entry.first);
                if (iter != std::end(m_db))
                {
                    iter->second.value = data_entry.second.int_value();
                }
            }
        }

        { // Fifth pass - manual override
            auto overrides = load_file_as_json("db_questlewts.json");
            for (auto data_entry : overrides["questlewts"].array_items())
            {
                auto iter = m_db.find(data_entry.string_value());
                if (iter != std::end(m_db))
                {
                    iter->second.overriden = true;
                }
            }
        }
    }

    LootItem* LootDatabase::query_loot(const std::string& id)
    {
        auto entry = m_db.find(id);
        return entry == std::end(m_db) ? nullptr : &entry->second;
    }

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
            case Polymorph::Type::FoodDrinkComponentDescriptor: return make_polymorph<FoodDrinkComponentDescriptor>(stream, type);
            case Polymorph::Type::ResourceItemComponentDescriptor: return make_polymorph<ResourceItemComponentDescriptor>(stream, type);
            case Polymorph::Type::LightComponentDescriptor: return make_polymorph<LightComponentDescriptor>(stream, type);
            case Polymorph::Type::LockableComponentDescriptor: return make_polymorph<LockableComponentDescriptor>(stream, type);
            case Polymorph::Type::MapComponentDescriptor: return make_polymorph< MapComponentDescriptor>(stream, type);
            case Polymorph::Type::MedKitComponentDescriptor: return make_polymorph<MedKitComponentDescriptor>(stream, type);
            case Polymorph::Type::RepairableComponentDescriptor: return make_polymorph<RepairableComponentDescriptor>(stream, type);
            case Polymorph::Type::SightComponentDescriptor: return make_polymorph<SightComponentDescriptor>(stream, type);
            case Polymorph::Type::TogglableComponentDescriptor: return make_polymorph<TogglableComponentDescriptor>(stream, type);
            case Polymorph::Type::FaceShieldComponentDescriptor: return make_polymorph<FaceShieldComponentDescriptor>(stream, type);
            case Polymorph::Type::FoldableComponentDescriptor: return make_polymorph<FoldableComponentDescriptor>(stream, type);
            case Polymorph::Type::FireModeComponentDescriptor: return make_polymorph<FireModeComponentDescriptor>(stream, type);
            case Polymorph::Type::DogTagComponentDescriptor: return make_polymorph<DogTagComponentDescriptor>(stream, type);
            case Polymorph::Type::TagComponentDescriptor: return make_polymorph<TagComponentDescriptor>(stream, type);
            case Polymorph::Type::KeyComponentDescriptor: return make_polymorph<KeyComponentDescriptor>(stream, type);
            case Polymorph::Type::JsonLootItemDescriptor: return make_polymorph<JsonLootItemDescriptor>(stream, type);
            case Polymorph::Type::JsonCorpseDescriptor: return make_polymorph<JsonCorpseDescriptor>(stream, type);
            case Polymorph::Type::InventorySlotItemAddressDescriptor: return make_polymorph<InventorySlotItemAddressDescriptor>(stream, type);
            case Polymorph::Type::InventoryStackSlotItemAddress: return make_polymorph<InventoryStackSlotItemAddress>(stream, type);
            case Polymorph::Type::InventoryContainerDescriptor: return make_polymorph<InventoryContainerDescriptor>(stream, type);
            case Polymorph::Type::InventoryGridItemAddressDescriptor: return make_polymorph<InventoryGridItemAddressDescriptor>(stream, type);
            case Polymorph::Type::InventoryOwnerItselfDescriptor: return make_polymorph<InventoryOwnerItselfDescriptor>(stream, type);
            case Polymorph::Type::InventoryRemoveOperationDescriptor: return make_polymorph<InventoryRemoveOperationDescriptor>(stream, type);
            case Polymorph::Type::InventoryExamineOperationDescriptor: return make_polymorph<InventoryExamineOperationDescriptor>(stream, type);
            case Polymorph::Type::InventoryCheckMagazineOperationDescriptor: return make_polymorph<InventoryCheckMagazineOperationDescriptor>(stream, type);
            case Polymorph::Type::InventoryBindItemOperationDescriptor: return make_polymorph<InventoryBindItemOperationDescriptor>(stream, type);
            case Polymorph::Type::InventoryMoveOperationDescriptor: return make_polymorph<InventoryMoveOperationDescriptor>(stream, type);
            case Polymorph::Type::InventorySplitOperationDescriptor: return make_polymorph<InventorySplitOperationDescriptor>(stream, type);
            case Polymorph::Type::InventoryMergeOperationDescriptor: return make_polymorph<InventoryMergeOperationDescriptor>(stream, type);
            case Polymorph::Type::InventoryTransferOperationDescriptor: return make_polymorph<InventoryTransferOperationDescriptor>(stream, type);
            case Polymorph::Type::InventorySwapOperationDescriptor: return make_polymorph<InventorySwapOperationDescriptor>(stream, type);
            case Polymorph::Type::InventoryThrowOperationDescriptor: return make_polymorph<InventoryThrowOperationDescriptor>(stream, type);
            case Polymorph::Type::InventoryToggleOperationDescriptor: return make_polymorph<InventoryToggleOperationDescriptor>(stream, type);
            case Polymorph::Type::InventoryFoldOperationDescriptor: return make_polymorph<InventoryFoldOperationDescriptor>(stream, type);
            case Polymorph::Type::InventoryShotOperationDescriptor: return make_polymorph<InventoryShotOperationDescriptor>(stream, type);
            case Polymorph::Type::SetupItemOperationDescriptor: return make_polymorph<SetupItemOperationDescriptor>(stream, type);
            case Polymorph::Type::ApplyHealthOperationDescriptor: return make_polymorph<ApplyHealthOperationDescriptor>(stream, type);
            case Polymorph::Type::OperateStationaryWeaponOperationDescription: return make_polymorph<OperateStationaryWeaponOperationDescription>(stream, type);
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
        
        int num = stream->ReadInt32();
        for(int i = 0; i < num; i++)
        {
            // Dumping these values for now
            auto dump = stream->ReadInt32();
        }
 
        int num2 = stream->ReadInt32();
        for (int i = 0; i < num2; i++)
        {
            // Dumping these values for now
            auto dump = stream->ReadInt32();
        }
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

    void InventorySlotItemAddressDescriptor::read(CSharpByteStream* stream)
    {
        container.read(stream);
    }

    void InventoryStackSlotItemAddress::read(CSharpByteStream* stream)
    {
        container.read(stream);
    }

    void InventoryContainerDescriptor::read(CSharpByteStream* stream)
    {
        parent_id = stream->ReadString();
        container_id = stream->ReadString();
    }

    void InventoryGridItemAddressDescriptor::read(CSharpByteStream* stream)
    {
        location_in_grid.read(stream);
        container.read(stream);
    }

    void InventoryOwnerItselfDescriptor::read(CSharpByteStream* stream)
    {
        container.read(stream);
    }

    void InventoryRemoveOperationDescriptor::read(CSharpByteStream* stream)
    {
        item_id = stream->ReadString();
        operation_id = stream->ReadUInt16();
    }

    void InventoryExamineOperationDescriptor::read(CSharpByteStream* stream)
    {
        item_id = stream->ReadString();
        operation_id = stream->ReadUInt16();
    }

    void InventoryCheckMagazineOperationDescriptor::read(CSharpByteStream* stream)
    {
        item_id = stream->ReadString();
        check_status = stream->ReadBool();
        skill_level = stream->ReadInt32();
        operation_id = stream->ReadUInt16();
    }

    void InventoryBindItemOperationDescriptor::read(CSharpByteStream* stream)
    {
        item_id = stream->ReadString();
        index = stream->ReadInt32();
        operation_id = stream->ReadUInt16();
    }

    void InventoryMoveOperationDescriptor::read(CSharpByteStream* stream)
    {
        item_id = stream->ReadString();
        from = read_polymorph(stream);
        to = read_polymorph(stream);
        operation_id = stream->ReadUInt16();
    }

    void InventorySplitOperationDescriptor::read(CSharpByteStream* stream)
    {
        item_id = stream->ReadString();
        from = read_polymorph(stream);
        to = read_polymorph(stream);
        count = stream->ReadInt32();
        operation_id = stream->ReadUInt16();
    }

    void InventoryMergeOperationDescriptor::read(CSharpByteStream* stream)
    {
        item_id = stream->ReadString();
        item1_id = stream->ReadString();
        operation_id = stream->ReadUInt16();
    }

    void InventoryTransferOperationDescriptor::read(CSharpByteStream* stream)
    {
        item_id = stream->ReadString();
        item1_id = stream->ReadString();
        count = stream->ReadInt32();
        operation_id = stream->ReadUInt16();
    }

    void InventorySwapOperationDescriptor::read(CSharpByteStream* stream)
    {
        item_id = stream->ReadString();
        to = read_polymorph(stream);
        item1_id = stream->ReadString();
        to1 = read_polymorph(stream);
        operation_id = stream->ReadUInt16();
    }

    void InventoryThrowOperationDescriptor::read(CSharpByteStream* stream)
    {
        item_id = stream->ReadString();
        operation_id = stream->ReadUInt16();
    }

    void InventoryToggleOperationDescriptor::read(CSharpByteStream* stream)
    {
        item_id = stream->ReadString();
        value = stream->ReadBool();
        operation_id = stream->ReadUInt16();
    }

    void InventoryFoldOperationDescriptor::read(CSharpByteStream* stream)
    {
        item_id = stream->ReadString();
        value = stream->ReadBool();
        operation_id = stream->ReadUInt16();
    }

    void InventoryShotOperationDescriptor::read(CSharpByteStream* stream)
    {
        item_id = stream->ReadString();
        operation_id = stream->ReadUInt16();
    }

    void SetupItemOperationDescriptor::read(CSharpByteStream* stream)
    {
        item_id = stream->ReadString();
        zone_id = stream->ReadString();
        position = stream->ReadVector();
        rotation = stream->ReadQuaternion();
        setup_time = stream->ReadSingle();
        operation_id = stream->ReadUInt16();
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
