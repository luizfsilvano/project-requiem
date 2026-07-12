using System.Collections;
using UnityEngine;
using UnityEngine.InputSystem;
#if UNITY_EDITOR
using UnityEditor;
#endif

public sealed class DevConsole : MonoBehaviour
{
    private enum ConsoleTab
    {
        Enemy,
        Player,
        Debug,
        Persistence
    }

    private const int WindowId = 19423;
    private const string EnemyTuningPath = "Assets/_Project/Settings/EnemyCombatTuning.asset";
    private const string ProjectEnemyPrefabPath = "Assets/_Project/Prefabs/Enemies/SkeletonEnemy.prefab";
    private const string GolemBossPrefabPath = "Assets/_Project/Prefabs/Enemies/GolemBoss.prefab";
    private const string EnemyPrefabPath = "Assets/SazenGames/Skeleton/Prefabs/Skeleton_110.prefab";
    private const string EnemyAnimatorControllerPath = "Assets/_Project/Animations/SkeletonEnemy.controller";
    private const float EnemyVisualTargetHeight = 2.05f;
    private const float EnemyVisualGroundSink = 0.02f;

    private Rect windowRect = new(20f, 20f, 360f, 520f);
    private Vector2 scrollPosition;
    private ConsoleTab selectedTab;
    private bool visible;
    private Transform player;
    private PlayerHealth playerHealth;
    private PlayerStamina playerStamina;
    private PlayerInventory playerInventory;
    private EnemyPlaceholderAI enemy;
    private TrainingDummyHealth enemyHealth;
    private GolemBossAI golemBoss;
    private TrainingDummyHealth golemBossHealth;
    private SaveGameService saveGameService;
    private bool deleteSaveConfirmationArmed;
    private float deleteSaveConfirmationExpiresAt;
    private string persistenceFeedback = string.Empty;
    [SerializeField] private EnemyCombatTuning enemyTuning;
    private Material enemyMaterial;
    private Material hitboxMaterial;
    private readonly System.Collections.Generic.Dictionary<Material, Material> convertedEnemyMaterials = new();

    [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
    private static void Install()
    {
        if (FindFirstObjectByType<DevConsole>() != null)
        {
            return;
        }

        GameObject consoleObject = new("DevConsole");
        consoleObject.AddComponent<DevConsole>();
    }

    private void Awake()
    {
        EnsurePlayer();
    }

    private void Update()
    {
        if (Keyboard.current != null && Keyboard.current.f2Key.wasPressedThisFrame)
        {
            if (visible)
            {
                CloseConsole();
            }
            else
            {
                OpenConsole();
            }
        }

        EnsurePlayer();
    }

    private void OnDestroy()
    {
        visible = false;
        DevSettings.ConsoleOpen = false;
        if (GameplayInputGate.IsOwnedBy(GameplayModalMode.DevConsole, this))
        {
            GameplayInputGate.TryCloseModal(GameplayModalMode.DevConsole, this);
        }
    }

    private void OnDisable()
    {
        if (Application.isPlaying && visible)
        {
            CloseConsole();
        }
    }

    private void OnGUI()
    {
        if (!visible)
        {
            return;
        }

        windowRect = GUI.Window(WindowId, windowRect, DrawWindow, "Combat Dev Console - F2");
    }

    private void DrawWindow(int windowId)
    {
        selectedTab = (ConsoleTab)GUILayout.Toolbar(
            (int)selectedTab,
            new[] { "Enemy", "Player", "Debug", "Persistence" });
        scrollPosition = GUILayout.BeginScrollView(scrollPosition);

        switch (selectedTab)
        {
            case ConsoleTab.Enemy:
                DrawEnemyTab();
                break;
            case ConsoleTab.Player:
                DrawPlayerTab();
                break;
            case ConsoleTab.Debug:
                DrawDebugTab();
                break;
            case ConsoleTab.Persistence:
                DrawPersistenceTab();
                break;
        }

        GUILayout.EndScrollView();
        GUI.DragWindow(new Rect(0f, 0f, windowRect.width, 22f));
    }

    private void DrawEnemyTab()
    {
        GUILayout.Label("Enemy");
        if (enemy == null)
        {
            if (GUILayout.Button("Spawn skeleton enemy"))
            {
                SpawnEnemy();
            }
        }
        else
        {
            GUILayout.Label($"State: {enemy.CurrentStateName}");
            DrawEnemyTuningSummary();

            GUILayout.BeginHorizontal();
            if (GUILayout.Button(enemy.CombatEnabled ? "Stop enemy" : "Start enemy"))
            {
                enemy.SetCombatEnabled(!enemy.CombatEnabled);
            }

            if (GUILayout.Button("Reset enemy"))
            {
                ResetEnemy();
            }
            GUILayout.EndHorizontal();

            if (GUILayout.Button("Despawn enemy"))
            {
                DespawnEnemy();
            }
        }

        GUILayout.Space(12f);
        GUILayout.Label("Golem Boss MVP");
        if (golemBoss == null)
        {
            if (GUILayout.Button("Spawn golem boss"))
            {
                SpawnGolemBoss();
            }
        }
        else
        {
            GUILayout.Label($"State: {golemBoss.CurrentStateName}");
            if (!string.IsNullOrEmpty(golemBoss.CurrentAttackName))
            {
                GUILayout.Label($"Attack: {golemBoss.CurrentAttackName}");
            }

            if (golemBossHealth != null)
            {
                GUILayout.Label($"HP: {golemBossHealth.CurrentHealth}/{golemBossHealth.MaxHealth}");
            }

            GUILayout.BeginHorizontal();
            if (GUILayout.Button(golemBoss.CombatEnabled ? "Stop boss" : "Start boss"))
            {
                golemBoss.SetCombatEnabled(!golemBoss.CombatEnabled);
            }

            if (GUILayout.Button("Reset boss"))
            {
                ResetGolemBoss();
            }
            GUILayout.EndHorizontal();

            if (GUILayout.Button("Despawn boss"))
            {
                DespawnGolemBoss();
            }
        }

        DevSettings.EnemyAIEnabled = GUILayout.Toggle(DevSettings.EnemyAIEnabled, "Enemy AI enabled");
        DevSettings.EnemyInvincible = GUILayout.Toggle(DevSettings.EnemyInvincible, "Enemy invincible");
        DevSettings.EnemySpeedMultiplier = Slider("Enemy speed", DevSettings.EnemySpeedMultiplier, 0.1f, 3f);
    }

    private void DrawPlayerTab()
    {
        GUILayout.Label("Player");
        DevSettings.PlayerInvincible = GUILayout.Toggle(DevSettings.PlayerInvincible, "Infinite health");
        DevSettings.InfiniteStamina = GUILayout.Toggle(DevSettings.InfiniteStamina, "Infinite stamina");
        DevSettings.PlayerSpeedMultiplier = Slider("Player speed", DevSettings.PlayerSpeedMultiplier, 0.25f, 2.5f);

        if (playerHealth != null)
        {
            GUILayout.Label($"HP: {playerHealth.CurrentHealth}/{playerHealth.MaxHealth}");
            if (GUILayout.Button("Restore player HP"))
            {
                playerHealth.RestoreFullHealth();
            }
        }

        if (playerStamina != null)
        {
            GUILayout.Label($"Stamina: {playerStamina.CurrentStamina:0}/{playerStamina.MaxStamina:0}");
            if (GUILayout.Button("Restore player stamina"))
            {
                playerStamina.RestoreFullStamina();
            }
        }

        if (playerInventory != null)
        {
            GUILayout.Label($"Active {playerInventory.ActiveWeaponSlot}: {playerInventory.EquippedWeaponName}");
            GUILayout.Label($"Primary [1]: {playerInventory.PrimaryWeaponName}");
            GUILayout.Label($"Secondary [2]: {playerInventory.SecondaryWeaponName}");
            GUILayout.Label($"Weapons: {playerInventory.WeaponCount} | Items: {playerInventory.ItemCount}");

            ItemInstance equippedItem = playerInventory.EquippedItem;
            if (equippedItem != null)
            {
                GUILayout.Label($"Durability: {equippedItem.CurrentDurability:0}/{equippedItem.MaxDurability:0}");
                GUILayout.Label("Q toggles primary/secondary");
            }
        }
    }

    private void DrawDebugTab()
    {
        GUILayout.Label("Debug View");
        DevSettings.ShowHitboxes = GUILayout.Toggle(DevSettings.ShowHitboxes, "Show hitboxes");

        GUILayout.Space(10f);
        GUILayout.Label("Notes");
        GUILayout.Label("Enemy is idle until you press Start enemy.");
        GUILayout.Label("Run does not spend stamina; dodge and attacks do.");
    }

    private void DrawPersistenceTab()
    {
        EnsurePersistenceService();
        GUILayout.Label("Local Save MVP");
        if (saveGameService == null)
        {
            GUILayout.Label("SaveGameService is missing from the active scene.");
            return;
        }

        GUILayout.Label($"Schema: v{saveGameService.SchemaVersion}");
        GUILayout.Label($"Main: {(saveGameService.SaveExists ? "present" : "missing")}");
        GUILayout.Label($"Backup: {(saveGameService.BackupExists ? "present" : "missing")}");
        GUILayout.Label($"Updated at: {saveGameService.SaveUpdatedAtUtc}");
        if (saveGameService.TryGetCurrentCounts(
                out int playerItemCount,
                out int containerCount,
                out int persistentObjectCount,
                out string countError))
        {
            GUILayout.Label($"Player items: {playerItemCount}");
            GUILayout.Label($"Containers: {containerCount}");
            GUILayout.Label($"Persistent objects: {persistentObjectCount}");
        }
        else
        {
            GUILayout.Label($"Counts unavailable: {countError}");
        }

        GUILayout.Label($"Last operation: {saveGameService.LastOperation}");
        GUILayout.Label($"Load source: {saveGameService.LastLoadSource}");
        GUILayout.Label(saveGameService.LastStatus);
        if (!string.IsNullOrWhiteSpace(saveGameService.LastError))
        {
            GUILayout.Label($"Error: {saveGameService.LastError}");
        }

        GUILayout.Space(8f);
        GUILayout.BeginHorizontal();
        if (GUILayout.Button("Save"))
        {
            persistenceFeedback = saveGameService.SaveGame()
                ? "Save completed."
                : saveGameService.LastError;
        }

        if (GUILayout.Button("Load"))
        {
            persistenceFeedback = saveGameService.LoadGame()
                ? $"Loaded from {saveGameService.LastLoadSource}."
                : saveGameService.LastError;
        }
        GUILayout.EndHorizontal();

        GUILayout.BeginHorizontal();
        if (GUILayout.Button("Show Folder"))
        {
            saveGameService.ShowSaveFolder();
            persistenceFeedback = saveGameService.SaveDirectory;
        }

        if (GUILayout.Button("Print Summary"))
        {
            persistenceFeedback = saveGameService.PrintSummary()
                ? saveGameService.LastStatus
                : saveGameService.LastError;
        }
        GUILayout.EndHorizontal();

        if (deleteSaveConfirmationArmed && Time.realtimeSinceStartup > deleteSaveConfirmationExpiresAt)
        {
            deleteSaveConfirmationArmed = false;
        }

        string deleteLabel = deleteSaveConfirmationArmed
            ? "Confirm Delete Save"
            : "Delete Save";
        if (GUILayout.Button(deleteLabel))
        {
            if (!deleteSaveConfirmationArmed)
            {
                deleteSaveConfirmationArmed = true;
                deleteSaveConfirmationExpiresAt = Time.realtimeSinceStartup + 5f;
                persistenceFeedback = "Press Confirm Delete Save within 5 seconds.";
            }
            else
            {
                deleteSaveConfirmationArmed = false;
                persistenceFeedback = saveGameService.DeleteSave()
                    ? "Save files deleted."
                    : saveGameService.LastError;
            }
        }

        GUILayout.Space(8f);
        GUILayout.BeginHorizontal();
        if (GUILayout.Button("Validate World IDs"))
        {
            saveGameService.ValidateWorldIds(out persistenceFeedback);
        }

        if (GUILayout.Button("Validate Registry"))
        {
            saveGameService.ValidateRegistry(out persistenceFeedback);
        }
        GUILayout.EndHorizontal();

        if (!string.IsNullOrWhiteSpace(persistenceFeedback))
        {
            GUILayout.Space(6f);
            GUILayout.Label(persistenceFeedback);
        }

        GUILayout.Space(8f);
        GUILayout.Label(saveGameService.SavePath);
    }

    private static float Slider(string label, float value, float min, float max)
    {
        GUILayout.Label($"{label}: {value:0.00}x");
        return GUILayout.HorizontalSlider(value, min, max);
    }

    private void OpenConsole()
    {
        if (!GameplayInputGate.TryOpenModal(GameplayModalMode.DevConsole, this))
        {
            return;
        }

        visible = true;
        DevSettings.ConsoleOpen = true;
    }

    public void CloseConsole()
    {
        visible = false;
        DevSettings.ConsoleOpen = false;
        GameplayInputGate.TryCloseModal(GameplayModalMode.DevConsole, this);
    }

    public bool IsOpen => visible;

    private void EnsurePlayer()
    {
        if (player == null)
        {
            PlayerCameraRelativeMovement movement = FindFirstObjectByType<PlayerCameraRelativeMovement>();
            if (movement != null)
            {
                player = movement.transform;
            }
        }

        if (player == null)
        {
            GameObject playerObject = GameObject.Find("PlayerPlaceholder");
            if (playerObject != null)
            {
                player = playerObject.transform;
            }
        }

        if (player == null)
        {
            return;
        }

        if (playerHealth == null)
        {
            playerHealth = player.GetComponent<PlayerHealth>();
            if (playerHealth == null)
            {
                playerHealth = player.gameObject.AddComponent<PlayerHealth>();
            }
        }

        if (playerStamina == null)
        {
            playerStamina = player.GetComponent<PlayerStamina>();
            if (playerStamina == null)
            {
                playerStamina = player.gameObject.AddComponent<PlayerStamina>();
            }
        }

        if (playerInventory == null)
        {
            playerInventory = player.GetComponent<PlayerInventory>();
            if (playerInventory == null)
            {
                playerInventory = player.gameObject.AddComponent<PlayerInventory>();
            }
        }
    }

    private void EnsurePersistenceService()
    {
        if (saveGameService == null)
        {
            saveGameService = FindFirstObjectByType<SaveGameService>(FindObjectsInactive.Include);
        }
    }

    private void SpawnEnemy()
    {
        EnsurePlayer();
        if (player == null)
        {
            Debug.LogWarning("DevConsole could not spawn enemy because player was not found.");
            return;
        }

        CleanupEnemyStatusBars();

        Vector3 spawnPosition = player.position + player.forward * 5f + player.right * 1.5f;
        spawnPosition.y = 0f;
        Quaternion spawnRotation = Quaternion.LookRotation(Vector3.ProjectOnPlane(player.position - spawnPosition, Vector3.up).normalized, Vector3.up);

        if (TrySpawnEnemyPrefab(spawnPosition, spawnRotation))
        {
            return;
        }

        GameObject root = new("EnemyPlaceholder");
        root.name = "EnemyPlaceholder";
        root.transform.position = spawnPosition;
        root.transform.rotation = spawnRotation;

        CapsuleCollider bodyCollider = root.AddComponent<CapsuleCollider>();
        bodyCollider.center = new Vector3(0f, 1.1f, 0f);
        bodyCollider.height = 2.2f;
        bodyCollider.radius = 0.58f;

        GameObject visual = CreateEnemyVisual(root.transform);
        if (visual == null)
        {
            CreateCapsuleFallbackVisual(root.transform);
        }

        enemyHealth = root.AddComponent<TrainingDummyHealth>();
        enemyHealth.SetFloatingBarOffset(new Vector3(0f, 2.85f, 0f));

        EnemyAttackHitbox hitbox = CreateEnemyAttackHitbox(root.transform);
        enemy = root.AddComponent<EnemyPlaceholderAI>();
        enemy.Initialize(player, hitbox);
        ConfigureEnemyAnimations(enemy);
        EnemyCombatTuning tuning = GetEnemyTuning();
        enemy.ApplyTuning(tuning);
        enemyHealth.ApplyTuning(tuning);
        enemy.SetCombatEnabled(false);

        Debug.Log("Enemy spawned. Use F2 > Start enemy to activate it.");
    }

    private void SpawnGolemBoss()
    {
        EnsurePlayer();
        if (player == null)
        {
            Debug.LogWarning("DevConsole could not spawn the golem boss because the player was not found.");
            return;
        }

#if UNITY_EDITOR
        GameObject prefab = AssetDatabase.LoadAssetAtPath<GameObject>(GolemBossPrefabPath);
        if (prefab == null)
        {
            Debug.LogWarning($"Golem boss prefab was not found at {GolemBossPrefabPath}. Run Tools/Combat Sandbox/Build Golem Boss MVP first.");
            return;
        }

        Vector3 spawnPosition = player.position + player.forward * 8f + player.right * 2.5f;
        spawnPosition.y = 0f;
        Vector3 lookDirection = Vector3.ProjectOnPlane(player.position - spawnPosition, Vector3.up).normalized;
        Quaternion spawnRotation = lookDirection.sqrMagnitude > 0.001f
            ? Quaternion.LookRotation(lookDirection, Vector3.up)
            : Quaternion.identity;

        GameObject root = Instantiate(prefab, spawnPosition, spawnRotation);
        root.name = "GolemBoss";
        golemBoss = root.GetComponent<GolemBossAI>();
        golemBossHealth = root.GetComponent<TrainingDummyHealth>();

        if (golemBoss == null || golemBossHealth == null)
        {
            Debug.LogWarning($"Golem boss prefab at {GolemBossPrefabPath} is missing required gameplay components.");
            Destroy(root);
            golemBoss = null;
            golemBossHealth = null;
            return;
        }

        golemBoss.SetCombatEnabled(false);
        Debug.Log("Golem boss spawned. Use F2 > Start boss to activate it.");
#else
        Debug.LogWarning("Golem boss spawning is currently available in the Unity Editor sandbox only.");
#endif
    }

    private void ResetGolemBoss()
    {
        DespawnGolemBoss();
        SpawnGolemBoss();
    }

    private void DespawnGolemBoss()
    {
        if (golemBoss == null)
        {
            return;
        }

        Destroy(golemBoss.gameObject);
        golemBoss = null;
        golemBossHealth = null;
    }

    private bool TrySpawnEnemyPrefab(Vector3 spawnPosition, Quaternion spawnRotation)
    {
#if UNITY_EDITOR
        GameObject prefab = AssetDatabase.LoadAssetAtPath<GameObject>(ProjectEnemyPrefabPath);
        if (prefab == null)
        {
            return false;
        }

        GameObject root = Instantiate(prefab, spawnPosition, spawnRotation);
        root.name = "SkeletonEnemy";
        enemy = root.GetComponent<EnemyPlaceholderAI>();
        enemyHealth = root.GetComponent<TrainingDummyHealth>();
        EnemyAttackHitbox hitbox = root.GetComponentInChildren<EnemyAttackHitbox>(true);

        if (enemy == null || enemyHealth == null || hitbox == null)
        {
            Debug.LogWarning($"Skeleton enemy prefab at {ProjectEnemyPrefabPath} is missing required gameplay components.");
            Destroy(root);
            enemy = null;
            enemyHealth = null;
            return false;
        }

        EnemyCombatTuning tuning = GetEnemyTuning();
        enemy.Initialize(player, hitbox);
        enemy.ApplyTuning(tuning);
        enemyHealth.ApplyTuning(tuning);
        enemy.SetCombatEnabled(false);

        Debug.Log("SkeletonEnemy prefab spawned. Use F2 > Start enemy to activate it.");
        return true;
#else
        return false;
#endif
    }

    private GameObject CreateEnemyVisual(Transform parent)
    {
#if UNITY_EDITOR
        GameObject prefab = AssetDatabase.LoadAssetAtPath<GameObject>(EnemyPrefabPath);
        if (prefab == null)
        {
            Debug.LogWarning($"Enemy prefab not found at {EnemyPrefabPath}. Falling back to capsule visual.");
            return null;
        }

        GameObject visual = Instantiate(prefab, parent);
        visual.name = "SkeletonVisual";
        visual.transform.localPosition = Vector3.zero;
        visual.transform.localRotation = Quaternion.identity;
        visual.transform.localScale = Vector3.one;

        AssignEnemyAnimatorController(visual);
        AssignUrpCompatibleMaterials(visual);
        ForceHighestDetailLod(visual);
        DisableVisualGameplayPhysics(visual);
        FitVisualToGround(visual, EnemyVisualTargetHeight);
        StartCoroutine(SnapVisualToGroundAfterAnimatorUpdate(visual));
        return visual;
#else
        return null;
#endif
    }

#if UNITY_EDITOR
    private static void AssignEnemyAnimatorController(GameObject visual)
    {
        RuntimeAnimatorController controller = AssetDatabase.LoadAssetAtPath<RuntimeAnimatorController>(EnemyAnimatorControllerPath);
        if (controller == null)
        {
            return;
        }

        foreach (Animator animator in visual.GetComponentsInChildren<Animator>(true))
        {
            animator.runtimeAnimatorController = controller;
            animator.applyRootMotion = false;
        }
    }

    private static void ConfigureEnemyAnimations(EnemyPlaceholderAI ai)
    {
        ai.InitializeAnimationControllers(null, null, null, null, null, "anim");
    }
#else
    private static void ConfigureEnemyAnimations(EnemyPlaceholderAI ai)
    {
    }
#endif

    private EnemyCombatTuning GetEnemyTuning()
    {
        if (enemyTuning != null)
        {
            return enemyTuning;
        }

#if UNITY_EDITOR
        enemyTuning = AssetDatabase.LoadAssetAtPath<EnemyCombatTuning>(EnemyTuningPath);
        if (enemyTuning != null)
        {
            return enemyTuning;
        }

        enemyTuning = ScriptableObject.CreateInstance<EnemyCombatTuning>();
        AssetDatabase.CreateAsset(enemyTuning, EnemyTuningPath);
        AssetDatabase.SaveAssets();
        AssetDatabase.Refresh();
        Debug.Log($"Created enemy tuning asset at {EnemyTuningPath}.");
        return enemyTuning;
#else
        enemyTuning = ScriptableObject.CreateInstance<EnemyCombatTuning>();
        return enemyTuning;
#endif
    }

    private void AssignUrpCompatibleMaterials(GameObject visual)
    {
        foreach (Renderer renderer in visual.GetComponentsInChildren<Renderer>(true))
        {
            Material[] materials = renderer.sharedMaterials;
            bool changed = false;

            for (int i = 0; i < materials.Length; i++)
            {
                Material converted = GetUrpCompatibleMaterial(materials[i]);
                if (converted == null)
                {
                    continue;
                }

                materials[i] = converted;
                changed = true;
            }

            if (changed)
            {
                renderer.sharedMaterials = materials;
            }
        }
    }

    private Material GetUrpCompatibleMaterial(Material source)
    {
        if (source == null)
        {
            return null;
        }

        if (convertedEnemyMaterials.TryGetValue(source, out Material converted))
        {
            return converted;
        }

        Shader shader = Shader.Find("Universal Render Pipeline/Simple Lit");
        shader = shader != null ? shader : Shader.Find("Universal Render Pipeline/Lit");
        if (shader == null)
        {
            return null;
        }

        converted = new Material(shader)
        {
            name = $"{source.name}_URP_Runtime"
        };

        CopyTexture(source, converted, "_MainTex", "_BaseMap");
        CopyTexture(source, converted, "_BumpMap", "_BumpMap");
        CopyColor(source, converted, "_Color", "_BaseColor", Color.white);
        CopyFloat(source, converted, "_BumpScale", "_BumpScale");

        converted.SetFloat("_Surface", 0f);
        converted.SetFloat("_Cull", 2f);
        SetMaterialFloat(converted, "_Metallic", 0f);
        SetMaterialFloat(converted, "_Smoothness", 0.18f);
        SetMaterialFloat(converted, "_SpecularHighlights", 0f);
        SetMaterialFloat(converted, "_EnvironmentReflections", 0f);
        SetMaterialColor(converted, "_SpecColor", new Color(0.06f, 0.06f, 0.06f, 1f));

        if (converted.HasProperty("_BumpMap") && converted.GetTexture("_BumpMap") != null)
        {
            converted.EnableKeyword("_NORMALMAP");
        }

        convertedEnemyMaterials.Add(source, converted);
        return converted;
    }

    private static void CopyTexture(Material source, Material target, string sourceProperty, string targetProperty)
    {
        if (!source.HasProperty(sourceProperty) || !target.HasProperty(targetProperty))
        {
            return;
        }

        Texture texture = source.GetTexture(sourceProperty);
        if (texture == null)
        {
            return;
        }

        target.SetTexture(targetProperty, texture);
        target.SetTextureScale(targetProperty, source.GetTextureScale(sourceProperty));
        target.SetTextureOffset(targetProperty, source.GetTextureOffset(sourceProperty));
    }

    private static void CopyColor(Material source, Material target, string sourceProperty, string targetProperty, Color fallback)
    {
        if (!target.HasProperty(targetProperty))
        {
            return;
        }

        Color color = source.HasProperty(sourceProperty) ? source.GetColor(sourceProperty) : fallback;
        target.SetColor(targetProperty, color);
    }

    private static void CopyFloat(Material source, Material target, string sourceProperty, string targetProperty)
    {
        if (!source.HasProperty(sourceProperty) || !target.HasProperty(targetProperty))
        {
            return;
        }

        target.SetFloat(targetProperty, source.GetFloat(sourceProperty));
    }

    private static void SetMaterialFloat(Material material, string property, float value)
    {
        if (material.HasProperty(property))
        {
            material.SetFloat(property, value);
        }
    }

    private static void SetMaterialColor(Material material, string property, Color value)
    {
        if (material.HasProperty(property))
        {
            material.SetColor(property, value);
        }
    }

    private static void ForceHighestDetailLod(GameObject visual)
    {
        foreach (LODGroup lodGroup in visual.GetComponentsInChildren<LODGroup>(true))
        {
            lodGroup.ForceLOD(0);
        }
    }

    private void CreateCapsuleFallbackVisual(Transform parent)
    {
        GameObject visual = GameObject.CreatePrimitive(PrimitiveType.Capsule);
        visual.name = "CapsuleFallbackVisual";
        visual.transform.SetParent(parent);
        visual.transform.localPosition = new Vector3(0f, 0.93f, 0f);
        visual.transform.localRotation = Quaternion.identity;
        visual.transform.localScale = new Vector3(0.96f, 0.93f, 0.96f);

        Renderer renderer = visual.GetComponent<Renderer>();
        renderer.sharedMaterial = GetEnemyMaterial();

        Collider collider = visual.GetComponent<Collider>();
        if (collider != null)
        {
            collider.enabled = false;
        }
    }

    private static void DisableVisualGameplayPhysics(GameObject visual)
    {
        foreach (Collider collider in visual.GetComponentsInChildren<Collider>(true))
        {
            collider.enabled = false;
        }

        foreach (Rigidbody body in visual.GetComponentsInChildren<Rigidbody>(true))
        {
            body.isKinematic = true;
            body.useGravity = false;
            body.detectCollisions = false;
        }
    }

    private static void FitVisualToGround(GameObject visual, float targetHeight)
    {
        if (!TryGetVisualBounds(visual, out Bounds bounds) || bounds.size.y <= 0.001f)
        {
            return;
        }

        float scale = Mathf.Clamp(targetHeight / bounds.size.y, 0.35f, 3f);
        visual.transform.localScale *= scale;

        if (!TryGetVisualBounds(visual, out bounds))
        {
            return;
        }

        SnapVisualToGround(visual);
    }

    private static void SnapVisualToGround(GameObject visual)
    {
        if (!TryGetVisualBounds(visual, out Bounds bounds))
        {
            return;
        }

        float groundY = visual.transform.parent != null ? visual.transform.parent.position.y : visual.transform.position.y;
        visual.transform.position += Vector3.up * (groundY - bounds.min.y - EnemyVisualGroundSink);
    }

    private IEnumerator SnapVisualToGroundAfterAnimatorUpdate(GameObject visual)
    {
        yield return null;
        yield return null;

        if (visual != null)
        {
            SnapVisualToGround(visual);
        }
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

    private EnemyAttackHitbox CreateEnemyAttackHitbox(Transform parent)
    {
        GameObject hitboxObject = GameObject.CreatePrimitive(PrimitiveType.Cube);
        hitboxObject.name = "EnemyAttackHitbox";
        hitboxObject.transform.SetParent(parent);
        hitboxObject.transform.localPosition = new Vector3(0f, 1.1f, 1.15f);
        hitboxObject.transform.localRotation = Quaternion.identity;
        hitboxObject.transform.localScale = new Vector3(1.45f, 1.25f, 1f);

        Renderer renderer = hitboxObject.GetComponent<Renderer>();
        renderer.sharedMaterial = GetHitboxMaterial();

        BoxCollider collider = hitboxObject.GetComponent<BoxCollider>();
        collider.isTrigger = true;
        collider.enabled = false;

        Rigidbody body = hitboxObject.AddComponent<Rigidbody>();
        body.isKinematic = true;
        body.useGravity = false;

        return hitboxObject.AddComponent<EnemyAttackHitbox>();
    }

    private void ResetEnemy()
    {
        DespawnEnemy();
        SpawnEnemy();
    }

    private void DespawnEnemy()
    {
        if (enemy == null)
        {
            return;
        }

        Destroy(enemy.gameObject);
        enemy = null;
        enemyHealth = null;
        CleanupEnemyStatusBars();
    }

    private void DrawEnemyTuningSummary()
    {
        EnemyCombatTuning tuning = GetEnemyTuning();

        GUILayout.Space(6f);
        GUILayout.Label("Enemy tuning asset");
        GUILayout.Label(tuning != null ? tuning.name : "Missing EnemyCombatTuning asset");

        if (tuning == null)
        {
            return;
        }

        GUILayout.Label($"Windup/contact: {tuning.attackWindup:0.00}s");
        GUILayout.Label($"Hitbox active: {tuning.attackActiveTime:0.00}s");
        GUILayout.Label($"Recovery: {tuning.attackRecovery:0.00}s");
        GUILayout.Label($"Cooldown: {tuning.attackCooldown:0.00}s");
        GUILayout.Label($"Stagger lock: {tuning.staggerLockDuration:0.00}s");
        GUILayout.Label($"Red telegraph: {(tuning.showAttackTelegraph ? "on" : "off")}");
    }

    private static void CleanupEnemyStatusBars()
    {
        foreach (Transform candidate in FindObjectsByType<Transform>(FindObjectsSortMode.None))
        {
            if (candidate != null && candidate.name == "EnemyPlaceholder_StatusBars")
            {
                Destroy(candidate.gameObject);
            }
        }
    }

    private Material GetEnemyMaterial()
    {
        if (enemyMaterial == null)
        {
            enemyMaterial = CreateRuntimeMaterial("EnemyPlaceholder_Mat", new Color(0.72f, 0.18f, 0.16f));
        }

        return enemyMaterial;
    }

    private Material GetHitboxMaterial()
    {
        if (hitboxMaterial == null)
        {
            hitboxMaterial = CreateRuntimeMaterial("EnemyHitbox_Mat", new Color(1f, 0.1f, 0.05f, 0.28f));
        }

        return hitboxMaterial;
    }

    private static Material CreateRuntimeMaterial(string materialName, Color color)
    {
        Shader shader = Shader.Find("Universal Render Pipeline/Lit");
        shader = shader != null ? shader : Shader.Find("Standard");

        Material material = new(shader)
        {
            name = materialName,
            color = color
        };

        if (color.a < 1f)
        {
            material.SetFloat("_Surface", 1f);
            material.SetOverrideTag("RenderType", "Transparent");
            material.EnableKeyword("_SURFACE_TYPE_TRANSPARENT");
            material.renderQueue = (int)UnityEngine.Rendering.RenderQueue.Transparent;
        }

        return material;
    }
}
