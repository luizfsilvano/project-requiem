using UnityEngine;

public sealed class PlayerAnimationDriver : MonoBehaviour
{
    [SerializeField] private Animator animator;
    [SerializeField] private CharacterController characterController;
    [SerializeField] private PlayerCameraRelativeMovement movement;
    [SerializeField] private BasicMeleeAttack meleeAttack;
    [SerializeField] private PlayerEquipment equipment;
    [SerializeField] private float runSpeedReference = 5f;
    [SerializeField] private float locomotionFade = 0.12f;
    [SerializeField] private float actionFade = 0.05f;
    [SerializeField] private float attackLockTime = 0.55f;
    [SerializeField] private float attackRecoveryLockTime = 0.22f;
    [SerializeField] private float weaponFinisherLockTime = 1.05f;
    [SerializeField] private float dodgeLockTime = 0.45f;

    private static readonly int SpeedHash = Animator.StringToHash("Speed");
    private static readonly int IdleStateHash = Animator.StringToHash("Idle");
    private static readonly int WalkStateHash = Animator.StringToHash("Walk");
    private static readonly int RunStateHash = Animator.StringToHash("Run");
    private static readonly int AttackJabStateHash = Animator.StringToHash("AttackJab");
    private static readonly int AttackCrossStateHash = Animator.StringToHash("AttackCross");
    private static readonly int SwordAttackAStateHash = Animator.StringToHash("SwordAttackA");
    private static readonly int SwordAttackBStateHash = Animator.StringToHash("SwordAttackB");
    private static readonly int SwordAttackCStateHash = Animator.StringToHash("SwordAttackC");
    private static readonly int SwordAttackARecoveryStateHash = Animator.StringToHash("SwordAttackA_REC");
    private static readonly int SwordAttackBRecoveryStateHash = Animator.StringToHash("SwordAttackB_REC");
    private static readonly int DodgeStateHash = Animator.StringToHash("Dodge");

    private float actionLockTimer;

    private void Awake()
    {
        if (animator == null)
        {
            animator = GetComponentInChildren<Animator>();
        }

        if (characterController == null)
        {
            characterController = GetComponent<CharacterController>();
        }

        if (movement == null)
        {
            movement = GetComponent<PlayerCameraRelativeMovement>();
        }

        if (meleeAttack == null)
        {
            meleeAttack = GetComponent<BasicMeleeAttack>();
        }

        if (equipment == null)
        {
            equipment = GetComponent<PlayerEquipment>();
        }
    }

    private void OnEnable()
    {
        if (movement != null)
        {
            movement.DodgeStarted += HandleDodgeStarted;
        }

        if (meleeAttack != null)
        {
            meleeAttack.AttackStarted += HandleAttackStarted;
            meleeAttack.AttackRecoveryStarted += HandleAttackRecoveryStarted;
        }
    }

    private void OnDisable()
    {
        if (movement != null)
        {
            movement.DodgeStarted -= HandleDodgeStarted;
        }

        if (meleeAttack != null)
        {
            meleeAttack.AttackStarted -= HandleAttackStarted;
            meleeAttack.AttackRecoveryStarted -= HandleAttackRecoveryStarted;
        }
    }

    private void Update()
    {
        if (animator == null || characterController == null)
        {
            return;
        }

        float planarSpeed = movement != null ? movement.CurrentPlanarSpeed : GetCharacterControllerPlanarSpeed();
        float normalizedSpeed = Mathf.Clamp01(planarSpeed / runSpeedReference);
        animator.SetFloat(SpeedHash, normalizedSpeed, 0.1f, Time.deltaTime);

        if (actionLockTimer > 0f)
        {
            actionLockTimer -= Time.deltaTime;
            return;
        }

        if (meleeAttack != null && meleeAttack.IsAttacking)
        {
            return;
        }

        CrossFadeIfNeeded(GetLocomotionState(normalizedSpeed), locomotionFade);
    }

    private int GetLocomotionState(float normalizedSpeed)
    {
        if (normalizedSpeed <= 0.1f)
        {
            return IdleStateHash;
        }

        return movement != null && movement.IsRunning ? RunStateHash : WalkStateHash;
    }

    private float GetCharacterControllerPlanarSpeed()
    {
        Vector3 planarVelocity = characterController.velocity;
        planarVelocity.y = 0f;
        return planarVelocity.magnitude;
    }

    private void HandleAttackStarted(int comboStep)
    {
        if (animator != null)
        {
            bool hasWeapon = equipment != null && equipment.HasWeapon;
            actionLockTimer = hasWeapon && comboStep >= 3 ? weaponFinisherLockTime : attackLockTime;
            int attackStateHash = GetAttackState(comboStep, hasWeapon);
            animator.CrossFadeInFixedTime(attackStateHash, actionFade);
        }
    }

    private void HandleAttackRecoveryStarted(int comboStep)
    {
        if (animator == null || equipment == null || !equipment.HasWeapon)
        {
            return;
        }

        actionLockTimer = attackRecoveryLockTime;
        int recoveryStateHash = comboStep >= 2 ? SwordAttackBRecoveryStateHash : SwordAttackARecoveryStateHash;
        animator.CrossFadeInFixedTime(recoveryStateHash, actionFade);
    }

    private static int GetAttackState(int comboStep, bool hasWeapon)
    {
        if (hasWeapon)
        {
            return comboStep switch
            {
                1 => SwordAttackAStateHash,
                2 => SwordAttackBStateHash,
                _ => SwordAttackCStateHash
            };
        }

        return comboStep >= 2 ? AttackCrossStateHash : AttackJabStateHash;
    }

    private void HandleDodgeStarted()
    {
        if (animator != null)
        {
            actionLockTimer = dodgeLockTime;
            animator.CrossFadeInFixedTime(DodgeStateHash, actionFade);
        }
    }

    private void CrossFadeIfNeeded(int stateHash, float fadeDuration)
    {
        AnimatorStateInfo currentState = animator.GetCurrentAnimatorStateInfo(0);
        AnimatorStateInfo nextState = animator.GetNextAnimatorStateInfo(0);

        if (currentState.shortNameHash == stateHash || nextState.shortNameHash == stateHash)
        {
            return;
        }

        animator.CrossFadeInFixedTime(stateHash, fadeDuration);
    }
}
