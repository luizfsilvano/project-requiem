using System;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.InputSystem;
using UnityEngine.UI;

[DefaultExecutionOrder(-5)]
public sealed class CharacterInventoryScreen : MonoBehaviour
{
    [Header("Screen")]
    [SerializeField] private GameObject screenRoot;
    [SerializeField] private PlayerInventory inventory;
    [SerializeField] private PlayerEquipment equipment;
    [SerializeField] private CharacterPreviewController previewController;

    [Header("Inventory")]
    [SerializeField] private Transform inventoryGrid;
    [SerializeField] private InventoryItemSlotView itemSlotPrefab;
    [SerializeField, Min(1)] private int minimumGridSlots = 20;
    [SerializeField] private InventoryFilterTabView[] filterTabs;

    [Header("Equipment")]
    [SerializeField] private EquipmentSlotView[] equipmentSlotViews;

    [Header("Details")]
    [SerializeField] private Text detailName;
    [SerializeField] private Text detailDescription;
    [SerializeField] private Text detailType;
    [SerializeField] private Text detailRarity;
    [SerializeField] private Text detailQuantity;
    [SerializeField] private Text detailDurability;
    [SerializeField] private Text detailDamage;
    [SerializeField] private Text detailMetadata;
    [SerializeField] private Text feedbackText;

    [Header("Actions")]
    [SerializeField] private Button equipButton;
    [SerializeField] private Button unequipButton;
    [SerializeField] private Button activateButton;
    [SerializeField] private Button discardButton;
    [SerializeField] private Button closeButton;

    [Header("Drag Feedback")]
    [SerializeField] private RectTransform dragGhost;
    [SerializeField] private Image dragGhostBackground;
    [SerializeField] private Text dragGhostLabel;

    private readonly List<InventoryItemSlotView> itemSlotPool = new();
    private readonly List<ItemInstance> filteredItems = new();
    private InventoryFilterType activeFilter = InventoryFilterType.All;
    private ItemInstance selectedItem;
    private ItemInstance draggedItem;
    private EquipmentSlotType? dragOriginSlot;
    private CursorLockMode previousCursorLockMode;
    private bool previousCursorVisible;
    private bool ownsCursorOverride;
    private bool listenersBound;

    public bool IsOpen { get; private set; }
    public ItemInstance SelectedItem => selectedItem;
    public PlayerEquipment Equipment => equipment;

    private void Awake()
    {
        ResolveDependencies();
        BindButtonListeners();

        if (screenRoot != null)
        {
            screenRoot.SetActive(false);
        }

        if (dragGhost != null)
        {
            dragGhost.gameObject.SetActive(false);
        }

        previewController?.SetVisible(false);
    }

    private void OnEnable()
    {
        ResolveDependencies();
        SubscribeToDomainEvents();
    }

    private void OnDisable()
    {
        UnsubscribeFromDomainEvents();
        if (Application.isPlaying && IsOpen)
        {
            CloseScreen();
        }
    }

    private void OnDestroy()
    {
        UnsubscribeFromDomainEvents();
        UnbindButtonListeners();
        if (IsOpen || GameplayInputGate.InventoryOpen)
        {
            GameplayInputGate.ResetInventoryState();
        }
    }

    private void Update()
    {
        if (Keyboard.current != null && Keyboard.current.iKey.wasPressedThisFrame)
        {
            if (IsOpen)
            {
                CloseScreen();
            }
            else if (!DevSettings.ConsoleOpen)
            {
                OpenScreen();
            }
        }

        if (IsOpen)
        {
            Cursor.lockState = CursorLockMode.None;
            Cursor.visible = true;
        }
    }

    public void OpenScreen()
    {
        if (IsOpen || !GameplayInputGate.TrySetInventoryOpen(true))
        {
            return;
        }

        previousCursorLockMode = Cursor.lockState;
        previousCursorVisible = Cursor.visible;
        ownsCursorOverride = true;
        Cursor.lockState = CursorLockMode.None;
        Cursor.visible = true;
        IsOpen = true;

        if (screenRoot != null)
        {
            screenRoot.SetActive(true);
        }

        ResolveDependencies();
        previewController?.SetVisible(true);
        RefreshAll();
        SetFeedback("Select an item or drag it onto a highlighted equipment slot.");
    }

    public void CloseScreen()
    {
        if (!IsOpen)
        {
            return;
        }

        EndDrag();
        IsOpen = false;
        previewController?.SetVisible(false);
        if (screenRoot != null)
        {
            screenRoot.SetActive(false);
        }

        GameplayInputGate.TrySetInventoryOpen(false);
        GameplayInputGate.SuppressForFrames(1);

        if (ownsCursorOverride)
        {
            Cursor.lockState = previousCursorLockMode;
            Cursor.visible = previousCursorVisible;
            ownsCursorOverride = false;
        }
    }

    public void SetFilter(InventoryFilterType filterType)
    {
        activeFilter = filterType;
        RefreshInventoryGrid();
        RefreshFilterTabs();
    }

    public void SelectItem(ItemInstance item)
    {
        selectedItem = item;
        RefreshInventoryGrid();
        RefreshEquipmentSlots();
        RefreshDetails();
    }

    public bool IsSelected(ItemInstance item)
    {
        return item != null
            && selectedItem != null
            && string.Equals(item.InstanceId, selectedItem.InstanceId, StringComparison.Ordinal);
    }

    public bool IsEquipped(ItemInstance item)
    {
        return item != null && equipment != null && equipment.IsItemSlotted(item.InstanceId);
    }

    public void ExecuteDefaultAction(ItemInstance item)
    {
        if (item == null || equipment == null)
        {
            return;
        }

        selectedItem = item;
        if (equipment.TryGetSlotOf(item.InstanceId, out EquipmentSlotType slotType))
        {
            bool isActiveWeapon = IsWeaponSlot(slotType)
                && equipment.EquippedItem != null
                && equipment.EquippedItem.InstanceId == item.InstanceId;
            if (IsWeaponSlot(slotType) && !isActiveWeapon)
            {
                TryActivateSelected();
            }
            else
            {
                TryUnequipSelected();
            }

            return;
        }

        TryEquipSelected();
    }

    public void BeginDrag(ItemInstance item, EquipmentSlotType? originSlot, Vector2 screenPosition)
    {
        if (item == null)
        {
            return;
        }

        selectedItem = item;
        draggedItem = item;
        dragOriginSlot = originSlot;
        if (dragGhost != null)
        {
            dragGhost.gameObject.SetActive(true);
        }

        if (dragGhostBackground != null)
        {
            dragGhostBackground.color = GetRarityColor(item.Rarity);
        }

        if (dragGhostLabel != null)
        {
            dragGhostLabel.text = GetItemMonogram(item.Definition.DisplayName);
        }

        UpdateDrag(screenPosition);
        RefreshDragHighlights();
        RefreshDetails();
    }

    public void UpdateDrag(Vector2 screenPosition)
    {
        if (dragGhost == null || !dragGhost.gameObject.activeSelf)
        {
            return;
        }

        RectTransform parentRect = dragGhost.parent as RectTransform;
        if (parentRect != null
            && RectTransformUtility.ScreenPointToLocalPointInRectangle(parentRect, screenPosition, null, out Vector2 localPoint))
        {
            dragGhost.anchoredPosition = localPoint;
        }
    }

    public void EndDrag()
    {
        draggedItem = null;
        dragOriginSlot = null;
        if (dragGhost != null)
        {
            dragGhost.gameObject.SetActive(false);
        }

        if (equipmentSlotViews != null)
        {
            foreach (EquipmentSlotView slotView in equipmentSlotViews)
            {
                slotView?.SetDragHighlight(false, false);
            }
        }
    }

    public void TryDropOnEquipment(EquipmentSlotType targetSlot)
    {
        if (draggedItem == null || equipment == null)
        {
            return;
        }

        bool success;
        EquipmentChangeFailure failure;
        if (dragOriginSlot.HasValue)
        {
            success = equipment.TryMoveOrSwap(
                dragOriginSlot.Value,
                targetSlot,
                makeMovedItemActive: IsWeaponSlot(targetSlot),
                out failure);
        }
        else
        {
            success = equipment.TryEquip(
                draggedItem,
                targetSlot,
                makeActive: IsWeaponSlot(targetSlot),
                out failure);
        }

        SetFeedback(success
            ? $"{draggedItem.Definition.DisplayName} moved to {GetSlotDisplayName(targetSlot)}."
            : GetFailureText(failure));
    }

    public void TryDropOnInventory()
    {
        if (draggedItem == null || equipment == null || !dragOriginSlot.HasValue)
        {
            return;
        }

        bool success = equipment.TryUnequip(dragOriginSlot.Value, out EquipmentChangeFailure failure);
        SetFeedback(success
            ? $"{draggedItem.Definition.DisplayName} returned to inventory."
            : GetFailureText(failure));
    }

    public void TryEquipSelected()
    {
        if (selectedItem == null || equipment == null)
        {
            return;
        }

        if (!TryGetDefaultEquipSlot(selectedItem, out EquipmentSlotType slotType))
        {
            SetFeedback("This item cannot be equipped in any available slot.");
            return;
        }

        bool success = equipment.TryEquip(
            selectedItem,
            slotType,
            makeActive: IsWeaponSlot(slotType),
            out EquipmentChangeFailure failure);
        SetFeedback(success
            ? $"Equipped {selectedItem.Definition.DisplayName} in {GetSlotDisplayName(slotType)}."
            : GetFailureText(failure));
    }

    public void TryUnequipSelected()
    {
        if (selectedItem == null
            || equipment == null
            || !equipment.TryGetSlotOf(selectedItem.InstanceId, out EquipmentSlotType slotType))
        {
            return;
        }

        bool success = equipment.TryUnequip(slotType, out EquipmentChangeFailure failure);
        SetFeedback(success
            ? $"Unequipped {selectedItem.Definition.DisplayName}."
            : GetFailureText(failure));
    }

    public void TryActivateSelected()
    {
        if (selectedItem == null
            || equipment == null
            || !equipment.TryGetSlotOf(selectedItem.InstanceId, out EquipmentSlotType slotType))
        {
            return;
        }

        bool success = equipment.TryActivateSlot(slotType, out EquipmentChangeFailure failure);
        SetFeedback(success
            ? $"{selectedItem.Definition.DisplayName} is now the active weapon."
            : GetFailureText(failure));
    }

    public void TryDiscardSelected()
    {
        if (selectedItem == null || inventory == null)
        {
            return;
        }

        string itemName = selectedItem.Definition.DisplayName;
        bool success = inventory.TryRemoveItem(selectedItem.InstanceId, out _);
        if (success)
        {
            selectedItem = null;
        }

        SetFeedback(success ? $"Discarded {itemName}." : "The item could not be discarded right now.");
    }

    private void ResolveDependencies()
    {
        if (inventory == null)
        {
            inventory = FindFirstObjectByType<PlayerInventory>();
        }

        if (equipment == null)
        {
            equipment = inventory != null ? inventory.Equipment : FindFirstObjectByType<PlayerEquipment>();
        }

        if (previewController == null)
        {
            previewController = FindFirstObjectByType<CharacterPreviewController>(FindObjectsInactive.Include);
        }

        filterTabs ??= Array.Empty<InventoryFilterTabView>();
        equipmentSlotViews ??= Array.Empty<EquipmentSlotView>();
    }

    private void SubscribeToDomainEvents()
    {
        if (inventory != null)
        {
            inventory.InventoryChanged -= HandleInventoryChanged;
            inventory.InventoryChanged += HandleInventoryChanged;
        }

        if (equipment != null)
        {
            equipment.EquipmentChanged -= HandleEquipmentChanged;
            equipment.EquipmentChanged += HandleEquipmentChanged;
        }
    }

    private void UnsubscribeFromDomainEvents()
    {
        if (inventory != null)
        {
            inventory.InventoryChanged -= HandleInventoryChanged;
        }

        if (equipment != null)
        {
            equipment.EquipmentChanged -= HandleEquipmentChanged;
        }
    }

    private void HandleInventoryChanged()
    {
        if (IsOpen)
        {
            RefreshAll();
        }
    }

    private void HandleEquipmentChanged()
    {
        if (IsOpen)
        {
            RefreshAll();
        }
    }

    private void BindButtonListeners()
    {
        if (listenersBound)
        {
            return;
        }

        equipButton?.onClick.AddListener(TryEquipSelected);
        unequipButton?.onClick.AddListener(TryUnequipSelected);
        activateButton?.onClick.AddListener(TryActivateSelected);
        discardButton?.onClick.AddListener(TryDiscardSelected);
        closeButton?.onClick.AddListener(CloseScreen);
        listenersBound = true;
    }

    private void UnbindButtonListeners()
    {
        if (!listenersBound)
        {
            return;
        }

        equipButton?.onClick.RemoveListener(TryEquipSelected);
        unequipButton?.onClick.RemoveListener(TryUnequipSelected);
        activateButton?.onClick.RemoveListener(TryActivateSelected);
        discardButton?.onClick.RemoveListener(TryDiscardSelected);
        closeButton?.onClick.RemoveListener(CloseScreen);
        listenersBound = false;
    }

    private void RefreshAll()
    {
        if (selectedItem != null && (inventory == null || !inventory.ContainsItem(selectedItem.InstanceId)))
        {
            selectedItem = null;
        }

        RefreshFilterTabs();
        RefreshInventoryGrid();
        RefreshEquipmentSlots();
        RefreshDetails();
    }

    private void RefreshFilterTabs()
    {
        if (filterTabs == null)
        {
            return;
        }

        foreach (InventoryFilterTabView tab in filterTabs)
        {
            tab?.SetSelected(tab.FilterType == activeFilter);
        }
    }

    private void RefreshInventoryGrid()
    {
        if (inventoryGrid == null || itemSlotPrefab == null || inventory == null)
        {
            return;
        }

        filteredItems.Clear();
        foreach (ItemInstance item in inventory.Items)
        {
            if (MatchesFilter(item, activeFilter))
            {
                filteredItems.Add(item);
            }
        }

        int desiredSlotCount = Mathf.Max(minimumGridSlots, filteredItems.Count);
        while (itemSlotPool.Count < desiredSlotCount)
        {
            InventoryItemSlotView slot = Instantiate(itemSlotPrefab, inventoryGrid);
            slot.gameObject.SetActive(true);
            itemSlotPool.Add(slot);
        }

        for (int i = 0; i < itemSlotPool.Count; i++)
        {
            bool visible = i < desiredSlotCount;
            itemSlotPool[i].gameObject.SetActive(visible);
            if (visible)
            {
                itemSlotPool[i].Bind(this, i < filteredItems.Count ? filteredItems[i] : null);
            }
        }
    }

    private void RefreshEquipmentSlots()
    {
        if (equipmentSlotViews == null)
        {
            return;
        }

        foreach (EquipmentSlotView slotView in equipmentSlotViews)
        {
            slotView?.Bind(this, equipment);
        }
    }

    private void RefreshDetails()
    {
        bool hasItem = selectedItem?.Definition != null;
        SetText(detailName, hasItem ? selectedItem.Definition.DisplayName : "Select an item");
        SetText(detailDescription, hasItem
            ? (string.IsNullOrWhiteSpace(selectedItem.Definition.Description)
                ? "No description has been written for this item yet."
                : selectedItem.Definition.Description)
            : "Choose an item from the inventory or an equipment slot to inspect it.");
        SetText(detailType, hasItem ? $"TYPE  {selectedItem.Definition.Category}" : "TYPE  —");
        SetText(detailRarity, hasItem
            ? $"RARITY  {selectedItem.Rarity}    QUALITY  {selectedItem.Quality}"
            : "RARITY  —    QUALITY  —");
        SetText(detailQuantity, hasItem ? $"QUANTITY  {selectedItem.Quantity}" : "QUANTITY  —");
        SetText(detailDurability, hasItem && selectedItem.MaxDurability > 0f
            ? $"DURABILITY  {selectedItem.CurrentDurability:0}/{selectedItem.MaxDurability:0}"
            : "DURABILITY  —");
        SetText(detailDamage, hasItem && selectedItem.Definition is WeaponData weaponData
            ? GetWeaponDamageSummary(weaponData)
            : string.Empty);

        string metadata = string.Empty;
        if (hasItem && !string.IsNullOrWhiteSpace(selectedItem.CreatorId))
        {
            metadata = $"CREATOR  {selectedItem.CreatorId}";
        }

        if (hasItem && !string.IsNullOrWhiteSpace(selectedItem.AffinityId))
        {
            metadata += string.IsNullOrEmpty(metadata)
                ? $"AFFINITY  {selectedItem.AffinityId}"
                : $"    AFFINITY  {selectedItem.AffinityId}";
        }

        SetText(detailMetadata, metadata);

        EquipmentSlotType selectedSlot = EquipmentSlotType.MainHand;
        bool slotted = hasItem
            && equipment != null
            && equipment.TryGetSlotOf(selectedItem.InstanceId, out selectedSlot);
        bool equippable = hasItem && selectedItem.Definition.AcceptedEquipmentSlots != EquipmentSlotMask.None;
        bool activeWeapon = slotted
            && IsWeaponSlot(selectedSlot)
            && equipment.EquippedItem != null
            && equipment.EquippedItem.InstanceId == selectedItem.InstanceId;

        SetButtonState(equipButton, hasItem && equippable && !slotted);
        SetButtonState(unequipButton, slotted);
        SetButtonState(activateButton, slotted && IsWeaponSlot(selectedSlot) && !activeWeapon);
        SetButtonState(discardButton, hasItem);
    }

    private void RefreshDragHighlights()
    {
        if (equipmentSlotViews == null || draggedItem?.Definition == null)
        {
            return;
        }

        foreach (EquipmentSlotView slotView in equipmentSlotViews)
        {
            if (slotView == null)
            {
                continue;
            }

            bool valid = draggedItem.Definition.AcceptsSlot(slotView.SlotType)
                && equipment != null
                && equipment.CanEquip(draggedItem, slotView.SlotType, out _);
            if (valid
                && dragOriginSlot.HasValue
                && slotView.Item != null
                && !slotView.Item.Definition.AcceptsSlot(dragOriginSlot.Value))
            {
                valid = false;
            }

            slotView.SetDragHighlight(true, valid);
        }
    }

    private bool TryGetDefaultEquipSlot(ItemInstance item, out EquipmentSlotType slotType)
    {
        slotType = EquipmentSlotType.MainHand;
        if (item?.Definition == null || equipment == null)
        {
            return false;
        }

        foreach (EquipmentSlotType candidate in Enum.GetValues(typeof(EquipmentSlotType)))
        {
            if (item.Definition.AcceptsSlot(candidate) && equipment.GetItem(candidate) == null)
            {
                slotType = candidate;
                return true;
            }
        }

        if (item.Definition.AcceptsSlot(equipment.ActiveSlot))
        {
            slotType = equipment.ActiveSlot;
            return true;
        }

        foreach (EquipmentSlotType candidate in Enum.GetValues(typeof(EquipmentSlotType)))
        {
            if (item.Definition.AcceptsSlot(candidate))
            {
                slotType = candidate;
                return true;
            }
        }

        return false;
    }

    private void SetFeedback(string message)
    {
        SetText(feedbackText, message);
    }

    private static bool MatchesFilter(ItemInstance item, InventoryFilterType filterType)
    {
        if (item?.Definition == null || filterType == InventoryFilterType.All)
        {
            return item?.Definition != null;
        }

        return filterType switch
        {
            InventoryFilterType.Weapons => item.Definition.Category == ItemCategory.Weapon,
            InventoryFilterType.Armor => item.Definition.Category == ItemCategory.Armor,
            InventoryFilterType.Consumables => item.Definition.Category == ItemCategory.Consumable,
            InventoryFilterType.Materials => item.Definition.Category == ItemCategory.Material,
            InventoryFilterType.Tools => item.Definition.Category == ItemCategory.Tool,
            InventoryFilterType.Other => item.Definition.Category == ItemCategory.Other,
            _ => true
        };
    }

    private static string GetWeaponDamageSummary(WeaponData weaponData)
    {
        if (weaponData?.attacks == null || weaponData.attacks.Length == 0)
        {
            return "DAMAGE  —";
        }

        int minDamage = int.MaxValue;
        int maxDamage = int.MinValue;
        int attackCount = 0;
        foreach (WeaponAttackData attack in weaponData.attacks)
        {
            if (attack == null)
            {
                continue;
            }

            minDamage = Mathf.Min(minDamage, attack.damage);
            maxDamage = Mathf.Max(maxDamage, attack.damage);
            attackCount++;
        }

        if (attackCount == 0)
        {
            return "DAMAGE  —";
        }

        string damage = minDamage == maxDamage ? minDamage.ToString() : $"{minDamage}–{maxDamage}";
        return $"DAMAGE  {damage}    COMBO  {Mathf.Min(weaponData.maxComboSteps, attackCount)} steps";
    }

    private static string GetFailureText(EquipmentChangeFailure failure)
    {
        return failure switch
        {
            EquipmentChangeFailure.InvalidItem => "The item data is invalid.",
            EquipmentChangeFailure.NotOwned => "The player does not own this item instance.",
            EquipmentChangeFailure.IncompatibleSlot => "That item is incompatible with this equipment slot.",
            EquipmentChangeFailure.AlreadyEquipped => "The same item instance cannot occupy two slots.",
            EquipmentChangeFailure.SlotEmpty => "That equipment slot is empty.",
            EquipmentChangeFailure.ActionLocked => "Equipment cannot change during an attack or dodge.",
            EquipmentChangeFailure.MissingVisual => "The weapon is missing its equipped visual prefab.",
            _ => "The equipment change could not be completed."
        };
    }

    private static void SetText(Text text, string value)
    {
        if (text != null)
        {
            text.text = value;
            text.gameObject.SetActive(!string.IsNullOrEmpty(value));
        }
    }

    private static void SetButtonState(Button button, bool interactable)
    {
        if (button != null)
        {
            button.interactable = interactable;
        }
    }

    private static bool IsWeaponSlot(EquipmentSlotType slotType)
    {
        return slotType == EquipmentSlotType.MainHand || slotType == EquipmentSlotType.OffHand;
    }

    public static Color GetRarityColor(ItemRarity rarity)
    {
        return rarity switch
        {
            ItemRarity.Uncommon => new Color(0.32f, 0.68f, 0.3f, 1f),
            ItemRarity.Rare => new Color(0.28f, 0.48f, 0.9f, 1f),
            ItemRarity.Epic => new Color(0.62f, 0.34f, 0.82f, 1f),
            ItemRarity.Legendary => new Color(0.9f, 0.52f, 0.16f, 1f),
            _ => new Color(0.58f, 0.54f, 0.46f, 1f)
        };
    }

    public static string GetRarityShortName(ItemRarity rarity)
    {
        return rarity switch
        {
            ItemRarity.Uncommon => "UNCOMMON",
            ItemRarity.Rare => "RARE",
            ItemRarity.Epic => "EPIC",
            ItemRarity.Legendary => "LEGEND",
            _ => "COMMON"
        };
    }

    public static string GetQualityShortName(ItemQuality quality)
    {
        return quality switch
        {
            ItemQuality.Crude => "CRUDE",
            ItemQuality.Fine => "FINE",
            ItemQuality.Masterwork => "MASTER",
            _ => "STANDARD"
        };
    }

    public static string GetItemMonogram(string displayName)
    {
        if (string.IsNullOrWhiteSpace(displayName))
        {
            return "?";
        }

        string[] words = displayName.Split(new[] { ' ', '-', '_' }, StringSplitOptions.RemoveEmptyEntries);
        if (words.Length >= 2)
        {
            return $"{char.ToUpperInvariant(words[0][0])}{char.ToUpperInvariant(words[1][0])}";
        }

        string word = words.Length == 1 ? words[0] : displayName;
        return word.Length >= 2
            ? word.Substring(0, 2).ToUpperInvariant()
            : word.ToUpperInvariant();
    }

    public static string GetSlotDisplayName(EquipmentSlotType slotType)
    {
        return slotType switch
        {
            EquipmentSlotType.MainHand => "Main Hand / Right",
            EquipmentSlotType.OffHand => "Off Hand / Left",
            EquipmentSlotType.Accessory1 => "Accessory I",
            EquipmentSlotType.Accessory2 => "Accessory II",
            EquipmentSlotType.AxeTool => "Axe Tool",
            EquipmentSlotType.PickaxeTool => "Pickaxe Tool",
            EquipmentSlotType.KnifeTool => "Knife Tool",
            _ => slotType.ToString()
        };
    }

    public static string GetSlotMonogram(EquipmentSlotType slotType)
    {
        return slotType switch
        {
            EquipmentSlotType.MainHand => "RH",
            EquipmentSlotType.OffHand => "LH",
            EquipmentSlotType.Head => "HD",
            EquipmentSlotType.Chest => "CH",
            EquipmentSlotType.Hands => "HN",
            EquipmentSlotType.Legs => "LG",
            EquipmentSlotType.Feet => "FT",
            EquipmentSlotType.Accessory1 => "A1",
            EquipmentSlotType.Accessory2 => "A2",
            EquipmentSlotType.AxeTool => "AX",
            EquipmentSlotType.PickaxeTool => "PK",
            EquipmentSlotType.KnifeTool => "KN",
            _ => "--"
        };
    }
}
