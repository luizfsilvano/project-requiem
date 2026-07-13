using System;
using System.Collections;
using UnityEngine;

public sealed class TrainingDummyHealth : MonoBehaviour
{
    private enum ImpactProfile
    {
        LightMob,
        HeavyMob,
        Boss
    }

    [Header("Vitals")]
    [SerializeField] private EnemyCombatTuning tuning;
    [SerializeField] private int maxHealth = 100;
    [SerializeField] private float maxPoise = 100f;
    [SerializeField] private float poiseRecoveryDelay = 1.2f;
    [SerializeField] private float poiseRecoveryPerSecond = 35f;

    [Header("Impact Rules")]
    [SerializeField] private ImpactProfile impactProfile = ImpactProfile.LightMob;
    [SerializeField] private bool allowStagger = true;
    [SerializeField] private bool allowHitStop = true;
    [SerializeField] private bool allowCameraShake = true;
    [SerializeField] private bool allowScalePunch = true;
    [SerializeField] private float heavyPoiseDamageMultiplier = 0.55f;
    [SerializeField] private float bossPoiseDamageMultiplier = 0f;
    [SerializeField] private float heavyKnockbackMultiplier = 0.45f;
    [SerializeField] private float bossKnockbackMultiplier = 0.08f;
    [SerializeField] private float staggerKnockbackMultiplier = 1.8f;

    [Header("Visual Feedback")]
    [SerializeField] private Renderer bodyRenderer;
    [SerializeField] private Renderer[] feedbackRenderers;
    [SerializeField] private Color hitColor = new(1f, 0.35f, 0.25f);
    [SerializeField] private Color staggerColor = new(1f, 0.92f, 0.35f);
    [SerializeField] private Color deathColor = new(0.22f, 0.22f, 0.22f);
    [SerializeField] private float staggerScalePunch = 1.18f;
    [SerializeField] private float deathColorDelay = 0.12f;

    [Header("Death Cleanup")]
    [SerializeField] private bool disableCollisionOnDeath = true;
    [SerializeField] private bool destroyAfterDeath = true;
    [SerializeField] private float deathDespawnDelay = 30f;
    [SerializeField] private float deathFadeDuration = 1.4f;

    [Header("Floating Bars")]
    [SerializeField] private bool showFloatingBars = true;
    [SerializeField] private Vector3 barWorldOffset = new(0f, 2.35f, 0f);
    [SerializeField] private Vector2 healthBarSize = new(1.15f, 0.09f);
    [SerializeField] private Vector2 poiseBarSize = new(1.15f, 0.045f);
    [SerializeField] private Color healthFillColor = new(0.86f, 0.08f, 0.05f);
    [SerializeField] private Color poiseFillColor = new(1f, 0.72f, 0.12f);
    [SerializeField] private Color barBackColor = new(0.04f, 0.04f, 0.04f);

    [Header("Targeting Overrides")]
    [SerializeField] private float lockOnHeightOverride = -1f;
    [SerializeField] private float lockOnIndicatorRadiusOverride = -1f;
    [SerializeField] private float lockOnIndicatorHeightOverride = -1f;

    [Header("Hit Sparks")]
    [SerializeField] private bool spawnHitSparks = true;
    [SerializeField] private int hitSparkCount = 8;
    [SerializeField] private int staggerSparkCount = 16;

    private int currentHealth;
    private float currentPoise;
    private float poiseRecoveryTimer;
    private Color defaultColor;
    private MaterialPropertyBlock feedbackBlock;
    private Coroutine flashRoutine;
    private Coroutine knockbackRoutine;
    private Coroutine scaleRoutine;
    private Coroutine deathRoutine;
    private Vector3 defaultScale;
    private bool isDead;
    private Transform barRoot;
    private Transform healthFill;
    private Transform poiseFill;
    private Camera mainCamera;
    private Material healthBarMaterial;
    private Material poiseBarMaterial;
    private Material barBackMaterial;
    private Material sparkMaterial;
    private Rigidbody[] rigidbodiesToDisableOnDeath;
    private Transform lastDamageAttacker;

    public float HealthRatio => maxHealth > 0 ? currentHealth / (float)maxHealth : 0f;
    public float PoiseRatio => maxPoise > 0f ? currentPoise / maxPoise : 0f;
    public int CurrentHealth => currentHealth;
    public int MaxHealth => maxHealth;
    public float MaxPoise => maxPoise;
    public bool IsDead => isDead;
    public bool IsStaggered => staggerLockTimer > 0f;
    public bool IsControlLocked => isDead || staggerLockTimer > 0f;
    public event Action<TrainingDummyHealth, Transform> Died;
    public float StaggerLockDuration
    {
        get => staggerLockDuration;
        set => staggerLockDuration = Mathf.Clamp(value, 0.05f, 3f);
    }

    [Header("AI Locks")]
    [SerializeField] private float staggerLockDuration = 0.85f;
    private float staggerLockTimer;

    private void Awake()
    {
        currentHealth = maxHealth;

        if (bodyRenderer == null)
        {
            bodyRenderer = GetComponentInChildren<Renderer>();
        }

        if (feedbackRenderers == null || feedbackRenderers.Length == 0)
        {
            feedbackRenderers = GetComponentsInChildren<Renderer>();
        }

        rigidbodiesToDisableOnDeath = GetComponentsInChildren<Rigidbody>();

        if (bodyRenderer != null)
        {
            defaultColor = bodyRenderer.sharedMaterial != null ? bodyRenderer.sharedMaterial.color : Color.white;
        }
        else
        {
            defaultColor = Color.white;
        }

        feedbackBlock = new MaterialPropertyBlock();

        ApplyTuning(tuning);
        defaultScale = transform.localScale;

        if (showFloatingBars)
        {
            CreateFloatingBars();
            UpdateFloatingBars();
        }
    }

    private void OnDestroy()
    {
        DestroyFloatingBars();
    }

    private void Update()
    {
        if (staggerLockTimer > 0f)
        {
            staggerLockTimer -= Time.deltaTime;
        }

        if (currentPoise <= 0f)
        {
            return;
        }

        if (poiseRecoveryTimer > 0f)
        {
            poiseRecoveryTimer -= Time.deltaTime;
            return;
        }

        currentPoise = Mathf.Max(0f, currentPoise - poiseRecoveryPerSecond * Time.deltaTime);
        UpdateFloatingBars();
    }

    private void LateUpdate()
    {
        UpdateFloatingBarTransform();
    }

    public void TakeDamage(int amount)
    {
        TakeHit(new CombatHit(
            amount,
            amount,
            0.12f,
            0.1f,
            0f,
            0f,
            0f,
            CombatImpactWeight.Light,
            transform.position,
            transform.forward,
            null,
            1));
    }

    public void TakeHit(CombatHit hit)
    {
        if (isDead)
        {
            return;
        }

        int appliedDamage = DevSettings.EnemyInvincible ? 0 : hit.Damage;
        if (appliedDamage > 0 && hit.Attacker != null)
        {
            lastDamageAttacker = hit.Attacker;
        }

        currentHealth = Mathf.Max(0, currentHealth - appliedDamage);
        currentPoise += GetAdjustedPoiseDamage(hit);
        poiseRecoveryTimer = poiseRecoveryDelay;

        CombatImpactReaction reaction = ResolveReaction();
        if (reaction == CombatImpactReaction.Stagger)
        {
            currentPoise = 0f;
            staggerLockTimer = staggerLockDuration;
        }

        UpdateFloatingBars();

        Debug.Log($"{name} took {appliedDamage} damage. HP: {currentHealth}/{maxHealth}. Poise: {currentPoise:0}/{maxPoise:0}. Profile: {impactProfile}. Impact: {hit.ImpactWeight}. Reaction: {reaction}");

        if (flashRoutine != null)
        {
            StopCoroutine(flashRoutine);
        }

        flashRoutine = StartCoroutine(FlashHit(reaction));
        SpawnHitSparks(hit, reaction);
        CombatFeedbackAudio.PlayEnemyHit(reaction, hit.ImpactWeight, hit.Point);
        CombatFeedbackVfx.SpawnEnemyHit(hit.Point, reaction, hit.ImpactWeight);
        StartKnockback(hit, reaction);
        StartScalePunch(hit, reaction);
        TriggerCameraShake(hit, reaction);
        TriggerHitStop(hit, reaction);

        if (reaction == CombatImpactReaction.Death)
        {
            BeginDeath();
        }
    }

    public void SetFloatingBarOffset(Vector3 offset)
    {
        barWorldOffset = offset;
        UpdateFloatingBarTransform();
    }

    public Vector3 GetLockOnPoint(float fallbackHeightOffset)
    {
        float height = lockOnHeightOverride >= 0f ? lockOnHeightOverride : fallbackHeightOffset;
        return transform.position + Vector3.up * height;
    }

    public Vector3 GetLockOnIndicatorCenter(float fallbackHeightOffset)
    {
        float height = lockOnIndicatorHeightOverride >= 0f ? lockOnIndicatorHeightOverride : fallbackHeightOffset;
        return transform.position + Vector3.up * height;
    }

    public float GetLockOnIndicatorRadius(float fallbackRadius)
    {
        return lockOnIndicatorRadiusOverride > 0f ? lockOnIndicatorRadiusOverride : fallbackRadius;
    }

    public void ApplyTuning(EnemyCombatTuning tuning)
    {
        if (tuning == null)
        {
            return;
        }

        StaggerLockDuration = tuning.staggerLockDuration;
    }

    private void ResetHealth()
    {
        currentHealth = maxHealth;
        currentPoise = 0f;
        poiseRecoveryTimer = 0f;
        staggerLockTimer = 0f;
        isDead = false;
        lastDamageAttacker = null;
        transform.localScale = defaultScale;

        if (bodyRenderer != null)
        {
            SetFeedbackTint(defaultColor);
        }

        UpdateFloatingBars();
        Debug.Log($"{name} reset to {maxHealth} HP.");
    }

    private IEnumerator FlashHit(CombatImpactReaction reaction)
    {
        if (feedbackRenderers == null || feedbackRenderers.Length == 0)
        {
            yield break;
        }

        SetFeedbackTint(GetReactionColor(reaction));
        yield return new WaitForSecondsRealtime(GetFlashDuration(reaction));

        if (reaction != CombatImpactReaction.Death)
        {
            SetFeedbackTint(defaultColor);
        }

        flashRoutine = null;
    }

    private void StartKnockback(CombatHit hit, CombatImpactReaction reaction)
    {
        if (knockbackRoutine != null)
        {
            StopCoroutine(knockbackRoutine);
        }

        float distance = hit.KnockbackDistance * GetKnockbackMultiplier(hit, reaction);
        if (distance <= 0f || hit.KnockbackDuration <= 0f)
        {
            return;
        }

        knockbackRoutine = StartCoroutine(Knockback(hit.Direction, distance, hit.KnockbackDuration));
    }

    private IEnumerator Knockback(Vector3 direction, float distance, float duration)
    {
        Vector3 start = transform.position;
        Vector3 target = start + Vector3.ProjectOnPlane(direction, Vector3.up).normalized * distance;
        float elapsed = 0f;

        while (elapsed < duration)
        {
            elapsed += Time.unscaledDeltaTime;
            float t = Mathf.Clamp01(elapsed / duration);
            float eased = 1f - (1f - t) * (1f - t);
            transform.position = Vector3.Lerp(start, target, eased);
            yield return null;
        }

        transform.position = target;
        knockbackRoutine = null;
    }

    private void StartScalePunch(CombatHit hit, CombatImpactReaction reaction)
    {
        if (!allowScalePunch)
        {
            return;
        }

        if (scaleRoutine != null)
        {
            StopCoroutine(scaleRoutine);
        }

        scaleRoutine = StartCoroutine(ScalePunch(GetScalePunchPeak(hit, reaction), GetScalePunchDuration(reaction)));
    }

    private IEnumerator ScalePunch(float peak, float duration)
    {
        float elapsed = 0f;

        while (elapsed < duration)
        {
            elapsed += Time.unscaledDeltaTime;
            float t = Mathf.Clamp01(elapsed / duration);
            float pulse = Mathf.Sin(t * Mathf.PI);
            transform.localScale = defaultScale * Mathf.Lerp(1f, peak, pulse);
            yield return null;
        }

        transform.localScale = defaultScale;
        scaleRoutine = null;
    }

    private void BeginDeath()
    {
        if (isDead)
        {
            return;
        }

        isDead = true;
        currentHealth = 0;
        currentPoise = 0f;
        poiseRecoveryTimer = 0f;
        staggerLockTimer = 0f;
        DestroyFloatingBars();
        DisableGameplayCollision();
        Died?.Invoke(this, lastDamageAttacker);

        deathRoutine ??= StartCoroutine(DeathSequence());
    }

    private IEnumerator DeathSequence()
    {
        BeginDeath();

        yield return new WaitForSecondsRealtime(deathColorDelay);

        if (bodyRenderer != null)
        {
            SetFeedbackTint(deathColor);
        }

        if (!destroyAfterDeath)
        {
            yield break;
        }

        float delayBeforeFade = Mathf.Max(0f, deathDespawnDelay);
        if (delayBeforeFade > 0f)
        {
            yield return new WaitForSeconds(delayBeforeFade);
        }

        yield return FadeOutAndDestroy();
    }

    private void DisableGameplayCollision()
    {
        if (!disableCollisionOnDeath)
        {
            return;
        }

        foreach (Collider collider in GetComponentsInChildren<Collider>(true))
        {
            if (collider != null)
            {
                collider.enabled = false;
            }
        }

        if (rigidbodiesToDisableOnDeath != null)
        {
            foreach (Rigidbody body in rigidbodiesToDisableOnDeath)
            {
                if (body == null)
                {
                    continue;
                }

                if (!body.isKinematic)
                {
                    body.linearVelocity = Vector3.zero;
                    body.angularVelocity = Vector3.zero;
                }

                body.detectCollisions = false;
                body.isKinematic = true;
            }
        }
    }

    private IEnumerator FadeOutAndDestroy()
    {
        PrepareRenderersForFade();

        float duration = Mathf.Max(0.05f, deathFadeDuration);
        float elapsed = 0f;

        while (elapsed < duration)
        {
            elapsed += Time.deltaTime;
            float alpha = 1f - Mathf.Clamp01(elapsed / duration);
            SetFeedbackTint(new Color(deathColor.r, deathColor.g, deathColor.b, alpha));
            yield return null;
        }

        Destroy(gameObject);
    }

    private void PrepareRenderersForFade()
    {
        if (feedbackRenderers == null)
        {
            return;
        }

        foreach (Renderer renderer in feedbackRenderers)
        {
            if (renderer == null || renderer.transform == barRoot || renderer.GetComponent<ParticleSystemRenderer>() != null)
            {
                continue;
            }

            Material[] materials = renderer.materials;
            foreach (Material material in materials)
            {
                if (material == null)
                {
                    continue;
                }

                SetMaterialTransparent(material);
            }

            renderer.materials = materials;
        }
    }

    private static void SetMaterialTransparent(Material material)
    {
        if (material.HasProperty("_Surface"))
        {
            material.SetFloat("_Surface", 1f);
        }

        if (material.HasProperty("_Blend"))
        {
            material.SetFloat("_Blend", 0f);
        }

        if (material.HasProperty("_Mode"))
        {
            material.SetFloat("_Mode", 2f);
        }

        if (material.HasProperty("_SrcBlend"))
        {
            material.SetFloat("_SrcBlend", (float)UnityEngine.Rendering.BlendMode.SrcAlpha);
        }

        if (material.HasProperty("_DstBlend"))
        {
            material.SetFloat("_DstBlend", (float)UnityEngine.Rendering.BlendMode.OneMinusSrcAlpha);
        }

        if (material.HasProperty("_SrcBlendAlpha"))
        {
            material.SetFloat("_SrcBlendAlpha", (float)UnityEngine.Rendering.BlendMode.One);
        }

        if (material.HasProperty("_DstBlendAlpha"))
        {
            material.SetFloat("_DstBlendAlpha", (float)UnityEngine.Rendering.BlendMode.OneMinusSrcAlpha);
        }

        if (material.HasProperty("_ZWrite"))
        {
            material.SetFloat("_ZWrite", 0f);
        }

        material.SetOverrideTag("RenderType", "Transparent");
        material.DisableKeyword("_ALPHATEST_ON");
        material.DisableKeyword("_ALPHAPREMULTIPLY_ON");
        material.EnableKeyword("_ALPHABLEND_ON");
        material.EnableKeyword("_SURFACE_TYPE_TRANSPARENT");
        material.renderQueue = (int)UnityEngine.Rendering.RenderQueue.Transparent;
    }

    private void SetFeedbackTint(Color color)
    {
        if (feedbackRenderers == null)
        {
            return;
        }

        foreach (Renderer renderer in feedbackRenderers)
        {
            if (renderer == null || renderer.transform == barRoot || renderer.GetComponent<ParticleSystemRenderer>() != null)
            {
                continue;
            }

            renderer.GetPropertyBlock(feedbackBlock);
            feedbackBlock.Clear();

            foreach (Material material in renderer.sharedMaterials)
            {
                if (material == null)
                {
                    continue;
                }

                if (material.HasProperty("_BaseColor"))
                {
                    feedbackBlock.SetColor("_BaseColor", color);
                }

                if (material.HasProperty("_Color"))
                {
                    feedbackBlock.SetColor("_Color", color);
                }
            }

            renderer.SetPropertyBlock(feedbackBlock);
        }
    }

    private void TriggerCameraShake(CombatHit hit, CombatImpactReaction reaction)
    {
        if (!allowCameraShake || hit.CameraShakeStrength <= 0f)
        {
            return;
        }

        ThirdPersonCameraController cameraController = FindFirstObjectByType<ThirdPersonCameraController>();
        if (cameraController == null)
        {
            return;
        }

        cameraController.AddShake(
            hit.CameraShakeStrength * GetCameraShakeMultiplier(hit, reaction),
            hit.CameraShakeDuration * GetCameraShakeDurationMultiplier(reaction));
    }

    private void TriggerHitStop(CombatHit hit, CombatImpactReaction reaction)
    {
        if (!ShouldHitStop(hit, reaction) || hit.HitStopDuration <= 0f)
        {
            return;
        }

        float duration = hit.HitStopDuration * GetHitStopMultiplier(reaction);
        CombatHitStop.TryPlay(duration);
    }

    private void SpawnHitSparks(CombatHit hit, CombatImpactReaction reaction)
    {
        if (!spawnHitSparks)
        {
            return;
        }

        GameObject sparkObject = new(reaction == CombatImpactReaction.Stagger ? "StaggerImpact" : "HitImpact");
        sparkObject.transform.position = hit.Point;
        sparkObject.transform.rotation = Quaternion.LookRotation(hit.Direction, Vector3.up);

        ParticleSystem particles = sparkObject.AddComponent<ParticleSystem>();
        particles.Stop(true, ParticleSystemStopBehavior.StopEmittingAndClear);

        ParticleSystem.MainModule main = particles.main;
        main.duration = 0.18f;
        main.loop = false;
        main.playOnAwake = false;
        main.simulationSpace = ParticleSystemSimulationSpace.World;
        main.startLifetime = IsStrongReaction(reaction) ? 0.28f : 0.16f;
        main.startSpeed = IsStrongReaction(reaction) ? 2.8f : 1.8f;
        main.startSize = GetSparkSize(hit, reaction);
        main.startColor = GetReactionColor(reaction);
        main.gravityModifier = 0.2f;

        ParticleSystem.EmissionModule emission = particles.emission;
        emission.SetBursts(new[]
        {
            new ParticleSystem.Burst(0f, (short)GetSparkCount(hit, reaction))
        });

        ParticleSystem.ShapeModule shape = particles.shape;
        shape.enabled = true;
        shape.shapeType = ParticleSystemShapeType.Cone;
        shape.angle = IsStrongReaction(reaction) ? 35f : 28f;
        shape.radius = 0.05f;

        ParticleSystemRenderer renderer = particles.GetComponent<ParticleSystemRenderer>();
        renderer.renderMode = ParticleSystemRenderMode.Billboard;
        if (sparkMaterial == null)
        {
            sparkMaterial = CreateRuntimeMaterial("TrainingDummy_Spark_Mat", Color.white, true);
        }

        renderer.material = sparkMaterial;
        particles.Play();

        Destroy(sparkObject, 0.8f);
    }

    private CombatImpactReaction ResolveReaction()
    {
        if (currentHealth <= 0)
        {
            return CombatImpactReaction.Death;
        }

        return CanStagger() && currentPoise >= maxPoise
            ? CombatImpactReaction.Stagger
            : CombatImpactReaction.Hit;
    }

    private float GetAdjustedPoiseDamage(CombatHit hit)
    {
        return hit.PoiseDamage * (impactProfile switch
        {
            ImpactProfile.HeavyMob => heavyPoiseDamageMultiplier,
            ImpactProfile.Boss => bossPoiseDamageMultiplier,
            _ => 1f
        });
    }

    private float GetKnockbackMultiplier(CombatHit hit, CombatImpactReaction reaction)
    {
        float profileMultiplier = impactProfile switch
        {
            ImpactProfile.HeavyMob => heavyKnockbackMultiplier,
            ImpactProfile.Boss => bossKnockbackMultiplier,
            _ => 1f
        };

        return profileMultiplier * GetImpactWeightMultiplier(hit) * (IsStrongReaction(reaction) ? staggerKnockbackMultiplier : 1f);
    }

    private float GetCameraShakeMultiplier(CombatHit hit, CombatImpactReaction reaction)
    {
        float profileMultiplier = impactProfile == ImpactProfile.Boss ? 0.7f : 1f;
        return profileMultiplier * GetImpactWeightMultiplier(hit) * (IsStrongReaction(reaction) ? 1.5f : 1f);
    }

    private static float GetCameraShakeDurationMultiplier(CombatImpactReaction reaction)
    {
        return IsStrongReaction(reaction) ? 1.4f : 1f;
    }

    private static float GetHitStopMultiplier(CombatImpactReaction reaction)
    {
        return IsStrongReaction(reaction) ? 1.35f : 1f;
    }

    private bool CanStagger()
    {
        return allowStagger && maxPoise > 0f;
    }

    private bool ShouldHitStop(CombatHit hit, CombatImpactReaction reaction)
    {
        if (!allowHitStop || impactProfile == ImpactProfile.Boss)
        {
            return false;
        }

        if (impactProfile == ImpactProfile.HeavyMob)
        {
            return IsStrongReaction(reaction);
        }

        return hit.ImpactWeight != CombatImpactWeight.Light || IsStrongReaction(reaction);
    }

    private static bool IsStrongReaction(CombatImpactReaction reaction)
    {
        return reaction == CombatImpactReaction.Stagger || reaction == CombatImpactReaction.Death;
    }

    private Color GetReactionColor(CombatImpactReaction reaction)
    {
        return reaction switch
        {
            CombatImpactReaction.Stagger => staggerColor,
            CombatImpactReaction.Death => deathColor,
            _ => hitColor
        };
    }

    private static float GetFlashDuration(CombatImpactReaction reaction)
    {
        return IsStrongReaction(reaction) ? 0.16f : 0.08f;
    }

    private float GetScalePunchPeak(CombatHit hit, CombatImpactReaction reaction)
    {
        if (IsStrongReaction(reaction))
        {
            return staggerScalePunch;
        }

        return hit.ImpactWeight switch
        {
            CombatImpactWeight.Heavy => 1.14f,
            CombatImpactWeight.Medium => 1.1f,
            _ => 1.06f
        };
    }

    private static float GetScalePunchDuration(CombatImpactReaction reaction)
    {
        return IsStrongReaction(reaction) ? 0.18f : 0.1f;
    }

    private static float GetImpactWeightMultiplier(CombatHit hit)
    {
        return hit.ImpactWeight switch
        {
            CombatImpactWeight.Heavy => 1.25f,
            CombatImpactWeight.Medium => 1f,
            _ => 0.8f
        };
    }

    private static float GetSparkSize(CombatHit hit, CombatImpactReaction reaction)
    {
        if (IsStrongReaction(reaction))
        {
            return 0.07f;
        }

        return hit.ImpactWeight switch
        {
            CombatImpactWeight.Heavy => 0.06f,
            CombatImpactWeight.Medium => 0.05f,
            _ => 0.04f
        };
    }

    private int GetSparkCount(CombatHit hit, CombatImpactReaction reaction)
    {
        if (IsStrongReaction(reaction))
        {
            return staggerSparkCount;
        }

        return hit.ImpactWeight switch
        {
            CombatImpactWeight.Heavy => Mathf.Max(hitSparkCount + 4, 10),
            CombatImpactWeight.Medium => Mathf.Max(hitSparkCount, 8),
            _ => Mathf.Max(hitSparkCount - 2, 4)
        };
    }

    private void CreateFloatingBars()
    {
        healthBarMaterial = CreateRuntimeMaterial("TrainingDummy_HealthBar_Mat", healthFillColor);
        poiseBarMaterial = CreateRuntimeMaterial("TrainingDummy_PoiseBar_Mat", poiseFillColor);
        barBackMaterial = CreateRuntimeMaterial("TrainingDummy_BarBack_Mat", barBackColor);

        barRoot = new GameObject($"{name}_StatusBars").transform;
        CreateBar("Health", 0f, healthBarSize, healthBarMaterial, out healthFill);
        CreateBar("Poise", -0.13f, poiseBarSize, poiseBarMaterial, out poiseFill);
    }

    private void CreateBar(string label, float yOffset, Vector2 size, Material fillMaterial, out Transform fill)
    {
        Transform back = CreateBarPart($"{label}_Back", barRoot, barBackMaterial);
        back.localPosition = new Vector3(0f, yOffset, 0f);
        back.localScale = new Vector3(size.x, size.y, 0.025f);

        fill = CreateBarPart($"{label}_Fill", barRoot, fillMaterial);
        fill.localPosition = new Vector3(0f, yOffset, -0.02f);
        fill.localScale = new Vector3(size.x, size.y, 0.03f);
    }

    private static Transform CreateBarPart(string objectName, Transform parent, Material material)
    {
        GameObject part = GameObject.CreatePrimitive(PrimitiveType.Cube);
        part.name = objectName;
        part.transform.SetParent(parent, false);

        Collider collider = part.GetComponent<Collider>();
        if (collider != null)
        {
            Destroy(collider);
        }

        Renderer renderer = part.GetComponent<Renderer>();
        renderer.sharedMaterial = material;
        return part.transform;
    }

    private void UpdateFloatingBars()
    {
        if (barRoot == null)
        {
            return;
        }

        SetBarFill(healthFill, healthBarSize, HealthRatio, 0f);
        SetBarFill(poiseFill, poiseBarSize, PoiseRatio, -0.13f);
    }

    private void DestroyFloatingBars()
    {
        if (barRoot == null)
        {
            return;
        }

        GameObject bars = barRoot.gameObject;
        bars.SetActive(false);
        barRoot = null;
        healthFill = null;
        poiseFill = null;
        Destroy(bars);
    }

    private void UpdateFloatingBarTransform()
    {
        if (barRoot == null)
        {
            return;
        }

        barRoot.position = transform.position + barWorldOffset;

        if (mainCamera == null)
        {
            mainCamera = Camera.main;
        }

        if (mainCamera == null)
        {
            return;
        }

        Vector3 toCamera = barRoot.position - mainCamera.transform.position;
        if (toCamera.sqrMagnitude > 0.001f)
        {
            barRoot.rotation = Quaternion.LookRotation(toCamera, Vector3.up);
        }
    }

    private static void SetBarFill(Transform fill, Vector2 size, float ratio, float yOffset)
    {
        if (fill == null)
        {
            return;
        }

        float normalized = Mathf.Clamp01(ratio);
        float width = Mathf.Max(0.001f, size.x * normalized);
        fill.localPosition = new Vector3((width - size.x) * 0.5f, yOffset, -0.02f);
        fill.localScale = new Vector3(width, size.y, 0.03f);
    }

    private static Material CreateRuntimeMaterial(string materialName, Color color, bool particleMaterial = false)
    {
        Shader shader = particleMaterial
            ? Shader.Find("Universal Render Pipeline/Particles/Unlit")
            : Shader.Find("Universal Render Pipeline/Unlit");

        shader = shader != null ? shader : Shader.Find(particleMaterial ? "Particles/Standard Unlit" : "Unlit/Color");
        shader = shader != null ? shader : Shader.Find("Standard");

        Material material = new(shader)
        {
            name = materialName
        };

        if (material.HasProperty("_BaseColor"))
        {
            material.SetColor("_BaseColor", color);
        }

        if (material.HasProperty("_Color"))
        {
            material.SetColor("_Color", color);
        }

        return material;
    }

}
