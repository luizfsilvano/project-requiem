using UnityEngine;

[System.Serializable]
public sealed class WeaponAttackData
{
    [Header("Timing")]
    [Min(0.05f)] public float duration = 0.38f;
    [Range(0.05f, 0.95f)] public float hitMoment = 0.74f;
    [Min(0f)] public float staminaCost = 14f;

    [Header("Impact")]
    [Min(0)] public int damage = 28;
    [Min(0f)] public float poiseDamage = 60f;
    [Min(0f)] public float knockbackDistance = 0.24f;
    public CombatImpactWeight impactWeight = CombatImpactWeight.Medium;

    [Header("Movement")]
    [Min(0f)] public float lungeDistance = 0.28f;

    [Header("Hitbox")]
    public Vector3 hitboxScaleMultiplier = Vector3.one;
    public Vector3 hitboxLocalOffset = Vector3.zero;
}
