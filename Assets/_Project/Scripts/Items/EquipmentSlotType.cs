using System;
using UnityEngine;

public enum EquipmentSlotType
{
    MainHand = 0,
    OffHand = 1,
    Head = 2,
    Chest = 3,
    Hands = 4,
    Legs = 5,
    Feet = 6,
    Accessory1 = 7,
    Accessory2 = 8,
    AxeTool = 9,
    PickaxeTool = 10,
    KnifeTool = 11
}

[Flags]
public enum EquipmentSlotMask
{
    None = 0,
    MainHand = 1 << (int)EquipmentSlotType.MainHand,
    OffHand = 1 << (int)EquipmentSlotType.OffHand,
    Head = 1 << (int)EquipmentSlotType.Head,
    Chest = 1 << (int)EquipmentSlotType.Chest,
    Hands = 1 << (int)EquipmentSlotType.Hands,
    Legs = 1 << (int)EquipmentSlotType.Legs,
    Feet = 1 << (int)EquipmentSlotType.Feet,
    Accessory1 = 1 << (int)EquipmentSlotType.Accessory1,
    Accessory2 = 1 << (int)EquipmentSlotType.Accessory2,
    AxeTool = 1 << (int)EquipmentSlotType.AxeTool,
    PickaxeTool = 1 << (int)EquipmentSlotType.PickaxeTool,
    KnifeTool = 1 << (int)EquipmentSlotType.KnifeTool,
    WeaponHands = MainHand | OffHand
}

public enum EquipmentChangeFailure
{
    None,
    InvalidItem,
    NotOwned,
    IncompatibleSlot,
    AlreadyEquipped,
    SlotEmpty,
    ActionLocked,
    MissingVisual
}

[Serializable]
public sealed class EquipmentSlotState
{
    [SerializeField] private EquipmentSlotType slotType;
    [SerializeField] private string itemInstanceId;

    public EquipmentSlotType SlotType => slotType;
    public string ItemInstanceId => itemInstanceId;
    public bool IsEmpty => string.IsNullOrWhiteSpace(itemInstanceId);

    public EquipmentSlotState(EquipmentSlotType type)
    {
        slotType = type;
        itemInstanceId = string.Empty;
    }

    internal void SetItem(ItemInstance item)
    {
        itemInstanceId = item != null ? item.InstanceId : string.Empty;
    }

    internal void ClearInvalidReference()
    {
        itemInstanceId = string.Empty;
    }
}
