#pragma once

#include "common.hpp"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace tk
{
    struct LootItem
    {
        enum Rarity
        {
            Common,
            Rare,
            SuperRare,
            NotExist
        };

        std::string id;
        std::string name;
        int value;
        bool lootable;
        Rarity rarity;
        std::string bundle_path;
        int width;
        int height;
        bool overriden;
    };

    class LootDatabase
    {
    public:
        LootDatabase();
        LootItem* query_loot(const std::string& id);

    private:
        std::unordered_map<std::string, LootItem> m_db;
    };

    struct Polymorph
    {
        virtual ~Polymorph() {}
        enum class Type
        {
            FoodDrinkComponentDescriptor = 13,
            ResourceItemComponentDescriptor = 14,
            LightComponentDescriptor = 15,
            LockableComponentDescriptor = 16,
            MapComponentDescriptor = 17,
            MedKitComponentDescriptor = 18,
            RepairableComponentDescriptor = 19,
            SightComponentDescriptor = 20,
            TogglableComponentDescriptor = 21,
            FaceShieldComponentDescriptor = 22,
            FoldableComponentDescriptor = 23,
            FireModeComponentDescriptor = 24,
            DogTagComponentDescriptor = 25,
            TagComponentDescriptor = 26,
            KeyComponentDescriptor = 27,
            JsonLootItemDescriptor = 28,
            JsonCorpseDescriptor = 29,
            InventorySlotItemAddressDescriptor = 32,
            InventoryStackSlotItemAddress = 33,
            InventoryContainerDescriptor = 34,
            InventoryGridItemAddressDescriptor = 35,
            InventoryOwnerItselfDescriptor = 36,
            InventoryRemoveOperationDescriptor = 39,
            InventoryExamineOperationDescriptor = 40,
            InventoryCheckMagazineOperationDescriptor = 41,
            InventoryBindItemOperationDescriptor = 42,
            InventoryMoveOperationDescriptor = 45,
            InventorySplitOperationDescriptor = 47,
            InventoryMergeOperationDescriptor = 48,
            InventoryTransferOperationDescriptor = 49,
            InventorySwapOperationDescriptor = 50,
            InventoryThrowOperationDescriptor = 51,
            InventoryToggleOperationDescriptor = 52,
            InventoryFoldOperationDescriptor = 53,
            InventoryShotOperationDescriptor = 54,
            SetupItemOperationDescriptor = 55,
            ApplyHealthOperationDescriptor = 57,
            OperateStationaryWeaponOperationDescription = 65
        } type;
    };

    class CSharpByteStream;

    std::unique_ptr<Polymorph> read_polymorph(CSharpByteStream* stream);
    std::vector<std::unique_ptr<Polymorph>> read_polymorphs(uint8_t* data, int size);

    struct FoodDrinkComponentDescriptor : public Polymorph
    {
        float hp_percent; // HpPercent = reader.ReadSingle()
        void read(CSharpByteStream* stream);
    };

    struct ResourceItemComponentDescriptor : public Polymorph
    {
        float resource; // Resource = reader.ReadSingle()
        void read(CSharpByteStream* stream);
    };

    struct LightComponentDescriptor : public Polymorph
    {
        bool active; // IsActive = reader.ReadBoolean()
        int mode; // SelectedMode = reader.ReadInt32()
        void read(CSharpByteStream* stream);
    };

    struct LockableComponentDescriptor : public Polymorph
    {
        bool locked; // Locked = reader.ReadBoolean()
        void read(CSharpByteStream* stream);
    };

    struct InventoryLogicMapMarker
    {
        int type; // (MapMarkerType)reader.ReadInt32(),
        int x; // reader.ReadInt32(),
        int y; // reader.ReadInt32(),
        std::string note; // reader.ReadString()
        void read(CSharpByteStream* stream);
    };

    struct MapComponentDescriptor : public Polymorph
    {
        // int num = reader.ReadInt32();
        // gclass.Markers = new List<MapMarker>(num);
        // for (int i = 0; i < num; i++)
        // {
        //     gclass.Markers.Add(reader.ReadEFTInventoryLogicMapMarker());
        // }
        std::vector<InventoryLogicMapMarker> markers;
        void read(CSharpByteStream* stream);
    };

    struct MedKitComponentDescriptor : public Polymorph
    {
        float hp; // HpResource = reader.ReadSingle()
        void read(CSharpByteStream* stream);
    };

    struct RepairableComponentDescriptor : public Polymorph
    {
        float durability; // Durability = reader.ReadSingle()
        float max_durability; // MaxDurability = reader.ReadSingle()
        void read(CSharpByteStream* stream);
    };

    struct SightComponentDescriptor : public Polymorph
    {
        int sight_mode; // SelectedSightMode = reader.ReadInt32()
        void read(CSharpByteStream* stream);
    };

    struct TogglableComponentDescriptor : public Polymorph
    {
        bool on; // IsOn = reader.ReadBoolean()
        void read(CSharpByteStream* stream);
    };

    struct FaceShieldComponentDescriptor : public Polymorph
    {
        uint8_t hits; // Hits = reader.ReadByte(),
        uint8_t hit_seed; // HitSeed = reader.ReadByte()
        void read(CSharpByteStream* stream);
    };

    struct FoldableComponentDescriptor : public Polymorph
    {
        bool folded; // Folded = reader.ReadBoolean()
        void read(CSharpByteStream* stream);
    };

    struct FireModeComponentDescriptor : public Polymorph
    {
        int fire_mode; // FireMode = (Weapon.1ireMode)reader.ReadInt32()
        void read(CSharpByteStream* stream);
    };

    struct DogTagComponentDescriptor : public Polymorph
    {
        std::string name; // Nickname = reader.ReadString(),
        int side; // Side = (EPlayerSide)reader.ReadInt32(),
        int level; // Level = reader.ReadInt32(),
        double time; // Time = reader.ReadDouble(),
        std::string status; // Status = reader.ReadString(),
        std::string killer_name; // KillerName = reader.ReadString(),
        std::string weapon_name; // WeaponName = reader.ReadString()
        void read(CSharpByteStream* stream);
    };

    struct TagComponentDescriptor : public Polymorph
    {
        std::string name; // Name = reader.ReadString(),
        int colour; // Color = reader.ReadInt32()
        void read(CSharpByteStream* stream);
    };

    struct KeyComponentDescriptor : public Polymorph
    {
        int uses; // NumberOfUsages = reader.ReadInt32()
        void read(CSharpByteStream* stream);
    };

    struct ItemDescriptor;

    struct SlotDescriptor
    {
        std::string id;  // Id = reader.ReadString(),
        std::unique_ptr<ItemDescriptor> contained_item; // ContainedItem = reader.ReadEFTItemDescriptor()
        void read(CSharpByteStream* stream);
    };

    struct LocationInGrid
    {
        int x; // x = reader.ReadInt32(),
        int y; // y = reader.ReadInt32(),
        int rotation; // r = (ItemRotation)reader.ReadInt32(),
        bool searched; // isSearched = reader.ReadBoolean()
        void read(CSharpByteStream* stream);
    };

    struct ItemInGridDescriptor
    {
        LocationInGrid location; //Location = reader.ReadLocationInGrid(),
        std::unique_ptr<ItemDescriptor> item; //Item = reader.ReadEFTItemDescriptor()
        void read(CSharpByteStream* stream);
    };

    struct GridDescriptor
    {
        std::string id; // gclass.Id = reader.ReadString();

        // int num = reader.ReadInt32();
        // gclass.ContainedItems = new List<GClass884>(num);
        // for (int i = 0; i < num; i++)
        // {
        //     gclass.ContainedItems.Add(reader.ReadEFTItemInGridDescriptor());
        // }
        std::vector<ItemInGridDescriptor> items;
        void read(CSharpByteStream* stream);
    };

    struct StackSlotDescriptor
    {
        std::string id; // gclass.Id = reader.ReadString();

        // int num = reader.ReadInt32();
        // gclass.ContainedItems = new List<GClass887>(num);
        // for (int i = 0; i < num; i++)
        // {
        //     gclass.ContainedItems.Add(reader.ReadEFTItemDescriptor());
        // }
        std::vector<std::unique_ptr<ItemDescriptor>> items;
        void read(CSharpByteStream* stream);
    };

    struct ItemDescriptor
    {
        std::string id; // gclass.Id = reader.ReadString();
        std::string template_id; // gclass.TemplateId = reader.ReadString();
        int stack_count; // gclass.StackCount = reader.ReadInt32();
        bool spawned_in_session; // gclass.SpawnedInSession = reader.ReadBoolean();

        // int num = reader.ReadInt32();
        // gclass.Components = new List<GClass888>(num);
        // for (int i = 0; i < num; i++)
        // {
        //     gclass.Components.Add(reader.ReadPolymorph<GClass888>());
        // }
        std::vector<std::unique_ptr<Polymorph>> components;

        // int num2 = reader.ReadInt32();
        // gclass.Slots = new List<GClass883>(num2);
        // for (int j = 0; j < num2; j++)
        // {
        //     gclass.Slots.Add(reader.ReadEFTSlotDescriptor());
        // }
        std::vector<SlotDescriptor> slots;

        // int num3 = reader.ReadInt32();
        // gclass.Grids = new List<GClass885>(num3);
        // for (int k = 0; k < num3; k++)
        // {
        //     gclass.Grids.Add(reader.ReadEFTGridDescriptor());
        // }
        std::vector<GridDescriptor> grids;

        // int num4 = reader.ReadInt32();
        // gclass.StackSlots = new List<GClass886>(num4);
        // for (int l = 0; l < num4; l++)
        // {
        //     gclass.StackSlots.Add(reader.ReadEFTStackSlotDescriptor());
        // }
        std::vector<StackSlotDescriptor> stack_slots;
        void read(CSharpByteStream* stream);
    };

    struct JsonLootItemDescriptor : public Polymorph
    {
        // if (reader.ReadBoolean())
        // {
        //     gclass.Id = reader.ReadString();
        // }
        std::string id;

        Vector3 position; // gclass.Position = reader.ReadClassVector3();
        Vector3 rotation; // gclass.Rotation = reader.ReadClassVector3();
        ItemDescriptor item; // gclass.Item = reader.ReadEFTItemDescriptor();

        // if (reader.ReadBoolean())
        // {
        //     int num = reader.ReadInt32();
        //     gclass.ValidProfiles = new string[num];
        //     for (int i = 0; i < num; i++)
        //     {
        //         gclass.ValidProfiles[i] = reader.ReadString();
        //     }
        // }
        std::vector<std::string> profiles;

        bool is_static; // gclass.IsStatic = reader.ReadBoolean();
        bool use_gravity; // gclass.UseGravity = reader.ReadBoolean();
        bool random_rotation; // gclass.RandomRotation = reader.ReadBoolean();
        Vector3 shift; // gclass.Shift = reader.ReadClassVector3();
        int16_t platform_id; // gclass.PlatformId = reader.ReadInt16();
        void read(CSharpByteStream* stream);
    };

    struct ClassTransformSync
    {
        Vector3 position; // Position = reader.ReadClassVector3(),
        Quaternion rotation; // Position = reader.ReadClassVector3(),
        void read(CSharpByteStream* stream);
    };

    struct JsonCorpseDescriptor : public Polymorph
    {
        // int num = reader.ReadInt32();
        // gclass.Customization = new Dictionary<int, string>();
        // for (int i = 0; i < num; i++)
        // {
        //     gclass.Customization[reader.ReadInt32()] = reader.ReadString();
        // }
        std::unordered_map<int, std::string> customization;

        int side; // gclass.Side = (EPlayerSide)reader.ReadInt32();

        // int num2 = reader.ReadInt32();
        // gclass.Bones = new ClassTransformSync[num2];
        // for (int j = 0; j < num2; j++)
        // {
        //     gclass.Bones[j] = reader.ReadClassTransformSync();
        // }
        std::vector<ClassTransformSync> bones;

        // if (reader.ReadBoolean())
        // {
        //     gclass.Id = reader.ReadString();
        // }
        std::string id;

        Vector3 position; // gclass.Position = reader.ReadClassVector3();
        Vector3 rotation; // gclass.Rotation = reader.ReadClassVector3();
        ItemDescriptor item;  // gclass.Item = reader.ReadEFTItemDescriptor();

        // if (reader.ReadBoolean())
        // {
        //     int num3 = reader.ReadInt32();
        //     gclass.ValidProfiles = new string[num3];
        //     for (int k = 0; k < num3; k++)
        //     {
        //         gclass.ValidProfiles[k] = reader.ReadString();
        //     }
        // }
        std::vector<std::string> profiles;
        bool is_static; // gclass.IsStatic = reader.ReadBoolean();
        bool use_gravity; // gclass.UseGravity = reader.ReadBoolean();
        bool random_rotation; // gclass.RandomRotation = reader.ReadBoolean();
        Vector3 shift; // gclass.Shift = reader.ReadClassVector3();
        int16_t platform_id; // gclass.PlatformId = reader.ReadInt16();
        void read(CSharpByteStream* stream);
    };

    struct InventoryContainerDescriptor : public Polymorph
    {
        std::string parent_id; // ParentId = reader.ReadString(),
        std::string container_id; // ContainerId = reader.ReadString()
        void read(CSharpByteStream* stream);
    };

    struct InventorySlotItemAddressDescriptor : public Polymorph
    {
        InventoryContainerDescriptor container; // Container = reader.ReadEFTContainerDescriptor()
        void read(CSharpByteStream* stream);
    };

    struct InventoryStackSlotItemAddress : public Polymorph
    {
        InventoryContainerDescriptor container; // Container = reader.ReadEFTContainerDescriptor()
        void read(CSharpByteStream* stream);
    };

    struct InventoryGridItemAddressDescriptor : public Polymorph
    {
        LocationInGrid location_in_grid; // LocationInGrid = reader.ReadLocationInGrid(),
        InventoryContainerDescriptor container; // Container = reader.ReadEFTContainerDescriptor()
        void read(CSharpByteStream* stream);
    };

    struct InventoryOwnerItselfDescriptor : public Polymorph
    {
        InventoryContainerDescriptor container; // Container = reader.ReadEFTContainerDescriptor()
        void read(CSharpByteStream* stream);
    };

    struct InventoryRemoveOperationDescriptor : public Polymorph
    {
        std::string item_id; // ItemId = reader.ReadString(),
        uint16_t operation_id; // OperationId = reader.ReadUInt16()
        void read(CSharpByteStream* stream);
    };

    struct InventoryExamineOperationDescriptor : public Polymorph
    {
        std::string item_id; // ItemId = reader.ReadString(),
        uint16_t operation_id; // OperationId = reader.ReadUInt16()
        void read(CSharpByteStream* stream);
    };

    struct InventoryCheckMagazineOperationDescriptor : public Polymorph
    {
        std::string item_id; // ItemId = reader.ReadString(),
        bool check_status; // CheckStatus = reader.ReadBoolean(),
        int skill_level; // SkillLevel = reader.ReadInt32(),
        uint16_t operation_id; // OperationId = reader.ReadUInt16()
        void read(CSharpByteStream* stream);
    };

    struct InventoryBindItemOperationDescriptor : public Polymorph
    {
        std::string item_id; // ItemId = reader.ReadString(),
        int index; // Index = (EBoundItem)reader.ReadInt32(),
        uint16_t operation_id; // OperationId = reader.ReadUInt16()
        void read(CSharpByteStream* stream);
    };

    struct InventoryMoveOperationDescriptor : public Polymorph
    {
        std::string item_id; // ItemId = reader.ReadString(),
        std::unique_ptr<Polymorph> from; // From = reader.ReadPolymorph<GClass908>(),
        std::unique_ptr<Polymorph> to;// To = reader.ReadPolymorph<GClass908>(),
        uint16_t operation_id; // OperationId = reader.ReadUInt16()
        void read(CSharpByteStream* stream);
    };

    struct InventorySplitOperationDescriptor : public Polymorph
    {
        std::string item_id; // ItemId = reader.ReadString(),
        std::unique_ptr<Polymorph> from; // From = reader.ReadPolymorph<GClass908>(),
        std::unique_ptr<Polymorph> to; // To = reader.ReadPolymorph<GClass908>(),
        int count; // Count = reader.ReadInt32(),
        uint16_t operation_id; // OperationId = reader.ReadUInt16()
        void read(CSharpByteStream* stream);
    };

    struct InventoryMergeOperationDescriptor : public Polymorph
    {
        std::string item_id; // ItemId = reader.ReadString(),
        std::string item1_id; // Item1Id = reader.ReadString(),
        uint16_t operation_id; // OperationId = reader.ReadUInt16()
        void read(CSharpByteStream* stream);
    };

    struct InventoryTransferOperationDescriptor : public Polymorph
    {
        std::string item_id; // ItemId = reader.ReadString(),
        std::string item1_id; // Item1Id = reader.ReadString(),
        int count; // Count = reader.ReadInt32(),
        uint16_t operation_id; // OperationId = reader.ReadUInt16()
        void read(CSharpByteStream* stream);
    };

    struct InventorySwapOperationDescriptor : public Polymorph
    {
        std::string item_id; // ItemId = reader.ReadString(),
        std::unique_ptr<Polymorph> to; // To = reader.ReadPolymorph<GClass908>(),
        std::string item1_id; // Item1Id = reader.ReadString(),
        std::unique_ptr<Polymorph> to1; // To1 = reader.ReadPolymorph<GClass908>(),
        uint16_t operation_id; // OperationId = reader.ReadUInt16()
        void read(CSharpByteStream* stream);
    };

    struct InventoryThrowOperationDescriptor : public Polymorph
    {
        std::string item_id; // ItemId = reader.ReadString(),
        uint16_t operation_id; // OperationId = reader.ReadUInt16()
        void read(CSharpByteStream* stream);
    };

    struct InventoryToggleOperationDescriptor : public Polymorph
    {
        std::string item_id; // ItemId = reader.ReadString(),
        bool value; // Value = reader.ReadBoolean(),
        uint16_t operation_id; // OperationId = reader.ReadUInt16()
        void read(CSharpByteStream* stream);
    };

    struct InventoryFoldOperationDescriptor : public Polymorph
    {
        std::string item_id; // ItemId = reader.ReadString(),
        bool value; // Value = reader.ReadBoolean(),
        uint16_t operation_id; // OperationId = reader.ReadUInt16()
        void read(CSharpByteStream* stream);
    };

    struct InventoryShotOperationDescriptor : public Polymorph
    {
        std::string item_id; // ItemId = reader.ReadString(),
        uint16_t operation_id; // OperationId = reader.ReadUInt16()
        void read(CSharpByteStream* stream);
    };

    struct SetupItemOperationDescriptor : public Polymorph
    {
        std::string item_id; // ItemId = reader.ReadString(),
        std::string zone_id; // ZoneId = reader.ReadString(),
        Vector3 position; // Position = reader.ReadClassVector3(),
        Quaternion rotation; // Rotation = reader.ReadClassQuaternion(),
        float setup_time; // SetupTime = reader.ReadSingle(),
        uint16_t operation_id; // OperationId = reader.ReadUInt16()
        void read(CSharpByteStream* stream);
    };

    struct ApplyHealthOperationDescriptor : public Polymorph
    {
        std::string item_id; // ItemId = reader.ReadString(),
        int body_part; // BodyPart = reader.ReadInt32(),
        int count; // Count = reader.ReadInt32(),
        uint16_t operation_id; // OperationId = reader.ReadUInt16()
        void read(CSharpByteStream* stream);
    };

    struct OperateStationaryWeaponOperationDescription : public Polymorph
    {
        std::string weapon_id; // WeaponId = reader.ReadString(),
        uint16_t operation_id; // OperationId = reader.ReadUInt16()
        void read(CSharpByteStream* stream);
    };

    class CSharpByteStream // too much mirrored with the other stream type...
    {
    public:
        CSharpByteStream(uint8_t* data, int len)
            : m_data(data), m_pos(0), m_len(len)
        { }

        uint8_t ReadByte()
        {
            return m_data[m_pos++];
        }

        uint16_t ReadUInt16()
        {
            return (uint16_t)ReadInt16();
        }

        int16_t ReadInt16()
        {
            int16_t data;
            memcpy(&data, m_data + m_pos, 2);
            m_pos += 2;
            return data;
        }

        int32_t ReadInt32()
        {
            int32_t data;
            memcpy(&data, m_data + m_pos, 4);
            m_pos += 4;
            return data;
        }

        bool ReadBool()
        {
            return ReadByte() > 0;
        }

        float ReadSingle()
        {
            float data;
            memcpy(&data, m_data + m_pos, 4);
            m_pos += 4;
            return data;
        }

        double ReadDouble()
        {
            double data;
            memcpy(&data, m_data + m_pos, 8);
            m_pos += 8;
            return data;
        }

        int Read7BitEncodedInt()
        {
            int num = 0;
            int num2 = 0;
            while (num2 != 35)
            {
                uint8_t b = ReadByte();
                num |= (b & 127) << num2;
                num2 += 7;
                if ((b & 128) == 0)
                {
                    return num;
                }
            }
            return 0;
        }

        std::string ReadString()
        {
            int len = Read7BitEncodedInt();

            std::string ret;
            for (int i = 0; i < len; ++i)
            {
                ret.push_back(ReadByte());
            }
            return ret;
        }

        Vector3 ReadVector()
        {
            return { ReadSingle(), ReadSingle(), ReadSingle() };
        }

        Quaternion ReadQuaternion()
        {
            return { ReadSingle(), ReadSingle(), ReadSingle(), ReadSingle() };
        }

        uint8_t* m_data;
        int m_pos;
        int m_len;
    };
}
