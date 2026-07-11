using UnityEngine;
using UnityEngine.Rendering;

public sealed class GolemGroundSlamVfx : MonoBehaviour
{
    [Header("Area")]
    [SerializeField] private BoxCollider damageArea;
    [SerializeField, Min(0f)] private float groundOffset = 0.025f;

    [Header("Textures")]
    [SerializeField] private Texture2D telegraphTexture;
    [SerializeField] private Texture2D impactTexture;

    [Header("Colors")]
    [SerializeField] private Color telegraphColor = new(0.42f, 0.36f, 0.28f, 0.26f);
    [SerializeField] private Color dustColor = new(0.38f, 0.31f, 0.22f, 0.58f);
    [SerializeField] private Color debrisColor = new(0.3f, 0.24f, 0.17f, 0.88f);

    private GameObject telegraphObject;
    private Material telegraphMaterial;
    private Material dustMaterial;
    private Material debrisMaterial;

    public bool TelegraphActive => telegraphObject != null;

    private void Awake()
    {
        CreateMaterials();
    }

    private void OnDisable()
    {
        CancelTelegraph();
    }

    private void OnDestroy()
    {
        CancelTelegraph();
        DestroyMaterial(telegraphMaterial);
        DestroyMaterial(dustMaterial);
        DestroyMaterial(debrisMaterial);
    }

    public void Configure(BoxCollider slamDamageArea, Texture2D smokeTexture, Texture2D dirtTexture)
    {
        damageArea = slamDamageArea;
        telegraphTexture = smokeTexture;
        impactTexture = dirtTexture;
    }

    public void BeginTelegraph(float duration)
    {
        CancelTelegraph();
        CreateMaterials();

        if (!TryGetGroundArea(out Vector3 center, out Quaternion rotation, out Vector3 size))
        {
            return;
        }

        float safeDuration = Mathf.Max(0.05f, duration);
        telegraphObject = CreateParticleObject("GolemSlamTelegraphVfx", center, rotation);
        ParticleSystem particles = telegraphObject.GetComponent<ParticleSystem>();

        ParticleSystem.MainModule main = particles.main;
        main.duration = safeDuration;
        main.loop = false;
        main.startLifetime = new ParticleSystem.MinMaxCurve(0.22f, 0.4f);
        main.startSpeed = new ParticleSystem.MinMaxCurve(0.01f, 0.05f);
        main.startSize = new ParticleSystem.MinMaxCurve(0.14f, 0.32f);
        main.startRotation = new ParticleSystem.MinMaxCurve(0f, Mathf.PI * 2f);
        main.startColor = Color.white;
        main.gravityModifier = 0f;
        main.simulationSpace = ParticleSystemSimulationSpace.World;
        main.maxParticles = 80;

        ParticleSystem.EmissionModule emission = particles.emission;
        emission.rateOverTime = 54f;

        ParticleSystem.ShapeModule shape = particles.shape;
        shape.enabled = true;
        shape.shapeType = ParticleSystemShapeType.Box;
        shape.scale = new Vector3(size.x * 0.92f, 0.035f, size.z * 0.92f);

        ParticleSystem.VelocityOverLifetimeModule velocity = particles.velocityOverLifetime;
        velocity.enabled = true;
        velocity.space = ParticleSystemSimulationSpace.World;
        velocity.x = new ParticleSystem.MinMaxCurve(0f, 0f);
        velocity.y = new ParticleSystem.MinMaxCurve(0.015f, 0.08f);
        velocity.z = new ParticleSystem.MinMaxCurve(0f, 0f);

        ConfigureFade(particles, 0f, 0.42f, 0f);
        AssignMaterial(particles, telegraphMaterial);

        particles.Play();
        Destroy(telegraphObject, safeDuration + 0.65f);
    }

    public void PlayImpact()
    {
        CancelTelegraph();
        CreateMaterials();

        if (!TryGetGroundArea(out Vector3 center, out Quaternion rotation, out Vector3 size))
        {
            return;
        }

        SpawnImpactLayer(
            "GolemSlamDustVfx",
            center,
            rotation,
            size,
            dustMaterial,
            28,
            new Vector2(0.28f, 0.62f),
            new Vector2(0.28f, 0.64f),
            0.42f,
            0.35f);

        SpawnImpactLayer(
            "GolemSlamDebrisVfx",
            center,
            rotation,
            size * 0.88f,
            debrisMaterial,
            34,
            new Vector2(0.08f, 0.22f),
            new Vector2(0.34f, 0.72f),
            1.1f,
            0.75f);
    }

    public void CancelTelegraph()
    {
        if (telegraphObject == null)
        {
            return;
        }

        ParticleSystem particles = telegraphObject.GetComponent<ParticleSystem>();
        if (particles != null)
        {
            particles.Stop(true, ParticleSystemStopBehavior.StopEmittingAndClear);
        }

        Destroy(telegraphObject, 0.05f);
        telegraphObject = null;
    }

    private void SpawnImpactLayer(
        string objectName,
        Vector3 center,
        Quaternion rotation,
        Vector3 size,
        Material material,
        int count,
        Vector2 particleSize,
        Vector2 lifetime,
        float upwardSpeed,
        float horizontalSpeed)
    {
        GameObject effectObject = CreateParticleObject(objectName, center, rotation);
        ParticleSystem particles = effectObject.GetComponent<ParticleSystem>();

        ParticleSystem.MainModule main = particles.main;
        main.duration = 0.12f;
        main.loop = false;
        main.startLifetime = new ParticleSystem.MinMaxCurve(lifetime.x, lifetime.y);
        main.startSpeed = new ParticleSystem.MinMaxCurve(0.02f, 0.14f);
        main.startSize = new ParticleSystem.MinMaxCurve(particleSize.x, particleSize.y);
        main.startRotation = new ParticleSystem.MinMaxCurve(0f, Mathf.PI * 2f);
        main.startColor = Color.white;
        main.gravityModifier = material == debrisMaterial ? 0.9f : 0.16f;
        main.simulationSpace = ParticleSystemSimulationSpace.World;
        main.maxParticles = count + 4;

        ParticleSystem.EmissionModule emission = particles.emission;
        emission.rateOverTime = 0f;
        emission.SetBursts(new[] { new ParticleSystem.Burst(0f, (short)count) });

        ParticleSystem.ShapeModule shape = particles.shape;
        shape.enabled = true;
        shape.shapeType = ParticleSystemShapeType.Box;
        shape.scale = new Vector3(size.x * 0.88f, 0.04f, size.z * 0.88f);

        ParticleSystem.VelocityOverLifetimeModule velocity = particles.velocityOverLifetime;
        velocity.enabled = true;
        velocity.space = ParticleSystemSimulationSpace.World;
        velocity.x = new ParticleSystem.MinMaxCurve(-horizontalSpeed, horizontalSpeed);
        velocity.y = new ParticleSystem.MinMaxCurve(upwardSpeed * 0.45f, upwardSpeed);
        velocity.z = new ParticleSystem.MinMaxCurve(-horizontalSpeed, horizontalSpeed);

        ConfigureFade(particles, 0.8f, 1f, 0f);
        AssignMaterial(particles, material);

        particles.Play();
        Destroy(effectObject, lifetime.y + 0.8f);
    }

    private bool TryGetGroundArea(out Vector3 center, out Quaternion rotation, out Vector3 size)
    {
        center = transform.position;
        rotation = transform.rotation;
        size = Vector3.zero;

        if (damageArea == null)
        {
            return false;
        }

        Transform areaTransform = damageArea.transform;
        Vector3 scale = areaTransform.lossyScale;
        scale = new Vector3(Mathf.Abs(scale.x), Mathf.Abs(scale.y), Mathf.Abs(scale.z));
        size = Vector3.Scale(damageArea.size, scale);
        rotation = areaTransform.rotation;

        Vector3 boxCenter = areaTransform.TransformPoint(damageArea.center);
        Vector3 boxBottom = boxCenter - areaTransform.up * (size.y * 0.5f);
        center = ResolveGroundPoint(boxBottom) + Vector3.up * groundOffset;
        return size.x > 0.01f && size.z > 0.01f;
    }

    private Vector3 ResolveGroundPoint(Vector3 expectedPoint)
    {
        Vector3 rayOrigin = expectedPoint + Vector3.up * 2f;
        RaycastHit[] hits = Physics.RaycastAll(
            rayOrigin,
            Vector3.down,
            4f,
            Physics.DefaultRaycastLayers,
            QueryTriggerInteraction.Ignore);

        float bestDelta = float.PositiveInfinity;
        Vector3 bestPoint = expectedPoint;
        foreach (RaycastHit hit in hits)
        {
            Transform hitTransform = hit.collider.transform;
            if (hitTransform == transform
                || hitTransform.IsChildOf(transform)
                || hit.collider.GetComponentInParent<PlayerHealth>() != null
                || hit.collider.GetComponentInParent<TrainingDummyHealth>() != null
                || Vector3.Dot(hit.normal, Vector3.up) < 0.6f)
            {
                continue;
            }

            float delta = Mathf.Abs(hit.point.y - expectedPoint.y);
            if (delta < bestDelta)
            {
                bestDelta = delta;
                bestPoint = hit.point;
            }
        }

        return bestPoint;
    }

    private static GameObject CreateParticleObject(string objectName, Vector3 position, Quaternion rotation)
    {
        GameObject effectObject = new(objectName);
        effectObject.transform.SetPositionAndRotation(position, rotation);

        ParticleSystem particles = effectObject.AddComponent<ParticleSystem>();
        particles.Stop(true, ParticleSystemStopBehavior.StopEmittingAndClear);
        return effectObject;
    }

    private static void ConfigureFade(ParticleSystem particles, float startAlpha, float middleAlpha, float endAlpha)
    {
        Gradient gradient = new();
        gradient.SetKeys(
            new[]
            {
                new GradientColorKey(Color.white, 0f),
                new GradientColorKey(Color.white, 1f)
            },
            new[]
            {
                new GradientAlphaKey(startAlpha, 0f),
                new GradientAlphaKey(middleAlpha, 0.18f),
                new GradientAlphaKey(endAlpha, 1f)
            });

        ParticleSystem.ColorOverLifetimeModule color = particles.colorOverLifetime;
        color.enabled = true;
        color.color = gradient;
    }

    private static void AssignMaterial(ParticleSystem particles, Material material)
    {
        ParticleSystemRenderer renderer = particles.GetComponent<ParticleSystemRenderer>();
        renderer.renderMode = ParticleSystemRenderMode.Billboard;
        renderer.sortingFudge = 6f;
        if (material != null)
        {
            renderer.sharedMaterial = material;
        }
    }

    private void CreateMaterials()
    {
        telegraphMaterial ??= CreateParticleMaterial("Golem Slam Telegraph", telegraphTexture, telegraphColor);
        dustMaterial ??= CreateParticleMaterial("Golem Slam Dust", telegraphTexture, dustColor);
        debrisMaterial ??= CreateParticleMaterial("Golem Slam Debris", impactTexture, debrisColor);
    }

    private static Material CreateParticleMaterial(string materialName, Texture texture, Color tint)
    {
        Shader shader = Shader.Find("Universal Render Pipeline/Particles/Unlit");
        if (shader == null)
        {
            shader = Shader.Find("Particles/Standard Unlit");
        }

        if (shader == null)
        {
            return null;
        }

        Material material = new(shader)
        {
            name = materialName,
            hideFlags = HideFlags.DontSave,
            color = tint,
            renderQueue = (int)RenderQueue.Transparent
        };

        if (texture != null)
        {
            material.mainTexture = texture;
            if (material.HasProperty("_BaseMap"))
            {
                material.SetTexture("_BaseMap", texture);
            }
        }

        if (material.HasProperty("_BaseColor"))
        {
            material.SetColor("_BaseColor", tint);
        }

        material.SetOverrideTag("RenderType", "Transparent");
        material.SetFloat("_Surface", 1f);
        material.SetFloat("_Blend", 0f);
        material.SetFloat("_SrcBlend", (float)BlendMode.SrcAlpha);
        material.SetFloat("_DstBlend", (float)BlendMode.OneMinusSrcAlpha);
        material.SetFloat("_ZWrite", 0f);
        material.EnableKeyword("_SURFACE_TYPE_TRANSPARENT");
        return material;
    }

    private static void DestroyMaterial(Material material)
    {
        if (material != null)
        {
            Destroy(material);
        }
    }
}
