using System.Collections.Generic;
using UnityEngine;

public sealed class EnemyAttackHitbox : MonoBehaviour
{
    private const int MaxPulseOverlaps = 32;

    [SerializeField] private Collider triggerCollider;
    [SerializeField] private Renderer hitboxRenderer;
    [SerializeField] private int damage = 12;
    [SerializeField] private float knockbackDistance = 0.18f;
    [SerializeField] private float knockbackDuration = 0.12f;

    private readonly HashSet<PlayerHealth> hitTargets = new();
    private readonly Collider[] pulseOverlaps = new Collider[MaxPulseOverlaps];
    private Transform attacker;

    private void Awake()
    {
        if (triggerCollider == null)
        {
            triggerCollider = GetComponent<Collider>();
        }

        if (hitboxRenderer == null)
        {
            hitboxRenderer = GetComponent<Renderer>();
        }

        DisableHitbox();
    }

    private void Update()
    {
        UpdateDebugVisual();
    }

    public void EnableHitbox(Transform hitAttacker)
    {
        hitTargets.Clear();
        attacker = hitAttacker;

        if (triggerCollider != null)
        {
            triggerCollider.enabled = true;
        }

        UpdateDebugVisual();
    }

    public void DisableHitbox()
    {
        if (triggerCollider != null)
        {
            triggerCollider.enabled = false;
        }

        UpdateDebugVisual();
    }

    public void Pulse(Transform hitAttacker)
    {
        hitTargets.Clear();
        attacker = hitAttacker;

        if (!TryGetWorldBox(out Vector3 center, out Vector3 halfExtents, out Quaternion rotation))
        {
            return;
        }

        int overlapCount = Physics.OverlapBoxNonAlloc(
            center,
            halfExtents,
            pulseOverlaps,
            rotation,
            Physics.AllLayers,
            QueryTriggerInteraction.Collide);

        for (int index = 0; index < overlapCount; index++)
        {
            TryHit(pulseOverlaps[index]);
            pulseOverlaps[index] = null;
        }
    }

    public bool TryGetWorldBox(out Vector3 center, out Vector3 halfExtents, out Quaternion rotation)
    {
        center = transform.position;
        halfExtents = Vector3.zero;
        rotation = transform.rotation;

        if (triggerCollider is not BoxCollider boxCollider)
        {
            return false;
        }

        Vector3 scale = boxCollider.transform.lossyScale;
        scale = new Vector3(Mathf.Abs(scale.x), Mathf.Abs(scale.y), Mathf.Abs(scale.z));
        center = boxCollider.transform.TransformPoint(boxCollider.center);
        halfExtents = Vector3.Scale(boxCollider.size * 0.5f, scale);
        rotation = boxCollider.transform.rotation;
        return halfExtents.x > 0.001f && halfExtents.y > 0.001f && halfExtents.z > 0.001f;
    }

    private void OnTriggerEnter(Collider other)
    {
        TryHit(other);
    }

    private void TryHit(Collider other)
    {
        if (other == null)
        {
            return;
        }

        if (other.GetComponent<MeleeHitbox>() != null)
        {
            return;
        }

        PlayerHealth target = other.GetComponentInParent<PlayerHealth>();
        if (target == null || hitTargets.Contains(target))
        {
            return;
        }

        hitTargets.Add(target);
        Vector3 targetPosition = target.transform.position;
        Vector3 origin = attacker != null ? attacker.position : transform.position;
        Vector3 direction = Vector3.ProjectOnPlane(targetPosition - origin, Vector3.up);
        Vector3 hitPoint = other.ClosestPoint(transform.position);

        target.TakeHit(new CombatHit(
            damage,
            damage,
            knockbackDistance,
            knockbackDuration,
            0f,
            0f,
            0f,
            CombatImpactWeight.Medium,
            hitPoint,
            direction,
            attacker,
            1));
    }

    private void UpdateDebugVisual()
    {
        if (hitboxRenderer != null)
        {
            hitboxRenderer.enabled = DevSettings.ShowHitboxes;
        }
    }
}
