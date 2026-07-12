using UnityEngine;

public enum ItemCategory
{
    Weapon,
    Armor,
    Consumable,
    Material,
    Tool,
    Other
}

public abstract class ItemDefinition : ScriptableObject
{
    [Header("Item Definition")]
    [SerializeField] private string definitionId;
    [SerializeField, TextArea(2, 5)] private string description;
    [SerializeField] private Sprite icon;
    [SerializeField] private ItemCategory category = ItemCategory.Other;
    [SerializeField, Min(1)] private int maxStackSize = 1;
    [SerializeField, Min(0f)] private float defaultMaxDurability = 100f;

    [Header("Equipment Compatibility")]
    [SerializeField] private EquipmentSlotMask acceptedEquipmentSlots = EquipmentSlotMask.None;

    public string DefinitionId => definitionId != null ? definitionId.Trim() : string.Empty;
    public abstract string DisplayName { get; }
    public string Description => description ?? string.Empty;
    public Sprite Icon => icon;
    public virtual ItemCategory Category => category;
    public int MaxStackSize => Mathf.Max(1, maxStackSize);
    public float DefaultMaxDurability => Mathf.Max(0f, defaultMaxDurability);
    public virtual EquipmentSlotMask AcceptedEquipmentSlots => acceptedEquipmentSlots;

    public bool AcceptsSlot(EquipmentSlotType slotType)
    {
        if (!System.Enum.IsDefined(typeof(EquipmentSlotType), slotType))
        {
            return false;
        }

        EquipmentSlotMask slotMask = (EquipmentSlotMask)(1 << (int)slotType);
        return (AcceptedEquipmentSlots & slotMask) != 0;
    }

    protected virtual void OnValidate()
    {
        definitionId = definitionId != null ? definitionId.Trim() : string.Empty;
        maxStackSize = Mathf.Max(1, maxStackSize);
        defaultMaxDurability = Mathf.Max(0f, defaultMaxDurability);
    }
}
