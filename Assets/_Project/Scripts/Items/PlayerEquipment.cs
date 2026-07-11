using System;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.Serialization;

[DefaultExecutionOrder(-10)]
public sealed class PlayerEquipment : MonoBehaviour
{
    [SerializeField] private Animator animator;
    [SerializeField] private Transform weaponSocket;
    [SerializeField] private BasicMeleeAttack meleeAttack;
    [SerializeField] private PlayerCameraRelativeMovement movement;
    [SerializeField] private Vector3 weaponLocalPosition = new(0.02f, 0.02f, 0.02f);
    [SerializeField] private Vector3 weaponLocalEuler = new(-80f, 180f, 0f);
    [SerializeField] private Vector3 weaponLocalScale = Vector3.one * 100f;

    [SerializeField, HideInInspector] private List<EquipmentSlotState> slots = new();
    [SerializeField, HideInInspector] private EquipmentSlotType activeSlot = EquipmentSlotType.MainHand;
    [FormerlySerializedAs("primaryItemInstanceId")]
    [SerializeField, HideInInspector] private string legacyPrimaryItemInstanceId;
    [FormerlySerializedAs("secondaryItemInstanceId")]
    [SerializeField, HideInInspector] private string legacySecondaryItemInstanceId;

    private PlayerInventory inventory;
    private ItemInstance equippedItemInstance;
    private GameObject equippedWeaponInstance;
    private readonly ItemContainer standaloneEquipmentItems = new();

    public event Action EquipmentChanged;
    public event Action<ItemInstance> ActiveWeaponChanged;

    public bool HasWeapon => equippedWeaponInstance != null && EquippedWeapon != null;
    public bool CanChangeWeapon => (meleeAttack == null || !meleeAttack.IsAttacking)
        && (movement == null || !movement.IsDodging);
    public IReadOnlyList<EquipmentSlotState> Slots => slots;
    public ItemInstance PrimaryWeapon => GetItem(EquipmentSlotType.MainHand);
    public ItemInstance SecondaryWeapon => GetItem(EquipmentSlotType.OffHand);
    public ItemInstance EquippedItem => equippedItemInstance;
    public WeaponData EquippedWeapon { get; private set; }
    public string EquippedWeaponName { get; private set; } = "Unarmed";
    public string PrimaryWeaponName => GetItemName(PrimaryWeapon);
    public string SecondaryWeaponName => GetItemName(SecondaryWeapon);
    public EquipmentSlotType ActiveSlot => activeSlot;

    private void Awake()
    {
        if (animator == null)
        {
            animator = GetComponentInChildren<Animator>();
        }

        if (meleeAttack == null)
        {
            meleeAttack = GetComponent<BasicMeleeAttack>();
        }

        if (movement == null)
        {
            movement = GetComponent<PlayerCameraRelativeMovement>();
        }

        inventory = GetComponent<PlayerInventory>();
        NormalizeSlots();
        EnsureWeaponSocket();
    }

    private void OnValidate()
    {
        NormalizeSlots();
    }

    public void BindInventory(PlayerInventory owner)
    {
        inventory = owner;
        NormalizeSlots();

        foreach (EquipmentSlotState state in slots)
        {
            if (!state.IsEmpty && ResolveOwnedItem(state.ItemInstanceId) == null)
            {
                state.ClearInvalidReference();
            }
        }
    }

    public ItemInstance GetItem(EquipmentSlotType slotType)
    {
        EquipmentSlotState state = GetSlotState(slotType);
        if (state == null || state.IsEmpty)
        {
            return null;
        }

        ItemInstance item = ResolveOwnedItem(state.ItemInstanceId);
        if (item == null)
        {
            state.ClearInvalidReference();
        }

        return item;
    }

    public bool TryGetSlotOf(string instanceId, out EquipmentSlotType slotType)
    {
        slotType = EquipmentSlotType.MainHand;
        if (string.IsNullOrWhiteSpace(instanceId))
        {
            return false;
        }

        foreach (EquipmentSlotState state in slots)
        {
            if (string.Equals(state.ItemInstanceId, instanceId, StringComparison.Ordinal))
            {
                slotType = state.SlotType;
                return true;
            }
        }

        return false;
    }

    public bool IsItemSlotted(string instanceId)
    {
        return TryGetSlotOf(instanceId, out _);
    }

    public bool CanEquip(ItemInstance item, EquipmentSlotType slotType, out EquipmentChangeFailure failure)
    {
        failure = EquipmentChangeFailure.None;
        if (item == null || item.Definition == null || !item.IsValid)
        {
            failure = EquipmentChangeFailure.InvalidItem;
            return false;
        }

        if (!Owns(item))
        {
            failure = EquipmentChangeFailure.NotOwned;
            return false;
        }

        if (!item.Definition.AcceptsSlot(slotType))
        {
            failure = EquipmentChangeFailure.IncompatibleSlot;
            return false;
        }

        if (IsWeaponSlot(slotType)
            && (item.Definition is not WeaponData weaponData || weaponData.equippedPrefab == null))
        {
            failure = EquipmentChangeFailure.MissingVisual;
            return false;
        }

        if (IsWeaponSlot(slotType) && !CanChangeWeapon)
        {
            failure = EquipmentChangeFailure.ActionLocked;
            return false;
        }

        return true;
    }

    public bool TryEquip(ItemInstance item, EquipmentSlotType slotType, bool makeActive, out EquipmentChangeFailure failure)
    {
        if (!CanEquip(item, slotType, out failure))
        {
            return false;
        }

        if (TryGetSlotOf(item.InstanceId, out EquipmentSlotType currentSlot))
        {
            if (currentSlot == slotType)
            {
                if (makeActive && IsWeaponSlot(slotType))
                {
                    return TryActivateSlot(slotType, out failure);
                }

                return true;
            }

            return TryMoveOrSwap(currentSlot, slotType, makeActive, out failure);
        }

        EquipmentSlotState targetState = GetSlotState(slotType);
        ItemInstance previousItem = GetItem(slotType);
        bool replacesDisplayedWeapon = IsWeaponSlot(slotType)
            && previousItem != null
            && equippedItemInstance != null
            && string.Equals(
                previousItem.InstanceId,
                equippedItemInstance.InstanceId,
                StringComparison.Ordinal);
        targetState.SetItem(item);

        bool shouldActivate = IsWeaponSlot(slotType) && (makeActive || replacesDisplayedWeapon);
        if (shouldActivate && !ApplyActiveWeapon(slotType, out failure))
        {
            targetState.SetItem(previousItem);
            return false;
        }

        NotifyEquipmentChanged(shouldActivate);
        return true;
    }

    public bool TryAssignWeapon(ItemInstance item, EquipmentSlotType slotType, bool makeActive)
    {
        return TryEquip(item, slotType, makeActive, out _);
    }

    public bool TryUnequip(EquipmentSlotType slotType, out EquipmentChangeFailure failure)
    {
        failure = EquipmentChangeFailure.None;
        EquipmentSlotState state = GetSlotState(slotType);
        ItemInstance item = GetItem(slotType);
        if (state == null || item == null)
        {
            failure = EquipmentChangeFailure.SlotEmpty;
            return false;
        }

        if (IsWeaponSlot(slotType) && !CanChangeWeapon)
        {
            failure = EquipmentChangeFailure.ActionLocked;
            return false;
        }

        bool removedActiveWeapon = IsWeaponSlot(slotType)
            && equippedItemInstance != null
            && string.Equals(equippedItemInstance.InstanceId, item.InstanceId, StringComparison.Ordinal);
        state.SetItem(null);

        if (removedActiveWeapon)
        {
            UnequipVisual();
            EquipmentSlotType fallbackSlot = OtherWeaponSlot(slotType);
            if (GetItem(fallbackSlot)?.Definition is WeaponData)
            {
                ApplyActiveWeapon(fallbackSlot, out _);
            }
        }

        NotifyEquipmentChanged(removedActiveWeapon);
        return true;
    }

    public bool TryMoveOrSwap(
        EquipmentSlotType fromSlot,
        EquipmentSlotType toSlot,
        bool makeMovedItemActive,
        out EquipmentChangeFailure failure)
    {
        failure = EquipmentChangeFailure.None;
        if (fromSlot == toSlot)
        {
            return makeMovedItemActive && IsWeaponSlot(toSlot)
                ? TryActivateSlot(toSlot, out failure)
                : GetItem(fromSlot) != null;
        }

        ItemInstance sourceItem = GetItem(fromSlot);
        if (sourceItem == null)
        {
            failure = EquipmentChangeFailure.SlotEmpty;
            return false;
        }

        if (!CanEquip(sourceItem, toSlot, out failure))
        {
            return false;
        }

        ItemInstance targetItem = GetItem(toSlot);
        if (targetItem != null && !CanEquip(targetItem, fromSlot, out failure))
        {
            return false;
        }

        if ((IsWeaponSlot(fromSlot) || IsWeaponSlot(toSlot)) && !CanChangeWeapon)
        {
            failure = EquipmentChangeFailure.ActionLocked;
            return false;
        }

        EquipmentSlotState sourceState = GetSlotState(fromSlot);
        EquipmentSlotState targetState = GetSlotState(toSlot);
        EquipmentSlotType previousActiveSlot = activeSlot;
        bool displayedWeaponWasSource = equippedItemInstance != null
            && string.Equals(equippedItemInstance.InstanceId, sourceItem.InstanceId, StringComparison.Ordinal);
        bool displayedWeaponWasTarget = targetItem != null
            && equippedItemInstance != null
            && string.Equals(equippedItemInstance.InstanceId, targetItem.InstanceId, StringComparison.Ordinal);
        sourceState.SetItem(targetItem);
        targetState.SetItem(sourceItem);

        EquipmentSlotType desiredActiveSlot = previousActiveSlot;
        bool activeWeaponChanged = false;
        if (makeMovedItemActive && IsWeaponSlot(toSlot))
        {
            desiredActiveSlot = toSlot;
            activeWeaponChanged = true;
        }
        else if (displayedWeaponWasSource && IsWeaponSlot(toSlot))
        {
            desiredActiveSlot = toSlot;
            activeWeaponChanged = true;
        }
        else if (displayedWeaponWasTarget && IsWeaponSlot(fromSlot))
        {
            desiredActiveSlot = fromSlot;
            activeWeaponChanged = true;
        }

        if (activeWeaponChanged && !ApplyActiveWeapon(desiredActiveSlot, out failure))
        {
            sourceState.SetItem(sourceItem);
            targetState.SetItem(targetItem);
            activeSlot = previousActiveSlot;
            ApplyActiveWeapon(previousActiveSlot, out _);
            return false;
        }

        NotifyEquipmentChanged(activeWeaponChanged);
        return true;
    }

    public bool TryActivateSlot(EquipmentSlotType slotType)
    {
        return TryActivateSlot(slotType, out _);
    }

    public bool TryActivateSlot(EquipmentSlotType slotType, out EquipmentChangeFailure failure)
    {
        failure = EquipmentChangeFailure.None;
        if (!IsWeaponSlot(slotType))
        {
            failure = EquipmentChangeFailure.IncompatibleSlot;
            return false;
        }

        if (!CanChangeWeapon)
        {
            failure = EquipmentChangeFailure.ActionLocked;
            return false;
        }

        ItemInstance item = GetItem(slotType);
        if (item == null)
        {
            failure = EquipmentChangeFailure.SlotEmpty;
            return false;
        }

        if (activeSlot == slotType
            && equippedItemInstance != null
            && equippedWeaponInstance != null
            && string.Equals(equippedItemInstance.InstanceId, item.InstanceId, StringComparison.Ordinal))
        {
            return true;
        }

        if (!ApplyActiveWeapon(slotType, out failure))
        {
            return false;
        }

        NotifyEquipmentChanged(activeWeaponChanged: true);
        return true;
    }

    public bool TryToggleActiveSlot()
    {
        return TryActivateSlot(OtherWeaponSlot(activeSlot));
    }

    public bool TryClearItem(string instanceId)
    {
        return TryGetSlotOf(instanceId, out EquipmentSlotType slotType)
            && TryUnequip(slotType, out _);
    }

    public bool Equip(GameObject weaponPrefab, string weaponName)
    {
        if (!CanChangeWeapon || !EquipVisual(weaponPrefab, weaponName))
        {
            return false;
        }

        equippedItemInstance = null;
        EquippedWeapon = null;
        EquippedWeaponName = weaponName;
        NotifyEquipmentChanged(activeWeaponChanged: true);
        return true;
    }

    public bool Equip(WeaponData weaponData)
    {
        if (!CanChangeWeapon || weaponData == null)
        {
            return false;
        }

        if (inventory != null)
        {
            return inventory.AddWeapon(weaponData, equipAfterPickup: true);
        }

        ItemInstance item = ItemInstance.Create(weaponData);
        if (item == null || !standaloneEquipmentItems.TryAdd(item))
        {
            return false;
        }

        EquipmentSlotType slotType = PrimaryWeapon == null
            ? EquipmentSlotType.MainHand
            : SecondaryWeapon == null
                ? EquipmentSlotType.OffHand
                : activeSlot;
        ItemInstance replacedItem = GetItem(slotType);
        if (!TryEquip(item, slotType, makeActive: true, out _))
        {
            standaloneEquipmentItems.TryRemove(item.InstanceId, out _);
            return false;
        }

        if (replacedItem != null && standaloneEquipmentItems.Contains(replacedItem.InstanceId))
        {
            standaloneEquipmentItems.TryRemove(replacedItem.InstanceId, out _);
        }

        return true;
    }

    public bool TryGetEquippedWeaponPose(out Vector3 localPosition, out Vector3 localEuler, out Vector3 localScale)
    {
        if (equippedWeaponInstance == null)
        {
            localPosition = Vector3.zero;
            localEuler = Vector3.zero;
            localScale = Vector3.one;
            return false;
        }

        Transform weaponTransform = equippedWeaponInstance.transform;
        localPosition = weaponTransform.localPosition;
        localEuler = weaponTransform.localEulerAngles;
        localScale = weaponTransform.localScale;
        return true;
    }

    private bool ApplyActiveWeapon(EquipmentSlotType slotType, out EquipmentChangeFailure failure)
    {
        failure = EquipmentChangeFailure.None;
        ItemInstance item = GetItem(slotType);
        if (item?.Definition is not WeaponData weaponData || weaponData.equippedPrefab == null)
        {
            failure = EquipmentChangeFailure.MissingVisual;
            return false;
        }

        if (!EquipVisual(weaponData.equippedPrefab, weaponData.displayName))
        {
            failure = EquipmentChangeFailure.MissingVisual;
            return false;
        }

        activeSlot = slotType;
        equippedItemInstance = item;
        EquippedWeapon = weaponData;
        EquippedWeaponName = weaponData.displayName;
        return true;
    }

    private ItemInstance ResolveOwnedItem(string instanceId)
    {
        if (string.IsNullOrWhiteSpace(instanceId))
        {
            return null;
        }

        if (inventory != null && inventory.TryGetItem(instanceId, out ItemInstance inventoryItem))
        {
            return inventoryItem;
        }

        if (inventory == null && standaloneEquipmentItems.TryGet(instanceId, out ItemInstance standaloneItem))
        {
            return standaloneItem;
        }

        return null;
    }

    private bool Owns(ItemInstance item)
    {
        ItemInstance ownedItem = ResolveOwnedItem(item.InstanceId);
        return ReferenceEquals(ownedItem, item);
    }

    private EquipmentSlotState GetSlotState(EquipmentSlotType slotType)
    {
        NormalizeSlots();
        for (int i = 0; i < slots.Count; i++)
        {
            if (slots[i].SlotType == slotType)
            {
                return slots[i];
            }
        }

        return null;
    }

    private void NormalizeSlots()
    {
        slots ??= new List<EquipmentSlotState>();
        List<EquipmentSlotState> normalized = new();
        HashSet<string> usedInstanceIds = new(StringComparer.Ordinal);

        foreach (EquipmentSlotType slotType in Enum.GetValues(typeof(EquipmentSlotType)))
        {
            EquipmentSlotState state = slots.Find(candidate => candidate != null && candidate.SlotType == slotType)
                ?? new EquipmentSlotState(slotType);
            if (!state.IsEmpty && !usedInstanceIds.Add(state.ItemInstanceId))
            {
                state.ClearInvalidReference();
            }

            normalized.Add(state);
        }

        slots = normalized;
        MigrateLegacySlot(EquipmentSlotType.MainHand, ref legacyPrimaryItemInstanceId);
        MigrateLegacySlot(EquipmentSlotType.OffHand, ref legacySecondaryItemInstanceId);

        usedInstanceIds.Clear();
        foreach (EquipmentSlotState state in slots)
        {
            if (!state.IsEmpty && !usedInstanceIds.Add(state.ItemInstanceId))
            {
                state.ClearInvalidReference();
            }
        }

        if (!IsWeaponSlot(activeSlot))
        {
            activeSlot = EquipmentSlotType.MainHand;
        }
    }

    private void MigrateLegacySlot(EquipmentSlotType slotType, ref string legacyInstanceId)
    {
        if (string.IsNullOrWhiteSpace(legacyInstanceId)
            || (inventory == null && standaloneEquipmentItems.Count == 0))
        {
            return;
        }

        EquipmentSlotState state = slots.Find(candidate => candidate.SlotType == slotType);
        if (state != null && state.IsEmpty)
        {
            ItemInstance item = ResolveOwnedItem(legacyInstanceId);
            if (item != null)
            {
                state.SetItem(item);
            }
        }

        if (state != null && !state.IsEmpty)
        {
            legacyInstanceId = string.Empty;
        }
    }

    private bool EquipVisual(GameObject weaponPrefab, string weaponName)
    {
        if (weaponPrefab == null)
        {
            return false;
        }

        EnsureWeaponSocket();
        if (weaponSocket == null)
        {
            return false;
        }

        GameObject nextWeapon = Instantiate(weaponPrefab, weaponSocket);
        if (nextWeapon == null)
        {
            return false;
        }

        nextWeapon.name = weaponName;
        nextWeapon.transform.localPosition = weaponLocalPosition;
        nextWeapon.transform.localRotation = Quaternion.Euler(weaponLocalEuler);
        nextWeapon.transform.localScale = weaponLocalScale;
        DisableEquippedPhysics(nextWeapon);

        if (equippedWeaponInstance != null)
        {
            Destroy(equippedWeaponInstance);
        }

        equippedWeaponInstance = nextWeapon;
        return true;
    }

    private void UnequipVisual()
    {
        if (equippedWeaponInstance != null)
        {
            Destroy(equippedWeaponInstance);
        }

        equippedWeaponInstance = null;
        equippedItemInstance = null;
        EquippedWeapon = null;
        EquippedWeaponName = "Unarmed";
    }

    private void EnsureWeaponSocket()
    {
        if (weaponSocket != null)
        {
            return;
        }

        Transform rightHand = animator != null ? animator.GetBoneTransform(HumanBodyBones.RightHand) : null;
        if (rightHand == null)
        {
            return;
        }

        GameObject socketObject = new("WeaponSocket");
        socketObject.transform.SetParent(rightHand);
        socketObject.transform.localPosition = Vector3.zero;
        socketObject.transform.localRotation = Quaternion.identity;
        socketObject.transform.localScale = Vector3.one;
        weaponSocket = socketObject.transform;
    }

    private void NotifyEquipmentChanged(bool activeWeaponChanged)
    {
        EquipmentChanged?.Invoke();
        if (activeWeaponChanged)
        {
            ActiveWeaponChanged?.Invoke(equippedItemInstance);
        }
    }

    private static bool IsWeaponSlot(EquipmentSlotType slotType)
    {
        return slotType == EquipmentSlotType.MainHand || slotType == EquipmentSlotType.OffHand;
    }

    private static EquipmentSlotType OtherWeaponSlot(EquipmentSlotType slotType)
    {
        return slotType == EquipmentSlotType.MainHand
            ? EquipmentSlotType.OffHand
            : EquipmentSlotType.MainHand;
    }

    private static string GetItemName(ItemInstance item)
    {
        return item?.Definition != null ? item.Definition.DisplayName : "Empty";
    }

    private static void DisableEquippedPhysics(GameObject weapon)
    {
        foreach (Collider collider in weapon.GetComponentsInChildren<Collider>(true))
        {
            collider.enabled = false;
        }

        foreach (Rigidbody body in weapon.GetComponentsInChildren<Rigidbody>(true))
        {
            body.isKinematic = true;
            body.useGravity = false;
            body.detectCollisions = false;
        }
    }
}
