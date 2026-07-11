using UnityEngine;
using UnityEngine.InputSystem;

[RequireComponent(typeof(Collider))]
public sealed class WeaponPickup : MonoBehaviour
{
    [SerializeField] private WeaponData weaponData;
    [SerializeField] private GameObject equippedWeaponPrefab;
    [SerializeField] private string weaponName = "Bronze Sword";
    [SerializeField, Min(1)] private int quantity = 1;
    [SerializeField] private Transform visualRoot;
    [SerializeField] private float idleRotationSpeed = 45f;

    private PlayerInventory nearbyInventory;
    private PlayerEquipment nearbyEquipment;

    private void Awake()
    {
        Collider pickupCollider = GetComponent<Collider>();
        pickupCollider.isTrigger = true;
    }

    private void Update()
    {
        if (visualRoot != null)
        {
            visualRoot.Rotate(Vector3.up, idleRotationSpeed * Time.deltaTime, Space.World);
        }

        if ((nearbyInventory == null && nearbyEquipment == null)
            || GameplayInputGate.IsBlocked
            || Keyboard.current == null
            || !Keyboard.current.eKey.wasPressedThisFrame)
        {
            return;
        }

        TryPickup(nearbyInventory, nearbyEquipment);
    }

    public void Pickup(PlayerEquipment equipment)
    {
        TryPickup(equipment);
    }

    public void Pickup(PlayerInventory inventory)
    {
        TryPickup(inventory);
    }

    public bool TryPickup(PlayerEquipment equipment)
    {
        return TryPickup(equipment != null ? equipment.GetComponent<PlayerInventory>() : null, equipment);
    }

    public bool TryPickup(PlayerInventory inventory)
    {
        return TryPickup(inventory, inventory != null ? inventory.GetComponent<PlayerEquipment>() : null);
    }

    private bool TryPickup(PlayerInventory inventory, PlayerEquipment equipment)
    {
        bool pickedUp;
        if (weaponData != null && inventory != null)
        {
            pickedUp = inventory.AddWeapon(weaponData, quantity, equipAfterPickup: true, out _);
        }
        else if (equipment == null)
        {
            return false;
        }
        else if (weaponData != null)
        {
            pickedUp = equipment.Equip(weaponData);
        }
        else
        {
            pickedUp = equipment.Equip(equippedWeaponPrefab, weaponName);
        }

        if (!pickedUp)
        {
            return false;
        }

        CombatFeedbackAudio.PlayPickup(transform.position);
        gameObject.SetActive(false);
        return true;
    }

    private void OnTriggerEnter(Collider other)
    {
        PlayerInventory inventory = other.GetComponentInParent<PlayerInventory>();
        PlayerEquipment equipment = other.GetComponentInParent<PlayerEquipment>();
        if (inventory != null || equipment != null)
        {
            nearbyEquipment = equipment;
            nearbyInventory = inventory != null ? inventory : equipment.GetComponent<PlayerInventory>();
        }
    }

    private void OnTriggerExit(Collider other)
    {
        PlayerInventory inventory = other.GetComponentInParent<PlayerInventory>();
        PlayerEquipment equipment = other.GetComponentInParent<PlayerEquipment>();
        if ((equipment != null && equipment == nearbyEquipment)
            || (inventory != null && inventory == nearbyInventory))
        {
            nearbyEquipment = null;
            nearbyInventory = null;
        }
    }
}
