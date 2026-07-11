#if UNITY_EDITOR
using System;
using System.IO;
using System.Linq;
using UnityEditor;
using UnityEditor.Animations;
using UnityEngine;

public static class GolemBossBootstrap
{
    private const string ImportedAssetPath = "Assets/Kevin Iglesias/Characters/Humanoid Giant";
    private const string GolemArtParentPath = "Assets/_Project/Art/Enemies/Golem";
    private const string OrganizedAssetPath = GolemArtParentPath + "/KevinIglesiasHumanoidGiant";
    private const string ControllerPath = "Assets/_Project/Animations/GolemBoss.controller";
    private const string PrefabPath = "Assets/_Project/Prefabs/Enemies/GolemBoss.prefab";
    private const string HitboxMaterialPath = "Assets/_Project/Materials/HitboxPreview_Mat.mat";
    private const string GroundSmokeTexturePath = "Assets/_Project/Art/VFX/Brackeys/Smoke_04_A.png";
    private const string GroundDirtTexturePath = "Assets/_Project/Art/VFX/Brackeys/Dirt_01_A.png";

    private const string ImportedIdlePath = OrganizedAssetPath + "/Animations/Giant@Idle01.fbx";
    private const string ImportedWalkPath = OrganizedAssetPath + "/Animations/Movement/Walk/Giant@Walk01 - Forward.fbx";
    private const string ImportedAttackPath = OrganizedAssetPath + "/Animations/Combat/Giant@UnarmedAttack01.fbx";
    private const string ImportedDamagePath = OrganizedAssetPath + "/Animations/Combat/Giant@Damage01.fbx";
    private const string GolemVisualPath = OrganizedAssetPath + "/Prefabs/URP/URP - Giant01 - Golem.prefab";
    private const string Ual1Path = "Assets/_Project/Art/AnimationLibraries/UAL1_Standard.fbx";
    private const string Ual2Path = "Assets/_Project/Art/AnimationLibraries/UAL2_Standard.fbx";

    private const float VisualTargetHeight = 4.35f;
    private const float VisualGroundOffset = -0.05f;

    [MenuItem("Tools/Combat Sandbox/Build Golem Boss MVP")]
    public static void Build()
    {
        if (EditorApplication.isPlayingOrWillChangePlaymode)
        {
            Debug.LogWarning("Stop Play Mode before rebuilding the Golem Boss MVP.");
            return;
        }

        OrganizeImportedAssets();
        AnimatorController controller = BuildAnimatorController();
        GameObject prefab = BuildPrefab(controller);

        if (prefab == null)
        {
            Debug.LogError("Golem Boss MVP build failed. Check the Console for the missing dependency.");
            return;
        }

        AssetDatabase.SaveAssets();
        AssetDatabase.Refresh();
        Selection.activeObject = prefab;
        EditorGUIUtility.PingObject(prefab);
        Debug.Log($"Golem Boss MVP built at {PrefabPath}. Spawn it from F2 > Enemy > Spawn golem boss.");
    }

    private static void OrganizeImportedAssets()
    {
        EnsureFolder(GolemArtParentPath);

        if (!AssetDatabase.IsValidFolder(OrganizedAssetPath))
        {
            if (!AssetDatabase.IsValidFolder(ImportedAssetPath))
            {
                throw new InvalidOperationException($"Could not find the imported golem assets at {ImportedAssetPath}.");
            }

            string error = AssetDatabase.MoveAsset(ImportedAssetPath, OrganizedAssetPath);
            if (!string.IsNullOrEmpty(error))
            {
                throw new InvalidOperationException($"Could not organize the golem assets: {error}");
            }
        }

        DeleteFolderIfEmpty("Assets/Kevin Iglesias/Characters");
        DeleteFolderIfEmpty("Assets/Kevin Iglesias");
    }

    private static AnimatorController BuildAnimatorController()
    {
        EnsureFolder("Assets/_Project/Animations");

        AnimatorController existing = AssetDatabase.LoadAssetAtPath<AnimatorController>(ControllerPath);
        AnimatorController controller = existing;
        if (controller == null)
        {
            controller = AnimatorController.CreateAnimatorControllerAtPath(ControllerPath);
        }

        AnimatorStateMachine stateMachine = controller.layers[0].stateMachine;
        foreach (ChildAnimatorState childState in stateMachine.states)
        {
            stateMachine.RemoveState(childState.state);
        }

        AddState(stateMachine, "Idle", LoadClip(ImportedIdlePath, "Giant@Idle01"), 0.78f, new Vector3(220f, 100f), true);
        AddState(stateMachine, "Walk", LoadClip(ImportedWalkPath, "Giant@Walk01 - Forward"), 0.72f, new Vector3(220f, 190f));
        AddState(stateMachine, "Slam", LoadClip(ImportedAttackPath, "Giant@UnarmedAttack01"), 0.75f, new Vector3(500f, 80f));
        AddState(stateMachine, "Sweep", LoadClip(Ual2Path, "Armature|Melee_Hook"), 0.38f, new Vector3(500f, 170f));
        AddState(stateMachine, "Stagger", LoadClip(ImportedDamagePath, "Giant@Damage01"), 0.8f, new Vector3(500f, 260f));
        AddState(stateMachine, "Death", LoadClip(Ual1Path, "Armature|Death01"), 1f, new Vector3(780f, 170f));

        EditorUtility.SetDirty(controller);
        AssetDatabase.SaveAssets();
        return controller;
    }

    private static void AddState(
        AnimatorStateMachine stateMachine,
        string stateName,
        AnimationClip clip,
        float speed,
        Vector3 position,
        bool isDefault = false)
    {
        if (clip == null)
        {
            throw new InvalidOperationException($"Cannot build animator state {stateName} because its clip is missing.");
        }

        AnimatorState state = stateMachine.AddState(stateName, position);
        state.motion = clip;
        state.speed = speed;
        state.writeDefaultValues = true;

        if (isDefault)
        {
            stateMachine.defaultState = state;
        }
    }

    private static AnimationClip LoadClip(string assetPath, string clipName)
    {
        return AssetDatabase.LoadAllAssetsAtPath(assetPath)
            .OfType<AnimationClip>()
            .FirstOrDefault(clip => clip.name == clipName);
    }

    private static GameObject BuildPrefab(RuntimeAnimatorController controller)
    {
        EnsureFolder("Assets/_Project/Prefabs/Enemies");

        GameObject visualPrefab = AssetDatabase.LoadAssetAtPath<GameObject>(GolemVisualPath);
        if (visualPrefab == null)
        {
            Debug.LogError($"Golem visual prefab not found at {GolemVisualPath}.");
            return null;
        }

        GameObject root = new("GolemBoss");
        try
        {
            root.transform.SetPositionAndRotation(Vector3.zero, Quaternion.identity);

            CapsuleCollider bodyCollider = root.AddComponent<CapsuleCollider>();
            bodyCollider.center = new Vector3(0f, 2.05f, 0f);
            bodyCollider.height = 4.1f;
            bodyCollider.radius = 1.15f;

            Rigidbody body = root.AddComponent<Rigidbody>();
            body.isKinematic = true;
            body.useGravity = false;

            GameObject visual = (GameObject)PrefabUtility.InstantiatePrefab(visualPrefab);
            visual.name = "GolemVisual";
            visual.transform.SetParent(root.transform, false);
            visual.transform.SetLocalPositionAndRotation(Vector3.zero, Quaternion.identity);
            visual.transform.localScale = Vector3.one;
            RemoveVisualPhysics(visual);
            FitVisualScale(visual, VisualTargetHeight);
            visual.transform.localPosition = new Vector3(0f, VisualGroundOffset, 0f);

            Animator animator = visual.GetComponentInChildren<Animator>(true);
            if (animator == null)
            {
                Debug.LogError("The imported golem visual has no Animator.");
                return null;
            }

            animator.runtimeAnimatorController = controller;
            animator.applyRootMotion = false;
            animator.cullingMode = AnimatorCullingMode.AlwaysAnimate;

            EnemyAttackHitbox slamHitbox = CreateAttackHitbox(
                root.transform,
                "SlamHitbox",
                new Vector3(0f, 0.45f, 2.65f),
                new Vector3(3.8f, 0.9f, 4.2f),
                36,
                0.78f,
                0.2f);

            EnemyAttackHitbox sweepHitbox = CreateAttackHitbox(
                root.transform,
                "SweepHitbox",
                new Vector3(0f, 1.75f, 1.85f),
                new Vector3(5.4f, 2.25f, 3.2f),
                27,
                0.62f,
                0.18f);

            TrainingDummyHealth health = root.AddComponent<TrainingDummyHealth>();
            ConfigureHealth(health);

            GolemGroundSlamVfx groundSlamVfx = root.AddComponent<GolemGroundSlamVfx>();
            groundSlamVfx.Configure(
                slamHitbox.GetComponent<BoxCollider>(),
                AssetDatabase.LoadAssetAtPath<Texture2D>(GroundSmokeTexturePath),
                AssetDatabase.LoadAssetAtPath<Texture2D>(GroundDirtTexturePath));

            GolemBossAI ai = root.AddComponent<GolemBossAI>();
            ai.Configure(health, animator, slamHitbox, sweepHitbox, groundSlamVfx);

            BossHealthHud hud = root.AddComponent<BossHealthHud>();
            hud.Configure(health, "STONE GOLEM");

            GameObject savedPrefab = PrefabUtility.SaveAsPrefabAsset(root, PrefabPath);
            return savedPrefab;
        }
        finally
        {
            UnityEngine.Object.DestroyImmediate(root);
        }
    }

    private static void ConfigureHealth(TrainingDummyHealth health)
    {
        SerializedObject serializedHealth = new(health);
        serializedHealth.FindProperty("maxHealth").intValue = 650;
        serializedHealth.FindProperty("maxPoise").floatValue = 280f;
        serializedHealth.FindProperty("poiseRecoveryDelay").floatValue = 2f;
        serializedHealth.FindProperty("poiseRecoveryPerSecond").floatValue = 32f;
        serializedHealth.FindProperty("impactProfile").enumValueIndex = 2;
        serializedHealth.FindProperty("allowStagger").boolValue = true;
        serializedHealth.FindProperty("allowHitStop").boolValue = false;
        serializedHealth.FindProperty("allowCameraShake").boolValue = true;
        serializedHealth.FindProperty("allowScalePunch").boolValue = false;
        serializedHealth.FindProperty("bossPoiseDamageMultiplier").floatValue = 0.45f;
        serializedHealth.FindProperty("bossKnockbackMultiplier").floatValue = 0.03f;
        serializedHealth.FindProperty("staggerKnockbackMultiplier").floatValue = 1f;
        serializedHealth.FindProperty("showFloatingBars").boolValue = false;
        serializedHealth.FindProperty("staggerLockDuration").floatValue = 1.35f;
        serializedHealth.FindProperty("deathDespawnDelay").floatValue = 30f;
        serializedHealth.FindProperty("deathFadeDuration").floatValue = 1.4f;
        serializedHealth.FindProperty("lockOnHeightOverride").floatValue = 2.45f;
        serializedHealth.FindProperty("lockOnIndicatorRadiusOverride").floatValue = 1.3f;
        serializedHealth.FindProperty("lockOnIndicatorHeightOverride").floatValue = 0.1f;
        serializedHealth.ApplyModifiedPropertiesWithoutUndo();
    }

    private static EnemyAttackHitbox CreateAttackHitbox(
        Transform parent,
        string objectName,
        Vector3 localPosition,
        Vector3 localScale,
        int damage,
        float knockbackDistance,
        float knockbackDuration)
    {
        GameObject hitboxObject = GameObject.CreatePrimitive(PrimitiveType.Cube);
        hitboxObject.name = objectName;
        hitboxObject.transform.SetParent(parent, false);
        hitboxObject.transform.localPosition = localPosition;
        hitboxObject.transform.localRotation = Quaternion.identity;
        hitboxObject.transform.localScale = localScale;

        Renderer renderer = hitboxObject.GetComponent<Renderer>();
        renderer.enabled = false;
        Material hitboxMaterial = AssetDatabase.LoadAssetAtPath<Material>(HitboxMaterialPath);
        if (hitboxMaterial != null)
        {
            renderer.sharedMaterial = hitboxMaterial;
        }

        BoxCollider collider = hitboxObject.GetComponent<BoxCollider>();
        collider.isTrigger = true;
        collider.enabled = false;

        Rigidbody body = hitboxObject.AddComponent<Rigidbody>();
        body.isKinematic = true;
        body.useGravity = false;

        EnemyAttackHitbox hitbox = hitboxObject.AddComponent<EnemyAttackHitbox>();
        SerializedObject serializedHitbox = new(hitbox);
        serializedHitbox.FindProperty("triggerCollider").objectReferenceValue = collider;
        serializedHitbox.FindProperty("hitboxRenderer").objectReferenceValue = renderer;
        serializedHitbox.FindProperty("damage").intValue = damage;
        serializedHitbox.FindProperty("knockbackDistance").floatValue = knockbackDistance;
        serializedHitbox.FindProperty("knockbackDuration").floatValue = knockbackDuration;
        serializedHitbox.ApplyModifiedPropertiesWithoutUndo();
        return hitbox;
    }

    private static void RemoveVisualPhysics(GameObject visual)
    {
        foreach (Collider collider in visual.GetComponentsInChildren<Collider>(true))
        {
            UnityEngine.Object.DestroyImmediate(collider);
        }

        foreach (Rigidbody rigidbody in visual.GetComponentsInChildren<Rigidbody>(true))
        {
            UnityEngine.Object.DestroyImmediate(rigidbody);
        }
    }

    private static void FitVisualScale(GameObject visual, float targetHeight)
    {
        if (!TryGetVisualBounds(visual, out Bounds bounds) || bounds.size.y <= 0.001f)
        {
            return;
        }

        float scale = Mathf.Clamp(targetHeight / bounds.size.y, 0.35f, 3f);
        visual.transform.localScale *= scale;
    }

    private static bool TryGetVisualBounds(GameObject visual, out Bounds bounds)
    {
        Renderer[] renderers = visual.GetComponentsInChildren<Renderer>(true);
        bounds = new Bounds(visual.transform.position, Vector3.zero);
        bool hasBounds = false;

        foreach (Renderer renderer in renderers)
        {
            if (!hasBounds)
            {
                bounds = renderer.bounds;
                hasBounds = true;
            }
            else
            {
                bounds.Encapsulate(renderer.bounds);
            }
        }

        return hasBounds;
    }

    private static void EnsureFolder(string folderPath)
    {
        if (AssetDatabase.IsValidFolder(folderPath))
        {
            return;
        }

        string parent = Path.GetDirectoryName(folderPath)?.Replace("\\", "/");
        string folderName = Path.GetFileName(folderPath);
        if (!string.IsNullOrEmpty(parent))
        {
            EnsureFolder(parent);
            AssetDatabase.CreateFolder(parent, folderName);
        }
    }

    private static void DeleteFolderIfEmpty(string folderPath)
    {
        string absolutePath = Path.GetFullPath(folderPath);
        if (Directory.Exists(absolutePath) && !Directory.EnumerateFileSystemEntries(absolutePath).Any())
        {
            AssetDatabase.DeleteAsset(folderPath);
        }
    }
}
#endif
