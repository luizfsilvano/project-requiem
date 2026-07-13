#if UNITY_EDITOR
using UnityEditor;
using UnityEngine;

public static class SkeletonEnemyPrefabBootstrap
{
    private const string EnemyPrefabPath = "Assets/_Project/Prefabs/Enemies/SkeletonEnemy.prefab";
    private const string EnemyPrefabFolder = "Assets/_Project/Prefabs/Enemies";
    private const string SkeletonVisualPath = "Assets/SazenGames/Skeleton/Prefabs/Skeleton_110.prefab";
    private const string SkeletonAnimatorPath = "Assets/_Project/Animations/SkeletonEnemy.controller";
    private const string EnemyTuningPath = "Assets/_Project/Settings/EnemyCombatTuning.asset";
    private const float VisualTargetHeight = 2.05f;
    private const float VisualGroundSink = 0.02f;

    [InitializeOnLoadMethod]
    private static void EnsurePrefabExists()
    {
        EditorApplication.delayCall += RebuildIfMissing;
    }

    [MenuItem("Tools/Combat Sandbox/Rebuild Skeleton Enemy Prefab")]
    public static void RebuildPrefab()
    {
        Rebuild(force: true);
    }

    private static void RebuildIfMissing()
    {
        Rebuild(force: false);
    }

    private static void Rebuild(bool force)
    {
        if (!force && AssetDatabase.LoadAssetAtPath<GameObject>(EnemyPrefabPath) != null)
        {
            return;
        }

        EnsureFolder(EnemyPrefabFolder);

        GameObject root = new("SkeletonEnemy");
        root.transform.position = Vector3.zero;
        root.transform.rotation = Quaternion.identity;

        CapsuleCollider bodyCollider = root.AddComponent<CapsuleCollider>();
        bodyCollider.center = new Vector3(0f, 1.1f, 0f);
        bodyCollider.height = 2.2f;
        bodyCollider.radius = 0.58f;

        Rigidbody body = root.AddComponent<Rigidbody>();
        body.isKinematic = true;
        body.useGravity = false;

        GameObject visual = CreateVisual(root.transform);
        GameObject hitboxObject = CreateAttackHitbox(root.transform);

        TrainingDummyHealth health = root.AddComponent<TrainingDummyHealth>();
        EnemyAttackHitbox hitbox = hitboxObject.GetComponent<EnemyAttackHitbox>();
        EnemyPlaceholderAI ai = root.AddComponent<EnemyPlaceholderAI>();
        EnemyCombatTuning tuning = AssetDatabase.LoadAssetAtPath<EnemyCombatTuning>(EnemyTuningPath);

        SerializedObject healthObject = new(health);
        healthObject.FindProperty("tuning").objectReferenceValue = tuning;
        healthObject.FindProperty("barWorldOffset").vector3Value = new Vector3(0f, 2.85f, 0f);
        healthObject.ApplyModifiedPropertiesWithoutUndo();

        SerializedObject aiObject = new(ai);
        aiObject.FindProperty("tuning").objectReferenceValue = tuning;
        aiObject.FindProperty("health").objectReferenceValue = health;
        aiObject.FindProperty("attackHitbox").objectReferenceValue = hitbox;
        aiObject.FindProperty("animators").arraySize = 0;
        aiObject.ApplyModifiedPropertiesWithoutUndo();

        PrefabUtility.SaveAsPrefabAsset(root, EnemyPrefabPath);
        Object.DestroyImmediate(root);
        QuestSystemBootstrap.RestoreQuestAugmentationsIfAvailable();
        AssetDatabase.SaveAssets();
        AssetDatabase.Refresh();
    }

    private static GameObject CreateVisual(Transform parent)
    {
        GameObject skeletonPrefab = AssetDatabase.LoadAssetAtPath<GameObject>(SkeletonVisualPath);
        if (skeletonPrefab == null)
        {
            GameObject fallback = GameObject.CreatePrimitive(PrimitiveType.Capsule);
            fallback.name = "SkeletonVisualFallback";
            fallback.transform.SetParent(parent, false);
            fallback.transform.localPosition = new Vector3(0f, 0.93f, 0f);
            fallback.transform.localScale = new Vector3(0.96f, 0.93f, 0.96f);
            Object.DestroyImmediate(fallback.GetComponent<Collider>());
            return fallback;
        }

        GameObject visual = (GameObject)PrefabUtility.InstantiatePrefab(skeletonPrefab);
        visual.name = "SkeletonVisual";
        visual.transform.SetParent(parent, false);
        visual.transform.localPosition = Vector3.zero;
        visual.transform.localRotation = Quaternion.identity;
        visual.transform.localScale = Vector3.one;

        RuntimeAnimatorController controller = AssetDatabase.LoadAssetAtPath<RuntimeAnimatorController>(SkeletonAnimatorPath);
        foreach (Animator animator in visual.GetComponentsInChildren<Animator>(true))
        {
            animator.runtimeAnimatorController = controller;
            animator.applyRootMotion = false;
        }

        foreach (Collider collider in visual.GetComponentsInChildren<Collider>(true))
        {
            Object.DestroyImmediate(collider);
        }

        foreach (Rigidbody rigidbody in visual.GetComponentsInChildren<Rigidbody>(true))
        {
            Object.DestroyImmediate(rigidbody);
        }

        foreach (LODGroup lodGroup in visual.GetComponentsInChildren<LODGroup>(true))
        {
            lodGroup.ForceLOD(0);
        }

        FitVisualToGround(visual, VisualTargetHeight);
        return visual;
    }

    private static GameObject CreateAttackHitbox(Transform parent)
    {
        GameObject hitboxObject = GameObject.CreatePrimitive(PrimitiveType.Cube);
        hitboxObject.name = "EnemyAttackHitbox";
        hitboxObject.transform.SetParent(parent, false);
        hitboxObject.transform.localPosition = new Vector3(0f, 1.1f, 1.15f);
        hitboxObject.transform.localRotation = Quaternion.identity;
        hitboxObject.transform.localScale = new Vector3(1.45f, 1.25f, 1f);

        BoxCollider collider = hitboxObject.GetComponent<BoxCollider>();
        collider.isTrigger = true;
        collider.enabled = false;

        Rigidbody body = hitboxObject.AddComponent<Rigidbody>();
        body.isKinematic = true;
        body.useGravity = false;

        hitboxObject.AddComponent<EnemyAttackHitbox>();
        return hitboxObject;
    }

    private static void FitVisualToGround(GameObject visual, float targetHeight)
    {
        if (!TryGetVisualBounds(visual, out Bounds bounds) || bounds.size.y <= 0.001f)
        {
            return;
        }

        float scale = Mathf.Clamp(targetHeight / bounds.size.y, 0.35f, 3f);
        visual.transform.localScale *= scale;
        SnapVisualToGround(visual);
    }

    private static void SnapVisualToGround(GameObject visual)
    {
        if (!TryGetVisualBounds(visual, out Bounds bounds))
        {
            return;
        }

        float groundY = visual.transform.parent != null ? visual.transform.parent.position.y : visual.transform.position.y;
        visual.transform.position += Vector3.up * (groundY - bounds.min.y - VisualGroundSink);
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
                continue;
            }

            bounds.Encapsulate(renderer.bounds);
        }

        return hasBounds;
    }

    private static void EnsureFolder(string folderPath)
    {
        if (AssetDatabase.IsValidFolder(folderPath))
        {
            return;
        }

        string parent = System.IO.Path.GetDirectoryName(folderPath)?.Replace("\\", "/");
        string folderName = System.IO.Path.GetFileName(folderPath);
        if (!string.IsNullOrEmpty(parent))
        {
            EnsureFolder(parent);
            AssetDatabase.CreateFolder(parent, folderName);
        }
    }
}
#endif
