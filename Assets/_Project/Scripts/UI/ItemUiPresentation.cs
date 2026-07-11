using System;
using UnityEngine;

public static class ItemUiPresentation
{
    public static Color GetRarityColor(ItemRarity rarity)
    {
        return rarity switch
        {
            ItemRarity.Uncommon => new Color(0.32f, 0.68f, 0.3f, 1f),
            ItemRarity.Rare => new Color(0.28f, 0.48f, 0.9f, 1f),
            ItemRarity.Epic => new Color(0.62f, 0.34f, 0.82f, 1f),
            ItemRarity.Legendary => new Color(0.9f, 0.52f, 0.16f, 1f),
            _ => new Color(0.58f, 0.54f, 0.46f, 1f)
        };
    }

    public static string GetRarityShortName(ItemRarity rarity)
    {
        return rarity switch
        {
            ItemRarity.Uncommon => "UNCOMMON",
            ItemRarity.Rare => "RARE",
            ItemRarity.Epic => "EPIC",
            ItemRarity.Legendary => "LEGEND",
            _ => "COMMON"
        };
    }

    public static string GetQualityShortName(ItemQuality quality)
    {
        return quality switch
        {
            ItemQuality.Crude => "CRUDE",
            ItemQuality.Fine => "FINE",
            ItemQuality.Masterwork => "MASTER",
            _ => "STANDARD"
        };
    }

    public static string GetItemMonogram(string displayName)
    {
        if (string.IsNullOrWhiteSpace(displayName))
        {
            return "?";
        }

        string[] words = displayName.Split(new[] { ' ', '-', '_' }, StringSplitOptions.RemoveEmptyEntries);
        if (words.Length >= 2)
        {
            return $"{char.ToUpperInvariant(words[0][0])}{char.ToUpperInvariant(words[1][0])}";
        }

        string word = words.Length == 1 ? words[0] : displayName;
        return word.Length >= 2
            ? word.Substring(0, 2).ToUpperInvariant()
            : word.ToUpperInvariant();
    }

    public static string GetDescription(ItemInstance item, string emptyText)
    {
        if (item?.Definition == null)
        {
            return emptyText;
        }

        return string.IsNullOrWhiteSpace(item.Definition.Description)
            ? "No description has been written for this item yet."
            : item.Definition.Description;
    }

    public static string GetTypeText(ItemInstance item)
    {
        return item?.Definition != null ? $"TYPE  {item.Definition.Category}" : "TYPE  —";
    }

    public static string GetRarityQualityText(ItemInstance item)
    {
        return item?.Definition != null
            ? $"RARITY  {item.Rarity}    QUALITY  {item.Quality}"
            : "RARITY  —    QUALITY  —";
    }

    public static string GetQuantityText(ItemInstance item)
    {
        return item?.Definition != null ? $"QUANTITY  {item.Quantity}" : "QUANTITY  —";
    }

    public static string GetDurabilityText(ItemInstance item)
    {
        return item?.Definition != null && item.MaxDurability > 0f
            ? $"DURABILITY  {item.CurrentDurability:0}/{item.MaxDurability:0}"
            : "DURABILITY  —";
    }

    public static string GetDamageText(ItemInstance item)
    {
        if (item?.Definition is not WeaponData weaponData)
        {
            return string.Empty;
        }

        return GetWeaponDamageText(weaponData);
    }

    public static string GetMetadataText(ItemInstance item)
    {
        if (item?.Definition == null)
        {
            return string.Empty;
        }

        string metadata = string.Empty;
        if (!string.IsNullOrWhiteSpace(item.CreatorId))
        {
            metadata = $"CREATOR  {item.CreatorId}";
        }

        if (!string.IsNullOrWhiteSpace(item.AffinityId))
        {
            metadata += string.IsNullOrEmpty(metadata)
                ? $"AFFINITY  {item.AffinityId}"
                : $"    AFFINITY  {item.AffinityId}";
        }

        return metadata;
    }

    public static string GetSlotDisplayName(EquipmentSlotType slotType)
    {
        return slotType switch
        {
            EquipmentSlotType.MainHand => "Main Hand / Right",
            EquipmentSlotType.OffHand => "Off Hand / Left",
            EquipmentSlotType.Accessory1 => "Accessory I",
            EquipmentSlotType.Accessory2 => "Accessory II",
            EquipmentSlotType.AxeTool => "Axe Tool",
            EquipmentSlotType.PickaxeTool => "Pickaxe Tool",
            EquipmentSlotType.KnifeTool => "Knife Tool",
            _ => slotType.ToString()
        };
    }

    public static string GetSlotMonogram(EquipmentSlotType slotType)
    {
        return slotType switch
        {
            EquipmentSlotType.MainHand => "RH",
            EquipmentSlotType.OffHand => "LH",
            EquipmentSlotType.Head => "HD",
            EquipmentSlotType.Chest => "CH",
            EquipmentSlotType.Hands => "HN",
            EquipmentSlotType.Legs => "LG",
            EquipmentSlotType.Feet => "FT",
            EquipmentSlotType.Accessory1 => "A1",
            EquipmentSlotType.Accessory2 => "A2",
            EquipmentSlotType.AxeTool => "AX",
            EquipmentSlotType.PickaxeTool => "PK",
            EquipmentSlotType.KnifeTool => "KN",
            _ => "--"
        };
    }

    private static string GetWeaponDamageText(WeaponData weaponData)
    {
        if (weaponData?.attacks == null || weaponData.attacks.Length == 0)
        {
            return "DAMAGE  —";
        }

        int minDamage = int.MaxValue;
        int maxDamage = int.MinValue;
        int attackCount = 0;
        foreach (WeaponAttackData attack in weaponData.attacks)
        {
            if (attack == null)
            {
                continue;
            }

            minDamage = Mathf.Min(minDamage, attack.damage);
            maxDamage = Mathf.Max(maxDamage, attack.damage);
            attackCount++;
        }

        if (attackCount == 0)
        {
            return "DAMAGE  —";
        }

        string damage = minDamage == maxDamage ? minDamage.ToString() : $"{minDamage}–{maxDamage}";
        return $"DAMAGE  {damage}    COMBO  {Mathf.Min(weaponData.maxComboSteps, attackCount)} steps";
    }
}
