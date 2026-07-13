#if UNITY_EDITOR
using System;
using System.Collections.Generic;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;
using UnityEngine.SceneManagement;

public static class PersistenceBootstrap
{
    private const string MenuPath = "Tools/Combat Sandbox/Build Persistence Foundation";
    private const string ValidateMenuPath = "Tools/Combat Sandbox/Validate Persistence Foundation";
    private const string ScenePath = "Assets/_Project/Scenes/CombatSandbox.unity";
    private const string RegistryPath = "Assets/_Project/Data/Items/ItemDefinitionRegistry.asset";
    private const string QuestRegistryPath = "Assets/_Project/Data/Quests/QuestDefinitionRegistry.asset";
    private const string SwordPath = "Assets/_Project/Data/Weapons/BronzeSword.asset";
    private const string AxePath = "Assets/_Project/Data/Weapons/BronzeAxe.asset";
    private const string GenericQuestRewardPath = "Assets/_Project/Data/Items/BoneFragment.asset";

    private static readonly (string rootName, string worldId, Type participantType)[] PersistentObjects =
    {
        ("WorldChest", "combat_sandbox.container.world_chest", typeof(WorldItemContainer)),
        ("BronzeSwordPickup", "combat_sandbox.pickup.bronze_sword", typeof(WeaponPickup)),
        ("BronzeAxePickup", "combat_sandbox.pickup.bronze_axe", typeof(WeaponPickup)),
        ("SimpleDoor", "combat_sandbox.door.simple_door", typeof(SimpleDoorInteractable))
    };

    [MenuItem(MenuPath)]
    public static void Build()
    {
        if (EditorApplication.isPlayingOrWillChangePlaymode)
        {
            throw new InvalidOperationException("Exit Play Mode before building the persistence foundation.");
        }

        Scene scene = SceneManager.GetActiveScene();
        if (scene.IsValid() && scene.isDirty)
        {
            throw new InvalidOperationException("Save or discard the active scene changes before building persistence.");
        }

        if (scene.path != ScenePath)
        {
            scene = EditorSceneManager.OpenScene(ScenePath, OpenSceneMode.Single);
        }

        EnsureFolder("Assets/_Project/Data/Items");
        EnsureFolder("Assets/_Project/Data/Quests");
        ItemDefinitionRegistry registry = CreateOrUpdateRegistry();
        QuestDefinitionRegistry questRegistry = CreateOrLoadQuestRegistry();

        SaveGameService service = CreateOrUpdateService(scene, registry, questRegistry);
        ConfigurePersistentObjects(scene);

        EditorSceneManager.MarkSceneDirty(scene);
        EditorSceneManager.SaveScene(scene);
        AssetDatabase.SaveAssets();
        AssetDatabase.Refresh();

        bool registryValid = service.ValidateRegistry(out string registryReport);
        bool worldValid = service.ValidateWorldIds(out string worldReport);
        if (!registryValid || !worldValid)
        {
            throw new InvalidOperationException(
                $"Persistence validation failed. Registry: {registryReport} World: {worldReport}");
        }

        Debug.Log($"Persistence foundation built. {registryReport} {worldReport}");
    }

    [MenuItem(ValidateMenuPath)]
    public static void ValidateCurrent()
    {
        SaveGameService[] services = UnityEngine.Object.FindObjectsByType<SaveGameService>(
            FindObjectsInactive.Include,
            FindObjectsSortMode.None);
        if (services.Length != 1)
        {
            throw new InvalidOperationException($"Expected exactly one SaveGameService, found {services.Length}.");
        }

        bool registryValid = services[0].ValidateRegistry(out string registryReport);
        bool worldValid = services[0].ValidateWorldIds(out string worldReport);
        if (!registryValid || !worldValid)
        {
            throw new InvalidOperationException(
                $"Persistence validation failed. Registry: {registryReport} World: {worldReport} "
                + "Generate a new ID from the PersistentWorldId component context menu when a scene instance was duplicated.");
        }

        Debug.Log($"Persistence validation passed. {registryReport} {worldReport}");
    }

    [MenuItem("CONTEXT/PersistentWorldId/Generate New Stable ID")]
    private static void GenerateNewStableWorldId(MenuCommand command)
    {
        PersistentWorldId persistentId = command.context as PersistentWorldId;
        if (persistentId == null || PrefabUtility.IsPartOfPrefabAsset(persistentId))
        {
            throw new InvalidOperationException("Stable world IDs must be assigned to scene instances, not prefab assets.");
        }

        Undo.RecordObject(persistentId, "Generate Persistent World ID");
        SerializedObject serialized = new(persistentId);
        RequireProperty(serialized, "worldId").stringValue = $"combat_sandbox.world.{Guid.NewGuid():N}";
        serialized.ApplyModifiedProperties();
        EditorUtility.SetDirty(persistentId);
        if (persistentId.gameObject.scene.IsValid())
        {
            EditorSceneManager.MarkSceneDirty(persistentId.gameObject.scene);
        }
    }

    private static ItemDefinitionRegistry CreateOrUpdateRegistry()
    {
        WeaponData sword = AssetDatabase.LoadAssetAtPath<WeaponData>(SwordPath);
        WeaponData axe = AssetDatabase.LoadAssetAtPath<WeaponData>(AxePath);
        if (sword == null || axe == null)
        {
            throw new InvalidOperationException("Bronze Sword and Bronze Axe definitions are required.");
        }

        ItemDefinitionRegistry registry = AssetDatabase.LoadAssetAtPath<ItemDefinitionRegistry>(RegistryPath);
        if (registry == null)
        {
            registry = ScriptableObject.CreateInstance<ItemDefinitionRegistry>();
            AssetDatabase.CreateAsset(registry, RegistryPath);
        }

        SerializedObject serialized = new(registry);
        SerializedProperty definitions = RequireProperty(serialized, "definitions");
        List<ItemDefinition> preservedDefinitions = new();
        HashSet<ItemDefinition> seenDefinitions = new();
        for (int i = 0; i < definitions.arraySize; i++)
        {
            ItemDefinition definition = definitions.GetArrayElementAtIndex(i).objectReferenceValue as ItemDefinition;
            if (definition != null && seenDefinitions.Add(definition))
            {
                preservedDefinitions.Add(definition);
            }
        }

        AddDefinitionIfMissing(preservedDefinitions, seenDefinitions, sword);
        AddDefinitionIfMissing(preservedDefinitions, seenDefinitions, axe);
        ItemDefinition genericQuestReward = AssetDatabase.LoadAssetAtPath<ItemDefinition>(GenericQuestRewardPath);
        AddDefinitionIfMissing(preservedDefinitions, seenDefinitions, genericQuestReward);

        definitions.arraySize = preservedDefinitions.Count;
        for (int i = 0; i < preservedDefinitions.Count; i++)
        {
            definitions.GetArrayElementAtIndex(i).objectReferenceValue = preservedDefinitions[i];
        }
        serialized.ApplyModifiedPropertiesWithoutUndo();
        EditorUtility.SetDirty(registry);
        return registry;
    }

    private static QuestDefinitionRegistry CreateOrLoadQuestRegistry()
    {
        QuestDefinitionRegistry registry = AssetDatabase.LoadAssetAtPath<QuestDefinitionRegistry>(QuestRegistryPath);
        if (registry == null)
        {
            registry = ScriptableObject.CreateInstance<QuestDefinitionRegistry>();
            AssetDatabase.CreateAsset(registry, QuestRegistryPath);
        }

        if (!registry.Validate(out string error))
        {
            throw new InvalidOperationException($"Quest definition registry is invalid: {error}");
        }

        return registry;
    }

    private static SaveGameService CreateOrUpdateService(
        Scene scene,
        ItemDefinitionRegistry registry,
        QuestDefinitionRegistry questRegistry)
    {
        SaveGameService[] services = UnityEngine.Object.FindObjectsByType<SaveGameService>(
            FindObjectsInactive.Include,
            FindObjectsSortMode.None);
        if (services.Length > 1)
        {
            throw new InvalidOperationException($"Expected at most one SaveGameService, found {services.Length}.");
        }

        SaveGameService service;
        if (services.Length == 0)
        {
            GameObject root = new("SaveGameService");
            SceneManager.MoveGameObjectToScene(root, scene);
            service = root.AddComponent<SaveGameService>();
        }
        else
        {
            service = services[0];
            service.gameObject.name = "SaveGameService";
        }

        SerializedObject serialized = new(service);
        RequireProperty(serialized, "itemDefinitionRegistry").objectReferenceValue = registry;
        RequireProperty(serialized, "questDefinitionRegistry").objectReferenceValue = questRegistry;
        RequireProperty(serialized, "autoLoadOnStart").boolValue = false;
        serialized.ApplyModifiedPropertiesWithoutUndo();
        EditorUtility.SetDirty(service);

        PlayerQuestLog[] questLogs = UnityEngine.Object.FindObjectsByType<PlayerQuestLog>(
            FindObjectsInactive.Include,
            FindObjectsSortMode.None);
        if (questLogs.Length > 1)
        {
            throw new InvalidOperationException($"Expected at most one PlayerQuestLog, found {questLogs.Length}.");
        }

        PlayerQuestLog questLog;
        if (questLogs.Length == 0)
        {
            PlayerInventory[] inventories = UnityEngine.Object.FindObjectsByType<PlayerInventory>(
                FindObjectsInactive.Include,
                FindObjectsSortMode.None);
            if (inventories.Length != 1)
            {
                throw new InvalidOperationException(
                    $"Expected exactly one PlayerInventory when creating PlayerQuestLog, found {inventories.Length}.");
            }

            questLog = inventories[0].gameObject.AddComponent<PlayerQuestLog>();
        }
        else
        {
            questLog = questLogs[0];
        }

        SerializedObject questLogSerialized = new(questLog);
        RequireProperty(questLogSerialized, "registry").objectReferenceValue = questRegistry;
        RequireProperty(questLogSerialized, "inventory").objectReferenceValue = questLog.GetComponent<PlayerInventory>();
        RequireProperty(questLogSerialized, "interactor").objectReferenceValue = questLog.GetComponent<PlayerInteractor>();
        questLogSerialized.ApplyModifiedPropertiesWithoutUndo();
        EditorUtility.SetDirty(questLog);
        return service;
    }

    private static void AddDefinitionIfMissing(
        List<ItemDefinition> definitions,
        HashSet<ItemDefinition> seenDefinitions,
        ItemDefinition definition)
    {
        if (definition != null && seenDefinitions.Add(definition))
        {
            definitions.Add(definition);
        }
    }

    private static void ConfigurePersistentObjects(Scene scene)
    {
        HashSet<string> ids = new(StringComparer.Ordinal);
        for (int i = 0; i < PersistentObjects.Length; i++)
        {
            (string rootName, string worldId, Type participantType) entry = PersistentObjects[i];
            if (!ids.Add(entry.worldId))
            {
                throw new InvalidOperationException($"Bootstrap contains duplicate world ID '{entry.worldId}'.");
            }

            GameObject root = FindSceneRoot(scene, entry.rootName);
            if (root == null || root.GetComponent(entry.participantType) == null)
            {
                throw new InvalidOperationException(
                    $"Scene root '{entry.rootName}' is missing required {entry.participantType.Name}.");
            }

            PersistentWorldId persistentId = root.GetComponent<PersistentWorldId>();
            if (persistentId == null)
            {
                persistentId = root.AddComponent<PersistentWorldId>();
            }

            SerializedObject serialized = new(persistentId);
            RequireProperty(serialized, "worldId").stringValue = entry.worldId;
            serialized.ApplyModifiedPropertiesWithoutUndo();
            EditorUtility.SetDirty(persistentId);
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

    private static SerializedProperty RequireProperty(SerializedObject serialized, string name)
    {
        SerializedProperty property = serialized.FindProperty(name);
        if (property == null)
        {
            throw new InvalidOperationException(
                $"Serialized field '{name}' was not found on {serialized.targetObject.GetType().Name}.");
        }

        return property;
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
}
#endif
