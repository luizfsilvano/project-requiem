using UnityEngine;
using UnityEngine.EventSystems;

public sealed class InventoryDropTarget : MonoBehaviour, IDropHandler
{
    private CharacterInventoryScreen screen;

    private void Awake()
    {
        screen = GetComponentInParent<CharacterInventoryScreen>(true);
    }

    public void OnDrop(PointerEventData eventData)
    {
        screen?.TryDropOnInventory();
    }
}
