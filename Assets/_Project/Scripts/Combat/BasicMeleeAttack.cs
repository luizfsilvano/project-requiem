using System.Collections;
using System;
using UnityEngine;
using UnityEngine.InputSystem;

[DefaultExecutionOrder(0)]
public sealed class BasicMeleeAttack : MonoBehaviour
{
    [SerializeField] private MeleeHitbox hitbox;
    [SerializeField] private PlayerCameraRelativeMovement movement;
    [SerializeField] private PlayerLockOnController lockOnController;
    [SerializeField] private PlayerStamina stamina;
    [SerializeField] private PlayerEquipment equipment;
    [SerializeField] private CharacterController characterController;
    [SerializeField] private float cooldown = 0.65f;
    [SerializeField] private float activeTime = 0.18f;
    [SerializeField] private float weaponFirstAttackDuration = 0.38f;
    [SerializeField] private float weaponSecondAttackDuration = 0.5f;
    [SerializeField] private float weaponThirdAttackDuration = 1.25f;
    [SerializeField, Range(0.05f, 0.95f)] private float unarmedFirstHitMoment = 0.58f;
    [SerializeField, Range(0.05f, 0.95f)] private float unarmedSecondHitMoment = 0.62f;
    [SerializeField, Range(0.05f, 0.95f)] private float weaponFirstHitMoment = 0.74f;
    [SerializeField, Range(0.05f, 0.95f)] private float weaponSecondHitMoment = 0.76f;
    [SerializeField, Range(0.05f, 0.95f)] private float weaponThirdHitMoment = 0.72f;
    [SerializeField] private float comboInputWindow = 0.35f;
    [SerializeField] private float comboStepGap = 0.05f;
    [SerializeField] private float weaponComboRecoveryTime = 0.16f;
    [SerializeField] private float weaponFinalRecoveryTime = 0.85f;
    [SerializeField] private float weaponFirstAttackLungeDistance = 0.28f;
    [SerializeField] private float weaponSecondAttackLungeDistance = 0f;
    [SerializeField] private float weaponThirdAttackLungeDistance = 0f;
    [SerializeField] private float weaponLungeDuration = 0.12f;

    private bool canAttack = true;
    private bool comboQueueOpen;
    private int queuedComboInputs;
    private int currentComboStep;
    private int activeComboMaxStep;
    private Coroutine attackRoutine;

    public event Action<int> AttackStarted;
    public event Action<int> AttackRecoveryStarted;
    public bool IsAttacking { get; private set; }
    public bool IsWeaponEquipped => equipment != null && equipment.HasWeapon;
    private WeaponData CurrentWeaponData => equipment != null ? equipment.EquippedWeapon : null;
    public bool CanRotateDuringAttack { get; private set; }

    private void Awake()
    {
        if (movement == null)
        {
            movement = GetComponent<PlayerCameraRelativeMovement>();
        }

        if (lockOnController == null)
        {
            lockOnController = GetComponent<PlayerLockOnController>();
        }

        if (stamina == null)
        {
            stamina = GetComponent<PlayerStamina>();
        }

        if (hitbox != null)
        {
            hitbox.DisableHitbox();
        }

        if (equipment == null)
        {
            equipment = GetComponent<PlayerEquipment>();
        }

        if (characterController == null)
        {
            characterController = GetComponent<CharacterController>();
        }
    }

    private void Update()
    {
        if (DevSettings.ConsoleOpen)
        {
            return;
        }

        if (Mouse.current == null || !Mouse.current.leftButton.wasPressedThisFrame)
        {
            return;
        }

        TryAttack();
    }

    public bool TryAttack()
    {
        if (IsAttacking)
        {
            if (CanQueueCombo())
            {
                queuedComboInputs++;
                return true;
            }

            return false;
        }

        bool isWeaponAttack = IsWeaponEquipped;
        if (!canAttack || IsDodgeLocked() || !CanSpendAttack(1, isWeaponAttack))
        {
            return false;
        }

        FaceLockOnTarget(1f);
        attackRoutine = StartCoroutine(Attack());
        return true;
    }

    private IEnumerator Attack()
    {
        canAttack = false;
        IsAttacking = true;
        queuedComboInputs = 0;
        activeComboMaxStep = IsWeaponEquipped ? GetWeaponComboSteps() : 2;

        for (int comboStep = 1; comboStep <= activeComboMaxStep; comboStep++)
        {
            bool isWeaponAttack = IsWeaponEquipped;
            if (!TrySpendAttack(comboStep, isWeaponAttack))
            {
                break;
            }

            comboQueueOpen = comboStep < activeComboMaxStep;
            yield return PlayAttackStep(comboStep);

            if (comboStep >= activeComboMaxStep)
            {
                comboQueueOpen = false;
                yield return new WaitForSeconds(GetFinalRecoveryTime(isWeaponAttack));
                break;
            }

            if (isWeaponAttack)
            {
                CanRotateDuringAttack = comboStep >= 2;
                AttackRecoveryStarted?.Invoke(comboStep);
                yield return WaitForComboQueueWithMinimumRecovery(GetWeaponComboRecoveryTime());
            }
            else
            {
                yield return WaitForComboQueue();
            }

            comboQueueOpen = false;

            if (queuedComboInputs <= 0)
            {
                break;
            }

            queuedComboInputs--;
            yield return new WaitForSeconds(comboStepGap);
        }

        currentComboStep = 0;
        activeComboMaxStep = 0;
        CanRotateDuringAttack = false;
        canAttack = true;
        IsAttacking = false;
        attackRoutine = null;
    }

    private IEnumerator PlayAttackStep(int comboStep)
    {
        currentComboStep = comboStep;
        bool isWeaponAttack = IsWeaponEquipped;
        CanRotateDuringAttack = isWeaponAttack && comboStep >= 3;
        AttackStarted?.Invoke(comboStep);
        CombatFeedbackAudio.PlayPlayerAttack(isWeaponAttack && comboStep >= 3, transform.position);

        float elapsed = 0f;
        WeaponAttackData weaponAttackData = GetWeaponAttack(comboStep, isWeaponAttack);
        float attackDuration = GetAttackDuration(comboStep, isWeaponAttack, weaponAttackData);
        float hitboxStartTime = GetHitboxStartTime(comboStep, isWeaponAttack, attackDuration, weaponAttackData);
        float hitboxEndTime = hitboxStartTime + activeTime;
        float lungeDuration = Mathf.Min(activeTime, GetLungeDuration(isWeaponAttack));
        float lungeSpeed = lungeDuration > 0f ? GetLungeDistance(comboStep, isWeaponAttack, weaponAttackData) / lungeDuration : 0f;
        bool hitboxActive = false;
        bool hitboxStarted = false;
        while (elapsed < attackDuration)
        {
            float deltaTime = Time.deltaTime > 0f ? Time.deltaTime : 1f / 60f;
            if (!hitboxStarted)
            {
                FaceLockOnTarget(18f * deltaTime);
            }

            if (characterController != null && elapsed < lungeDuration && lungeSpeed > 0f)
            {
                float lungeDelta = Mathf.Min(deltaTime, lungeDuration - elapsed);
                characterController.Move(transform.forward * (lungeSpeed * lungeDelta));
            }

            elapsed += deltaTime;

            if (!hitboxStarted && elapsed >= hitboxStartTime)
            {
                CanRotateDuringAttack = false;
                if (hitbox != null)
                {
                    hitbox.EnableHitbox(comboStep, transform, isWeaponAttack, weaponAttackData);
                }

                hitboxStarted = true;
                hitboxActive = hitbox != null;
            }

            if (hitboxActive && elapsed >= hitboxEndTime)
            {
                hitbox.DisableHitbox();
                hitboxActive = false;
            }

            yield return null;
        }

        if (hitboxActive)
        {
            hitbox.DisableHitbox();
        }
    }

    private void OnDisable()
    {
        if (attackRoutine != null)
        {
            StopCoroutine(attackRoutine);
            attackRoutine = null;
        }

        if (hitbox != null)
        {
            hitbox.DisableHitbox();
        }

        comboQueueOpen = false;
        queuedComboInputs = 0;
        currentComboStep = 0;
        activeComboMaxStep = 0;
        CanRotateDuringAttack = false;
        canAttack = true;
        IsAttacking = false;
    }

    private IEnumerator WaitForComboQueue()
    {
        float recoveryTimer = Mathf.Max(0f, cooldown - activeTime);
        float comboTimer = comboInputWindow;
        while (recoveryTimer > 0f && queuedComboInputs <= 0)
        {
            if (comboTimer <= 0f)
            {
                comboQueueOpen = false;
            }

            comboTimer -= Time.deltaTime;
            recoveryTimer -= Time.deltaTime;
            yield return null;
        }
    }

    private IEnumerator WaitForComboQueueWithMinimumRecovery(float minimumRecoveryTime)
    {
        float elapsed = 0f;
        float maxWindow = Mathf.Max(comboInputWindow, minimumRecoveryTime);
        while (elapsed < maxWindow)
        {
            if (queuedComboInputs > 0 && elapsed >= minimumRecoveryTime)
            {
                break;
            }

            elapsed += Time.deltaTime;
            yield return null;
        }
    }

    private bool CanQueueCombo()
    {
        return comboQueueOpen
            && currentComboStep > 0
            && currentComboStep + queuedComboInputs < activeComboMaxStep
            && CanSpendAttack(currentComboStep + queuedComboInputs + 1, IsWeaponEquipped)
            && !IsDodgeLocked();
    }

    private bool IsDodgeLocked()
    {
        return movement != null && movement.IsDodging;
    }

    private bool CanSpendAttack(int comboStep, bool isWeaponAttack)
    {
        float cost = GetAttackStaminaCost(comboStep, isWeaponAttack);
        return stamina == null || stamina.CanSpend(cost);
    }

    private bool TrySpendAttack(int comboStep, bool isWeaponAttack)
    {
        float cost = GetAttackStaminaCost(comboStep, isWeaponAttack);
        return stamina == null || stamina.TrySpend(cost);
    }

    private void FaceLockOnTarget(float turnAmount)
    {
        if (lockOnController == null || !lockOnController.TryGetDirectionToTarget(out Vector3 direction))
        {
            return;
        }

        Quaternion targetRotation = Quaternion.LookRotation(direction, Vector3.up);
        transform.rotation = Quaternion.Slerp(transform.rotation, targetRotation, Mathf.Clamp01(turnAmount));
    }

    private float GetAttackStaminaCost(int comboStep, bool isWeaponAttack)
    {
        WeaponAttackData weaponAttackData = GetWeaponAttack(comboStep, isWeaponAttack);
        if (weaponAttackData != null)
        {
            return weaponAttackData.staminaCost;
        }

        return stamina != null ? stamina.GetAttackCost(comboStep, isWeaponAttack) : 0f;
    }

    private float GetLungeDistance(int comboStep, bool isWeaponAttack, WeaponAttackData weaponAttackData)
    {
        if (!isWeaponAttack)
        {
            return 0f;
        }

        if (weaponAttackData != null)
        {
            return weaponAttackData.lungeDistance;
        }

        return comboStep switch
        {
            1 => weaponFirstAttackLungeDistance,
            2 => weaponSecondAttackLungeDistance,
            _ => weaponThirdAttackLungeDistance
        };
    }

    private float GetFinalRecoveryTime(bool isWeaponAttack)
    {
        if (!isWeaponAttack)
        {
            return Mathf.Max(0f, cooldown - activeTime);
        }

        return CurrentWeaponData != null ? CurrentWeaponData.finalRecoveryTime : weaponFinalRecoveryTime;
    }

    private float GetAttackDuration(int comboStep, bool isWeaponAttack, WeaponAttackData weaponAttackData)
    {
        if (!isWeaponAttack)
        {
            return activeTime;
        }

        if (weaponAttackData != null)
        {
            return weaponAttackData.duration;
        }

        return comboStep switch
        {
            1 => weaponFirstAttackDuration,
            2 => weaponSecondAttackDuration,
            _ => weaponThirdAttackDuration
        };
    }

    private float GetHitboxStartTime(int comboStep, bool isWeaponAttack, float attackDuration, WeaponAttackData weaponAttackData)
    {
        float contactMoment = GetHitMoment(comboStep, isWeaponAttack, weaponAttackData);
        float contactTime = Mathf.Clamp01(contactMoment) * attackDuration;
        float halfWindow = activeTime * 0.5f;
        return Mathf.Clamp(contactTime - halfWindow, 0f, Mathf.Max(0f, attackDuration - 0.02f));
    }

    private float GetHitMoment(int comboStep, bool isWeaponAttack, WeaponAttackData weaponAttackData)
    {
        if (isWeaponAttack)
        {
            if (weaponAttackData != null)
            {
                return weaponAttackData.hitMoment;
            }

            return comboStep switch
            {
                1 => weaponFirstHitMoment,
                2 => weaponSecondHitMoment,
                _ => weaponThirdHitMoment
            };
        }

        return comboStep >= 2 ? unarmedSecondHitMoment : unarmedFirstHitMoment;
    }

    private WeaponAttackData GetWeaponAttack(int comboStep, bool isWeaponAttack)
    {
        return isWeaponAttack ? CurrentWeaponData?.GetAttack(comboStep) : null;
    }

    private int GetWeaponComboSteps()
    {
        return CurrentWeaponData != null ? CurrentWeaponData.maxComboSteps : 3;
    }

    private float GetWeaponComboRecoveryTime()
    {
        return CurrentWeaponData != null ? CurrentWeaponData.comboRecoveryTime : weaponComboRecoveryTime;
    }

    private float GetLungeDuration(bool isWeaponAttack)
    {
        if (!isWeaponAttack)
        {
            return weaponLungeDuration;
        }

        return CurrentWeaponData != null ? CurrentWeaponData.lungeDuration : weaponLungeDuration;
    }
}
