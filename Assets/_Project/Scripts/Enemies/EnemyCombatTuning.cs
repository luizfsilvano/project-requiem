using UnityEngine;

[CreateAssetMenu(fileName = "EnemyCombatTuning", menuName = "Combat Sandbox/Enemy Combat Tuning")]
public sealed class EnemyCombatTuning : ScriptableObject
{
    [Header("Movement")]
    [Min(0.1f)] public float moveSpeed = 2.4f;
    [Min(0.1f)] public float rotationSpeed = 9f;
    [Min(0.1f)] public float stoppingDistance = 1.65f;
    [Min(0.1f)] public float attackRange = 1.9f;

    [Header("Attack Timing")]
    [Min(0.05f)] public float attackWindup = 0.78f;
    [Min(0.03f)] public float attackActiveTime = 0.12f;
    [Min(0.05f)] public float attackRecovery = 0.35f;
    [Min(0.1f)] public float attackCooldown = 1.15f;

    [Header("Attack Debug")]
    public bool showAttackTelegraph;

    [Header("Stagger")]
    [Min(0.05f)] public float staggerLockDuration = 0.85f;
}
