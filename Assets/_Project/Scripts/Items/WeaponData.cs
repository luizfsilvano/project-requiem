using UnityEngine;

[CreateAssetMenu(menuName = "Combat Sandbox/Weapon Data", fileName = "WeaponData")]
public sealed class WeaponData : ItemDefinition
{
    [Header("Identity")]
    public string displayName = "Weapon";
    public GameObject equippedPrefab;

    [Header("Combo")]
    [Min(1)] public int maxComboSteps = 3;
    [Min(0f)] public float comboRecoveryTime = 0.16f;
    [Min(0f)] public float finalRecoveryTime = 0.85f;
    [Min(0f)] public float lungeDuration = 0.12f;
    public WeaponAttackData[] attacks = new WeaponAttackData[3];

    public bool HasWeaponPrefab => equippedPrefab != null;
    public override string DisplayName => displayName;
    public override ItemCategory Category => ItemCategory.Weapon;
    public override EquipmentSlotMask AcceptedEquipmentSlots => base.AcceptedEquipmentSlots == EquipmentSlotMask.None
        ? EquipmentSlotMask.WeaponHands
        : base.AcceptedEquipmentSlots;

    public WeaponAttackData GetAttack(int comboStep)
    {
        if (attacks == null || attacks.Length == 0)
        {
            return null;
        }

        int index = Mathf.Clamp(comboStep - 1, 0, attacks.Length - 1);
        return attacks[index];
    }

    protected override void OnValidate()
    {
        base.OnValidate();
        maxComboSteps = Mathf.Max(1, maxComboSteps);
    }
}
