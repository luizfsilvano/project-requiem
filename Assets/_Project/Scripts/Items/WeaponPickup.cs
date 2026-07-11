using UnityEngine;

[RequireComponent(typeof(Collider))]
public sealed class WeaponPickup : InteractableBehaviour
{
    [SerializeField] private WeaponData weaponData;
    [SerializeField] private GameObject equippedWeaponPrefab;
    [SerializeField] private string weaponName = "Bronze Sword";
    [SerializeField, Min(1)] private int quantity = 1;
    [SerializeField] private Transform visualRoot;
    [SerializeField] private float idleRotationSpeed = 45f;

    private bool consumed;

    public override InteractionKind Kind => InteractionKind.Pickup;
    public override string ActionName => "Coletar";
    public override bool IsAvailable => base.IsAvailable && !consumed && weaponData != null;
    public GameObject LegacyEquippedWeaponPrefab => equippedWeaponPrefab;

    private string PickupDisplayName => weaponData != null
        ? weaponData.DisplayName
        : string.IsNullOrWhiteSpace(weaponName)
            ? DisplayName
            : weaponName;

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
    }

    public override bool CanInteract(InteractionContext context)
    {
        return base.CanInteract(context)
            && !consumed
            && weaponData != null
            && context.GetActorComponent<PlayerInventory>() != null;
    }

    public override InteractionPromptData GetPrompt(InteractionContext context)
    {
        string targetName = PickupDisplayName;
        return new InteractionPromptData(
            Kind,
            ActionName,
            targetName,
            $"E — {ActionName} {targetName}");
    }

    public override bool TryInteract(InteractionContext context)
    {
        return CanInteract(context)
            && TryPickup(context.GetActorComponent<PlayerInventory>());
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
        return TryPickup(equipment != null ? equipment.GetComponent<PlayerInventory>() : null);
    }

    public bool TryPickup(PlayerInventory inventory)
    {
        if (consumed || !isActiveAndEnabled || inventory == null || weaponData == null)
        {
            return false;
        }

        consumed = true;
        if (!inventory.AddWeapon(weaponData, quantity, equipAfterPickup: true, out _))
        {
            consumed = false;
            return false;
        }

        NotifyInteractionStateChanged();
        CombatFeedbackAudio.PlayPickup(transform.position);
        gameObject.SetActive(false);
        return true;
    }

    protected override void OnValidate()
    {
        base.OnValidate();
        quantity = Mathf.Max(1, quantity);
    }
}
