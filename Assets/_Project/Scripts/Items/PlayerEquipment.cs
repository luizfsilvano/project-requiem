using System;
using UnityEngine;

public enum WeaponEquipmentSlot
{
    Primary,
    Secondary
}

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

    [SerializeField, HideInInspector] private string primaryItemInstanceId;
    [SerializeField, HideInInspector] private string secondaryItemInstanceId;
    [SerializeField, HideInInspector] private WeaponEquipmentSlot activeSlot = WeaponEquipmentSlot.Primary;

    private PlayerInventory inventory;
    private ItemInstance primaryWeaponInstance;
    private ItemInstance secondaryWeaponInstance;
    private ItemInstance equippedItemInstance;
    private GameObject equippedWeaponInstance;
    private readonly ItemContainer standaloneEquipmentItems = new();

    public bool HasWeapon => equippedWeaponInstance != null;
    public bool CanChangeWeapon => (meleeAttack == null || !meleeAttack.IsAttacking)
        && (movement == null || !movement.IsDodging);
    public ItemInstance PrimaryWeapon => ResolveSlotItem(WeaponEquipmentSlot.Primary);
    public ItemInstance SecondaryWeapon => ResolveSlotItem(WeaponEquipmentSlot.Secondary);
    public ItemInstance EquippedItem => equippedItemInstance;
    public WeaponData EquippedWeapon { get; private set; }
    public string EquippedWeaponName { get; private set; } = "Unarmed";
    public string PrimaryWeaponName => GetWeaponName(PrimaryWeapon);
    public string SecondaryWeaponName => GetWeaponName(SecondaryWeapon);
    public WeaponEquipmentSlot ActiveSlot => activeSlot;

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
        EnsureWeaponSocket();
    }

    public void BindInventory(PlayerInventory owner)
    {
        inventory = owner;
        primaryWeaponInstance = ResolveFromInventory(primaryItemInstanceId);
        secondaryWeaponInstance = ResolveFromInventory(secondaryItemInstanceId);
    }

    public bool TryAssignWeapon(ItemInstance item, WeaponEquipmentSlot slot, bool makeActive)
    {
        if (!TryGetWeaponData(item, out _))
        {
            return false;
        }

        if (inventory != null
            && (!inventory.TryGetItem(item.InstanceId, out ItemInstance ownedItem)
                || !ReferenceEquals(ownedItem, item)))
        {
            return false;
        }

        WeaponEquipmentSlot otherSlot = slot == WeaponEquipmentSlot.Primary
            ? WeaponEquipmentSlot.Secondary
            : WeaponEquipmentSlot.Primary;
        ItemInstance otherItem = ResolveSlotItem(otherSlot);
        if (otherItem != null && otherItem.InstanceId == item.InstanceId)
        {
            return false;
        }

        ItemInstance previousItem = ResolveSlotItem(slot);
        bool replacesActiveItem = activeSlot == slot
            && previousItem != null
            && previousItem.InstanceId != item.InstanceId
            && equippedItemInstance != null;
        if (replacesActiveItem && !CanChangeWeapon)
        {
            return false;
        }

        SetSlotItem(slot, item);

        if (makeActive || replacesActiveItem)
        {
            if (!TryActivateSlot(slot))
            {
                SetSlotItem(slot, previousItem);
                return false;
            }
        }

        return true;
    }

    public bool TryActivateSlot(WeaponEquipmentSlot slot)
    {
        if (!CanChangeWeapon || !TryGetWeaponData(ResolveSlotItem(slot), out WeaponData weaponData))
        {
            return false;
        }

        ItemInstance item = ResolveSlotItem(slot);
        if (activeSlot == slot
            && equippedItemInstance != null
            && equippedItemInstance.InstanceId == item.InstanceId
            && equippedWeaponInstance != null)
        {
            return true;
        }

        if (!EquipVisual(weaponData.equippedPrefab, weaponData.displayName))
        {
            return false;
        }

        activeSlot = slot;
        equippedItemInstance = item;
        EquippedWeapon = weaponData;
        EquippedWeaponName = weaponData.displayName;
        return true;
    }

    public bool TryToggleActiveSlot()
    {
        WeaponEquipmentSlot target = activeSlot == WeaponEquipmentSlot.Primary
            ? WeaponEquipmentSlot.Secondary
            : WeaponEquipmentSlot.Primary;
        return TryActivateSlot(target);
    }

    public bool IsItemSlotted(string instanceId)
    {
        if (string.IsNullOrWhiteSpace(instanceId))
        {
            return false;
        }

        return primaryItemInstanceId == instanceId || secondaryItemInstanceId == instanceId;
    }

    public bool TryClearItem(string instanceId)
    {
        if (!CanChangeWeapon || !IsItemSlotted(instanceId))
        {
            return false;
        }

        bool removedActiveItem = equippedItemInstance != null && equippedItemInstance.InstanceId == instanceId;
        if (primaryItemInstanceId == instanceId)
        {
            SetSlotItem(WeaponEquipmentSlot.Primary, null);
        }

        if (secondaryItemInstanceId == instanceId)
        {
            SetSlotItem(WeaponEquipmentSlot.Secondary, null);
        }

        if (!removedActiveItem)
        {
            return true;
        }

        UnequipVisual();
        WeaponEquipmentSlot fallbackSlot = activeSlot == WeaponEquipmentSlot.Primary
            ? WeaponEquipmentSlot.Secondary
            : WeaponEquipmentSlot.Primary;
        TryActivateSlot(fallbackSlot);
        return true;
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

        WeaponEquipmentSlot slot = PrimaryWeapon == null
            ? WeaponEquipmentSlot.Primary
            : SecondaryWeapon == null
                ? WeaponEquipmentSlot.Secondary
                : activeSlot;
        ItemInstance replacedItem = ResolveSlotItem(slot);
        if (!TryAssignWeapon(item, slot, makeActive: true))
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

    private ItemInstance ResolveSlotItem(WeaponEquipmentSlot slot)
    {
        string instanceId = slot == WeaponEquipmentSlot.Primary ? primaryItemInstanceId : secondaryItemInstanceId;
        ItemInstance cachedItem = slot == WeaponEquipmentSlot.Primary ? primaryWeaponInstance : secondaryWeaponInstance;

        if (cachedItem != null && cachedItem.InstanceId == instanceId)
        {
            if (inventory == null
                || (inventory.TryGetItem(instanceId, out ItemInstance ownedItem)
                    && ReferenceEquals(ownedItem, cachedItem)))
            {
                return cachedItem;
            }
        }

        ItemInstance resolvedItem = ResolveFromInventory(instanceId);
        if (slot == WeaponEquipmentSlot.Primary)
        {
            primaryWeaponInstance = resolvedItem;
        }
        else
        {
            secondaryWeaponInstance = resolvedItem;
        }

        if (resolvedItem == null && inventory != null)
        {
            if (slot == WeaponEquipmentSlot.Primary)
            {
                primaryItemInstanceId = string.Empty;
            }
            else
            {
                secondaryItemInstanceId = string.Empty;
            }
        }

        return resolvedItem;
    }

    private ItemInstance ResolveFromInventory(string instanceId)
    {
        if (inventory != null && inventory.TryGetItem(instanceId, out ItemInstance item))
        {
            return item;
        }

        return null;
    }

    private void SetSlotItem(WeaponEquipmentSlot slot, ItemInstance item)
    {
        if (slot == WeaponEquipmentSlot.Primary)
        {
            primaryWeaponInstance = item;
            primaryItemInstanceId = item != null ? item.InstanceId : string.Empty;
        }
        else
        {
            secondaryWeaponInstance = item;
            secondaryItemInstanceId = item != null ? item.InstanceId : string.Empty;
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

        if (equippedWeaponInstance != null)
        {
            Destroy(equippedWeaponInstance);
        }

        equippedWeaponInstance = Instantiate(weaponPrefab, weaponSocket);
        equippedWeaponInstance.name = weaponName;
        equippedWeaponInstance.transform.localPosition = weaponLocalPosition;
        equippedWeaponInstance.transform.localRotation = Quaternion.Euler(weaponLocalEuler);
        equippedWeaponInstance.transform.localScale = weaponLocalScale;
        DisableEquippedPhysics(equippedWeaponInstance);
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

    private static bool TryGetWeaponData(ItemInstance item, out WeaponData weaponData)
    {
        weaponData = item != null ? item.Definition as WeaponData : null;
        return weaponData != null && weaponData.equippedPrefab != null;
    }

    private static string GetWeaponName(ItemInstance item)
    {
        return item?.Definition is WeaponData weaponData ? weaponData.displayName : "Empty";
    }

    private static void DisableEquippedPhysics(GameObject weapon)
    {
        foreach (Collider collider in weapon.GetComponentsInChildren<Collider>())
        {
            collider.enabled = false;
        }

        foreach (Rigidbody body in weapon.GetComponentsInChildren<Rigidbody>())
        {
            body.isKinematic = true;
            body.useGravity = false;
        }
    }
}
