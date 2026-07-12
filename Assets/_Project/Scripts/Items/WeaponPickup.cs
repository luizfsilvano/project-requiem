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
    private Collider pickupCollider;
    private Renderer[] visualRenderers = System.Array.Empty<Renderer>();

    public override InteractionKind Kind => InteractionKind.Pickup;
    public override string ActionName => "Coletar";
    public override bool IsAvailable => base.IsAvailable && !consumed && weaponData != null;
    public GameObject LegacyEquippedWeaponPrefab => equippedWeaponPrefab;
    public bool IsCollected => consumed;

    private string PickupDisplayName => weaponData != null
        ? weaponData.DisplayName
        : string.IsNullOrWhiteSpace(weaponName)
            ? DisplayName
            : weaponName;

    private void Awake()
    {
        pickupCollider = GetComponent<Collider>();
        pickupCollider.isTrigger = true;
        visualRenderers = visualRoot != null
            ? visualRoot.GetComponentsInChildren<Renderer>(true)
            : GetComponentsInChildren<Renderer>(true);
    }

    private void Update()
    {
        if (!consumed && visualRoot != null)
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

        SetCollectedState(true);
        CombatFeedbackAudio.PlayPickup(transform.position);
        return true;
    }

    public void SetCollectedState(bool collected)
    {
        consumed = collected;
        if (pickupCollider == null)
        {
            pickupCollider = GetComponent<Collider>();
        }

        if (pickupCollider != null)
        {
            pickupCollider.enabled = !collected;
        }

        if (visualRoot != null && visualRoot != transform)
        {
            visualRoot.gameObject.SetActive(!collected);
        }
        else
        {
            if (visualRenderers == null || visualRenderers.Length == 0)
            {
                visualRenderers = GetComponentsInChildren<Renderer>(true);
            }
            for (int i = 0; i < visualRenderers.Length; i++)
            {
                if (visualRenderers[i] != null)
                {
                    visualRenderers[i].enabled = !collected;
                }
            }
        }

        NotifyInteractionStateChanged();
    }

    protected override void OnValidate()
    {
        base.OnValidate();
        quantity = Mathf.Max(1, quantity);
    }
}
