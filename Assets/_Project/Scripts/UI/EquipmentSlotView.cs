using UnityEngine;
using UnityEngine.EventSystems;
using UnityEngine.UI;

[RequireComponent(typeof(CanvasGroup))]
public sealed class EquipmentSlotView : MonoBehaviour,
    IPointerClickHandler,
    IBeginDragHandler,
    IDragHandler,
    IEndDragHandler,
    IDropHandler
{
    [SerializeField] private EquipmentSlotType slotType;
    [SerializeField] private Image background;
    [SerializeField] private Image border;
    [SerializeField] private Image icon;
    [SerializeField] private Text iconFallback;
    [SerializeField] private Text slotLabel;
    [SerializeField] private Text itemLabel;
    [SerializeField] private Text activeBadge;
    [SerializeField] private GameObject selectionFrame;
    [SerializeField] private Image dragHighlight;

    private CharacterInventoryScreen screen;
    private PlayerEquipment equipment;
    private ItemInstance item;
    private CanvasGroup canvasGroup;

    public EquipmentSlotType SlotType => slotType;
    public ItemInstance Item => item;

    private void Awake()
    {
        canvasGroup = GetComponent<CanvasGroup>();
    }

    public void Bind(CharacterInventoryScreen owner, PlayerEquipment playerEquipment)
    {
        screen = owner;
        equipment = playerEquipment;
        Refresh();
    }

    public void Refresh()
    {
        item = equipment != null ? equipment.GetItem(slotType) : null;
        bool hasItem = item?.Definition != null;

        if (slotLabel != null)
        {
            slotLabel.text = CharacterInventoryScreen.GetSlotDisplayName(slotType);
        }

        if (itemLabel != null)
        {
            itemLabel.text = hasItem ? item.Definition.DisplayName : "Empty";
            itemLabel.color = hasItem
                ? new Color(0.91f, 0.87f, 0.76f, 1f)
                : new Color(0.42f, 0.4f, 0.36f, 1f);
        }

        if (background != null)
        {
            background.color = hasItem
                ? new Color(0.105f, 0.09f, 0.075f, 0.98f)
                : new Color(0.055f, 0.052f, 0.05f, 0.9f);
        }

        if (border != null)
        {
            Color rarityColor = hasItem
                ? CharacterInventoryScreen.GetRarityColor(item.Rarity)
                : new Color(0.22f, 0.19f, 0.15f, 0.9f);
            rarityColor.a = hasItem ? 0.36f : 0.52f;
            border.color = rarityColor;
        }

        if (icon != null)
        {
            icon.sprite = hasItem ? item.Definition.Icon : null;
            icon.enabled = icon.sprite != null;
        }

        if (iconFallback != null)
        {
            iconFallback.text = hasItem
                ? CharacterInventoryScreen.GetItemMonogram(item.Definition.DisplayName)
                : CharacterInventoryScreen.GetSlotMonogram(slotType);
            iconFallback.color = hasItem
                ? new Color(0.86f, 0.72f, 0.42f, 1f)
                : new Color(0.34f, 0.31f, 0.27f, 1f);
        }

        bool isActive = hasItem
            && equipment != null
            && equipment.EquippedItem != null
            && equipment.EquippedItem.InstanceId == item.InstanceId;
        if (activeBadge != null)
        {
            activeBadge.gameObject.SetActive(isActive);
            activeBadge.text = "ACTIVE";
        }

        if (selectionFrame != null)
        {
            selectionFrame.SetActive(hasItem && screen != null && screen.IsSelected(item));
        }

        SetDragHighlight(false, false);
        if (canvasGroup != null)
        {
            canvasGroup.alpha = 1f;
            canvasGroup.blocksRaycasts = true;
        }
    }

    public void SetDragHighlight(bool visible, bool valid)
    {
        if (dragHighlight == null)
        {
            return;
        }

        dragHighlight.gameObject.SetActive(visible);
        dragHighlight.color = valid
            ? new Color(0.55f, 0.42f, 0.16f, 0.58f)
            : new Color(0.55f, 0.08f, 0.06f, 0.58f);
    }

    public void OnPointerClick(PointerEventData eventData)
    {
        if (item == null || screen == null)
        {
            return;
        }

        screen.SelectItem(item);
        if (eventData.button == PointerEventData.InputButton.Left && eventData.clickCount >= 2)
        {
            screen.ExecuteDefaultAction(item);
        }
    }

    public void OnBeginDrag(PointerEventData eventData)
    {
        if (item == null || screen == null || eventData.button != PointerEventData.InputButton.Left)
        {
            return;
        }

        screen.BeginDrag(item, slotType, eventData.position);
        if (canvasGroup != null)
        {
            canvasGroup.alpha = 0.42f;
            canvasGroup.blocksRaycasts = false;
        }
    }

    public void OnDrag(PointerEventData eventData)
    {
        screen?.UpdateDrag(eventData.position);
    }

    public void OnEndDrag(PointerEventData eventData)
    {
        if (canvasGroup != null)
        {
            canvasGroup.alpha = 1f;
            canvasGroup.blocksRaycasts = true;
        }

        screen?.EndDrag();
    }

    public void OnDrop(PointerEventData eventData)
    {
        screen?.TryDropOnEquipment(slotType);
    }
}
