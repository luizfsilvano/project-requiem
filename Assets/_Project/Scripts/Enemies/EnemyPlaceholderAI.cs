using System.Collections;
using UnityEngine;

public sealed class EnemyPlaceholderAI : MonoBehaviour
{
    private enum EnemyState
    {
        Idle,
        Chase,
        Windup,
        Attack,
        Recovery,
        Stagger,
        Death
    }

    [SerializeField] private Transform target;
    [SerializeField] private EnemyCombatTuning tuning;
    [SerializeField] private TrainingDummyHealth health;
    [SerializeField] private EnemyAttackHitbox attackHitbox;
    [SerializeField] private Animator[] animators;
    [SerializeField] private float moveSpeed = 2.4f;
    [SerializeField] private float rotationSpeed = 9f;
    [SerializeField] private float stoppingDistance = 1.65f;
    [SerializeField] private float attackRange = 1.9f;
    [SerializeField] private float attackCooldown = 1.15f;
    [SerializeField] private float attackWindup = 0.78f;
    [SerializeField] private float attackActiveTime = 0.12f;
    [SerializeField] private float attackRecovery = 0.35f;
    [SerializeField] private bool showAttackTelegraph;
    [SerializeField] private Vector3 attackTelegraphLocalPosition = new(0f, 0.035f, 1.15f);
    [SerializeField] private Vector3 attackTelegraphScale = new(1.65f, 0.035f, 1.35f);
    [SerializeField] private Color attackTelegraphColor = new(1f, 0.08f, 0.02f, 0.42f);
    [SerializeField] private string idleStateName = "Idle";
    [SerializeField] private string walkStateName = "Walk";
    [SerializeField] private string attackStateName = "Attack";
    [SerializeField] private string staggerStateName = "Stagger";
    [SerializeField] private string deathStateName = "Death";
    [SerializeField] private float locomotionFade = 0.12f;
    [SerializeField] private float attackFade = 0.05f;
    [SerializeField] private string singleStateAnimationName = "anim";
    [SerializeField] private RuntimeAnimatorController idleController;
    [SerializeField] private RuntimeAnimatorController walkController;
    [SerializeField] private RuntimeAnimatorController attackController;
    [SerializeField] private RuntimeAnimatorController staggerController;
    [SerializeField] private RuntimeAnimatorController deathController;

    private bool combatEnabled;
    private float cooldownTimer;
    private Coroutine deathAnimationRoutine;
    private EnemyState state = EnemyState.Idle;
    private float stateTimer;
    private string currentAnimationState;
    private bool deathAnimationStarted;
    private Renderer attackTelegraphRenderer;
    private Material attackTelegraphMaterial;

    public bool CombatEnabled => combatEnabled;
    public string CurrentStateName => state.ToString();
    public float AttackWindup
    {
        get => attackWindup;
        set => attackWindup = Mathf.Clamp(value, 0.05f, 2f);
    }

    public float AttackActiveTime
    {
        get => attackActiveTime;
        set => attackActiveTime = Mathf.Clamp(value, 0.03f, 0.75f);
    }

    public float AttackRecovery
    {
        get => attackRecovery;
        set => attackRecovery = Mathf.Clamp(value, 0.05f, 2f);
    }

    public float AttackCooldown
    {
        get => attackCooldown;
        set => attackCooldown = Mathf.Clamp(value, 0.1f, 3f);
    }

    public bool ShowAttackTelegraph
    {
        get => showAttackTelegraph;
        set
        {
            showAttackTelegraph = value;
            if (!showAttackTelegraph)
            {
                SetAttackTelegraphVisible(false);
            }
        }
    }

    private void Awake()
    {
        if (health == null)
        {
            health = GetComponent<TrainingDummyHealth>();
        }

        if (attackHitbox == null)
        {
            attackHitbox = GetComponentInChildren<EnemyAttackHitbox>();
        }

        if (animators == null || animators.Length == 0)
        {
            animators = GetComponentsInChildren<Animator>();
        }

        ApplyTuning(tuning);
        EnsureAttackTelegraph();
    }

    private void Update()
    {
        if (cooldownTimer > 0f)
        {
            cooldownTimer -= Time.deltaTime;
        }

        if (health != null && health.IsDead)
        {
            ChangeState(EnemyState.Death);
            StartDeathAnimation();
            return;
        }

        if (health != null && health.IsStaggered)
        {
            ChangeState(EnemyState.Stagger);
            return;
        }

        if (!CanThink())
        {
            ChangeState(EnemyState.Idle);
            return;
        }

        EnsureTarget();
        if (target == null)
        {
            ChangeState(EnemyState.Idle);
            return;
        }

        switch (state)
        {
            case EnemyState.Idle:
            case EnemyState.Chase:
                TickLocomotion();
                break;
            case EnemyState.Windup:
                TickWindup();
                break;
            case EnemyState.Attack:
                TickAttack();
                break;
            case EnemyState.Recovery:
                TickRecovery();
                break;
            case EnemyState.Stagger:
                TickStagger();
                break;
        }
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
            ChangeState(EnemyState.Idle);
        }
    }

    public void Initialize(Transform playerTarget, EnemyAttackHitbox hitbox)
    {
        target = playerTarget;
        attackHitbox = hitbox;
    }

    public void InitializeAnimationControllers(
        RuntimeAnimatorController idle,
        RuntimeAnimatorController walk,
        RuntimeAnimatorController attack,
        RuntimeAnimatorController stagger,
        RuntimeAnimatorController death,
        string stateName = "anim")
    {
        idleController = idle;
        walkController = walk;
        attackController = attack;
        staggerController = stagger;
        deathController = death;
        singleStateAnimationName = stateName;
    }

    public void ApplyTuning(EnemyCombatTuning tuning)
    {
        if (tuning == null)
        {
            return;
        }

        moveSpeed = tuning.moveSpeed;
        rotationSpeed = tuning.rotationSpeed;
        stoppingDistance = tuning.stoppingDistance;
        attackRange = tuning.attackRange;
        AttackWindup = tuning.attackWindup;
        AttackActiveTime = tuning.attackActiveTime;
        AttackRecovery = tuning.attackRecovery;
        AttackCooldown = tuning.attackCooldown;
        ShowAttackTelegraph = tuning.showAttackTelegraph;
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

    private void Face(Vector3 direction)
    {
        Quaternion targetRotation = Quaternion.LookRotation(direction, Vector3.up);
        transform.rotation = Quaternion.Slerp(transform.rotation, targetRotation, rotationSpeed * Time.deltaTime);
    }

    private void TickLocomotion()
    {
        if (!TryGetTargetDirection(out Vector3 direction, out float distance))
        {
            ChangeState(EnemyState.Idle);
            return;
        }

        Face(direction);

        if (distance <= attackRange && cooldownTimer <= 0f)
        {
            ChangeState(EnemyState.Windup);
            return;
        }

        if (distance > stoppingDistance)
        {
            ChangeState(EnemyState.Chase);
            transform.position += direction * (moveSpeed * DevSettings.EnemySpeedMultiplier * Time.deltaTime);
            PlayAnimation(walkStateName, locomotionFade);
            return;
        }

        ChangeState(EnemyState.Idle);
        PlayAnimation(idleStateName, locomotionFade);
    }

    private void TickWindup()
    {
        stateTimer -= Time.deltaTime;

        if (TryGetTargetDirection(out Vector3 direction, out _))
        {
            Face(direction);
        }

        if (stateTimer > 0f)
        {
            return;
        }

        ChangeState(EnemyState.Attack);
    }

    private void TickAttack()
    {
        stateTimer -= Time.deltaTime;

        if (stateTimer > 0f)
        {
            return;
        }

        ChangeState(EnemyState.Recovery);
    }

    private void TickRecovery()
    {
        stateTimer -= Time.deltaTime;

        if (stateTimer > 0f)
        {
            return;
        }

        ChangeState(EnemyState.Idle);
    }

    private void TickStagger()
    {
        CancelAttack();
        PlayAnimation(staggerStateName, attackFade);

        if (health == null || !health.IsStaggered)
        {
            ChangeState(EnemyState.Idle);
        }
    }

    private void CancelAttack()
    {
        if (attackHitbox != null)
        {
            attackHitbox.DisableHitbox();
        }

        SetAttackTelegraphVisible(false);
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
        PlayAnimation(deathStateName, attackFade);

        float duration = Mathf.Max(0.2f, GetAnimationClipLength(deathStateName));
        yield return new WaitForSeconds(Mathf.Max(0.05f, duration - attackFade));

        foreach (Animator animator in animators)
        {
            if (animator == null || animator.runtimeAnimatorController == null)
            {
                continue;
            }

            animator.Play(GetPlayableStateName(deathStateName), 0, 0.999f);
            animator.speed = 0f;
        }
    }

    private float GetAnimationClipLength(string stateName)
    {
        float fallbackLength = 1.2f;

        foreach (Animator animator in animators)
        {
            if (animator == null || animator.runtimeAnimatorController == null)
            {
                continue;
            }

            foreach (AnimationClip clip in animator.runtimeAnimatorController.animationClips)
            {
                if (clip == null)
                {
                    continue;
                }

                if (clip.name == stateName || stateName.EndsWith(clip.name) || clip.name.EndsWith(stateName))
                {
                    return clip.length;
                }

                if (clip.name.Contains("Death"))
                {
                    fallbackLength = Mathf.Max(fallbackLength, clip.length);
                }
            }
        }

        return fallbackLength;
    }

    private void PlayAnimation(string stateName, float fadeDuration)
    {
        if (string.IsNullOrWhiteSpace(stateName) || currentAnimationState == stateName)
        {
            return;
        }

        bool played = false;
        foreach (Animator animator in animators)
        {
            if (animator == null || animator.runtimeAnimatorController == null)
            {
                continue;
            }

            animator.applyRootMotion = false;
            animator.speed = 1f;
            RuntimeAnimatorController controller = GetControllerForState(stateName);
            string playableStateName = GetPlayableStateName(stateName);

            if (controller != null)
            {
                if (animator.runtimeAnimatorController != controller)
                {
                    animator.runtimeAnimatorController = controller;
                }
            }

            animator.CrossFadeInFixedTime(playableStateName, fadeDuration);
            played = true;
        }

        if (played)
        {
            currentAnimationState = stateName;
        }
    }

    private RuntimeAnimatorController GetControllerForState(string stateName)
    {
        if (stateName == idleStateName)
        {
            return idleController;
        }

        if (stateName == walkStateName)
        {
            return walkController;
        }

        if (stateName == attackStateName)
        {
            return attackController;
        }

        if (stateName == staggerStateName)
        {
            return staggerController;
        }

        if (stateName == deathStateName)
        {
            return deathController;
        }

        return null;
    }

    private string GetPlayableStateName(string stateName)
    {
        return GetControllerForState(stateName) != null ? singleStateAnimationName : stateName;
    }

    private void ChangeState(EnemyState nextState)
    {
        if (state == nextState)
        {
            return;
        }

        ExitState(state);
        state = nextState;
        EnterState(nextState);
    }

    private void EnterState(EnemyState nextState)
    {
        switch (nextState)
        {
            case EnemyState.Idle:
                PlayAnimation(idleStateName, locomotionFade);
                break;
            case EnemyState.Chase:
                PlayAnimation(walkStateName, locomotionFade);
                break;
            case EnemyState.Windup:
                stateTimer = attackWindup;
                SetAttackTelegraphVisible(showAttackTelegraph);
                PlayAnimation(attackStateName, attackFade);
                break;
            case EnemyState.Attack:
                stateTimer = attackActiveTime;
                SetAttackTelegraphVisible(false);
                cooldownTimer = attackCooldown;
                if (attackHitbox != null)
                {
                    attackHitbox.EnableHitbox(transform);
                }

                CombatFeedbackAudio.PlayEnemyAttack(transform.position);
                break;
            case EnemyState.Recovery:
                stateTimer = attackRecovery;
                if (attackHitbox != null)
                {
                    attackHitbox.DisableHitbox();
                }
                break;
            case EnemyState.Stagger:
                CancelAttack();
                PlayAnimation(staggerStateName, attackFade);
                break;
            case EnemyState.Death:
                CancelAttack();
                break;
        }
    }

    private void ExitState(EnemyState previousState)
    {
        if (previousState == EnemyState.Windup)
        {
            SetAttackTelegraphVisible(false);
        }

        if (previousState == EnemyState.Attack && attackHitbox != null)
        {
            attackHitbox.DisableHitbox();
        }
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

    private void EnsureAttackTelegraph()
    {
        if (attackTelegraphRenderer != null)
        {
            return;
        }

        GameObject telegraph = GameObject.CreatePrimitive(PrimitiveType.Cube);
        telegraph.name = "EnemyAttackTelegraph";
        telegraph.transform.SetParent(transform, false);
        telegraph.transform.localPosition = attackTelegraphLocalPosition;
        telegraph.transform.localRotation = Quaternion.identity;
        telegraph.transform.localScale = attackTelegraphScale;

        Collider telegraphCollider = telegraph.GetComponent<Collider>();
        if (telegraphCollider != null)
        {
            Destroy(telegraphCollider);
        }

        attackTelegraphRenderer = telegraph.GetComponent<Renderer>();
        attackTelegraphMaterial = CreateTelegraphMaterial();
        attackTelegraphRenderer.sharedMaterial = attackTelegraphMaterial;
        SetAttackTelegraphVisible(false);
    }

    private void SetAttackTelegraphVisible(bool visible)
    {
        if (attackTelegraphRenderer != null)
        {
            attackTelegraphRenderer.enabled = visible;
        }
    }

    private Material CreateTelegraphMaterial()
    {
        Shader shader = Shader.Find("Universal Render Pipeline/Unlit");
        shader = shader != null ? shader : Shader.Find("Unlit/Color");
        shader = shader != null ? shader : Shader.Find("Standard");

        Material material = new(shader)
        {
            name = "EnemyAttackTelegraph_Mat",
            color = attackTelegraphColor
        };

        if (material.HasProperty("_BaseColor"))
        {
            material.SetColor("_BaseColor", attackTelegraphColor);
        }

        if (material.HasProperty("_Color"))
        {
            material.SetColor("_Color", attackTelegraphColor);
        }

        material.SetFloat("_Surface", 1f);
        material.SetOverrideTag("RenderType", "Transparent");
        material.EnableKeyword("_SURFACE_TYPE_TRANSPARENT");
        material.renderQueue = (int)UnityEngine.Rendering.RenderQueue.Transparent;
        return material;
    }
}
