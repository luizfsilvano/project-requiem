using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using UnityEngine;

[Serializable]
public sealed class ItemContainer : IItemContainer, ISerializationCallbackReceiver
{
    [SerializeField, Min(0)] private int capacity;
    [SerializeField] private List<ItemInstance> items = new();

    [NonSerialized] private bool runtimeStateReady;
    [NonSerialized] private ReadOnlyCollection<ItemInstance> readOnlyItems;

    public IReadOnlyList<ItemInstance> Items
    {
        get
        {
            EnsureRuntimeState();
            return readOnlyItems ??= items.AsReadOnly();
        }
    }

    public int Count
    {
        get
        {
            EnsureRuntimeState();
            return items.Count;
        }
    }
    public int Capacity => Mathf.Max(0, capacity);

    public bool CanAdd(ItemInstance item)
    {
        EnsureRuntimeState();
        if (item == null || !item.IsValid || !item.CanAttachTo(this) || Contains(item.InstanceId))
        {
            return false;
        }

        return Capacity == 0 || items.Count < Capacity;
    }

    public bool TryAdd(ItemInstance item)
    {
        EnsureRuntimeState();
        if (item == null || !item.EnsureValidState() || !CanAdd(item))
        {
            return false;
        }

        if (!item.TryAttachTo(this))
        {
            return false;
        }

        items.Add(item);
        return true;
    }

    public bool Contains(string instanceId)
    {
        return TryGet(instanceId, out _);
    }

    public bool TryGet(string instanceId, out ItemInstance item)
    {
        EnsureRuntimeState();
        item = null;
        if (string.IsNullOrWhiteSpace(instanceId))
        {
            return false;
        }

        for (int i = 0; i < items.Count; i++)
        {
            ItemInstance candidate = items[i];
            if (candidate != null && string.Equals(candidate.InstanceId, instanceId, StringComparison.Ordinal))
            {
                item = candidate;
                return true;
            }
        }

        return false;
    }

    public bool TryRemove(string instanceId, out ItemInstance item)
    {
        EnsureRuntimeState();
        item = null;
        if (string.IsNullOrWhiteSpace(instanceId))
        {
            return false;
        }

        for (int i = 0; i < items.Count; i++)
        {
            ItemInstance candidate = items[i];
            if (candidate == null || !string.Equals(candidate.InstanceId, instanceId, StringComparison.Ordinal))
            {
                continue;
            }

            item = candidate;
            items.RemoveAt(i);
            item.DetachFrom(this);
            return true;
        }

        return false;
    }

    public bool TryMoveTo(string instanceId, IItemContainer destination)
    {
        EnsureRuntimeState();
        if (destination == null || ReferenceEquals(this, destination) || !TryGet(instanceId, out ItemInstance item))
        {
            return false;
        }

        if (destination is ItemContainer itemDestination && !itemDestination.CanAddIgnoringOwnership(item))
        {
            return false;
        }

        int sourceIndex = items.IndexOf(item);
        string originalInstanceId = item.InstanceId;
        if (!TryRemove(instanceId, out ItemInstance removedItem))
        {
            return false;
        }

        if (destination.CanAdd(removedItem) && destination.TryAdd(removedItem))
        {
            return true;
        }

        if (destination.TryGet(originalInstanceId, out ItemInstance destinationItem)
            && ReferenceEquals(destinationItem, removedItem))
        {
            return true;
        }

        if (!RestoreRemovedItem(removedItem, originalInstanceId, sourceIndex))
        {
            Debug.LogError(
                $"Item transfer rollback failed for '{originalInstanceId}'. "
                + "The source or destination container violated the IItemContainer ownership contract.");
        }

        return false;
    }

    public void NormalizeContents()
    {
        items ??= new List<ItemInstance>();
        readOnlyItems = null;
        HashSet<ItemInstance> references = new();
        HashSet<string> instanceIds = new(StringComparer.Ordinal);

        for (int i = items.Count - 1; i >= 0; i--)
        {
            ItemInstance item = items[i];
            if (item == null || !references.Add(item) || !item.EnsureValidState() || !item.TryAttachTo(this))
            {
                items.RemoveAt(i);
                continue;
            }

            while (!instanceIds.Add(item.InstanceId))
            {
                item.RegenerateInstanceId();
            }
        }

        runtimeStateReady = true;
    }

    public bool TryReplaceContents(IReadOnlyList<ItemInstance> replacementItems, out string error)
    {
        EnsureRuntimeState();
        error = string.Empty;
        if (replacementItems == null)
        {
            error = "Replacement item list is null.";
            return false;
        }

        if (Capacity > 0 && replacementItems.Count > Capacity)
        {
            error = $"Container capacity {Capacity} cannot hold {replacementItems.Count} items.";
            return false;
        }

        HashSet<ItemInstance> references = new();
        HashSet<string> instanceIds = new(StringComparer.Ordinal);
        for (int i = 0; i < replacementItems.Count; i++)
        {
            ItemInstance item = replacementItems[i];
            if (item == null || !item.IsValid)
            {
                error = $"Replacement item at index {i} is invalid.";
                return false;
            }

            if (!references.Add(item) || !instanceIds.Add(item.InstanceId))
            {
                error = $"Replacement contains duplicate item '{item.InstanceId}'.";
                return false;
            }

            if (!item.CanAttachTo(this))
            {
                error = $"Item '{item.InstanceId}' is already owned by another container.";
                return false;
            }
        }

        List<ItemInstance> previousItems = new(items);
        for (int i = 0; i < previousItems.Count; i++)
        {
            previousItems[i]?.DetachFrom(this);
        }

        List<ItemInstance> nextItems = new(replacementItems.Count);
        for (int i = 0; i < replacementItems.Count; i++)
        {
            ItemInstance item = replacementItems[i];
            if (item.TryAttachTo(this))
            {
                nextItems.Add(item);
                continue;
            }

            for (int attachedIndex = 0; attachedIndex < nextItems.Count; attachedIndex++)
            {
                nextItems[attachedIndex].DetachFrom(this);
            }

            for (int previousIndex = 0; previousIndex < previousItems.Count; previousIndex++)
            {
                previousItems[previousIndex]?.TryAttachTo(this);
            }

            error = $"Could not attach item '{item.InstanceId}' while replacing container contents.";
            return false;
        }

        items = nextItems;
        readOnlyItems = null;
        runtimeStateReady = true;
        return true;
    }

    private bool CanAddIgnoringOwnership(ItemInstance item)
    {
        EnsureRuntimeState();
        if (item == null || !item.IsValid || Contains(item.InstanceId))
        {
            return false;
        }

        return Capacity == 0 || items.Count < Capacity;
    }

    private bool RestoreRemovedItem(ItemInstance item, string originalInstanceId, int sourceIndex)
    {
        if (item == null
            || !string.Equals(item.InstanceId, originalInstanceId, StringComparison.Ordinal)
            || Contains(originalInstanceId)
            || !item.TryAttachTo(this))
        {
            return false;
        }

        int safeIndex = Mathf.Clamp(sourceIndex, 0, items.Count);
        items.Insert(safeIndex, item);
        return TryGet(originalInstanceId, out ItemInstance restoredItem)
            && ReferenceEquals(restoredItem, item);
    }

    public void OnBeforeSerialize()
    {
    }

    public void OnAfterDeserialize()
    {
        runtimeStateReady = false;
        readOnlyItems = null;
    }

    private void EnsureRuntimeState()
    {
        if (!runtimeStateReady)
        {
            NormalizeContents();
        }
    }
}
