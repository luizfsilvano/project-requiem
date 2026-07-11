using System.Collections.Generic;

public interface IItemContainer
{
    IReadOnlyList<ItemInstance> Items { get; }
    int Count { get; }
    int Capacity { get; }

    bool CanAdd(ItemInstance item);
    bool Contains(string instanceId);
    bool TryGet(string instanceId, out ItemInstance item);
    bool TryAdd(ItemInstance item);
    bool TryRemove(string instanceId, out ItemInstance item);
    bool TryMoveTo(string instanceId, IItemContainer destination);
}
