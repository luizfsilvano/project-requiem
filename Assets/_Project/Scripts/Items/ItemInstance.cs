using System;
using UnityEngine;

public enum ItemQuality
{
    Standard,
    Crude,
    Fine,
    Masterwork
}

public enum ItemRarity
{
    Common,
    Uncommon,
    Rare,
    Epic,
    Legendary
}

[Serializable]
public sealed class ItemInstance
{
    [SerializeField] private string instanceId;
    [SerializeField] private ItemDefinition definition;
    [SerializeField, Min(1)] private int quantity = 1;
    [SerializeField, Min(0f)] private float currentDurability;
    [SerializeField, Min(0f)] private float maxDurability;

    [Header("Future Item Metadata")]
    [SerializeField] private ItemQuality quality = ItemQuality.Standard;
    [SerializeField] private ItemRarity rarity = ItemRarity.Common;
    [SerializeField] private string creatorId;
    [SerializeField] private string affinityId;
    [SerializeField] private bool isStolen;
    [SerializeField] private string originalOwnerId;

    [NonSerialized] private ItemContainer owner;

    public event Action<ItemInstance> Changed;

    public string InstanceId => instanceId;
    public ItemDefinition Definition => definition;
    public int Quantity => quantity;
    public float CurrentDurability => currentDurability;
    public float MaxDurability => maxDurability;
    public ItemQuality Quality => quality;
    public ItemRarity Rarity => rarity;
    public string CreatorId => creatorId;
    public string AffinityId => affinityId;
    public bool IsStolen => isStolen;
    public string OriginalOwnerId => originalOwnerId;
    public bool IsValid => definition != null
        && !string.IsNullOrWhiteSpace(instanceId)
        && quantity > 0
        && quantity <= definition.MaxStackSize;

    private ItemInstance(ItemDefinition itemDefinition, int itemQuantity)
    {
        instanceId = Guid.NewGuid().ToString("N");
        definition = itemDefinition;
        quantity = itemQuantity;
        maxDurability = itemDefinition != null ? itemDefinition.DefaultMaxDurability : 0f;
        currentDurability = maxDurability;
    }

    public static bool TryCreate(ItemDefinition definition, int quantity, out ItemInstance instance)
    {
        instance = null;
        if (definition == null || quantity < 1 || quantity > definition.MaxStackSize)
        {
            return false;
        }

        instance = new ItemInstance(definition, quantity);
        return true;
    }

    public static ItemInstance Create(ItemDefinition definition, int quantity = 1)
    {
        return TryCreate(definition, quantity, out ItemInstance instance) ? instance : null;
    }

    public bool EnsureValidState()
    {
        if (definition == null)
        {
            return false;
        }

        if (string.IsNullOrWhiteSpace(instanceId))
        {
            instanceId = Guid.NewGuid().ToString("N");
        }

        quantity = Mathf.Clamp(quantity, 1, definition.MaxStackSize);
        maxDurability = Mathf.Max(0f, maxDurability);
        currentDurability = Mathf.Clamp(currentDurability, 0f, maxDurability);
        return IsValid;
    }

    public bool TrySetQuantity(int newQuantity)
    {
        if (definition == null || newQuantity < 1 || newQuantity > definition.MaxStackSize)
        {
            return false;
        }

        if (quantity == newQuantity)
        {
            return true;
        }

        quantity = newQuantity;
        Changed?.Invoke(this);
        return true;
    }

    public bool TrySetDurability(float newCurrentDurability, float newMaxDurability)
    {
        if (newMaxDurability < 0f || newCurrentDurability < 0f || newCurrentDurability > newMaxDurability)
        {
            return false;
        }

        if (Mathf.Approximately(maxDurability, newMaxDurability)
            && Mathf.Approximately(currentDurability, newCurrentDurability))
        {
            return true;
        }

        maxDurability = newMaxDurability;
        currentDurability = newCurrentDurability;
        Changed?.Invoke(this);
        return true;
    }

    internal bool CanAttachTo(ItemContainer container)
    {
        return container != null && (owner == null || ReferenceEquals(owner, container));
    }

    internal bool TryAttachTo(ItemContainer container)
    {
        if (!CanAttachTo(container))
        {
            return false;
        }

        owner = container;
        return true;
    }

    internal void DetachFrom(ItemContainer container)
    {
        if (ReferenceEquals(owner, container))
        {
            owner = null;
        }
    }

    internal void RegenerateInstanceId()
    {
        instanceId = Guid.NewGuid().ToString("N");
    }
}
