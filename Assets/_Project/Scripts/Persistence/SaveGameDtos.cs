using System;
using System.Collections.Generic;

[Serializable]
public sealed class SaveGameData
{
    public int schemaVersion;
    public string savedAtUtc;
    public string sceneName;
    public PlayerSaveData player = new();
    public WorldSaveData world = new();
}

[Serializable]
public sealed class PlayerSaveData
{
    public SerializableVector3Data position = new();
    public SerializableQuaternionData rotation = new();
    public int health;
    public float stamina;
    public string lastSanctuaryId;
    public List<ItemInstanceSaveData> inventoryItems = new();
    public List<EquipmentSlotSaveData> equipmentSlots = new();
    public int activeWeaponSlot;
    public string activeItemInstanceId;
}

[Serializable]
public sealed class WorldSaveData
{
    public List<WorldContainerSaveData> containers = new();
    public List<WorldPickupSaveData> pickups = new();
    public List<WorldDoorSaveData> doors = new();
}

[Serializable]
public sealed class ItemInstanceSaveData
{
    public string instanceId;
    public string definitionId;
    public int quantity;
    public float currentDurability;
    public float maxDurability;
    public int quality;
    public int rarity;
    public string creatorId;
    public string affinityId;
    public bool isStolen;
    public string originalOwnerId;
}

[Serializable]
public sealed class EquipmentSlotSaveData
{
    public int slotType;
    public string itemInstanceId;
}

[Serializable]
public sealed class WorldContainerSaveData
{
    public string worldId;
    public bool initializedOrRestored;
    public List<ItemInstanceSaveData> items = new();
}

[Serializable]
public sealed class WorldPickupSaveData
{
    public string worldId;
    public bool collected;
}

[Serializable]
public sealed class WorldDoorSaveData
{
    public string worldId;
    public bool isOpen;
}

[Serializable]
public sealed class SerializableVector3Data
{
    public float x;
    public float y;
    public float z;
}

[Serializable]
public sealed class SerializableQuaternionData
{
    public float x;
    public float y;
    public float z;
    public float w = 1f;
}
