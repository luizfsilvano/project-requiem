using UnityEngine;

public interface IItemSlotViewHost
{
    bool IsSelected(ItemInstance item, IItemContainer source);
    bool IsEquipped(ItemInstance item);
    void SelectItem(ItemInstance item, IItemContainer source);
    void ExecuteDefaultAction(ItemInstance item, IItemContainer source);
    void BeginItemDrag(ItemInstance item, IItemContainer source, Vector2 screenPosition);
    void UpdateItemDrag(Vector2 screenPosition);
    void EndItemDrag();
    void DropItemOnContainer(IItemContainer destination);
}
