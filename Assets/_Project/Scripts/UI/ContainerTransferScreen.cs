using System;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.InputSystem;
using UnityEngine.UI;

[DefaultExecutionOrder(-4)]
public sealed class ContainerTransferScreen : MonoBehaviour, IItemSlotViewHost
{
    [Header("Screen")]
    [SerializeField] private GameObject screenRoot;
    [SerializeField] private Text containerTitle;

    [Header("Item Grids")]
    [SerializeField] private Transform playerGrid;
    [SerializeField] private Transform containerGrid;
    [SerializeField] private InventoryItemSlotView itemSlotPrefab;
    [SerializeField, Min(1)] private int minimumGridSlots = 20;

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
    [SerializeField] private Button takeButton;
    [SerializeField] private Button storeButton;
    [SerializeField] private Button takeAllButton;
    [SerializeField] private Button closeButton;

    [Header("Drag Feedback")]
    [SerializeField] private RectTransform dragGhost;
    [SerializeField] private Image dragGhostBackground;
    [SerializeField] private Text dragGhostLabel;

    private readonly List<InventoryItemSlotView> playerSlotPool = new();
    private readonly List<InventoryItemSlotView> containerSlotPool = new();
    private PlayerInteractor interactor;
    private PlayerInventory playerInventory;
    private PlayerEquipment playerEquipment;
    private PlayerHealth playerHealth;
    private WorldItemContainer activeContainer;
    private ItemInstance selectedItem;
    private IItemContainer selectedSource;
    private ItemInstance draggedItem;
    private IItemContainer dragSource;
    private int openedFrame;
    private bool listenersBound;

    public bool IsOpen { get; private set; }
    public WorldItemContainer ActiveContainer => activeContainer;
    public ItemInstance SelectedItem => selectedItem;

    private void Awake()
    {
        BindButtonListeners();
        if (screenRoot != null)
        {
            screenRoot.SetActive(false);
        }

        if (dragGhost != null)
        {
            dragGhost.gameObject.SetActive(false);
        }
    }

    private void OnDisable()
    {
        if (Application.isPlaying && IsOpen)
        {
            Close();
        }
    }

    private void OnDestroy()
    {
        UnbindButtonListeners();
        UnsubscribeFromSources();
        if (GameplayInputGate.IsOwnedBy(GameplayModalMode.Container, this))
        {
            GameplayInputGate.TryCloseModal(GameplayModalMode.Container, this, 1);
        }
    }

    private void Update()
    {
        if (!IsOpen)
        {
            return;
        }

        GameplayInputGate.EnsureModalCursor();
        if (activeContainer == null
            || !activeContainer.isActiveAndEnabled
            || interactor == null
            || IsOutsideInteractionRange()
            || playerHealth != null && playerHealth.CurrentHealth <= 0)
        {
            Close();
            return;
        }

        if (Time.frameCount <= openedFrame || Keyboard.current == null)
        {
            return;
        }

        if (Keyboard.current.escapeKey.wasPressedThisFrame
            || Keyboard.current.eKey.wasPressedThisFrame)
        {
            Close();
        }
    }

    public bool Open(WorldItemContainer container, PlayerInteractor ownerInteractor)
    {
        if (IsOpen || container == null || ownerInteractor == null || !container.IsAvailable)
        {
            return false;
        }

        PlayerInventory inventory = ownerInteractor.GetComponent<PlayerInventory>();
        if (inventory == null || !GameplayInputGate.TryOpenModal(GameplayModalMode.Container, this))
        {
            return false;
        }

        interactor = ownerInteractor;
        playerInventory = inventory;
        playerEquipment = inventory.Equipment;
        playerHealth = ownerInteractor.GetComponent<PlayerHealth>();
        activeContainer = container;
        selectedItem = null;
        selectedSource = null;
        openedFrame = Time.frameCount;
        IsOpen = true;

        SubscribeToSources();
        activeContainer.SetUiOpen(true);
        if (screenRoot != null)
        {
            screenRoot.SetActive(true);
        }

        RefreshAll();
        SetFeedback("Select an item, double-click it, or drag it to the other side.");
        return true;
    }

    public void Close()
    {
        if (!IsOpen)
        {
            return;
        }

        EndItemDrag();
        UnsubscribeFromSources();
        WorldItemContainer closingContainer = activeContainer;
        IsOpen = false;
        activeContainer = null;
        selectedItem = null;
        selectedSource = null;
        interactor = null;
        playerInventory = null;
        playerEquipment = null;
        playerHealth = null;

        if (screenRoot != null)
        {
            screenRoot.SetActive(false);
        }

        if (closingContainer != null)
        {
            closingContainer.SetUiOpen(false);
        }

        GameplayInputGate.TryCloseModal(GameplayModalMode.Container, this, 1);
    }

    public bool IsSelected(ItemInstance item, IItemContainer source)
    {
        return item != null
            && selectedItem != null
            && string.Equals(item.InstanceId, selectedItem.InstanceId, StringComparison.Ordinal)
            && ReferenceEquals(source, selectedSource);
    }

    public bool IsEquipped(ItemInstance item)
    {
        return item != null && playerEquipment != null && playerEquipment.IsItemSlotted(item.InstanceId);
    }

    public void SelectItem(ItemInstance item, IItemContainer source)
    {
        selectedItem = item;
        selectedSource = item != null ? source : null;
        RefreshAll();
    }

    public void ExecuteDefaultAction(ItemInstance item, IItemContainer source)
    {
        SelectItem(item, source);
        TransferSelected();
    }

    public void BeginItemDrag(ItemInstance item, IItemContainer source, Vector2 screenPosition)
    {
        if (item == null || source == null)
        {
            return;
        }

        selectedItem = item;
        selectedSource = source;
        draggedItem = item;
        dragSource = source;
        if (dragGhost != null)
        {
            dragGhost.gameObject.SetActive(true);
        }

        if (dragGhostBackground != null)
        {
            dragGhostBackground.color = ItemUiPresentation.GetRarityColor(item.Rarity);
        }

        if (dragGhostLabel != null)
        {
            dragGhostLabel.text = ItemUiPresentation.GetItemMonogram(item.Definition.DisplayName);
        }

        UpdateItemDrag(screenPosition);
        RefreshAll();
    }

    public void UpdateItemDrag(Vector2 screenPosition)
    {
        if (dragGhost == null || !dragGhost.gameObject.activeSelf)
        {
            return;
        }

        RectTransform parentRect = dragGhost.parent as RectTransform;
        if (parentRect != null
            && RectTransformUtility.ScreenPointToLocalPointInRectangle(
                parentRect,
                screenPosition,
                null,
                out Vector2 localPoint))
        {
            dragGhost.anchoredPosition = localPoint;
        }
    }

    public void EndItemDrag()
    {
        draggedItem = null;
        dragSource = null;
        if (dragGhost != null)
        {
            dragGhost.gameObject.SetActive(false);
        }
    }

    public void DropItemOnContainer(IItemContainer destination)
    {
        if (draggedItem == null || dragSource == null || destination == null)
        {
            return;
        }

        if (ReferenceEquals(dragSource, destination))
        {
            SetFeedback("The item is already in that container.");
            return;
        }

        Transfer(draggedItem, dragSource, destination);
    }

    public void TakeSelected()
    {
        if (selectedItem == null || !ReferenceEquals(selectedSource, activeContainer))
        {
            SetFeedback("Select an item from the container first.");
            return;
        }

        Transfer(selectedItem, activeContainer, playerInventory);
    }

    public void StoreSelected()
    {
        if (selectedItem == null || !ReferenceEquals(selectedSource, playerInventory))
        {
            SetFeedback("Select an item from the player inventory first.");
            return;
        }

        Transfer(selectedItem, playerInventory, activeContainer);
    }

    public void TakeAll()
    {
        if (activeContainer == null || playerInventory == null || activeContainer.Count == 0)
        {
            SetFeedback("The container is empty.");
            return;
        }

        bool success = ItemTransferService.TryTransferAll(
            activeContainer,
            playerInventory,
            playerEquipment,
            out ItemTransferFailure failure,
            out int movedCount);
        SetFeedback(success
            ? $"Transferred {movedCount} item{(movedCount == 1 ? string.Empty : "s")} to the player."
            : GetTransferFailureText(failure));
        RefreshAll();
    }

    private void TransferSelected()
    {
        if (selectedItem == null || selectedSource == null)
        {
            return;
        }

        IItemContainer destination = ReferenceEquals(selectedSource, activeContainer)
            ? playerInventory
            : activeContainer;
        Transfer(selectedItem, selectedSource, destination);
    }

    private void Transfer(ItemInstance item, IItemContainer source, IItemContainer destination)
    {
        if (item == null || source == null || destination == null)
        {
            return;
        }

        string itemName = item.Definition != null ? item.Definition.DisplayName : "Item";
        bool success = ItemTransferService.TryTransfer(
            source,
            destination,
            item.InstanceId,
            playerEquipment,
            out ItemTransferFailure failure);
        SetFeedback(success ? $"Transferred {itemName}." : GetTransferFailureText(failure));
        RefreshAll();
    }

    private void SubscribeToSources()
    {
        if (playerInventory != null)
        {
            playerInventory.InventoryChanged -= HandleContentsChanged;
            playerInventory.InventoryChanged += HandleContentsChanged;
        }

        if (activeContainer != null)
        {
            activeContainer.ContentsChanged -= HandleContentsChanged;
            activeContainer.ContentsChanged += HandleContentsChanged;
            activeContainer.BecameUnavailable -= HandleContainerUnavailable;
            activeContainer.BecameUnavailable += HandleContainerUnavailable;
        }
    }

    private void UnsubscribeFromSources()
    {
        if (playerInventory != null)
        {
            playerInventory.InventoryChanged -= HandleContentsChanged;
        }

        if (activeContainer != null)
        {
            activeContainer.ContentsChanged -= HandleContentsChanged;
            activeContainer.BecameUnavailable -= HandleContainerUnavailable;
        }
    }

    private void HandleContentsChanged()
    {
        if (IsOpen)
        {
            RefreshAll();
        }
    }

    private void HandleContainerUnavailable(WorldItemContainer container)
    {
        if (IsOpen && ReferenceEquals(container, activeContainer))
        {
            Close();
        }
    }

    private void RefreshAll()
    {
        ResolveSelectedSource();
        RefreshGrid(playerInventory, playerGrid, playerSlotPool);
        RefreshGrid(activeContainer, containerGrid, containerSlotPool);
        RefreshDetails();

        if (containerTitle != null)
        {
            containerTitle.text = activeContainer != null
                ? activeContainer.ContainerDisplayName.ToUpperInvariant()
                : "CONTAINER";
        }

        bool fromContainer = selectedItem != null && ReferenceEquals(selectedSource, activeContainer);
        bool fromPlayer = selectedItem != null && ReferenceEquals(selectedSource, playerInventory);
        SetButtonState(takeButton, fromContainer);
        SetButtonState(storeButton, fromPlayer && !IsEquipped(selectedItem));
        SetButtonState(takeAllButton, activeContainer != null && activeContainer.Count > 0);
    }

    private void ResolveSelectedSource()
    {
        if (selectedItem == null)
        {
            selectedSource = null;
            return;
        }

        if (playerInventory != null && playerInventory.Contains(selectedItem.InstanceId))
        {
            selectedSource = playerInventory;
            return;
        }

        if (activeContainer != null && activeContainer.Contains(selectedItem.InstanceId))
        {
            selectedSource = activeContainer;
            return;
        }

        selectedItem = null;
        selectedSource = null;
    }

    private void RefreshGrid(
        IItemContainer source,
        Transform grid,
        List<InventoryItemSlotView> pool)
    {
        if (grid == null || itemSlotPrefab == null || source == null)
        {
            return;
        }

        int desiredSlotCount = Mathf.Max(minimumGridSlots, source.Count);
        while (pool.Count < desiredSlotCount)
        {
            InventoryItemSlotView slot = Instantiate(itemSlotPrefab, grid);
            slot.gameObject.SetActive(true);
            pool.Add(slot);
        }

        for (int i = 0; i < pool.Count; i++)
        {
            bool visible = i < desiredSlotCount;
            pool[i].gameObject.SetActive(visible);
            if (visible)
            {
                pool[i].Bind(this, source, i < source.Count ? source.Items[i] : null);
            }
        }
    }

    private void RefreshDetails()
    {
        bool hasItem = selectedItem?.Definition != null;
        SetText(detailName, hasItem ? selectedItem.Definition.DisplayName : "Select an item");
        SetText(detailDescription, ItemUiPresentation.GetDescription(
            selectedItem,
            "Choose an item from either side to inspect and transfer it."));
        SetText(detailType, ItemUiPresentation.GetTypeText(selectedItem));
        SetText(detailRarity, ItemUiPresentation.GetRarityQualityText(selectedItem));
        SetText(detailQuantity, ItemUiPresentation.GetQuantityText(selectedItem));
        SetText(detailDurability, ItemUiPresentation.GetDurabilityText(selectedItem));
        SetText(detailDamage, ItemUiPresentation.GetDamageText(selectedItem));
        SetText(detailMetadata, ItemUiPresentation.GetMetadataText(selectedItem));
    }

    private bool IsOutsideInteractionRange()
    {
        if (activeContainer == null || interactor == null || activeContainer.InteractionPoint == null)
        {
            return true;
        }

        float allowedRange = Mathf.Min(interactor.SearchRange, activeContainer.InteractionRange) + 0.75f;
        Vector3 delta = interactor.transform.position - activeContainer.InteractionPoint.position;
        return delta.sqrMagnitude > allowedRange * allowedRange;
    }

    private void BindButtonListeners()
    {
        if (listenersBound)
        {
            return;
        }

        takeButton?.onClick.AddListener(TakeSelected);
        storeButton?.onClick.AddListener(StoreSelected);
        takeAllButton?.onClick.AddListener(TakeAll);
        closeButton?.onClick.AddListener(Close);
        listenersBound = true;
    }

    private void UnbindButtonListeners()
    {
        if (!listenersBound)
        {
            return;
        }

        takeButton?.onClick.RemoveListener(TakeSelected);
        storeButton?.onClick.RemoveListener(StoreSelected);
        takeAllButton?.onClick.RemoveListener(TakeAll);
        closeButton?.onClick.RemoveListener(Close);
        listenersBound = false;
    }

    private void SetFeedback(string message)
    {
        SetText(feedbackText, message);
    }

    private static string GetTransferFailureText(ItemTransferFailure failure)
    {
        return failure switch
        {
            ItemTransferFailure.InvalidContainer => "The source or destination container is invalid.",
            ItemTransferFailure.SameContainer => "Choose the other container.",
            ItemTransferFailure.InvalidInstanceId => "The item instance ID is invalid.",
            ItemTransferFailure.ItemNotFound => "That item is no longer in the source container.",
            ItemTransferFailure.InvalidItem => "The item instance is invalid.",
            ItemTransferFailure.ItemEquipped => "Unequip the item before storing it.",
            ItemTransferFailure.DuplicateInstanceId => "The destination already contains that item ID.",
            ItemTransferFailure.DestinationFull => "The destination container is full.",
            ItemTransferFailure.DestinationRejected => "The destination cannot receive that item.",
            ItemTransferFailure.TransferFailed => "The transfer failed and the item remained in its original container.",
            ItemTransferFailure.InvariantViolation => "The transfer was cancelled because ownership validation failed.",
            ItemTransferFailure.RollbackFailed => "Transfer rollback failed. Check the Console immediately.",
            _ => "The item could not be transferred."
        };
    }

    private static void SetText(Text text, string value)
    {
        if (text != null)
        {
            text.text = value ?? string.Empty;
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
}
