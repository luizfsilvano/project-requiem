using UnityEngine;

public readonly struct CombatHit
{
    public CombatHit(
        int damage,
        float poiseDamage,
        float knockbackDistance,
        float knockbackDuration,
        float hitStopDuration,
        float cameraShakeStrength,
        float cameraShakeDuration,
        CombatImpactWeight impactWeight,
        Vector3 point,
        Vector3 direction,
        Transform attacker,
        int comboStep)
    {
        Damage = damage;
        PoiseDamage = poiseDamage;
        KnockbackDistance = knockbackDistance;
        KnockbackDuration = knockbackDuration;
        HitStopDuration = hitStopDuration;
        CameraShakeStrength = cameraShakeStrength;
        CameraShakeDuration = cameraShakeDuration;
        ImpactWeight = impactWeight;
        Point = point;
        Direction = direction.sqrMagnitude > 0.001f ? direction.normalized : Vector3.forward;
        Attacker = attacker;
        ComboStep = comboStep;
    }

    public int Damage { get; }
    public float PoiseDamage { get; }
    public float KnockbackDistance { get; }
    public float KnockbackDuration { get; }
    public float HitStopDuration { get; }
    public float CameraShakeStrength { get; }
    public float CameraShakeDuration { get; }
    public CombatImpactWeight ImpactWeight { get; }
    public Vector3 Point { get; }
    public Vector3 Direction { get; }
    public Transform Attacker { get; }
    public int ComboStep { get; }
}
