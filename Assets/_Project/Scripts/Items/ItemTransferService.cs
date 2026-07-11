using System.Collections.Generic;

public enum ItemTransferFailure
{
    None = 0,
    InvalidContainer,
    InvalidSource = InvalidContainer,
    InvalidDestination = InvalidContainer,
    SameContainer,
    InvalidInstanceId,
    ItemNotFound,
    InvalidItem,
    ItemEquipped,
    DuplicateInstanceId,
    DestinationFull,
    DestinationRejected,
    TransferFailed,
    InvariantViolation,
    RollbackFailed
}

public static class ItemTransferService
{
    public static bool TryTransfer(
        IItemContainer source,
        IItemContainer destination,
        string instanceId,
        PlayerEquipment equipment,
        out ItemTransferFailure failure)
    {
        if (!TryValidateContainers(source, destination, out failure))
        {
            return false;
        }

        if (string.IsNullOrWhiteSpace(instanceId))
        {
            failure = ItemTransferFailure.InvalidInstanceId;
            return false;
        }

        if (!source.TryGet(instanceId, out ItemInstance item))
        {
            failure = ItemTransferFailure.ItemNotFound;
            return false;
        }

        if (item == null || !item.IsValid)
        {
            failure = ItemTransferFailure.InvalidItem;
            return false;
        }

        string originalInstanceId = item.InstanceId;
        if (!string.Equals(originalInstanceId, instanceId, System.StringComparison.Ordinal))
        {
            failure = ItemTransferFailure.InvariantViolation;
            return false;
        }

        if (source is PlayerInventory
            && equipment != null
            && equipment.IsItemSlotted(originalInstanceId))
        {
            failure = ItemTransferFailure.ItemEquipped;
            return false;
        }

        if (destination.Contains(originalInstanceId))
        {
            failure = ItemTransferFailure.DuplicateInstanceId;
            return false;
        }

        if (destination.Capacity > 0 && destination.Count >= destination.Capacity)
        {
            failure = ItemTransferFailure.DestinationFull;
            return false;
        }

        if (!source.TryMoveTo(originalInstanceId, destination))
        {
            if (IsExactOwner(source, item, originalInstanceId)
                && !destination.Contains(originalInstanceId))
            {
                failure = ItemTransferFailure.DestinationRejected;
                return false;
            }

            failure = TryRestoreOriginalOwner(source, destination, item, originalInstanceId)
                ? ItemTransferFailure.InvariantViolation
                : ItemTransferFailure.RollbackFailed;
            return false;
        }

        bool transferInvariantHolds = string.Equals(
                item.InstanceId,
                originalInstanceId,
                System.StringComparison.Ordinal)
            && !source.Contains(originalInstanceId)
            && IsExactOwner(destination, item, originalInstanceId);
        if (transferInvariantHolds)
        {
            failure = ItemTransferFailure.None;
            return true;
        }

        failure = TryRestoreOriginalOwner(source, destination, item, originalInstanceId)
            ? ItemTransferFailure.InvariantViolation
            : ItemTransferFailure.RollbackFailed;
        return false;
    }

    public static bool TryTransferAll(
        IItemContainer source,
        IItemContainer destination,
        PlayerEquipment equipment,
        out ItemTransferFailure failure,
        out int movedCount)
    {
        movedCount = 0;
        if (!TryValidateContainers(source, destination, out failure))
        {
            return false;
        }

        List<ItemInstance> snapshot = new(source.Count);
        for (int i = 0; i < source.Items.Count; i++)
        {
            snapshot.Add(source.Items[i]);
        }

        if (snapshot.Count == 0)
        {
            failure = ItemTransferFailure.None;
            return true;
        }

        if (destination.Capacity > 0
            && destination.Capacity - destination.Count < snapshot.Count)
        {
            failure = ItemTransferFailure.DestinationFull;
            return false;
        }

        HashSet<string> ids = new(System.StringComparer.Ordinal);
        for (int i = 0; i < snapshot.Count; i++)
        {
            ItemInstance item = snapshot[i];
            if (item == null || !item.IsValid || !ids.Add(item.InstanceId))
            {
                failure = ItemTransferFailure.InvalidItem;
                return false;
            }

            if (!IsExactOwner(source, item, item.InstanceId))
            {
                failure = ItemTransferFailure.InvariantViolation;
                return false;
            }

            if (source is PlayerInventory
                && equipment != null
                && equipment.IsItemSlotted(item.InstanceId))
            {
                failure = ItemTransferFailure.ItemEquipped;
                return false;
            }

            if (destination.Contains(item.InstanceId))
            {
                failure = ItemTransferFailure.DuplicateInstanceId;
                return false;
            }
        }

        List<ItemInstance> movedItems = new(snapshot.Count);
        for (int i = 0; i < snapshot.Count; i++)
        {
            ItemInstance item = snapshot[i];
            if (TryTransfer(source, destination, item.InstanceId, equipment, out failure))
            {
                movedItems.Add(item);
                continue;
            }

            ItemTransferFailure originalFailure = failure;
            int remainingMoved = RollbackBatch(destination, source, movedItems);
            movedCount = remainingMoved;
            failure = remainingMoved == 0 ? originalFailure : ItemTransferFailure.RollbackFailed;
            return false;
        }

        movedCount = movedItems.Count;
        failure = ItemTransferFailure.None;
        return true;
    }

    private static int RollbackBatch(
        IItemContainer currentOwner,
        IItemContainer originalOwner,
        List<ItemInstance> movedItems)
    {
        int remainingMoved = movedItems.Count;
        for (int i = movedItems.Count - 1; i >= 0; i--)
        {
            ItemInstance item = movedItems[i];
            if (item == null)
            {
                continue;
            }

            if (currentOwner.TryMoveTo(item.InstanceId, originalOwner)
                && IsExactOwner(originalOwner, item, item.InstanceId)
                && !currentOwner.Contains(item.InstanceId))
            {
                remainingMoved--;
            }
        }

        return remainingMoved;
    }

    private static bool TryValidateContainers(
        IItemContainer source,
        IItemContainer destination,
        out ItemTransferFailure failure)
    {
        if (source == null)
        {
            failure = ItemTransferFailure.InvalidSource;
            return false;
        }

        if (destination == null)
        {
            failure = ItemTransferFailure.InvalidDestination;
            return false;
        }

        if (ReferenceEquals(source, destination))
        {
            failure = ItemTransferFailure.SameContainer;
            return false;
        }

        failure = ItemTransferFailure.None;
        return true;
    }

    private static bool TryRestoreOriginalOwner(
        IItemContainer originalOwner,
        IItemContainer destination,
        ItemInstance item,
        string originalInstanceId)
    {
        if (!string.Equals(item?.InstanceId, originalInstanceId, System.StringComparison.Ordinal))
        {
            return false;
        }

        if (IsExactOwner(originalOwner, item, originalInstanceId)
            && !destination.Contains(originalInstanceId))
        {
            return true;
        }

        if (!IsExactOwner(destination, item, originalInstanceId)
            || !destination.TryMoveTo(originalInstanceId, originalOwner))
        {
            return false;
        }

        return IsExactOwner(originalOwner, item, originalInstanceId)
            && !destination.Contains(originalInstanceId);
    }

    private static bool IsExactOwner(
        IItemContainer container,
        ItemInstance expectedItem,
        string instanceId)
    {
        return container != null
            && container.TryGet(instanceId, out ItemInstance actualItem)
            && ReferenceEquals(actualItem, expectedItem);
    }
}
