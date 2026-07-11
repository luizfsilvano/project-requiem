using UnityEngine;

public sealed class CombatFeedbackVfx : MonoBehaviour
{
    private static CombatFeedbackVfx instance;
    private Material sparkMaterial;
    private Material bloodMaterial;
    private Material impactMaterial;

    private void Awake()
    {
        instance = this;
        sparkMaterial = CreateParticleMaterial(new Color(1f, 0.78f, 0.28f, 1f));
        bloodMaterial = CreateParticleMaterial(new Color(1f, 0.08f, 0.03f, 1f));
        impactMaterial = CreateParticleMaterial(new Color(1f, 0.95f, 0.72f, 1f));
    }

    private void OnDestroy()
    {
        if (instance == this)
        {
            instance = null;
        }
    }

    public static void SpawnEnemyHit(Vector3 position, CombatImpactReaction reaction, CombatImpactWeight impactWeight)
    {
        if (instance == null)
        {
            return;
        }

        bool heavy = reaction == CombatImpactReaction.Stagger || impactWeight == CombatImpactWeight.Heavy;
        instance.SpawnBurst(position, heavy ? instance.impactMaterial : instance.bloodMaterial, heavy ? 18 : 10, heavy ? 0.42f : 0.26f);

        if (heavy || reaction == CombatImpactReaction.Death)
        {
            instance.SpawnBurst(position + Vector3.up * 0.08f, instance.sparkMaterial, 12, 0.34f);
        }
    }

    private void SpawnBurst(Vector3 position, Material material, int count, float speed)
    {
        GameObject burstObject = new("CombatHitVfx");
        burstObject.transform.position = position;

        ParticleSystem particles = burstObject.AddComponent<ParticleSystem>();
        particles.Stop(true, ParticleSystemStopBehavior.StopEmittingAndClear);
        ParticleSystem.MainModule main = particles.main;
        main.playOnAwake = false;
        main.duration = 0.14f;
        main.loop = false;
        main.startLifetime = new ParticleSystem.MinMaxCurve(0.12f, 0.26f);
        main.startSpeed = new ParticleSystem.MinMaxCurve(speed * 0.5f, speed);
        main.startSize = new ParticleSystem.MinMaxCurve(0.08f, 0.2f);
        main.startColor = Color.white;
        main.gravityModifier = 0.15f;
        main.simulationSpace = ParticleSystemSimulationSpace.World;

        ParticleSystem.EmissionModule emission = particles.emission;
        emission.SetBursts(new[] { new ParticleSystem.Burst(0f, (short)count) });

        ParticleSystem.ShapeModule shape = particles.shape;
        shape.enabled = true;
        shape.shapeType = ParticleSystemShapeType.Cone;
        shape.angle = 35f;
        shape.radius = 0.04f;

        ParticleSystemRenderer renderer = particles.GetComponent<ParticleSystemRenderer>();
        renderer.renderMode = ParticleSystemRenderMode.Billboard;
        renderer.sortingFudge = 8f;
        if (material != null)
        {
            renderer.material = material;
        }

        particles.Play();
        Destroy(burstObject, 0.8f);
    }

    private static Material CreateParticleMaterial(Color tint)
    {
        Shader shader = Shader.Find("Universal Render Pipeline/Particles/Unlit");
        if (shader == null)
        {
            shader = Shader.Find("Particles/Standard Unlit");
        }

        Material material = new(shader);
        material.color = tint;
        material.SetOverrideTag("RenderType", "Transparent");
        material.renderQueue = (int)UnityEngine.Rendering.RenderQueue.Transparent;
        material.SetFloat("_Surface", 1f);
        material.SetFloat("_Blend", 2f);
        material.SetFloat("_SrcBlend", (float)UnityEngine.Rendering.BlendMode.SrcAlpha);
        material.SetFloat("_DstBlend", (float)UnityEngine.Rendering.BlendMode.One);
        material.SetFloat("_ZWrite", 0f);
        material.EnableKeyword("_SURFACE_TYPE_TRANSPARENT");
        material.EnableKeyword("_BLENDMODE_ADD");
        return material;
    }
}
