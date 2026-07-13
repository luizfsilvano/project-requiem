#if UNITY_EDITOR
using System;
using System.Collections.Generic;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;
using UnityEngine.EventSystems;
using UnityEngine.InputSystem.UI;
using UnityEngine.SceneManagement;
using UnityEngine.UI;

public static class QuestSystemBootstrap
{
    private const string BuildMenuPath = "Tools/Combat Sandbox/Build Quest System MVP";
    private const string ValidateMenuPath = "Tools/Combat Sandbox/Validate Quest System MVP";
    private const string ScenePath = "Assets/_Project/Scenes/CombatSandbox.unity";
    private const string PlayerPrefabPath = "Assets/_Project/Prefabs/PlayerPlaceholder.prefab";
    private const string NpcPrefabPath = "Assets/_Project/Prefabs/World/NpcPlaceholder.prefab";
    private const string SkeletonPrefabPath = "Assets/_Project/Prefabs/Enemies/SkeletonEnemy.prefab";
    private const string DialoguePrefabPath = "Assets/_Project/Prefabs/UI/DialoguePanel.prefab";
    private const string JournalPrefabPath = "Assets/_Project/Prefabs/UI/QuestJournalScreen.prefab";
    private const string TrackerPrefabPath = "Assets/_Project/Prefabs/UI/QuestTrackerHud.prefab";
    private const string QuestRegistryPath = "Assets/_Project/Data/Quests/QuestDefinitionRegistry.asset";
    private const string QuestDefinitionPath = "Assets/_Project/Data/Quests/SkeletonTrouble.asset";
    private const string RewardItemPath = "Assets/_Project/Data/Items/BoneFragment.asset";
    private const string ItemRegistryPath = "Assets/_Project/Data/Items/ItemDefinitionRegistry.asset";

    private const string QuestId = "quest.problemas_com_esqueletos";
    private const string DefeatObjectiveId = "defeat_skeletons";
    private const string NpcId = "npc.combat_sandbox.aldeao";
    private const string NpcSceneStableId = "combat_sandbox.npc.quest_giver";
    private const string SkeletonTargetId = "enemy.skeleton";
    private const string SkeletonEncounterRootName = "SkeletonQuestEncounter";
    private const int PlayerInventoryCapacity = 24;

    private static readonly string[] QuestOwnedRoots =
    {
        "QuestJournalScreen",
        "QuestTrackerHud",
        SkeletonEncounterRootName
    };

    private static readonly string[] QuestSkeletonNames =
    {
        "QuestSkeleton_01",
        "QuestSkeleton_02",
        "QuestSkeleton_03"
    };

    private static readonly Vector3[] SkeletonPositions =
    {
        new(9f, 0f, 8f),
        new(13f, 0f, 0f),
        new(9f, 0f, -8f)
    };

    private static readonly Color Background = new(0.018f, 0.018f, 0.021f, 0.985f);
    private static readonly Color Panel = new(0.052f, 0.047f, 0.041f, 0.99f);
    private static readonly Color PanelRaised = new(0.09f, 0.075f, 0.058f, 1f);
    private static readonly Color Bronze = new(0.66f, 0.47f, 0.2f, 1f);
    private static readonly Color BronzeMuted = new(0.34f, 0.25f, 0.14f, 1f);
    private static readonly Color Ivory = new(0.91f, 0.87f, 0.76f, 1f);
    private static readonly Color Muted = new(0.62f, 0.59f, 0.52f, 1f);
    private static Font Font => Resources.GetBuiltinResource<Font>("LegacyRuntime.ttf");

    [MenuItem(BuildMenuPath)]
    public static void Build()
    {
        RefuseUnsafeBuild();
        EnsureFolders();

        GenericItemDefinition reward = CreateOrUpdateRewardItem();
        QuestDefinition definition = CreateOrUpdateQuestDefinition(reward);
        QuestDefinitionRegistry questRegistry = CreateOrUpdateQuestRegistry(definition);
        AddRewardToItemRegistry(reward);

        GameObject journalPrefab = BuildJournalPrefab();
        GameObject trackerPrefab = BuildTrackerPrefab();
        AugmentDialoguePrefab();
        UpdatePlayerPrefab(questRegistry);
        AugmentNpcPrefab(definition);
        AugmentSkeletonPrefab();
        IntegrateScene(journalPrefab, trackerPrefab, questRegistry);

        AssetDatabase.SaveAssets();
        AssetDatabase.Refresh();
        ValidateCurrent();
        Debug.Log("Quest System MVP built without running CombatSandboxSceneBuilder.");
    }

    [MenuItem(ValidateMenuPath)]
    public static void ValidateCurrent()
    {
        QuestDefinitionRegistry registry = AssetDatabase.LoadAssetAtPath<QuestDefinitionRegistry>(QuestRegistryPath);
        if (registry == null)
        {
            throw new InvalidOperationException("Quest registry is missing.");
        }

        if (!registry.Validate(out string registryError))
        {
            throw new InvalidOperationException($"Quest registry is invalid: {registryError}");
        }

        ItemDefinitionRegistry itemRegistry = AssetDatabase.LoadAssetAtPath<ItemDefinitionRegistry>(ItemRegistryPath);
        if (itemRegistry == null)
        {
            throw new InvalidOperationException("Item registry is missing.");
        }

        if (!itemRegistry.Validate(out string itemError))
        {
            throw new InvalidOperationException($"Item registry is invalid: {itemError}");
        }

        if (!itemRegistry.TryResolve("material.bone_fragment", out _))
        {
            throw new InvalidOperationException("Bone Fragment is missing from ItemDefinitionRegistry.");
        }

        Scene scene = SceneManager.GetActiveScene();
        if (!scene.IsValid() || scene.path != ScenePath)
        {
            throw new InvalidOperationException("Open CombatSandbox before validating the Quest System MVP.");
        }

        RequireSingleComponent<QuestJournalScreen>();
        RequireSingleComponent<QuestTrackerHud>();
        RequireSingleComponent<DialoguePanel>();
        PlayerQuestLog questLog = RequireSingleComponent<PlayerQuestLog>();
        SaveGameService saveService = RequireSingleComponent<SaveGameService>();

        if (!registry.TryResolve(QuestId, out QuestDefinition definition))
        {
            throw new InvalidOperationException($"Quest registry does not contain required quest '{QuestId}'.");
        }

        QuestDefinition expectedDefinition = AssetDatabase.LoadAssetAtPath<QuestDefinition>(QuestDefinitionPath);
        if (expectedDefinition == null || !ReferenceEquals(definition, expectedDefinition))
        {
            throw new InvalidOperationException(
                $"Quest registry must reference the definition at '{QuestDefinitionPath}' for '{QuestId}'.");
        }

        if (!string.Equals(definition.GiverNpcId, NpcId, StringComparison.Ordinal)
            || !string.Equals(definition.TurnInNpcId, NpcId, StringComparison.Ordinal))
        {
            throw new InvalidOperationException($"Quest '{QuestId}' has invalid giver or turn-in NPC references.");
        }

        if (!ReferenceEquals(questLog.Registry, registry))
        {
            throw new InvalidOperationException("PlayerQuestLog does not reference the active QuestDefinitionRegistry.");
        }

        SerializedObject saveSerialized = new(saveService);
        if (!ReferenceEquals(
                RequireProperty(saveSerialized, "questDefinitionRegistry").objectReferenceValue,
                registry))
        {
            throw new InvalidOperationException("SaveGameService does not reference the active QuestDefinitionRegistry.");
        }

        EventSystem[] eventSystems = UnityEngine.Object.FindObjectsByType<EventSystem>(
            FindObjectsInactive.Include,
            FindObjectsSortMode.None);
        if (eventSystems.Length != 1 || eventSystems[0].GetComponent<InputSystemUIInputModule>() == null)
        {
            throw new InvalidOperationException(
                $"Expected exactly one EventSystem with InputSystemUIInputModule, found {eventSystems.Length}.");
        }

        for (int i = 0; i < QuestOwnedRoots.Length; i++)
        {
            if (FindSceneRoot(scene, QuestOwnedRoots[i]) == null)
            {
                throw new InvalidOperationException($"Quest-owned scene root '{QuestOwnedRoots[i]}' is missing.");
            }
        }

        ValidateNpcSceneInstance(scene, definition);

        GameObject encounterRoot = FindSceneRoot(scene, SkeletonEncounterRootName);
        QuestEncounterController encounter = encounterRoot != null
            ? encounterRoot.GetComponent<QuestEncounterController>()
            : null;
        if (encounter == null
            || !string.Equals(encounter.QuestId, QuestId, StringComparison.Ordinal)
            || !string.Equals(encounter.ObjectiveId, DefeatObjectiveId, StringComparison.Ordinal)
            || encounter.EncounterObjectCount != QuestSkeletonNames.Length)
        {
            throw new InvalidOperationException("Skeleton quest encounter controller is missing or invalid.");
        }

        SerializedObject encounterSerialized = new(encounter);
        if (!ReferenceEquals(RequireProperty(encounterSerialized, "questLog").objectReferenceValue, questLog))
        {
            throw new InvalidOperationException("Skeleton quest encounter does not reference the active PlayerQuestLog.");
        }

        SerializedProperty encounterObjects = RequireProperty(encounterSerialized, "encounterObjects");
        HashSet<string> stableIds = new(StringComparer.Ordinal);
        for (int i = 0; i < 3; i++)
        {
            GameObject skeleton = FindDirectChild(encounterRoot.transform, QuestSkeletonNames[i]);
            QuestTargetIdentity identity = skeleton != null ? skeleton.GetComponent<QuestTargetIdentity>() : null;
            if (identity == null
                || !string.Equals(identity.TargetId, SkeletonTargetId, StringComparison.Ordinal)
                || string.IsNullOrWhiteSpace(identity.StableInstanceId)
                || !stableIds.Add(identity.StableInstanceId)
                || !ReferenceEquals(
                    encounterObjects.GetArrayElementAtIndex(i).objectReferenceValue,
                    skeleton))
            {
                throw new InvalidOperationException(
                    $"Quest skeleton {i + 1} has an invalid encounter reference or duplicate quest identity.");
            }
        }

        Debug.Log("Quest System MVP validation passed.");
    }

    public static void RestoreQuestAugmentationsIfAvailable()
    {
        QuestDefinition definition = AssetDatabase.LoadAssetAtPath<QuestDefinition>(QuestDefinitionPath);
        if (definition == null)
        {
            return;
        }

        AugmentDialoguePrefab();
        AugmentNpcPrefab(definition);
        AugmentSkeletonPrefab();
    }

    public static void RestoreQuestNpcSceneInstanceIfAvailable(GameObject npc)
    {
        QuestDefinition definition = AssetDatabase.LoadAssetAtPath<QuestDefinition>(QuestDefinitionPath);
        if (definition == null)
        {
            return;
        }

        if (npc == null)
        {
            throw new InvalidOperationException("NpcPlaceholder scene instance is missing.");
        }

        ConfigureNpcQuestComponents(npc, definition, NpcSceneStableId);
    }

    public static void AugmentDialoguePrefab()
    {
        GameObject root = PrefabUtility.LoadPrefabContents(DialoguePrefabPath);
        if (root == null)
        {
            throw new InvalidOperationException("DialoguePanel prefab is required before building quests.");
        }

        try
        {
            DialoguePanel panel = root.GetComponent<DialoguePanel>();
            RectTransform panelRoot = FindChild(root.transform, "PanelRoot") as RectTransform;
            if (panel == null || panelRoot == null)
            {
                throw new InvalidOperationException("DialoguePanel prefab is missing its component or PanelRoot.");
            }

            panelRoot.sizeDelta = new Vector2(980f, 260f);
            panelRoot.anchoredPosition = new Vector2(0f, 190f);
            DestroyDirectChild(panelRoot, "QuestFeedback");
            DestroyDirectChild(panelRoot, "PrimaryAction");
            DestroyDirectChild(panelRoot, "SecondaryAction");

            Text feedback = CreateText("QuestFeedback", panelRoot, string.Empty, 14, TextAnchor.MiddleLeft, Bronze, FontStyle.Italic);
            SetAnchors(feedback.rectTransform, new Vector2(0.035f, 0.235f), new Vector2(0.95f, 0.315f));
            Button primary = CreateButton("PrimaryAction", panelRoot, "ACEITAR", 14);
            SetAnchors(primary.GetComponent<RectTransform>(), new Vector2(0.035f, 0.045f), new Vector2(0.205f, 0.205f));
            Button secondary = CreateButton("SecondaryAction", panelRoot, "RECUSAR", 14);
            SetAnchors(secondary.GetComponent<RectTransform>(), new Vector2(0.22f, 0.045f), new Vector2(0.39f, 0.205f));

            Text primaryLabel = primary.GetComponentInChildren<Text>(true);
            Text secondaryLabel = secondary.GetComponentInChildren<Text>(true);
            SerializedObject serialized = new(panel);
            SetObject(serialized, "feedbackText", feedback);
            SetObject(serialized, "primaryActionButton", primary);
            SetObject(serialized, "primaryActionText", primaryLabel);
            SetObject(serialized, "secondaryActionButton", secondary);
            SetObject(serialized, "secondaryActionText", secondaryLabel);
            serialized.ApplyModifiedPropertiesWithoutUndo();

            primary.gameObject.SetActive(false);
            secondary.gameObject.SetActive(false);
            PrefabUtility.SaveAsPrefabAsset(root, DialoguePrefabPath);
        }
        finally
        {
            PrefabUtility.UnloadPrefabContents(root);
        }
    }

    private static GenericItemDefinition CreateOrUpdateRewardItem()
    {
        GenericItemDefinition reward = AssetDatabase.LoadAssetAtPath<GenericItemDefinition>(RewardItemPath);
        if (reward == null)
        {
            reward = ScriptableObject.CreateInstance<GenericItemDefinition>();
            AssetDatabase.CreateAsset(reward, RewardItemPath);
        }

        SerializedObject serialized = new(reward);
        SetString(serialized, "definitionId", "material.bone_fragment");
        SetString(serialized, "displayName", "Fragmento de Osso");
        SetString(serialized, "description", "Um fragmento ressecado recolhido dos esqueletos da floresta.");
        SetInt(serialized, "category", (int)ItemCategory.Material);
        SetInt(serialized, "maxStackSize", 99);
        SetFloat(serialized, "defaultMaxDurability", 0f);
        SetInt(serialized, "acceptedEquipmentSlots", 0);
        serialized.ApplyModifiedPropertiesWithoutUndo();
        EditorUtility.SetDirty(reward);
        return reward;
    }

    private static QuestDefinition CreateOrUpdateQuestDefinition(GenericItemDefinition reward)
    {
        QuestDefinition definition = AssetDatabase.LoadAssetAtPath<QuestDefinition>(QuestDefinitionPath);
        if (definition == null)
        {
            definition = ScriptableObject.CreateInstance<QuestDefinition>();
            AssetDatabase.CreateAsset(definition, QuestDefinitionPath);
        }

        SerializedObject serialized = new(definition);
        SetString(serialized, "questId", QuestId);
        SetString(serialized, "title", "Problemas com Esqueletos");
        SetString(serialized, "description", "Um aldeao pediu ajuda para conter os esqueletos que cercam a regiao.");
        SetBool(serialized, "availableByDefault", true);
        SetString(serialized, "giverNpcId", NpcId);
        SetString(serialized, "turnInNpcId", NpcId);
        SerializedProperty objectives = RequireProperty(serialized, "objectives");
        objectives.arraySize = 2;
        ConfigureObjective(
            objectives.GetArrayElementAtIndex(0),
            "defeat_skeletons",
            QuestObjectiveType.DefeatTarget,
            SkeletonTargetId,
            3,
            "Elimine os esqueletos");
        ConfigureObjective(
            objectives.GetArrayElementAtIndex(1),
            "return_to_villager",
            QuestObjectiveType.TalkToNpc,
            NpcId,
            1,
            "Retorne ao aldeao");
        SetObject(serialized, "rewardItem", reward);
        SetInt(serialized, "rewardQuantity", 3);
        SetString(serialized, "offerText", "Esqueletos estao cercando a floresta. Pode eliminar tres deles e voltar para me avisar?");
        SetString(serialized, "declinedText", "Entendo. Se mudar de ideia, ainda precisaremos de ajuda.");
        SetString(serialized, "activeText", "Os esqueletos ainda estao la. Tome cuidado e volte quando a area estiver segura.");
        SetString(serialized, "readyText", "Voce conseguiu! Tenho uma pequena recompensa pelo seu trabalho.");
        SetString(serialized, "completedText", "Obrigado novamente. A floresta esta mais segura por sua causa.");
        serialized.ApplyModifiedPropertiesWithoutUndo();
        EditorUtility.SetDirty(definition);
        return definition;
    }

    private static QuestDefinitionRegistry CreateOrUpdateQuestRegistry(QuestDefinition definition)
    {
        QuestDefinitionRegistry registry = AssetDatabase.LoadAssetAtPath<QuestDefinitionRegistry>(QuestRegistryPath);
        if (registry == null)
        {
            registry = ScriptableObject.CreateInstance<QuestDefinitionRegistry>();
            AssetDatabase.CreateAsset(registry, QuestRegistryPath);
        }

        SerializedObject serialized = new(registry);
        SerializedProperty definitions = RequireProperty(serialized, "definitions");
        List<QuestDefinition> mergedDefinitions = new();
        HashSet<QuestDefinition> seenDefinitions = new();
        bool insertedRequiredDefinition = false;
        for (int i = 0; i < definitions.arraySize; i++)
        {
            QuestDefinition existing = definitions.GetArrayElementAtIndex(i).objectReferenceValue as QuestDefinition;
            if (existing == null)
            {
                continue;
            }

            if (string.Equals(existing.QuestId, definition.QuestId, StringComparison.Ordinal))
            {
                if (!insertedRequiredDefinition && seenDefinitions.Add(definition))
                {
                    mergedDefinitions.Add(definition);
                    insertedRequiredDefinition = true;
                }

                continue;
            }

            if (seenDefinitions.Add(existing))
            {
                mergedDefinitions.Add(existing);
            }
        }

        if (!insertedRequiredDefinition && seenDefinitions.Add(definition))
        {
            mergedDefinitions.Add(definition);
        }

        definitions.arraySize = mergedDefinitions.Count;
        for (int i = 0; i < mergedDefinitions.Count; i++)
        {
            definitions.GetArrayElementAtIndex(i).objectReferenceValue = mergedDefinitions[i];
        }

        serialized.ApplyModifiedPropertiesWithoutUndo();
        EditorUtility.SetDirty(registry);
        return registry;
    }

    private static void AddRewardToItemRegistry(GenericItemDefinition reward)
    {
        ItemDefinitionRegistry registry = AssetDatabase.LoadAssetAtPath<ItemDefinitionRegistry>(ItemRegistryPath);
        if (registry == null)
        {
            throw new InvalidOperationException("Existing ItemDefinitionRegistry is required.");
        }

        SerializedObject serialized = new(registry);
        SerializedProperty definitions = RequireProperty(serialized, "definitions");
        List<UnityEngine.Object> values = new();
        for (int i = 0; i < definitions.arraySize; i++)
        {
            UnityEngine.Object value = definitions.GetArrayElementAtIndex(i).objectReferenceValue;
            if (value != null && value != reward)
            {
                values.Add(value);
            }
        }

        values.Add(reward);
        definitions.arraySize = values.Count;
        for (int i = 0; i < values.Count; i++)
        {
            definitions.GetArrayElementAtIndex(i).objectReferenceValue = values[i];
        }

        serialized.ApplyModifiedPropertiesWithoutUndo();
        EditorUtility.SetDirty(registry);
    }

    private static void ConfigureObjective(
        SerializedProperty objective,
        string objectiveId,
        QuestObjectiveType type,
        string targetId,
        int requiredAmount,
        string description)
    {
        objective.FindPropertyRelative("objectiveId").stringValue = objectiveId;
        objective.FindPropertyRelative("objectiveType").enumValueIndex = (int)type;
        objective.FindPropertyRelative("targetId").stringValue = targetId;
        objective.FindPropertyRelative("requiredAmount").intValue = requiredAmount;
        objective.FindPropertyRelative("description").stringValue = description;
    }

    private static void UpdatePlayerPrefab(QuestDefinitionRegistry registry)
    {
        GameObject root = PrefabUtility.LoadPrefabContents(PlayerPrefabPath);
        try
        {
            PlayerQuestLog questLog = root.GetComponent<PlayerQuestLog>();
            if (questLog == null)
            {
                questLog = root.AddComponent<PlayerQuestLog>();
            }

            SerializedObject serialized = new(questLog);
            SetObject(serialized, "registry", registry);
            SetObject(serialized, "inventory", root.GetComponent<PlayerInventory>());
            SetObject(serialized, "interactor", root.GetComponent<PlayerInteractor>());
            serialized.ApplyModifiedPropertiesWithoutUndo();

            SerializedObject inventorySerialized = new(root.GetComponent<PlayerInventory>());
            SerializedProperty itemContainer = RequireProperty(inventorySerialized, "itemContainer");
            itemContainer.FindPropertyRelative("capacity").intValue = PlayerInventoryCapacity;
            inventorySerialized.ApplyModifiedPropertiesWithoutUndo();
            PrefabUtility.SaveAsPrefabAsset(root, PlayerPrefabPath);
        }
        finally
        {
            PrefabUtility.UnloadPrefabContents(root);
        }
    }

    private static void AugmentNpcPrefab(QuestDefinition definition)
    {
        GameObject root = PrefabUtility.LoadPrefabContents(NpcPrefabPath);
        try
        {
            ConfigureNpcQuestComponents(root, definition, string.Empty);
            PrefabUtility.SaveAsPrefabAsset(root, NpcPrefabPath);
        }
        finally
        {
            PrefabUtility.UnloadPrefabContents(root);
        }
    }

    private static void AugmentSkeletonPrefab()
    {
        GameObject root = PrefabUtility.LoadPrefabContents(SkeletonPrefabPath);
        try
        {
            QuestTargetIdentity identity = root.GetComponent<QuestTargetIdentity>();
            if (identity == null)
            {
                identity = root.AddComponent<QuestTargetIdentity>();
            }

            SerializedObject serialized = new(identity);
            SetString(serialized, "targetId", SkeletonTargetId);
            SetString(serialized, "stableInstanceId", string.Empty);
            SetInt(serialized, "targetKind", (int)QuestTargetKind.Enemy);
            SetObject(serialized, "enemyHealth", root.GetComponent<TrainingDummyHealth>());
            SetBoolIfPresent(serialized, "autoStartCombat", false);
            serialized.ApplyModifiedPropertiesWithoutUndo();
            PrefabUtility.SaveAsPrefabAsset(root, SkeletonPrefabPath);
        }
        finally
        {
            PrefabUtility.UnloadPrefabContents(root);
        }
    }

    private static void IntegrateScene(
        GameObject journalPrefab,
        GameObject trackerPrefab,
        QuestDefinitionRegistry registry)
    {
        Scene scene = SceneManager.GetActiveScene();
        if (scene.path != ScenePath)
        {
            scene = EditorSceneManager.OpenScene(ScenePath, OpenSceneMode.Single);
        }

        for (int i = 0; i < QuestOwnedRoots.Length; i++)
        {
            GameObject existing = FindSceneRoot(scene, QuestOwnedRoots[i]);
            if (existing != null)
            {
                UnityEngine.Object.DestroyImmediate(existing);
            }
        }

        for (int i = 0; i < QuestSkeletonNames.Length; i++)
        {
            GameObject legacySkeletonRoot = FindSceneRoot(scene, QuestSkeletonNames[i]);
            if (legacySkeletonRoot != null)
            {
                UnityEngine.Object.DestroyImmediate(legacySkeletonRoot);
            }
        }

        GameObject player = FindSceneRoot(scene, "PlayerPlaceholder");
        if (player == null)
        {
            throw new InvalidOperationException("PlayerPlaceholder is missing from CombatSandbox.");
        }

        PlayerQuestLog questLog = player.GetComponent<PlayerQuestLog>();
        if (questLog == null)
        {
            questLog = player.AddComponent<PlayerQuestLog>();
        }

        SerializedObject questLogSerialized = new(questLog);
        SetObject(questLogSerialized, "registry", registry);
        SetObject(questLogSerialized, "inventory", player.GetComponent<PlayerInventory>());
        SetObject(questLogSerialized, "interactor", player.GetComponent<PlayerInteractor>());
        questLogSerialized.ApplyModifiedPropertiesWithoutUndo();

        SerializedObject playerInventorySerialized = new(player.GetComponent<PlayerInventory>());
        SerializedProperty playerItemContainer = RequireProperty(playerInventorySerialized, "itemContainer");
        playerItemContainer.FindPropertyRelative("capacity").intValue = PlayerInventoryCapacity;
        playerInventorySerialized.ApplyModifiedPropertiesWithoutUndo();

        GameObject journal = (GameObject)PrefabUtility.InstantiatePrefab(journalPrefab, scene);
        journal.name = "QuestJournalScreen";
        SerializedObject journalSerialized = new(journal.GetComponent<QuestJournalScreen>());
        SetObject(journalSerialized, "questLog", questLog);
        journalSerialized.ApplyModifiedPropertiesWithoutUndo();

        GameObject tracker = (GameObject)PrefabUtility.InstantiatePrefab(trackerPrefab, scene);
        tracker.name = "QuestTrackerHud";
        SerializedObject trackerSerialized = new(tracker.GetComponent<QuestTrackerHud>());
        SetObject(trackerSerialized, "questLog", questLog);
        trackerSerialized.ApplyModifiedPropertiesWithoutUndo();

        GameObject npc = FindSceneRoot(scene, "NpcPlaceholder");
        QuestTargetIdentity npcIdentity = npc != null ? npc.GetComponent<QuestTargetIdentity>() : null;
        if (npc == null || npc.GetComponent<QuestGiver>() == null || npcIdentity == null)
        {
            throw new InvalidOperationException("NpcPlaceholder did not inherit the quest giver augmentation.");
        }

        RestoreQuestNpcSceneInstanceIfAvailable(npc);

        GameObject encounterRoot = new(SkeletonEncounterRootName);
        SceneManager.MoveGameObjectToScene(encounterRoot, scene);
        QuestEncounterController encounter = encounterRoot.AddComponent<QuestEncounterController>();

        GameObject skeletonPrefab = AssetDatabase.LoadAssetAtPath<GameObject>(SkeletonPrefabPath);
        if (skeletonPrefab == null)
        {
            throw new InvalidOperationException("SkeletonEnemy prefab is missing.");
        }

        GameObject[] encounterObjects = new GameObject[QuestSkeletonNames.Length];
        for (int i = 0; i < encounterObjects.Length; i++)
        {
            GameObject skeleton = (GameObject)PrefabUtility.InstantiatePrefab(skeletonPrefab, scene);
            skeleton.name = QuestSkeletonNames[i];
            skeleton.transform.SetPositionAndRotation(
                SkeletonPositions[i],
                Quaternion.LookRotation(Vector3.ProjectOnPlane(-SkeletonPositions[i], Vector3.up).normalized));
            skeleton.transform.SetParent(encounterRoot.transform, true);
            QuestTargetIdentity identity = skeleton.GetComponent<QuestTargetIdentity>();
            if (identity == null)
            {
                throw new InvalidOperationException("SkeletonEnemy did not inherit QuestTargetIdentity.");
            }

            SerializedObject identitySerialized = new(identity);
            SetString(identitySerialized, "stableInstanceId", $"combat_sandbox.enemy.quest_skeleton_{i + 1:00}");
            SetBoolIfPresent(identitySerialized, "autoStartCombat", true);
            identitySerialized.ApplyModifiedPropertiesWithoutUndo();
            skeleton.SetActive(false);
            encounterObjects[i] = skeleton;
        }

        SerializedObject encounterSerialized = new(encounter);
        SetObject(encounterSerialized, "questLog", questLog);
        SetString(encounterSerialized, "questId", QuestId);
        SetString(encounterSerialized, "objectiveId", DefeatObjectiveId);
        SerializedProperty encounterObjectsProperty = RequireProperty(encounterSerialized, "encounterObjects");
        encounterObjectsProperty.arraySize = encounterObjects.Length;
        for (int i = 0; i < encounterObjects.Length; i++)
        {
            encounterObjectsProperty.GetArrayElementAtIndex(i).objectReferenceValue = encounterObjects[i];
        }

        encounterSerialized.ApplyModifiedPropertiesWithoutUndo();

        SaveGameService saveService = UnityEngine.Object.FindFirstObjectByType<SaveGameService>(FindObjectsInactive.Include);
        if (saveService == null)
        {
            throw new InvalidOperationException("SaveGameService is missing from CombatSandbox.");
        }

        SerializedObject saveSerialized = new(saveService);
        SetObject(saveSerialized, "questDefinitionRegistry", registry);
        saveSerialized.ApplyModifiedPropertiesWithoutUndo();

        EnsureSingleEventSystem(scene);
        EditorSceneManager.MarkSceneDirty(scene);
        EditorSceneManager.SaveScene(scene);
    }

    private static GameObject BuildJournalPrefab()
    {
        GameObject root = CreateCanvasRoot("QuestJournalScreen", 45);
        QuestJournalScreen screen = root.AddComponent<QuestJournalScreen>();
        GameObject screenRoot = CreatePanel("ScreenRoot", root.transform, Background);
        Stretch(screenRoot.GetComponent<RectTransform>());

        GameObject frame = CreatePanel("JournalFrame", screenRoot.transform, Panel);
        SetAnchors(frame.GetComponent<RectTransform>(), new Vector2(0.08f, 0.08f), new Vector2(0.92f, 0.92f));
        AddOutline(frame, BronzeMuted, 2f);

        Image topRule = CreateImage("TopRule", frame.transform, Bronze);
        SetAnchors(topRule.rectTransform, new Vector2(0f, 0.992f), Vector2.one);
        Text title = CreateText("Header", frame.transform, "DIARIO DE MISSOES", 30, TextAnchor.MiddleLeft, Ivory, FontStyle.Bold);
        SetAnchors(title.rectTransform, new Vector2(0.03f, 0.905f), new Vector2(0.72f, 0.985f));
        Text hint = CreateText("Hint", frame.transform, "J / Esc - Fechar", 13, TextAnchor.MiddleRight, Muted, FontStyle.Italic);
        SetAnchors(hint.rectTransform, new Vector2(0.67f, 0.915f), new Vector2(0.89f, 0.975f));
        Button close = CreateButton("CloseButton", frame.transform, "X", 15);
        SetAnchors(close.GetComponent<RectTransform>(), new Vector2(0.91f, 0.92f), new Vector2(0.97f, 0.975f));

        GameObject listPanel = CreatePanel("QuestLists", frame.transform, new Color(0.035f, 0.032f, 0.029f, 0.99f));
        SetAnchors(listPanel.GetComponent<RectTransform>(), new Vector2(0.025f, 0.055f), new Vector2(0.37f, 0.89f));
        AddOutline(listPanel, BronzeMuted, 1f);
        Text activeHeader = CreateText("ActiveHeader", listPanel.transform, "ATIVAS", 17, TextAnchor.MiddleLeft, Bronze, FontStyle.Bold);
        SetAnchors(activeHeader.rectTransform, new Vector2(0.055f, 0.91f), new Vector2(0.95f, 0.985f));
        Transform activeList = CreateScrollList("ActiveQuestList", listPanel.transform, new Vector2(0.04f, 0.52f), new Vector2(0.96f, 0.905f));
        Text emptyActive = CreateText("EmptyActive", listPanel.transform, "Nenhuma missao ativa.", 14, TextAnchor.UpperLeft, Muted, FontStyle.Italic);
        SetAnchors(emptyActive.rectTransform, new Vector2(0.07f, 0.73f), new Vector2(0.93f, 0.86f));

        Text completedHeader = CreateText("CompletedHeader", listPanel.transform, "CONCLUIDAS", 17, TextAnchor.MiddleLeft, Bronze, FontStyle.Bold);
        SetAnchors(completedHeader.rectTransform, new Vector2(0.055f, 0.43f), new Vector2(0.95f, 0.505f));
        Transform completedList = CreateScrollList("CompletedQuestList", listPanel.transform, new Vector2(0.04f, 0.055f), new Vector2(0.96f, 0.425f));
        Text emptyCompleted = CreateText("EmptyCompleted", listPanel.transform, "Nenhuma missao concluida.", 14, TextAnchor.UpperLeft, Muted, FontStyle.Italic);
        SetAnchors(emptyCompleted.rectTransform, new Vector2(0.07f, 0.24f), new Vector2(0.93f, 0.37f));

        Button entryTemplate = CreateButton("QuestEntryTemplate", activeList, "Missao", 14);
        RectTransform entryRect = entryTemplate.GetComponent<RectTransform>();
        entryRect.sizeDelta = new Vector2(0f, 64f);
        LayoutElement entryLayout = entryTemplate.gameObject.AddComponent<LayoutElement>();
        entryLayout.preferredHeight = 64f;
        Text entryLabel = entryTemplate.GetComponentInChildren<Text>(true);
        entryLabel.alignment = TextAnchor.MiddleLeft;
        entryLabel.rectTransform.offsetMin = new Vector2(12f, 5f);
        entryLabel.rectTransform.offsetMax = new Vector2(-8f, -5f);
        entryTemplate.gameObject.SetActive(false);

        GameObject detailPanel = CreatePanel("QuestDetails", frame.transform, new Color(0.045f, 0.04f, 0.035f, 0.99f));
        SetAnchors(detailPanel.GetComponent<RectTransform>(), new Vector2(0.39f, 0.055f), new Vector2(0.975f, 0.89f));
        AddOutline(detailPanel, BronzeMuted, 1f);
        Text detailTitle = CreateText("QuestTitle", detailPanel.transform, "Selecione uma missao", 27, TextAnchor.MiddleLeft, Ivory, FontStyle.Bold);
        SetAnchors(detailTitle.rectTransform, new Vector2(0.045f, 0.86f), new Vector2(0.78f, 0.965f));
        Text detailState = CreateText("QuestState", detailPanel.transform, string.Empty, 13, TextAnchor.MiddleRight, Bronze, FontStyle.Bold);
        SetAnchors(detailState.rectTransform, new Vector2(0.7f, 0.875f), new Vector2(0.955f, 0.95f));
        Text detailDescription = CreateText("Description", detailPanel.transform, "Missoes ativas e concluidas aparecem aqui.", 18, TextAnchor.UpperLeft, Ivory, FontStyle.Normal);
        SetAnchors(detailDescription.rectTransform, new Vector2(0.045f, 0.67f), new Vector2(0.955f, 0.855f));
        Text objectivesHeader = CreateText("ObjectivesHeader", detailPanel.transform, "OBJETIVOS", 14, TextAnchor.MiddleLeft, Bronze, FontStyle.Bold);
        SetAnchors(objectivesHeader.rectTransform, new Vector2(0.045f, 0.605f), new Vector2(0.955f, 0.67f));
        Text objectives = CreateText("Objectives", detailPanel.transform, string.Empty, 17, TextAnchor.UpperLeft, Ivory, FontStyle.Normal);
        SetAnchors(objectives.rectTransform, new Vector2(0.045f, 0.33f), new Vector2(0.955f, 0.61f));
        Text reward = CreateText("Reward", detailPanel.transform, string.Empty, 15, TextAnchor.MiddleLeft, Muted, FontStyle.Bold);
        SetAnchors(reward.rectTransform, new Vector2(0.045f, 0.245f), new Vector2(0.955f, 0.325f));
        Text feedback = CreateText("Feedback", detailPanel.transform, string.Empty, 13, TextAnchor.MiddleLeft, Bronze, FontStyle.Italic);
        SetAnchors(feedback.rectTransform, new Vector2(0.045f, 0.135f), new Vector2(0.7f, 0.23f));
        Button track = CreateButton("TrackButton", detailPanel.transform, "RASTREAR", 13);
        SetAnchors(track.GetComponent<RectTransform>(), new Vector2(0.72f, 0.115f), new Vector2(0.955f, 0.215f));
        Text trackLabel = track.GetComponentInChildren<Text>(true);

        SerializedObject serialized = new(screen);
        SetObject(serialized, "screenRoot", screenRoot);
        SetObject(serialized, "activeQuestList", activeList);
        SetObject(serialized, "completedQuestList", completedList);
        SetObject(serialized, "questEntryTemplate", entryTemplate);
        SetObject(serialized, "emptyActiveText", emptyActive);
        SetObject(serialized, "emptyCompletedText", emptyCompleted);
        SetObject(serialized, "titleText", detailTitle);
        SetObject(serialized, "stateText", detailState);
        SetObject(serialized, "descriptionText", detailDescription);
        SetObject(serialized, "objectivesText", objectives);
        SetObject(serialized, "rewardText", reward);
        SetObject(serialized, "feedbackText", feedback);
        SetObject(serialized, "trackButton", track);
        SetObject(serialized, "trackButtonText", trackLabel);
        SetObject(serialized, "closeButton", close);
        serialized.ApplyModifiedPropertiesWithoutUndo();

        screenRoot.SetActive(false);
        GameObject prefab = PrefabUtility.SaveAsPrefabAsset(root, JournalPrefabPath);
        UnityEngine.Object.DestroyImmediate(root);
        return prefab;
    }

    private static GameObject BuildTrackerPrefab()
    {
        GameObject root = CreateCanvasRoot("QuestTrackerHud", 10);
        QuestTrackerHud tracker = root.AddComponent<QuestTrackerHud>();
        GameObject trackerRoot = CreatePanel("TrackerRoot", root.transform, new Color(0.03f, 0.028f, 0.025f, 0.93f));
        RectTransform rect = trackerRoot.GetComponent<RectTransform>();
        rect.anchorMin = Vector2.one;
        rect.anchorMax = Vector2.one;
        rect.pivot = Vector2.one;
        rect.anchoredPosition = new Vector2(-24f, -100f);
        rect.sizeDelta = new Vector2(340f, 110f);
        AddOutline(trackerRoot, BronzeMuted, 1f);
        Image rule = CreateImage("TopRule", trackerRoot.transform, Bronze);
        SetAnchors(rule.rectTransform, new Vector2(0f, 0.965f), Vector2.one);
        Text title = CreateText("QuestTitle", trackerRoot.transform, "MISSAO", 16, TextAnchor.MiddleLeft, Ivory, FontStyle.Bold);
        SetAnchors(title.rectTransform, new Vector2(0.055f, 0.62f), new Vector2(0.945f, 0.94f));
        Text objective = CreateText("Objective", trackerRoot.transform, string.Empty, 14, TextAnchor.UpperLeft, Ivory, FontStyle.Normal);
        SetAnchors(objective.rectTransform, new Vector2(0.055f, 0.18f), new Vector2(0.945f, 0.64f));
        Text state = CreateText("State", trackerRoot.transform, string.Empty, 11, TextAnchor.MiddleRight, Bronze, FontStyle.Bold);
        SetAnchors(state.rectTransform, new Vector2(0.45f, 0.02f), new Vector2(0.945f, 0.2f));

        SerializedObject serialized = new(tracker);
        SetObject(serialized, "trackerRoot", trackerRoot);
        SetObject(serialized, "titleText", title);
        SetObject(serialized, "objectiveText", objective);
        SetObject(serialized, "stateText", state);
        serialized.ApplyModifiedPropertiesWithoutUndo();
        trackerRoot.SetActive(false);

        GameObject prefab = PrefabUtility.SaveAsPrefabAsset(root, TrackerPrefabPath);
        UnityEngine.Object.DestroyImmediate(root);
        return prefab;
    }

    private static Transform CreateScrollList(
        string name,
        Transform parent,
        Vector2 anchorMin,
        Vector2 anchorMax)
    {
        GameObject viewport = CreatePanel(name, parent, new Color(0.02f, 0.019f, 0.018f, 0.92f));
        SetAnchors(viewport.GetComponent<RectTransform>(), anchorMin, anchorMax);
        Mask mask = viewport.AddComponent<Mask>();
        mask.showMaskGraphic = true;
        ScrollRect scroll = viewport.AddComponent<ScrollRect>();
        scroll.horizontal = false;
        scroll.vertical = true;
        scroll.movementType = ScrollRect.MovementType.Clamped;
        scroll.scrollSensitivity = 28f;

        GameObject content = new("Content", typeof(RectTransform));
        content.transform.SetParent(viewport.transform, false);
        RectTransform contentRect = content.GetComponent<RectTransform>();
        contentRect.anchorMin = new Vector2(0f, 1f);
        contentRect.anchorMax = new Vector2(1f, 1f);
        contentRect.pivot = new Vector2(0.5f, 1f);
        contentRect.anchoredPosition = Vector2.zero;
        contentRect.sizeDelta = new Vector2(-12f, 0f);
        VerticalLayoutGroup layout = content.AddComponent<VerticalLayoutGroup>();
        layout.padding = new RectOffset(6, 6, 6, 6);
        layout.spacing = 7f;
        layout.childAlignment = TextAnchor.UpperCenter;
        layout.childControlWidth = true;
        layout.childControlHeight = false;
        layout.childForceExpandWidth = true;
        layout.childForceExpandHeight = false;
        ContentSizeFitter fitter = content.AddComponent<ContentSizeFitter>();
        fitter.verticalFit = ContentSizeFitter.FitMode.PreferredSize;
        scroll.viewport = viewport.GetComponent<RectTransform>();
        scroll.content = contentRect;
        return content.transform;
    }

    private static void RefuseUnsafeBuild()
    {
        if (EditorApplication.isPlayingOrWillChangePlaymode)
        {
            throw new InvalidOperationException("Exit Play Mode before building the Quest System MVP.");
        }

        Scene active = SceneManager.GetActiveScene();
        if (active.IsValid() && active.isDirty)
        {
            throw new InvalidOperationException("Save or discard active scene changes before building quests.");
        }
    }

    private static void EnsureFolders()
    {
        EnsureFolder("Assets/_Project/Data/Quests");
        EnsureFolder("Assets/_Project/Data/Items");
        EnsureFolder("Assets/_Project/Prefabs/UI");
    }

    private static void EnsureFolder(string path)
    {
        if (AssetDatabase.IsValidFolder(path))
        {
            return;
        }

        int separator = path.LastIndexOf('/');
        string parent = path.Substring(0, separator);
        string name = path.Substring(separator + 1);
        EnsureFolder(parent);
        AssetDatabase.CreateFolder(parent, name);
    }

    private static void EnsureSingleEventSystem(Scene scene)
    {
        EventSystem[] eventSystems = UnityEngine.Object.FindObjectsByType<EventSystem>(
            FindObjectsInactive.Include,
            FindObjectsSortMode.None);
        if (eventSystems.Length > 1)
        {
            throw new InvalidOperationException("CombatSandbox contains more than one EventSystem.");
        }

        if (eventSystems.Length == 0)
        {
            GameObject root = new("UIEventSystem");
            SceneManager.MoveGameObjectToScene(root, scene);
            root.AddComponent<EventSystem>();
            root.AddComponent<InputSystemUIInputModule>();
            return;
        }

        if (eventSystems[0].GetComponent<InputSystemUIInputModule>() == null)
        {
            BaseInputModule oldModule = eventSystems[0].GetComponent<BaseInputModule>();
            if (oldModule != null)
            {
                UnityEngine.Object.DestroyImmediate(oldModule);
            }

            eventSystems[0].gameObject.AddComponent<InputSystemUIInputModule>();
        }
    }

    private static GameObject CreateCanvasRoot(string name, int sortingOrder)
    {
        GameObject root = new(name, typeof(RectTransform));
        Canvas canvas = root.AddComponent<Canvas>();
        canvas.renderMode = RenderMode.ScreenSpaceOverlay;
        canvas.sortingOrder = sortingOrder;
        CanvasScaler scaler = root.AddComponent<CanvasScaler>();
        scaler.uiScaleMode = CanvasScaler.ScaleMode.ScaleWithScreenSize;
        scaler.referenceResolution = new Vector2(1920f, 1080f);
        scaler.screenMatchMode = CanvasScaler.ScreenMatchMode.MatchWidthOrHeight;
        scaler.matchWidthOrHeight = 0.5f;
        root.AddComponent<GraphicRaycaster>();
        return root;
    }

    private static GameObject CreatePanel(string name, Transform parent, Color color)
    {
        GameObject root = new(name, typeof(RectTransform));
        root.transform.SetParent(parent, false);
        Image image = root.AddComponent<Image>();
        image.color = color;
        return root;
    }

    private static Image CreateImage(string name, Transform parent, Color color)
    {
        GameObject root = new(name, typeof(RectTransform));
        root.transform.SetParent(parent, false);
        Image image = root.AddComponent<Image>();
        image.color = color;
        image.raycastTarget = false;
        return image;
    }

    private static Text CreateText(
        string name,
        Transform parent,
        string value,
        int fontSize,
        TextAnchor alignment,
        Color color,
        FontStyle style)
    {
        GameObject root = new(name, typeof(RectTransform));
        root.transform.SetParent(parent, false);
        Text text = root.AddComponent<Text>();
        text.font = Font;
        text.text = value;
        text.fontSize = fontSize;
        text.fontStyle = style;
        text.alignment = alignment;
        text.color = color;
        text.raycastTarget = false;
        text.horizontalOverflow = HorizontalWrapMode.Wrap;
        text.verticalOverflow = VerticalWrapMode.Truncate;
        return text;
    }

    private static Button CreateButton(string name, Transform parent, string label, int fontSize)
    {
        GameObject root = CreatePanel(name, parent, PanelRaised);
        AddOutline(root, BronzeMuted, 1f);
        Button button = root.AddComponent<Button>();
        button.targetGraphic = root.GetComponent<Image>();
        ColorBlock colors = button.colors;
        colors.normalColor = Color.white;
        colors.highlightedColor = new Color(1.16f, 1.1f, 0.94f, 1f);
        colors.pressedColor = new Color(0.74f, 0.67f, 0.56f, 1f);
        colors.disabledColor = new Color(0.35f, 0.35f, 0.35f, 0.5f);
        button.colors = colors;
        Text text = CreateText("Label", root.transform, label, fontSize, TextAnchor.MiddleCenter, Ivory, FontStyle.Bold);
        Stretch(text.rectTransform, 5f);
        return button;
    }

    private static void AddOutline(GameObject target, Color color, float distance)
    {
        Outline outline = target.GetComponent<Outline>();
        if (outline == null)
        {
            outline = target.AddComponent<Outline>();
        }

        outline.effectColor = color;
        outline.effectDistance = new Vector2(distance, -distance);
    }

    private static void Stretch(RectTransform rect, float inset = 0f)
    {
        rect.anchorMin = Vector2.zero;
        rect.anchorMax = Vector2.one;
        rect.pivot = new Vector2(0.5f, 0.5f);
        rect.offsetMin = new Vector2(inset, inset);
        rect.offsetMax = new Vector2(-inset, -inset);
        rect.localScale = Vector3.one;
    }

    private static void SetAnchors(RectTransform rect, Vector2 min, Vector2 max)
    {
        rect.anchorMin = min;
        rect.anchorMax = max;
        rect.pivot = new Vector2(0.5f, 0.5f);
        rect.anchoredPosition = Vector2.zero;
        rect.sizeDelta = Vector2.zero;
        rect.localScale = Vector3.one;
    }

    private static Transform FindChild(Transform root, string name)
    {
        Transform[] children = root.GetComponentsInChildren<Transform>(true);
        for (int i = 0; i < children.Length; i++)
        {
            if (string.Equals(children[i].name, name, StringComparison.Ordinal))
            {
                return children[i];
            }
        }

        return null;
    }

    private static void DestroyDirectChild(Transform parent, string name)
    {
        for (int i = parent.childCount - 1; i >= 0; i--)
        {
            Transform child = parent.GetChild(i);
            if (string.Equals(child.name, name, StringComparison.Ordinal))
            {
                UnityEngine.Object.DestroyImmediate(child.gameObject);
            }
        }
    }

    private static GameObject FindSceneRoot(Scene scene, string name)
    {
        foreach (GameObject root in scene.GetRootGameObjects())
        {
            if (string.Equals(root.name, name, StringComparison.Ordinal))
            {
                return root;
            }
        }

        return null;
    }

    private static GameObject FindDirectChild(Transform parent, string name)
    {
        if (parent == null)
        {
            return null;
        }

        for (int i = 0; i < parent.childCount; i++)
        {
            Transform child = parent.GetChild(i);
            if (string.Equals(child.name, name, StringComparison.Ordinal))
            {
                return child.gameObject;
            }
        }

        return null;
    }

    private static void ConfigureNpcQuestComponents(
        GameObject root,
        QuestDefinition definition,
        string stableInstanceId)
    {
        QuestGiver giver = root.GetComponent<QuestGiver>();
        if (giver == null)
        {
            giver = root.AddComponent<QuestGiver>();
        }

        SerializedObject giverSerialized = new(giver);
        SetString(giverSerialized, "npcId", NpcId);
        SetObject(giverSerialized, "questDefinition", definition);
        giverSerialized.ApplyModifiedPropertiesWithoutUndo();

        QuestTargetIdentity identity = root.GetComponent<QuestTargetIdentity>();
        if (identity == null)
        {
            identity = root.AddComponent<QuestTargetIdentity>();
        }

        SerializedObject identitySerialized = new(identity);
        SetString(identitySerialized, "targetId", NpcId);
        SetString(identitySerialized, "stableInstanceId", stableInstanceId);
        SetInt(identitySerialized, "targetKind", (int)QuestTargetKind.Npc);
        SetObject(identitySerialized, "enemyHealth", null);
        SetBoolIfPresent(identitySerialized, "autoStartCombat", false);
        identitySerialized.ApplyModifiedPropertiesWithoutUndo();
        EditorUtility.SetDirty(giver);
        EditorUtility.SetDirty(identity);
    }

    private static void ValidateNpcSceneInstance(Scene scene, QuestDefinition definition)
    {
        GameObject npc = FindSceneRoot(scene, "NpcPlaceholder");
        if (npc == null)
        {
            throw new InvalidOperationException("NpcPlaceholder scene root is missing.");
        }

        SimpleNpcInteractable interactable = npc.GetComponent<SimpleNpcInteractable>();
        QuestGiver giver = npc.GetComponent<QuestGiver>();
        QuestTargetIdentity identity = npc.GetComponent<QuestTargetIdentity>();
        if (interactable == null || giver == null || identity == null)
        {
            throw new InvalidOperationException(
                "NpcPlaceholder requires SimpleNpcInteractable, QuestGiver, and QuestTargetIdentity.");
        }

        if (!string.Equals(giver.NpcId, NpcId, StringComparison.Ordinal)
            || !ReferenceEquals(giver.QuestDefinition, definition))
        {
            throw new InvalidOperationException("NpcPlaceholder QuestGiver has an invalid NPC ID or quest reference.");
        }

        if (!string.Equals(identity.TargetId, NpcId, StringComparison.Ordinal)
            || !string.Equals(identity.StableInstanceId, NpcSceneStableId, StringComparison.Ordinal)
            || identity.TargetKind != QuestTargetKind.Npc)
        {
            throw new InvalidOperationException("NpcPlaceholder has an invalid quest target or stable instance identity.");
        }
    }

    private static T RequireSingleComponent<T>() where T : UnityEngine.Object
    {
        T[] components = UnityEngine.Object.FindObjectsByType<T>(
            FindObjectsInactive.Include,
            FindObjectsSortMode.None);
        if (components.Length != 1)
        {
            throw new InvalidOperationException($"Expected exactly one {typeof(T).Name}, found {components.Length}.");
        }

        return components[0];
    }

    private static SerializedProperty RequireProperty(SerializedObject serialized, string name)
    {
        SerializedProperty property = serialized.FindProperty(name);
        if (property == null)
        {
            throw new InvalidOperationException(
                $"Serialized field '{name}' is missing on {serialized.targetObject.GetType().Name}.");
        }

        return property;
    }

    private static void SetObject(SerializedObject serialized, string name, UnityEngine.Object value)
    {
        RequireProperty(serialized, name).objectReferenceValue = value;
    }

    private static void SetString(SerializedObject serialized, string name, string value)
    {
        RequireProperty(serialized, name).stringValue = value ?? string.Empty;
    }

    private static void SetInt(SerializedObject serialized, string name, int value)
    {
        RequireProperty(serialized, name).intValue = value;
    }

    private static void SetFloat(SerializedObject serialized, string name, float value)
    {
        RequireProperty(serialized, name).floatValue = value;
    }

    private static void SetBool(SerializedObject serialized, string name, bool value)
    {
        RequireProperty(serialized, name).boolValue = value;
    }

    private static void SetBoolIfPresent(SerializedObject serialized, string name, bool value)
    {
        SerializedProperty property = serialized.FindProperty(name);
        if (property != null)
        {
            property.boolValue = value;
        }
    }
}
#endif
