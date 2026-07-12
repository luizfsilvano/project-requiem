using System;
using System.Collections.Generic;
using UnityEngine;

[RequireComponent(typeof(Collider))]
public sealed class WorldItemContainer : InteractableBehaviour, IItemContainer
{
    [Serializable]
    public sealed class InitialItemSeed
    {
        [SerializeField] private ItemDefinition definition;
        [SerializeField, Min(1)] private int quantity = 1;

        public ItemDefinition Definition => definition;
        public int Quantity => Mathf.Max(1, quantity);
    }

    [Header("Container")]
    [SerializeField] private ItemContainer itemContainer = new();
    [SerializeField] private List<InitialItemSeed> initialContents = new();

    [Header("Open Feedback")]
    [SerializeField] private Transform lid;
    [SerializeField] private Vector3 closedLidEuler;
    [SerializeField] private Vector3 openLidEuler = new(-70f, 0f, 0f);
    [SerializeField, Min(1f)] private float lidRotationSpeed = 180f;

    private readonly HashSet<ItemInstance> subscribedItems = new();
    private bool initialized;
    private bool hasRestoredState;
    private bool isOpen;
    private bool unavailableNotified;

    public event Action ContentsChanged;
    public event Action<WorldItemContainer> BecameUnavailable;

    public override InteractionKind Kind => InteractionKind.ItemContainer;
    public override string ActionName => isOpen ? "Fechar Baú" : "Abrir Baú";
    public override bool IsAvailable => base.IsAvailable && initialized;
    public string ContainerDisplayName => DisplayName;
    public bool IsOpen => isOpen;
    public bool IsInitialized => initialized;
    public bool HasRestoredState => hasRestoredState;
    public IItemContainer Container => this;
    public IReadOnlyList<ItemInstance> Items => itemContainer.Items;
    public int Count => itemContainer.Count;
    public int Capacity => itemContainer.Capacity;

    private void Awake()
    {
        EnsureInitializedForPersistence();

        if (lid != null)
        {
            lid.localRotation = Quaternion.Euler(closedLidEuler);
        }
    }

    public void EnsureInitializedForPersistence()
    {
        if (initialized)
        {
            return;
        }

        itemContainer ??= new ItemContainer();
        itemContainer.NormalizeContents();
        InitializeContents();
        SubscribeToAllItems();
        initialized = true;
    }

    private void OnEnable()
    {
        unavailableNotified = false;
        SubscribeToAllItems();
    }

    private void OnDisable()
    {
        isOpen = false;
        if (lid != null)
        {
            lid.localRotation = Quaternion.Euler(closedLidEuler);
        }

        UnsubscribeFromAllItems();
        NotifyBecameUnavailable();
    }

    private void OnDestroy()
    {
        UnsubscribeFromAllItems();
        NotifyBecameUnavailable();
    }

    private void Update()
    {
        if (lid == null)
        {
            return;
        }

        Quaternion targetRotation = Quaternion.Euler(isOpen ? openLidEuler : closedLidEuler);
        lid.localRotation = Quaternion.RotateTowards(
            lid.localRotation,
            targetRotation,
            lidRotationSpeed * Time.unscaledDeltaTime);
    }

    public override bool CanInteract(InteractionContext context)
    {
        return base.CanInteract(context)
            && !isOpen
            && context.Interactor != null
            && context.GetActorComponent<PlayerInventory>() != null;
    }

    public override bool TryInteract(InteractionContext context)
    {
        return CanInteract(context)
            && context.Interactor.TryOpenContainer(this);
    }

    public void SetUiOpen(bool open)
    {
        if (open && !isActiveAndEnabled)
        {
            return;
        }

        if (isOpen == open)
        {
            return;
        }

        isOpen = open;
        NotifyInteractionStateChanged();
    }

    public bool CanAdd(ItemInstance item)
    {
        return itemContainer.CanAdd(item);
    }

    public bool Contains(string instanceId)
    {
        return itemContainer.Contains(instanceId);
    }

    public bool TryGet(string instanceId, out ItemInstance item)
    {
        return itemContainer.TryGet(instanceId, out item);
    }

    public bool TryAdd(ItemInstance item)
    {
        if (!itemContainer.TryAdd(item))
        {
            return false;
        }

        SubscribeToItem(item);
        ContentsChanged?.Invoke();
        return true;
    }

    public bool TryRemove(string instanceId, out ItemInstance item)
    {
        if (!itemContainer.TryRemove(instanceId, out item))
        {
            return false;
        }

        UnsubscribeFromItem(item);
        ContentsChanged?.Invoke();
        return true;
    }

    public bool TryMoveTo(string instanceId, IItemContainer destination)
    {
        if (destination == null
            || ReferenceEquals(destination, this)
            || !itemContainer.TryGet(instanceId, out ItemInstance item)
            || !itemContainer.TryMoveTo(instanceId, destination))
        {
            return false;
        }

        UnsubscribeFromItem(item);
        ContentsChanged?.Invoke();
        return true;
    }

    public bool TryRestoreContents(IReadOnlyList<ItemInstance> restoredItems, out string error)
    {
        List<ItemInstance> previousItems = new(itemContainer.Items);
        UnsubscribeFromAllItems();
        if (!itemContainer.TryReplaceContents(restoredItems, out error))
        {
            SubscribeToItems(previousItems);
            return false;
        }

        SubscribeToItems(restoredItems);
        initialized = true;
        hasRestoredState = true;
        SetUiOpen(false);
        ContentsChanged?.Invoke();
        NotifyInteractionStateChanged();
        return true;
    }

    protected override void OnValidate()
    {
        base.OnValidate();
        itemContainer ??= new ItemContainer();
        initialContents ??= new List<InitialItemSeed>();
        lidRotationSpeed = Mathf.Max(1f, lidRotationSpeed);
    }

    private void InitializeContents()
    {
        if (initialized || itemContainer.Count > 0 || initialContents == null)
        {
            return;
        }

        for (int i = 0; i < initialContents.Count; i++)
        {
            InitialItemSeed seed = initialContents[i];
            ItemDefinition definition = seed?.Definition;
            if (definition == null)
            {
                continue;
            }

            int quantity = Mathf.Clamp(seed.Quantity, 1, definition.MaxStackSize);
            ItemInstance item = ItemInstance.Create(definition, quantity);
            if (item != null && itemContainer.TryAdd(item))
            {
                SubscribeToItem(item);
            }
        }
    }

    private void SubscribeToAllItems()
    {
        if (itemContainer == null)
        {
            return;
        }

        foreach (ItemInstance item in itemContainer.Items)
        {
            SubscribeToItem(item);
        }
    }

    private void SubscribeToItems(IReadOnlyList<ItemInstance> items)
    {
        if (items == null)
        {
            return;
        }

        for (int i = 0; i < items.Count; i++)
        {
            SubscribeToItem(items[i]);
        }
    }

    private void UnsubscribeFromAllItems()
    {
        foreach (ItemInstance item in subscribedItems)
        {
            if (item != null)
            {
                item.Changed -= HandleItemChanged;
            }
        }

        subscribedItems.Clear();
    }

    private void SubscribeToItem(ItemInstance item)
    {
        if (item != null && subscribedItems.Add(item))
        {
            item.Changed += HandleItemChanged;
        }
    }

    private void UnsubscribeFromItem(ItemInstance item)
    {
        if (item != null && subscribedItems.Remove(item))
        {
            item.Changed -= HandleItemChanged;
        }
    }

    private void HandleItemChanged(ItemInstance item)
    {
        ContentsChanged?.Invoke();
    }

    private void NotifyBecameUnavailable()
    {
        if (unavailableNotified)
        {
            return;
        }

        unavailableNotified = true;
        BecameUnavailable?.Invoke(this);
    }
}
