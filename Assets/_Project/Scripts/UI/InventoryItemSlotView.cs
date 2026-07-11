using UnityEngine;
using UnityEngine.EventSystems;
using UnityEngine.UI;

[RequireComponent(typeof(CanvasGroup))]
public sealed class InventoryItemSlotView : MonoBehaviour,
    IPointerClickHandler,
    IBeginDragHandler,
    IDragHandler,
    IEndDragHandler,
    IDropHandler
{
    [SerializeField] private Image background;
    [SerializeField] private Image rarityBorder;
    [SerializeField] private Image icon;
    [SerializeField] private Text iconFallback;
    [SerializeField] private Text quantityText;
    [SerializeField] private Text rarityText;
    [SerializeField] private Text qualityText;
    [SerializeField] private Text equippedText;
    [SerializeField] private Image durabilityTrack;
    [SerializeField] private Image durabilityFill;
    [SerializeField] private GameObject selectionFrame;

    private IItemSlotViewHost host;
    private IItemContainer source;
    private ItemInstance item;
    private CanvasGroup canvasGroup;

    public ItemInstance Item => item;

    private void Awake()
    {
        canvasGroup = GetComponent<CanvasGroup>();
    }

    public void Bind(IItemSlotViewHost owner, IItemContainer sourceContainer, ItemInstance boundItem)
    {
        host = owner;
        source = sourceContainer;
        item = boundItem;
        Refresh();
    }

    public void Refresh()
    {
        bool hasItem = item?.Definition != null;
        if (background != null)
        {
            background.color = hasItem
                ? new Color(0.095f, 0.085f, 0.075f, 0.98f)
                : new Color(0.05f, 0.048f, 0.046f, 0.72f);
        }

        if (rarityBorder != null)
        {
            Color rarityColor = hasItem
                ? ItemUiPresentation.GetRarityColor(item.Rarity)
                : new Color(0.18f, 0.16f, 0.13f, 0.8f);
            rarityColor.a = hasItem ? 0.34f : 0.5f;
            rarityBorder.color = rarityColor;
        }

        if (icon != null)
        {
            icon.sprite = hasItem ? item.Definition.Icon : null;
            icon.enabled = icon.sprite != null;
        }

        if (iconFallback != null)
        {
            iconFallback.gameObject.SetActive(hasItem && (icon == null || icon.sprite == null));
            iconFallback.text = hasItem ? ItemUiPresentation.GetItemMonogram(item.Definition.DisplayName) : string.Empty;
        }

        if (quantityText != null)
        {
            quantityText.text = hasItem && item.Quantity > 1 ? $"x{item.Quantity}" : string.Empty;
        }

        if (rarityText != null)
        {
            rarityText.text = hasItem ? ItemUiPresentation.GetRarityShortName(item.Rarity) : string.Empty;
            rarityText.color = hasItem ? ItemUiPresentation.GetRarityColor(item.Rarity) : Color.clear;
        }

        if (qualityText != null)
        {
            qualityText.text = hasItem ? ItemUiPresentation.GetQualityShortName(item.Quality) : string.Empty;
        }

        bool equipped = hasItem && host != null && host.IsEquipped(item);
        if (equippedText != null)
        {
            equippedText.gameObject.SetActive(equipped);
            equippedText.text = "EQUIPPED";
        }

        bool hasDurability = hasItem && item.MaxDurability > 0f;
        if (durabilityTrack != null)
        {
            durabilityTrack.gameObject.SetActive(hasDurability);
        }

        if (durabilityFill != null)
        {
            durabilityFill.fillAmount = hasDurability
                ? Mathf.Clamp01(item.CurrentDurability / item.MaxDurability)
                : 0f;
            durabilityFill.color = hasDurability && item.CurrentDurability / item.MaxDurability < 0.25f
                ? new Color(0.72f, 0.16f, 0.12f, 1f)
                : new Color(0.72f, 0.53f, 0.24f, 1f);
        }

        if (selectionFrame != null)
        {
            selectionFrame.SetActive(hasItem && host != null && host.IsSelected(item, source));
        }

        if (canvasGroup != null)
        {
            canvasGroup.alpha = 1f;
            canvasGroup.blocksRaycasts = true;
        }
    }

    public void OnPointerClick(PointerEventData eventData)
    {
        if (item == null || host == null || source == null)
        {
            return;
        }

        host.SelectItem(item, source);
        if (eventData.button == PointerEventData.InputButton.Left && eventData.clickCount >= 2)
        {
            host.ExecuteDefaultAction(item, source);
        }
    }

    public void OnBeginDrag(PointerEventData eventData)
    {
        if (item == null || host == null || source == null || eventData.button != PointerEventData.InputButton.Left)
        {
            return;
        }

        host.BeginItemDrag(item, source, eventData.position);
        if (canvasGroup != null)
        {
            canvasGroup.alpha = 0.42f;
            canvasGroup.blocksRaycasts = false;
        }
    }

    public void OnDrag(PointerEventData eventData)
    {
        host?.UpdateItemDrag(eventData.position);
    }

    public void OnEndDrag(PointerEventData eventData)
    {
        if (canvasGroup != null)
        {
            canvasGroup.alpha = 1f;
            canvasGroup.blocksRaycasts = true;
        }

        host?.EndItemDrag();
    }

    public void OnDrop(PointerEventData eventData)
    {
        if (source != null)
        {
            host?.DropItemOnContainer(source);
        }
    }
}
