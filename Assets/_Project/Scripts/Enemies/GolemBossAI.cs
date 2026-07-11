using System;
using System.Collections;
using UnityEngine;

public sealed class GolemBossAI : MonoBehaviour
{
    private enum BossState
    {
        Idle,
        Chase,
        Windup,
        Telegraph,
        Attack,
        Recovery,
        Stagger,
        Death
    }

    [Serializable]
    private sealed class BossAttack
    {
        [SerializeField] private string displayName;
        [SerializeField] private string animationStateName;
        [SerializeField] private EnemyAttackHitbox hitbox;
        [SerializeField, Min(0.1f)] private float windup;
        [SerializeField, Min(0f)] private float telegraphDuration;
        [SerializeField, Min(0.03f)] private float activeTime;
        [SerializeField, Min(0.1f)] private float recovery;
        [SerializeField, Min(0f)] private float cooldown;

        public BossAttack(
            string displayName,
            string animationStateName,
            float windup,
            float telegraphDuration,
            float activeTime,
            float recovery,
            float cooldown)
        {
            this.displayName = displayName;
            this.animationStateName = animationStateName;
            this.windup = windup;
            this.telegraphDuration = telegraphDuration;
            this.activeTime = activeTime;
            this.recovery = recovery;
            this.cooldown = cooldown;
        }

        public string DisplayName => displayName;
        public string AnimationStateName => animationStateName;
        public EnemyAttackHitbox Hitbox => hitbox;
        public float Windup => windup;
        public float TelegraphDuration => telegraphDuration;
        public float ActiveTime => activeTime;
        public float Recovery => recovery;
        public float Cooldown => cooldown;

        public void SetHitbox(EnemyAttackHitbox value)
        {
            hitbox = value;
        }
    }

    [Header("References")]
    [SerializeField] private Transform target;
    [SerializeField] private TrainingDummyHealth health;
    [SerializeField] private Animator animator;
    [SerializeField] private GolemGroundSlamVfx groundSlamVfx;

    [Header("Activation")]
    [SerializeField] private bool combatEnabled = true;

    [Header("Movement")]
    [SerializeField, Min(0.1f)] private float moveSpeed = 1.55f;
    [SerializeField, Min(0.1f)] private float rotationSpeed = 4.5f;
    [SerializeField, Min(0.1f)] private float stoppingDistance = 3.25f;
    [SerializeField, Min(0.1f)] private float attackRange = 3.8f;

    [Header("Attacks")]
    [SerializeField] private BossAttack slamAttack = new(
        "Pisao sismico",
        "Slam",
        1.25f,
        0.5f,
        0.14f,
        1.1f,
        0.55f);
    [SerializeField] private BossAttack sweepAttack = new(
        "Varrida lateral",
        "Sweep",
        0.9f,
        0f,
        0.2f,
        0.72f,
        0.5f);

    [Header("Animation States")]
    [SerializeField] private string idleStateName = "Idle";
    [SerializeField] private string walkStateName = "Walk";
    [SerializeField] private string staggerStateName = "Stagger";
    [SerializeField] private string deathStateName = "Death";
    [SerializeField, Min(0f)] private float locomotionFade = 0.18f;
    [SerializeField, Min(0f)] private float actionFade = 0.08f;

    private BossState state = BossState.Idle;
    private BossAttack currentAttack;
    private float stateTimer;
    private float cooldownTimer;
    private int nextAttackIndex;
    private string currentAnimationState;
    private bool deathAnimationStarted;
    private Coroutine deathAnimationRoutine;

    public bool CombatEnabled => combatEnabled;
    public string CurrentStateName => state.ToString();
    public string CurrentAttackName => currentAttack != null ? currentAttack.DisplayName : string.Empty;

    private void Awake()
    {
        if (health == null)
        {
            health = GetComponent<TrainingDummyHealth>();
        }

        if (animator == null)
        {
            animator = GetComponentInChildren<Animator>(true);
        }

        DisableAttackHitboxes();
    }

    private void OnDisable()
    {
        CancelAttack();
    }

    private void Update()
    {
        if (cooldownTimer > 0f)
        {
            cooldownTimer -= Time.deltaTime;
        }

        if (health != null && health.IsDead)
        {
            ChangeState(BossState.Death);
            StartDeathAnimation();
            return;
        }

        if (health != null && health.IsStaggered)
        {
            ChangeState(BossState.Stagger);
            return;
        }

        if (state == BossState.Stagger)
        {
            ChangeState(BossState.Idle);
        }

        if (!CanThink())
        {
            ChangeState(BossState.Idle);
            return;
        }

        EnsureTarget();
        if (target == null)
        {
            ChangeState(BossState.Idle);
            return;
        }

        switch (state)
        {
            case BossState.Idle:
            case BossState.Chase:
                TickLocomotion();
                break;
            case BossState.Windup:
                TickWindup();
                break;
            case BossState.Telegraph:
                TickTelegraph();
                break;
            case BossState.Attack:
                TickAttack();
                break;
            case BossState.Recovery:
                TickRecovery();
                break;
        }
    }

    public void Configure(
        TrainingDummyHealth bossHealth,
        Animator bossAnimator,
        EnemyAttackHitbox slamHitbox,
        EnemyAttackHitbox sweepHitbox,
        GolemGroundSlamVfx slamVfx)
    {
        health = bossHealth;
        animator = bossAnimator;
        slamAttack.SetHitbox(slamHitbox);
        sweepAttack.SetHitbox(sweepHitbox);
        groundSlamVfx = slamVfx;
        DisableAttackHitboxes();
    }

    public void SetCombatEnabled(bool enabled)
    {
        combatEnabled = enabled;

        if (!enabled)
        {
            CancelAttack();
        }

        if (!enabled && (health == null || !health.IsDead))
        {
            ChangeState(BossState.Idle);
        }
    }

    private bool CanThink()
    {
        return combatEnabled
            && DevSettings.EnemyAIEnabled
            && (health == null || !health.IsControlLocked);
    }

    private void EnsureTarget()
    {
        if (target != null)
        {
            return;
        }

        PlayerHealth player = FindFirstObjectByType<PlayerHealth>();
        if (player != null)
        {
            target = player.transform;
        }
    }

    private void TickLocomotion()
    {
        if (!TryGetTargetDirection(out Vector3 direction, out float distance))
        {
            ChangeState(BossState.Idle);
            return;
        }

        Face(direction);

        if (distance <= attackRange && cooldownTimer <= 0f)
        {
            SelectNextAttack();
            ChangeState(BossState.Windup);
            return;
        }

        if (distance > stoppingDistance)
        {
            ChangeState(BossState.Chase);
            transform.position += direction * (moveSpeed * DevSettings.EnemySpeedMultiplier * Time.deltaTime);
            PlayAnimation(walkStateName, locomotionFade);
            return;
        }

        ChangeState(BossState.Idle);
        PlayAnimation(idleStateName, locomotionFade);
    }

    private void TickWindup()
    {
        stateTimer -= Time.deltaTime;

        if (TryGetTargetDirection(out Vector3 direction, out _)
            && currentAttack != null
            && stateTimer > currentAttack.Windup * 0.22f)
        {
            Face(direction);
        }

        if (stateTimer <= 0f)
        {
            ChangeState(currentAttack != null && currentAttack.TelegraphDuration > 0f
                ? BossState.Telegraph
                : BossState.Attack);
        }
    }

    private void TickTelegraph()
    {
        stateTimer -= Time.deltaTime;
        if (stateTimer <= 0f)
        {
            ChangeState(BossState.Attack);
        }
    }

    private void TickAttack()
    {
        stateTimer -= Time.deltaTime;
        if (stateTimer <= 0f)
        {
            ChangeState(BossState.Recovery);
        }
    }

    private void TickRecovery()
    {
        stateTimer -= Time.deltaTime;
        if (stateTimer <= 0f)
        {
            cooldownTimer = currentAttack != null ? currentAttack.Cooldown : 0.5f;
            currentAttack = null;
            ChangeState(BossState.Idle);
        }
    }

    private void SelectNextAttack()
    {
        currentAttack = nextAttackIndex == 0 ? slamAttack : sweepAttack;
        nextAttackIndex = (nextAttackIndex + 1) % 2;
    }

    private void ChangeState(BossState nextState)
    {
        if (state == nextState)
        {
            return;
        }

        ExitState(state);
        state = nextState;
        EnterState(nextState);
    }

    private void EnterState(BossState nextState)
    {
        switch (nextState)
        {
            case BossState.Idle:
                PlayAnimation(idleStateName, locomotionFade);
                break;
            case BossState.Chase:
                PlayAnimation(walkStateName, locomotionFade);
                break;
            case BossState.Windup:
                if (currentAttack == null)
                {
                    ChangeState(BossState.Idle);
                    return;
                }

                stateTimer = currentAttack.Windup;
                PlayAnimation(currentAttack.AnimationStateName, actionFade);
                break;
            case BossState.Telegraph:
                if (currentAttack == null)
                {
                    ChangeState(BossState.Idle);
                    return;
                }

                DisableAttackHitboxes();
                stateTimer = currentAttack.TelegraphDuration;
                groundSlamVfx?.BeginTelegraph(stateTimer);
                break;
            case BossState.Attack:
                if (currentAttack == null)
                {
                    ChangeState(BossState.Idle);
                    return;
                }

                stateTimer = currentAttack.ActiveTime;
                if (currentAttack.TelegraphDuration > 0f)
                {
                    groundSlamVfx?.CancelTelegraph();
                    currentAttack.Hitbox?.Pulse(transform);
                    groundSlamVfx?.PlayImpact();
                }
                else
                {
                    currentAttack.Hitbox?.EnableHitbox(transform);
                }

                CombatFeedbackAudio.PlayEnemyAttack(transform.position);
                break;
            case BossState.Recovery:
                DisableAttackHitboxes();
                stateTimer = currentAttack != null ? currentAttack.Recovery : 0.5f;
                break;
            case BossState.Stagger:
                CancelAttack();
                PlayAnimation(staggerStateName, actionFade);
                break;
            case BossState.Death:
                CancelAttack();
                break;
        }
    }

    private void ExitState(BossState previousState)
    {
        if (previousState == BossState.Telegraph)
        {
            groundSlamVfx?.CancelTelegraph();
        }

        if (previousState == BossState.Attack)
        {
            DisableAttackHitboxes();
        }
    }

    private void Face(Vector3 direction)
    {
        if (direction.sqrMagnitude <= 0.001f)
        {
            return;
        }

        Quaternion targetRotation = Quaternion.LookRotation(direction, Vector3.up);
        transform.rotation = Quaternion.Slerp(transform.rotation, targetRotation, rotationSpeed * Time.deltaTime);
    }

    private bool TryGetTargetDirection(out Vector3 direction, out float distance)
    {
        direction = Vector3.zero;
        distance = 0f;

        if (target == null)
        {
            return false;
        }

        Vector3 toTarget = Vector3.ProjectOnPlane(target.position - transform.position, Vector3.up);
        distance = toTarget.magnitude;
        if (distance <= 0.001f)
        {
            return false;
        }

        direction = toTarget / distance;
        return true;
    }

    private void CancelAttack()
    {
        DisableAttackHitboxes();
        groundSlamVfx?.CancelTelegraph();
        currentAttack = null;
    }

    private void DisableAttackHitboxes()
    {
        slamAttack?.Hitbox?.DisableHitbox();
        sweepAttack?.Hitbox?.DisableHitbox();
    }

    private void PlayAnimation(string stateName, float fadeDuration)
    {
        if (animator == null
            || animator.runtimeAnimatorController == null
            || string.IsNullOrWhiteSpace(stateName)
            || currentAnimationState == stateName)
        {
            return;
        }

        animator.applyRootMotion = false;
        animator.speed = 1f;
        animator.CrossFadeInFixedTime(stateName, fadeDuration);
        currentAnimationState = stateName;
    }

    private void StartDeathAnimation()
    {
        if (deathAnimationStarted)
        {
            return;
        }

        deathAnimationStarted = true;
        if (deathAnimationRoutine != null)
        {
            StopCoroutine(deathAnimationRoutine);
        }

        deathAnimationRoutine = StartCoroutine(PlayDeathAnimationToEnd());
    }

    private IEnumerator PlayDeathAnimationToEnd()
    {
        CancelAttack();
        PlayAnimation(deathStateName, actionFade);

        float duration = Mathf.Max(0.2f, GetAnimationClipLength(deathStateName));
        yield return new WaitForSeconds(Mathf.Max(0.05f, duration - actionFade));

        if (animator != null && animator.runtimeAnimatorController != null)
        {
            animator.Play(deathStateName, 0, 0.999f);
            animator.speed = 0f;
        }
    }

    private float GetAnimationClipLength(string stateName)
    {
        if (animator == null || animator.runtimeAnimatorController == null)
        {
            return 1.5f;
        }

        float fallback = 1.5f;
        foreach (AnimationClip clip in animator.runtimeAnimatorController.animationClips)
        {
            if (clip == null)
            {
                continue;
            }

            fallback = Mathf.Max(fallback, clip.length);
            if (clip.name == stateName
                || stateName.EndsWith(clip.name, StringComparison.Ordinal)
                || clip.name.EndsWith(stateName, StringComparison.Ordinal))
            {
                return clip.length;
            }
        }

        return fallback;
    }
}
