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

        if (!TryRemove(instanceId, out ItemInstance removedItem))
        {
            return false;
        }

        if (destination.CanAdd(removedItem) && destination.TryAdd(removedItem))
        {
            return true;
        }

        TryAdd(removedItem);
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

    private bool CanAddIgnoringOwnership(ItemInstance item)
    {
        EnsureRuntimeState();
        if (item == null || !item.IsValid || Contains(item.InstanceId))
        {
            return false;
        }

        return Capacity == 0 || items.Count < Capacity;
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
