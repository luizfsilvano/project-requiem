using System.Collections.Generic;
using UnityEngine;

public sealed class MeleeHitbox : MonoBehaviour
{
    [SerializeField] private Collider triggerCollider;
    [SerializeField] private int firstHitDamage = 18;
    [SerializeField] private int secondHitDamage = 26;
    [SerializeField] private int weaponFirstHitDamage = 28;
    [SerializeField] private int weaponSecondHitDamage = 38;
    [SerializeField] private int weaponThirdHitDamage = 46;
    [SerializeField] private float firstHitPoiseDamage = 45f;
    [SerializeField] private float secondHitPoiseDamage = 70f;
    [SerializeField] private float weaponFirstHitPoiseDamage = 60f;
    [SerializeField] private float weaponSecondHitPoiseDamage = 85f;
    [SerializeField] private float weaponThirdHitPoiseDamage = 105f;
    [SerializeField] private float firstHitKnockback = 0.18f;
    [SerializeField] private float secondHitKnockback = 0.34f;
    [SerializeField] private float weaponFirstHitKnockback = 0.24f;
    [SerializeField] private float weaponSecondHitKnockback = 0.42f;
    [SerializeField] private float weaponThirdHitKnockback = 0.52f;
    [SerializeField] private float knockbackDuration = 0.12f;
    [SerializeField] private float hitStopDuration = 0.045f;
    [SerializeField] private float cameraShakeStrength = 0.045f;
    [SerializeField] private float cameraShakeDuration = 0.08f;
    [SerializeField] private Vector3 weaponThirdHitboxScaleMultiplier = new(1.85f, 1f, 1.18f);
    [SerializeField] private Vector3 weaponThirdHitboxLocalOffset = new(0f, 0f, 0.18f);

    private readonly HashSet<TrainingDummyHealth> hitTargets = new();
    private int currentComboStep = 1;
    private bool isWeaponAttack;
    private WeaponAttackData currentWeaponAttack;
    private Transform attacker;
    private Vector3 defaultLocalPosition;
    private Vector3 defaultLocalScale;
    private bool cachedDefaultShape;
    private Renderer hitboxRenderer;

    private void Awake()
    {
        CacheDefaultHitboxShape();

        if (triggerCollider == null)
        {
            triggerCollider = GetComponent<Collider>();
        }

        hitboxRenderer = GetComponent<Renderer>();
        DisableHitbox();
    }

    private void Update()
    {
        UpdateDebugVisual();
    }

    public void EnableHitbox(int comboStep, Transform hitAttacker, bool weaponAttack = false, WeaponAttackData weaponAttackData = null)
    {
        hitTargets.Clear();
        currentComboStep = Mathf.Max(1, comboStep);
        isWeaponAttack = weaponAttack;
        currentWeaponAttack = weaponAttackData;
        attacker = hitAttacker;
        ApplyHitboxShape();

        if (triggerCollider != null)
        {
            triggerCollider.enabled = true;
        }
    }

    public void DisableHitbox()
    {
        if (triggerCollider != null)
        {
            triggerCollider.enabled = false;
        }

        currentWeaponAttack = null;
        ResetHitboxShape();
        UpdateDebugVisual();
    }

    private void OnTriggerEnter(Collider other)
    {
        if (other.GetComponent<EnemyAttackHitbox>() != null)
        {
            return;
        }

        TrainingDummyHealth target = other.GetComponentInParent<TrainingDummyHealth>();
        if (target == null || hitTargets.Contains(target))
        {
            return;
        }

        hitTargets.Add(target);
        Vector3 hitPoint = other.ClosestPoint(transform.position);
        target.TakeHit(CreateHit(target.transform.position, hitPoint));
    }

    private CombatHit CreateHit(Vector3 targetPosition, Vector3 hitPoint)
    {
        bool isSecondHit = currentComboStep >= 2;
        Vector3 origin = attacker != null ? attacker.position : transform.position;
        Vector3 direction = Vector3.ProjectOnPlane(targetPosition - origin, Vector3.up);
        Vector3 impactPoint = hitPoint;
        if (impactPoint.sqrMagnitude <= 0.001f)
        {
            impactPoint = targetPosition + Vector3.up;
        }

        return new CombatHit(
            GetDamage(isSecondHit),
            GetPoiseDamage(isSecondHit),
            GetKnockback(isSecondHit),
            knockbackDuration,
            hitStopDuration,
            GetCameraShakeStrength(),
            GetCameraShakeDuration(),
            GetImpactWeight(),
            impactPoint,
            direction,
            attacker,
            currentComboStep);
    }

    private int GetDamage(bool isSecondHit)
    {
        if (isWeaponAttack)
        {
            if (currentWeaponAttack != null)
            {
                return currentWeaponAttack.damage;
            }

            if (currentComboStep >= 3)
            {
                return weaponThirdHitDamage;
            }

            return isSecondHit ? weaponSecondHitDamage : weaponFirstHitDamage;
        }

        return isSecondHit ? secondHitDamage : firstHitDamage;
    }

    private float GetPoiseDamage(bool isSecondHit)
    {
        if (isWeaponAttack)
        {
            if (currentWeaponAttack != null)
            {
                return currentWeaponAttack.poiseDamage;
            }

            if (currentComboStep >= 3)
            {
                return weaponThirdHitPoiseDamage;
            }

            return isSecondHit ? weaponSecondHitPoiseDamage : weaponFirstHitPoiseDamage;
        }

        return isSecondHit ? secondHitPoiseDamage : firstHitPoiseDamage;
    }

    private float GetKnockback(bool isSecondHit)
    {
        if (isWeaponAttack)
        {
            if (currentWeaponAttack != null)
            {
                return currentWeaponAttack.knockbackDistance;
            }

            if (currentComboStep >= 3)
            {
                return weaponThirdHitKnockback;
            }

            return isSecondHit ? weaponSecondHitKnockback : weaponFirstHitKnockback;
        }

        return isSecondHit ? secondHitKnockback : firstHitKnockback;
    }

    private CombatImpactWeight GetImpactWeight()
    {
        if (isWeaponAttack)
        {
            if (currentWeaponAttack != null)
            {
                return currentWeaponAttack.impactWeight;
            }

            return currentComboStep >= 3 ? CombatImpactWeight.Heavy : CombatImpactWeight.Medium;
        }

        return currentComboStep >= 2 ? CombatImpactWeight.Medium : CombatImpactWeight.Light;
    }

    private float GetCameraShakeStrength()
    {
        return GetImpactWeight() switch
        {
            CombatImpactWeight.Heavy => cameraShakeStrength * 1.9f,
            CombatImpactWeight.Medium => cameraShakeStrength * 1.25f,
            _ => cameraShakeStrength
        };
    }

    private float GetCameraShakeDuration()
    {
        return GetImpactWeight() == CombatImpactWeight.Heavy
            ? cameraShakeDuration * 1.35f
            : cameraShakeDuration;
    }

    private void CacheDefaultHitboxShape()
    {
        if (cachedDefaultShape)
        {
            return;
        }

        defaultLocalPosition = transform.localPosition;
        defaultLocalScale = transform.localScale;
        cachedDefaultShape = true;
    }

    private void ApplyHitboxShape()
    {
        CacheDefaultHitboxShape();

        if (isWeaponAttack && currentWeaponAttack != null)
        {
            transform.localPosition = defaultLocalPosition + currentWeaponAttack.hitboxLocalOffset;
            transform.localScale = Vector3.Scale(defaultLocalScale, currentWeaponAttack.hitboxScaleMultiplier);
            return;
        }

        if (isWeaponAttack && currentComboStep >= 3)
        {
            transform.localPosition = defaultLocalPosition + weaponThirdHitboxLocalOffset;
            transform.localScale = Vector3.Scale(defaultLocalScale, weaponThirdHitboxScaleMultiplier);
            return;
        }

        ResetHitboxShape();
    }

    private void ResetHitboxShape()
    {
        CacheDefaultHitboxShape();
        transform.localPosition = defaultLocalPosition;
        transform.localScale = defaultLocalScale;
    }

    private void UpdateDebugVisual()
    {
        if (hitboxRenderer != null)
        {
            hitboxRenderer.enabled = DevSettings.ShowHitboxes;
        }
    }
}
