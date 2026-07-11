using System;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.InputSystem;
using UnityEngine.Serialization;

[DefaultExecutionOrder(10)]
public sealed class PlayerInventory : MonoBehaviour, IItemContainer
{
    [SerializeField] private PlayerEquipment equipment;
    [SerializeField] private BasicMeleeAttack meleeAttack;
    [SerializeField] private PlayerCameraRelativeMovement movement;
    [SerializeField] private ItemContainer itemContainer = new();

    [FormerlySerializedAs("weapons")]
    [SerializeField, HideInInspector] private List<WeaponData> legacyWeapons = new();
    [FormerlySerializedAs("equippedWeaponIndex")]
    [SerializeField, HideInInspector] private int legacyEquippedWeaponIndex = -1;

    private readonly List<WeaponData> weaponDefinitionsView = new();

    public event Action InventoryChanged;

    public IItemContainer Container => this;
    public IReadOnlyList<ItemInstance> Items => itemContainer.Items;
    public int ItemCount => itemContainer.Count;
    public int Count => ItemCount;
    public int Capacity => itemContainer.Capacity;
    public int WeaponCount => CountWeapons();
    public IReadOnlyList<WeaponData> Weapons
    {
        get
        {
            weaponDefinitionsView.Clear();
            foreach (ItemInstance item in itemContainer.Items)
            {
                if (item?.Definition is WeaponData weaponData)
                {
                    weaponDefinitionsView.Add(weaponData);
                }
            }

            return weaponDefinitionsView;
        }
    }

    public ItemInstance EquippedItem => equipment != null ? equipment.EquippedItem : null;
    public WeaponData EquippedWeapon => equipment != null ? equipment.EquippedWeapon : null;
    public string EquippedWeaponName => equipment != null ? equipment.EquippedWeaponName : "Unarmed";
    public string PrimaryWeaponName => equipment != null ? equipment.PrimaryWeaponName : "Empty";
    public string SecondaryWeaponName => equipment != null ? equipment.SecondaryWeaponName : "Empty";
    public WeaponEquipmentSlot ActiveWeaponSlot => equipment != null ? equipment.ActiveSlot : WeaponEquipmentSlot.Primary;
    public bool CanChangeWeapon => equipment != null
        ? equipment.CanChangeWeapon
        : (meleeAttack == null || !meleeAttack.IsAttacking) && (movement == null || !movement.IsDodging);

    private void Awake()
    {
        itemContainer ??= new ItemContainer();
        itemContainer.NormalizeContents();

        if (equipment == null)
        {
            equipment = GetComponent<PlayerEquipment>();
        }

        if (meleeAttack == null)
        {
            meleeAttack = GetComponent<BasicMeleeAttack>();
        }

        if (movement == null)
        {
            movement = GetComponent<PlayerCameraRelativeMovement>();
        }

        equipment?.BindInventory(this);
        MigrateLegacyWeapons();
        AssignAvailableWeaponSlots();
    }

    private void OnEnable()
    {
        itemContainer?.NormalizeContents();
        equipment?.BindInventory(this);
    }

    private void Update()
    {
        if (DevSettings.ConsoleOpen || Keyboard.current == null)
        {
            return;
        }

        if (Keyboard.current.digit1Key.wasPressedThisFrame || Keyboard.current.numpad1Key.wasPressedThisFrame)
        {
            TryActivateWeaponSlot(WeaponEquipmentSlot.Primary);
            return;
        }

        if (Keyboard.current.digit2Key.wasPressedThisFrame || Keyboard.current.numpad2Key.wasPressedThisFrame)
        {
            TryActivateWeaponSlot(WeaponEquipmentSlot.Secondary);
            return;
        }

        if (Keyboard.current.qKey.wasPressedThisFrame)
        {
            ToggleActiveWeapon();
        }
    }

    public bool AddItem(ItemInstance item, bool autoAssignWeapon = true, bool equipAfterPickup = true)
    {
        if (!itemContainer.TryAdd(item))
        {
            return false;
        }

        bool activatedWeapon = false;
        if (autoAssignWeapon
            && item.Definition is WeaponData
            && equipment != null
            && TryGetEmptyWeaponSlot(out WeaponEquipmentSlot slot))
        {
            bool makeActive = equipAfterPickup && CanChangeWeapon;
            equipment.TryAssignWeapon(item, slot, makeActive);
            activatedWeapon = makeActive
                && equipment.EquippedItem != null
                && equipment.EquippedItem.InstanceId == item.InstanceId;
        }

        if (activatedWeapon)
        {
            CombatFeedbackAudio.PlayEquip(transform.position);
        }

        InventoryChanged?.Invoke();
        return true;
    }

    public bool CanAdd(ItemInstance item)
    {
        return itemContainer.CanAdd(item);
    }

    public bool Contains(string instanceId)
    {
        return ContainsItem(instanceId);
    }

    public bool TryGet(string instanceId, out ItemInstance item)
    {
        return TryGetItem(instanceId, out item);
    }

    public bool TryAdd(ItemInstance item)
    {
        return AddItem(item, autoAssignWeapon: true, equipAfterPickup: false);
    }

    public bool TryRemove(string instanceId, out ItemInstance item)
    {
        return TryRemoveItem(instanceId, out item);
    }

    public bool TryMoveTo(string instanceId, IItemContainer destination)
    {
        return TryMoveItemTo(instanceId, destination);
    }

    public bool AddWeapon(WeaponData weaponData, bool equipAfterPickup = true)
    {
        return AddWeapon(weaponData, 1, equipAfterPickup, out _);
    }

    public bool AddWeapon(WeaponData weaponData, int quantity, bool equipAfterPickup, out ItemInstance addedInstance)
    {
        addedInstance = ItemInstance.Create(weaponData, quantity);
        if (addedInstance == null || !AddItem(addedInstance, autoAssignWeapon: true, equipAfterPickup))
        {
            addedInstance = null;
            return false;
        }

        return true;
    }

    public bool TryActivateWeaponSlot(WeaponEquipmentSlot slot)
    {
        if (equipment == null || !equipment.TryActivateSlot(slot))
        {
            return false;
        }

        CombatFeedbackAudio.PlayEquip(transform.position);
        InventoryChanged?.Invoke();
        return true;
    }

    public bool ToggleActiveWeapon()
    {
        if (equipment == null || !equipment.TryToggleActiveSlot())
        {
            return false;
        }

        CombatFeedbackAudio.PlayEquip(transform.position);
        InventoryChanged?.Invoke();
        return true;
    }

    public bool EquipWeapon(int weaponIndex)
    {
        ItemInstance item = GetWeaponAtIndex(weaponIndex);
        if (item == null || equipment == null || !CanChangeWeapon)
        {
            return false;
        }

        if (equipment.PrimaryWeapon?.InstanceId == item.InstanceId)
        {
            return TryActivateWeaponSlot(WeaponEquipmentSlot.Primary);
        }

        if (equipment.SecondaryWeapon?.InstanceId == item.InstanceId)
        {
            return TryActivateWeaponSlot(WeaponEquipmentSlot.Secondary);
        }

        WeaponEquipmentSlot targetSlot = equipment.PrimaryWeapon == null
            ? WeaponEquipmentSlot.Primary
            : equipment.SecondaryWeapon == null
                ? WeaponEquipmentSlot.Secondary
                : equipment.ActiveSlot;
        if (!equipment.TryAssignWeapon(item, targetSlot, makeActive: true))
        {
            return false;
        }

        CombatFeedbackAudio.PlayEquip(transform.position);
        InventoryChanged?.Invoke();
        return true;
    }

    public bool TryGetItem(string instanceId, out ItemInstance item)
    {
        return itemContainer.TryGet(instanceId, out item);
    }

    public bool ContainsItem(string instanceId)
    {
        return itemContainer.Contains(instanceId);
    }

    public bool TryRemoveItem(string instanceId, out ItemInstance removedItem)
    {
        removedItem = null;
        if (!itemContainer.TryGet(instanceId, out ItemInstance item))
        {
            return false;
        }

        if (equipment != null && equipment.IsItemSlotted(instanceId) && !equipment.TryClearItem(instanceId))
        {
            return false;
        }

        if (!itemContainer.TryRemove(instanceId, out removedItem))
        {
            return false;
        }

        InventoryChanged?.Invoke();
        return true;
    }

    public bool TryMoveItemTo(string instanceId, IItemContainer destination)
    {
        if (destination == null
            || ReferenceEquals(destination, this)
            || (equipment != null && equipment.IsItemSlotted(instanceId)))
        {
            return false;
        }

        if (!itemContainer.TryMoveTo(instanceId, destination))
        {
            return false;
        }

        InventoryChanged?.Invoke();
        return true;
    }

    private void MigrateLegacyWeapons()
    {
        if (legacyWeapons == null || legacyWeapons.Count == 0)
        {
            return;
        }

        foreach (WeaponData weaponData in legacyWeapons)
        {
            ItemInstance item = ItemInstance.Create(weaponData);
            if (item != null)
            {
                itemContainer.TryAdd(item);
            }
        }

        legacyWeapons.Clear();
    }

    private void AssignAvailableWeaponSlots()
    {
        if (equipment == null)
        {
            return;
        }

        foreach (ItemInstance item in itemContainer.Items)
        {
            if (item?.Definition is not WeaponData || equipment.IsItemSlotted(item.InstanceId))
            {
                continue;
            }

            if (!TryGetEmptyWeaponSlot(out WeaponEquipmentSlot slot))
            {
                break;
            }

            equipment.TryAssignWeapon(item, slot, makeActive: false);
        }

        if (equipment.EquippedItem != null || !CanChangeWeapon)
        {
            legacyEquippedWeaponIndex = -1;
            return;
        }

        ItemInstance legacyEquippedItem = GetWeaponAtIndex(legacyEquippedWeaponIndex);
        if (legacyEquippedItem != null && equipment.SecondaryWeapon?.InstanceId == legacyEquippedItem.InstanceId)
        {
            equipment.TryActivateSlot(WeaponEquipmentSlot.Secondary);
        }
        else if (equipment.PrimaryWeapon != null)
        {
            equipment.TryActivateSlot(WeaponEquipmentSlot.Primary);
        }
        else if (equipment.SecondaryWeapon != null)
        {
            equipment.TryActivateSlot(WeaponEquipmentSlot.Secondary);
        }

        legacyEquippedWeaponIndex = -1;
    }

    private bool TryGetEmptyWeaponSlot(out WeaponEquipmentSlot slot)
    {
        slot = WeaponEquipmentSlot.Primary;
        if (equipment == null)
        {
            return false;
        }

        if (equipment.PrimaryWeapon == null)
        {
            return true;
        }

        slot = WeaponEquipmentSlot.Secondary;
        return equipment.SecondaryWeapon == null;
    }

    private ItemInstance GetWeaponAtIndex(int weaponIndex)
    {
        if (weaponIndex < 0)
        {
            return null;
        }

        int currentIndex = 0;
        foreach (ItemInstance item in itemContainer.Items)
        {
            if (item?.Definition is not WeaponData)
            {
                continue;
            }

            if (currentIndex == weaponIndex)
            {
                return item;
            }

            currentIndex++;
        }

        return null;
    }

    private int CountWeapons()
    {
        int count = 0;
        foreach (ItemInstance item in itemContainer.Items)
        {
            if (item?.Definition is WeaponData)
            {
                count++;
            }
        }

        return count;
    }
}
