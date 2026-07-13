using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Text;
using UnityEngine;
using UnityEngine.SceneManagement;
#if UNITY_EDITOR
using UnityEditor;
#endif

[DisallowMultipleComponent]
public sealed class SaveGameService : MonoBehaviour
{
    public const int CurrentSchemaVersion = 2;
    public const string SaveFileName = "combat-sandbox-save.json";
    public const string TempFileName = "combat-sandbox-save.tmp";
    public const string BackupFileName = "combat-sandbox-save.bak";
    public const string DefaultSanctuaryId = "combat_sandbox.sanctuary.default";

    [SerializeField] private ItemDefinitionRegistry itemDefinitionRegistry;
    [SerializeField] private QuestDefinitionRegistry questDefinitionRegistry;
    [SerializeField] private bool autoLoadOnStart;

    public static SaveGameService Instance { get; private set; }

    public string SavePath => Path.Combine(Application.persistentDataPath, SaveFileName);
    public string TempPath => Path.Combine(Application.persistentDataPath, TempFileName);
    public string BackupPath => Path.Combine(Application.persistentDataPath, BackupFileName);
    public string SaveDirectory => Application.persistentDataPath;
    public bool SaveExists => File.Exists(SavePath);
    public bool BackupExists => File.Exists(BackupPath);
    public int SchemaVersion => CurrentSchemaVersion;
    public string LastOperation { get; private set; } = "None";
    public string LastStatus { get; private set; } = "No persistence operation has run.";
    public string LastError { get; private set; } = string.Empty;
    public string LastLoadSource { get; private set; } = "None";
    public string LastSavedAtUtc { get; private set; } = string.Empty;
    public string CurrentSanctuaryId { get; private set; } = DefaultSanctuaryId;
    public string SaveUpdatedAtUtc
    {
        get
        {
            try
            {
                return SaveExists
                    ? File.GetLastWriteTimeUtc(SavePath).ToString("O", CultureInfo.InvariantCulture)
                    : "None";
            }
            catch
            {
                return "Unavailable";
            }
        }
    }

    private sealed class PlayerBindings
    {
        public Transform transform;
        public PlayerInventory inventory;
        public PlayerEquipment equipment;
        public PlayerHealth health;
        public PlayerStamina stamina;
        public CharacterController characterController;
        public PlayerQuestLog questLog;
    }

    private sealed class WorldBindings
    {
        public readonly Dictionary<string, WorldItemContainer> containers = new(StringComparer.Ordinal);
        public readonly Dictionary<string, WeaponPickup> pickups = new(StringComparer.Ordinal);
        public readonly Dictionary<string, SimpleDoorInteractable> doors = new(StringComparer.Ordinal);
    }

    private sealed class RestorePlan
    {
        public Vector3 playerPosition;
        public Quaternion playerRotation;
        public int playerHealth;
        public float playerStamina;
        public string lastSanctuaryId;
        public readonly List<ItemInstance> playerItems = new();
        public readonly Dictionary<EquipmentSlotType, string> equipmentAssignments = new();
        public EquipmentSlotType activeWeaponSlot;
        public string activeItemInstanceId;
        public readonly Dictionary<string, List<ItemInstance>> containerItems = new(StringComparer.Ordinal);
        public readonly Dictionary<string, bool> pickupStates = new(StringComparer.Ordinal);
        public readonly Dictionary<string, bool> doorStates = new(StringComparer.Ordinal);
        public QuestRestorePlan questLogRestore;
    }

    private void Awake()
    {
        if (Instance != null && Instance != this)
        {
            Debug.Log("[SaveGame] Duplicate SaveGameService disabled.");
            enabled = false;
            return;
        }

        Instance = this;
    }

    private void Start()
    {
        if (autoLoadOnStart)
        {
            LoadGame();
        }
    }

    private void OnDestroy()
    {
        if (Instance == this)
        {
            Instance = null;
        }
    }

    public bool SaveGame()
    {
        LastOperation = "Save";
        LastLoadSource = "None";
        LastError = string.Empty;

        if (!TryCaptureSnapshot(out SaveGameData snapshot, out string captureError))
        {
            return Fail("Save", captureError);
        }

        string json;
        try
        {
            json = JsonUtility.ToJson(snapshot, prettyPrint: true);
        }
        catch (Exception exception)
        {
            return Fail("Save", $"Could not serialize the snapshot: {exception.Message}");
        }

        if (string.IsNullOrWhiteSpace(json))
        {
            return Fail("Save", "Serialization produced an empty document.");
        }

        try
        {
            Directory.CreateDirectory(SaveDirectory);
            if (File.Exists(TempPath))
            {
                File.Delete(TempPath);
            }

            using (FileStream stream = new(TempPath, FileMode.CreateNew, FileAccess.Write, FileShare.None))
            using (StreamWriter writer = new(stream, new UTF8Encoding(encoderShouldEmitUTF8Identifier: false)))
            {
                writer.Write(json);
                writer.Flush();
                stream.Flush(flushToDisk: true);
            }

            if (!TryReadAndValidateFile(TempPath, out _, out _, out string tempValidationError))
            {
                File.Delete(TempPath);
                return Fail("Save", $"Temporary save validation failed: {tempValidationError}");
            }

            if (File.Exists(SavePath))
            {
                File.Replace(TempPath, SavePath, BackupPath, ignoreMetadataErrors: true);
            }
            else
            {
                File.Move(TempPath, SavePath);
            }
        }
        catch (Exception exception)
        {
            TryDeleteTempFile();
            return Fail("Save", $"Atomic save write failed: {exception.Message}");
        }

        LastSavedAtUtc = snapshot.savedAtUtc;
        LastStatus = $"Saved schema v{snapshot.schemaVersion} at {snapshot.savedAtUtc}.";
        Debug.Log($"[SaveGame] {LastStatus} Path: {SavePath}");
        return true;
    }

    public bool LoadGame()
    {
        LastOperation = "Load";
        LastLoadSource = "None";
        LastError = string.Empty;

        bool loadedBackup = false;
        string mainError = string.Empty;
        if (!TryReadAndValidateFile(SavePath, out SaveGameData data, out RestorePlan plan, out mainError))
        {
            if (!TryReadAndValidateFile(BackupPath, out data, out plan, out string backupError))
            {
                return Fail(
                    "Load",
                    $"Main save failed validation: {mainError} Backup failed validation: {backupError}");
            }

            loadedBackup = true;
        }

        if (!TryResolveBindings(out PlayerBindings player, out WorldBindings world, out string bindingError))
        {
            return Fail("Load", bindingError);
        }

        if (!TryCaptureSnapshot(out SaveGameData rollbackData, out string rollbackCaptureError))
        {
            return Fail("Load", $"Could not prepare rollback state: {rollbackCaptureError}");
        }

        if (!TryBuildRestorePlan(
                rollbackData,
                player,
                world,
                out RestorePlan rollbackPlan,
                out string rollbackPlanError))
        {
            return Fail("Load", $"Could not prepare rollback state: {rollbackPlanError}");
        }

        bool applySucceeded;
        string applyError;
        try
        {
            PrepareForLoad();
            applySucceeded = TryApplyRestorePlan(plan, player, world, out applyError);
        }
        catch (Exception exception)
        {
            applySucceeded = false;
            applyError = $"Unexpected exception during apply: {exception.GetType().Name}: {exception.Message}";
        }

        if (!applySucceeded)
        {
            bool rollbackSucceeded;
            string rollbackError;
            try
            {
                rollbackSucceeded = TryApplyRestorePlan(rollbackPlan, player, world, out rollbackError);
            }
            catch (Exception exception)
            {
                rollbackSucceeded = false;
                rollbackError = $"Unexpected rollback exception: {exception.GetType().Name}: {exception.Message}";
            }

            string rollbackMessage = rollbackSucceeded
                ? "The pre-load state was restored."
                : $"Rollback also failed: {rollbackError}";
            return Fail("Load", $"Applying the validated save failed: {applyError} {rollbackMessage}");
        }

        LastLoadSource = loadedBackup ? "Backup" : "Main";
        LastSavedAtUtc = data.savedAtUtc ?? string.Empty;
        LastStatus = loadedBackup
            ? $"Loaded schema v{data.schemaVersion} from backup because the main save was invalid: {mainError}"
            : $"Loaded schema v{data.schemaVersion} from the main save.";
        Debug.Log($"[SaveGame] {LastStatus}");
        return true;
    }

    public bool DeleteSave()
    {
        LastOperation = "Delete";
        LastError = string.Empty;
        LastLoadSource = "None";

        try
        {
            DeleteIfPresent(SavePath);
            DeleteIfPresent(TempPath);
            DeleteIfPresent(BackupPath);
        }
        catch (Exception exception)
        {
            return Fail("Delete", $"Could not delete persistence files: {exception.Message}");
        }

        LastSavedAtUtc = string.Empty;
        LastStatus = "Deleted main, temporary, and backup save files.";
        Debug.Log($"[SaveGame] {LastStatus}");
        return true;
    }

    public void ShowSaveFolder()
    {
        Directory.CreateDirectory(SaveDirectory);
#if UNITY_EDITOR
        EditorUtility.RevealInFinder(SaveDirectory);
#else
        Application.OpenURL("file:///" + SaveDirectory.Replace('\\', '/'));
#endif
        LastOperation = "Show Folder";
        LastError = string.Empty;
        LastStatus = $"Opened save folder: {SaveDirectory}";
    }

    public bool PrintSummary()
    {
        LastOperation = "Print Summary";
        LastError = string.Empty;
        bool usedBackup = false;
        if (!TryReadAndValidateFile(SavePath, out SaveGameData data, out _, out string mainError))
        {
            if (!TryReadAndValidateFile(BackupPath, out data, out _, out string backupError))
            {
                return Fail("Print Summary", $"Main: {mainError} Backup: {backupError}");
            }

            usedBackup = true;
        }

        int containerItemCount = 0;
        for (int i = 0; i < data.world.containers.Count; i++)
        {
            containerItemCount += data.world.containers[i].items.Count;
        }

        LastLoadSource = usedBackup ? "Backup" : "Main";
        string trackedQuestId = string.IsNullOrWhiteSpace(data.questLog.trackedQuestId)
            ? "none"
            : data.questLog.trackedQuestId;
        LastStatus = $"Schema v{data.schemaVersion}; saved {data.savedAtUtc}; "
            + $"player items {data.player.inventoryItems.Count}; equipped slots {CountEquipped(data.player.equipmentSlots)}; "
            + $"containers {data.world.containers.Count} ({containerItemCount} items); "
            + $"pickups {data.world.pickups.Count}; doors {data.world.doors.Count}; "
            + $"quests {data.questLog.quests.Count}; tracked quest {trackedQuestId}; source {LastLoadSource}.";
        Debug.Log($"[SaveGame] {LastStatus}");
        return true;
    }

    public bool ValidateRegistry(out string report)
    {
        if (itemDefinitionRegistry == null)
        {
            report = "ItemDefinitionRegistry reference is missing.";
            return false;
        }

        if (!itemDefinitionRegistry.Validate(out string error))
        {
            report = error;
            return false;
        }

        if (!ValidateQuestRegistry(out string questReport))
        {
            report = questReport;
            return false;
        }

        report = $"Registries valid: {itemDefinitionRegistry.Count} unique item definitions; {questReport}";
        return true;
    }

    public bool ValidateQuestRegistry(out string report)
    {
        if (itemDefinitionRegistry == null)
        {
            report = "ItemDefinitionRegistry reference is missing.";
            return false;
        }

        if (!itemDefinitionRegistry.Validate(out string itemError))
        {
            report = itemError;
            return false;
        }

        if (questDefinitionRegistry == null)
        {
            report = "QuestDefinitionRegistry reference is missing.";
            return false;
        }

        if (!questDefinitionRegistry.Validate(out string error))
        {
            report = error;
            return false;
        }

        for (int i = 0; i < questDefinitionRegistry.Definitions.Count; i++)
        {
            QuestDefinition quest = questDefinitionRegistry.Definitions[i];
            ItemDefinition reward = quest != null ? quest.RewardItem : null;
            if (reward != null
                && (!itemDefinitionRegistry.TryResolve(reward.DefinitionId, out ItemDefinition registeredReward)
                    || !ReferenceEquals(registeredReward, reward)))
            {
                report = $"Quest '{quest.QuestId}' reward '{reward.DefinitionId}' is not registered consistently.";
                return false;
            }
        }

        report = $"{questDefinitionRegistry.Count} unique quest definitions";
        return true;
    }

    public bool ValidateWorldIds(out string report)
    {
        if (!TryBuildWorldBindings(out WorldBindings world, out string error))
        {
            report = error;
            return false;
        }

        report = $"World IDs valid: {world.containers.Count} containers, "
            + $"{world.pickups.Count} pickups, {world.doors.Count} doors.";
        return true;
    }

    public bool TryGetCurrentCounts(
        out int playerItemCount,
        out int containerCount,
        out int persistentObjectCount,
        out string error)
    {
        playerItemCount = 0;
        containerCount = 0;
        persistentObjectCount = 0;
        if (!TryResolveBindings(out PlayerBindings player, out WorldBindings world, out error))
        {
            return false;
        }

        playerItemCount = player.inventory.ItemCount;
        containerCount = world.containers.Count;
        persistentObjectCount = world.containers.Count + world.pickups.Count + world.doors.Count;
        return true;
    }

    public bool TryGetCurrentQuestCounts(
        out int questCount,
        out string trackedQuestId,
        out string error)
    {
        questCount = 0;
        trackedQuestId = string.Empty;
        if (!TryResolveBindings(out PlayerBindings player, out _, out error))
        {
            return false;
        }

        QuestLogSaveData snapshot = new();
        if (!TryCaptureQuestLog(player.questLog, snapshot, out error))
        {
            return false;
        }

        questCount = snapshot.quests.Count;
        trackedQuestId = snapshot.trackedQuestId ?? string.Empty;
        return true;
    }

    private bool TryCaptureSnapshot(out SaveGameData data, out string error)
    {
        data = null;
        if (!TryResolveBindings(out PlayerBindings player, out WorldBindings world, out error))
        {
            return false;
        }

        SaveGameData snapshot = new()
        {
            schemaVersion = CurrentSchemaVersion,
            savedAtUtc = DateTimeOffset.UtcNow.ToString("O", CultureInfo.InvariantCulture),
            sceneName = SceneManager.GetActiveScene().name
        };
        snapshot.player.position = FromVector3(player.transform.position);
        snapshot.player.rotation = FromQuaternion(player.transform.rotation);
        snapshot.player.health = Mathf.Clamp(player.health.CurrentHealth, 1, player.health.MaxHealth);
        snapshot.player.stamina = Mathf.Clamp(player.stamina.CurrentStamina, 0f, player.stamina.MaxStamina);
        snapshot.player.lastSanctuaryId = string.IsNullOrWhiteSpace(CurrentSanctuaryId)
            ? DefaultSanctuaryId
            : CurrentSanctuaryId;

        HashSet<string> globalInstanceIds = new(StringComparer.Ordinal);
        if (!TryCaptureItems(player.inventory.Items, snapshot.player.inventoryItems, globalInstanceIds, out error))
        {
            return false;
        }

        HashSet<EquipmentSlotType> capturedSlots = new();
        foreach (EquipmentSlotState slot in player.equipment.Slots)
        {
            if (slot == null
                || !Enum.IsDefined(typeof(EquipmentSlotType), slot.SlotType)
                || !capturedSlots.Add(slot.SlotType))
            {
                error = "Player equipment contains a missing or duplicate slot state.";
                return false;
            }

            snapshot.player.equipmentSlots.Add(new EquipmentSlotSaveData
            {
                slotType = (int)slot.SlotType,
                itemInstanceId = slot.ItemInstanceId ?? string.Empty
            });
        }

        snapshot.player.activeWeaponSlot = (int)player.equipment.ActiveSlot;
        snapshot.player.activeItemInstanceId = player.equipment.EquippedItem?.InstanceId ?? string.Empty;

        List<string> containerIds = SortedKeys(world.containers);
        for (int i = 0; i < containerIds.Count; i++)
        {
            string worldId = containerIds[i];
            WorldItemContainer container = world.containers[worldId];
            container.EnsureInitializedForPersistence();
            WorldContainerSaveData containerData = new()
            {
                worldId = worldId,
                initializedOrRestored = container.IsInitialized || container.HasRestoredState
            };
            if (!TryCaptureItems(container.Items, containerData.items, globalInstanceIds, out error))
            {
                return false;
            }

            snapshot.world.containers.Add(containerData);
        }

        List<string> pickupIds = SortedKeys(world.pickups);
        for (int i = 0; i < pickupIds.Count; i++)
        {
            string worldId = pickupIds[i];
            snapshot.world.pickups.Add(new WorldPickupSaveData
            {
                worldId = worldId,
                collected = world.pickups[worldId].IsCollected
            });
        }

        List<string> doorIds = SortedKeys(world.doors);
        for (int i = 0; i < doorIds.Count; i++)
        {
            string worldId = doorIds[i];
            snapshot.world.doors.Add(new WorldDoorSaveData
            {
                worldId = worldId,
                isOpen = world.doors[worldId].IsOpen
            });
        }

        if (!TryCaptureQuestLog(player.questLog, snapshot.questLog, out error))
        {
            return false;
        }

        if (!TryBuildRestorePlan(snapshot, player, world, out _, out error))
        {
            return false;
        }

        data = snapshot;
        return true;
    }

    private static bool TryCaptureQuestLog(
        PlayerQuestLog questLog,
        QuestLogSaveData destination,
        out string error)
    {
        error = string.Empty;
        if (questLog == null || destination == null)
        {
            error = "Quest log capture received missing runtime data.";
            return false;
        }

        QuestLogSnapshot snapshot;
        try
        {
            snapshot = questLog.CaptureSnapshot();
        }
        catch (Exception exception)
        {
            error = $"Could not capture the quest log: {exception.GetType().Name}: {exception.Message}";
            return false;
        }

        if (snapshot == null || snapshot.quests == null)
        {
            error = "Player quest log produced an invalid snapshot.";
            return false;
        }

        destination.trackedQuestId = snapshot.trackedQuestId ?? string.Empty;
        destination.quests.Clear();
        for (int i = 0; i < snapshot.quests.Count; i++)
        {
            QuestProgressSnapshot source = snapshot.quests[i];
            if (source == null || source.objectives == null)
            {
                error = $"Quest snapshot record {i} is missing required data.";
                return false;
            }

            QuestProgressSaveData questData = new()
            {
                questId = source.questId ?? string.Empty,
                state = source.state,
                currentObjectiveId = source.currentObjectiveId ?? string.Empty,
                rewardStatus = source.rewardStatus,
                rewardItemInstanceId = source.rewardItemInstanceId ?? string.Empty,
                rewardDefinitionId = source.rewardDefinitionId ?? string.Empty,
                rewardQuantity = source.rewardQuantity
            };
            for (int objectiveIndex = 0; objectiveIndex < source.objectives.Count; objectiveIndex++)
            {
                QuestObjectiveProgressSnapshot objective = source.objectives[objectiveIndex];
                if (objective == null || objective.creditedTargetInstanceIds == null)
                {
                    error = $"Quest '{questData.questId}' objective snapshot {objectiveIndex} is missing.";
                    return false;
                }

                questData.objectives.Add(new QuestObjectiveProgressSaveData
                {
                    objectiveId = objective.objectiveId ?? string.Empty,
                    progress = objective.progress,
                    creditedTargetInstanceIds = new List<string>(objective.creditedTargetInstanceIds)
                });
            }

            destination.quests.Add(questData);
        }

        return true;
    }

    private bool TryCaptureItems(
        IReadOnlyList<ItemInstance> source,
        List<ItemInstanceSaveData> destination,
        HashSet<string> globalInstanceIds,
        out string error)
    {
        error = string.Empty;
        if (source == null || destination == null)
        {
            error = "An item collection is missing.";
            return false;
        }

        for (int i = 0; i < source.Count; i++)
        {
            ItemInstance item = source[i];
            if (item == null || !item.IsValid || item.Definition == null)
            {
                error = $"Item at index {i} is invalid.";
                return false;
            }

            if (!globalInstanceIds.Add(item.InstanceId))
            {
                error = $"Duplicate global ItemInstance ID '{item.InstanceId}'.";
                return false;
            }

            if (!itemDefinitionRegistry.TryResolve(item.Definition.DefinitionId, out ItemDefinition registeredDefinition)
                || !ReferenceEquals(registeredDefinition, item.Definition))
            {
                error = $"Item definition '{item.Definition.DefinitionId}' is not registered consistently.";
                return false;
            }

            destination.Add(new ItemInstanceSaveData
            {
                instanceId = item.InstanceId,
                definitionId = item.Definition.DefinitionId,
                quantity = item.Quantity,
                currentDurability = item.CurrentDurability,
                maxDurability = item.MaxDurability,
                quality = (int)item.Quality,
                rarity = (int)item.Rarity,
                creatorId = item.CreatorId ?? string.Empty,
                affinityId = item.AffinityId ?? string.Empty,
                isStolen = item.IsStolen,
                originalOwnerId = item.OriginalOwnerId ?? string.Empty
            });
        }

        return true;
    }

    private bool TryReadAndValidateFile(
        string path,
        out SaveGameData data,
        out RestorePlan plan,
        out string error)
    {
        data = null;
        plan = null;
        error = string.Empty;
        if (!File.Exists(path))
        {
            error = $"File not found: {path}";
            return false;
        }

        string json;
        try
        {
            json = File.ReadAllText(path, Encoding.UTF8);
        }
        catch (Exception exception)
        {
            error = $"Could not read '{path}': {exception.Message}";
            return false;
        }

        if (string.IsNullOrWhiteSpace(json))
        {
            error = $"File '{path}' is empty.";
            return false;
        }

        SaveGameData parsed;
        try
        {
            parsed = JsonUtility.FromJson<SaveGameData>(json);
        }
        catch (Exception exception)
        {
            error = $"Invalid JSON in '{path}': {exception.Message}";
            return false;
        }

        if (parsed == null)
        {
            error = $"JSON in '{path}' did not produce a save document.";
            return false;
        }

        if (!TryResolveBindings(out PlayerBindings player, out WorldBindings world, out error)
            || !TryBuildRestorePlan(parsed, player, world, out RestorePlan builtPlan, out error))
        {
            return false;
        }

        data = parsed;
        plan = builtPlan;
        return true;
    }

    private bool TryBuildRestorePlan(
        SaveGameData data,
        PlayerBindings player,
        WorldBindings world,
        out RestorePlan plan,
        out string error)
    {
        plan = null;
        error = string.Empty;
        if (data == null || player == null || world == null)
        {
            error = "Save validation received missing runtime data.";
            return false;
        }

        if (!TryMigrateToCurrentSchema(data, out error))
        {
            return false;
        }

        if (!ValidateRegistry(out error))
        {
            return false;
        }

        if (string.IsNullOrWhiteSpace(data.savedAtUtc)
            || !DateTimeOffset.TryParse(data.savedAtUtc, CultureInfo.InvariantCulture, DateTimeStyles.RoundtripKind, out _))
        {
            error = "Save timestamp is missing or invalid.";
            return false;
        }

        string activeSceneName = SceneManager.GetActiveScene().name;
        if (!string.Equals(data.sceneName, activeSceneName, StringComparison.Ordinal))
        {
            error = $"Save scene '{data.sceneName}' does not match active scene '{activeSceneName}'.";
            return false;
        }

        if (data.player == null
            || data.player.position == null
            || data.player.rotation == null
            || data.player.inventoryItems == null
            || data.player.equipmentSlots == null
            || data.world == null
            || data.world.containers == null
            || data.world.pickups == null
            || data.world.doors == null
            || data.questLog == null
            || data.questLog.quests == null)
        {
            error = "Save is missing one or more required sections.";
            return false;
        }

        if (!TryReadVector3(data.player.position, out Vector3 position)
            || !TryReadQuaternion(data.player.rotation, out Quaternion rotation))
        {
            error = "Player transform contains invalid numeric values.";
            return false;
        }

        if (data.player.health < 1 || data.player.health > player.health.MaxHealth)
        {
            error = $"Player health {data.player.health} is outside 1..{player.health.MaxHealth}.";
            return false;
        }

        if (!IsFinite(data.player.stamina)
            || data.player.stamina < 0f
            || data.player.stamina > player.stamina.MaxStamina)
        {
            error = $"Player stamina {data.player.stamina} is outside 0..{player.stamina.MaxStamina}.";
            return false;
        }

        if (string.IsNullOrWhiteSpace(data.player.lastSanctuaryId)
            || !string.Equals(
                data.player.lastSanctuaryId,
                data.player.lastSanctuaryId.Trim(),
                StringComparison.Ordinal))
        {
            error = "Player lastSanctuaryId is empty or untrimmed.";
            return false;
        }

        if (player.inventory.Capacity > 0
            && data.player.inventoryItems.Count > player.inventory.Capacity)
        {
            error = $"Player inventory exceeds capacity {player.inventory.Capacity}.";
            return false;
        }

        RestorePlan nextPlan = new()
        {
            playerPosition = position,
            playerRotation = rotation,
            playerHealth = data.player.health,
            playerStamina = data.player.stamina,
            lastSanctuaryId = data.player.lastSanctuaryId,
            activeItemInstanceId = data.player.activeItemInstanceId ?? string.Empty
        };

        HashSet<string> globalInstanceIds = new(StringComparer.Ordinal);
        Dictionary<string, ItemInstance> playerItemsById = new(StringComparer.Ordinal);
        if (!TryBuildItems(
                data.player.inventoryItems,
                nextPlan.playerItems,
                playerItemsById,
                globalInstanceIds,
                "player inventory",
                out error))
        {
            return false;
        }

        Array slotValues = Enum.GetValues(typeof(EquipmentSlotType));
        if (data.player.equipmentSlots.Count != slotValues.Length)
        {
            error = $"Equipment section must contain exactly {slotValues.Length} slot records.";
            return false;
        }

        HashSet<EquipmentSlotType> seenSlots = new();
        HashSet<string> equippedIds = new(StringComparer.Ordinal);
        for (int i = 0; i < data.player.equipmentSlots.Count; i++)
        {
            EquipmentSlotSaveData slotData = data.player.equipmentSlots[i];
            if (slotData == null || !Enum.IsDefined(typeof(EquipmentSlotType), slotData.slotType))
            {
                error = $"Equipment slot record {i} is missing or unknown.";
                return false;
            }

            EquipmentSlotType slotType = (EquipmentSlotType)slotData.slotType;
            if (!seenSlots.Add(slotType))
            {
                error = $"Equipment slot '{slotType}' appears more than once.";
                return false;
            }

            string instanceId = slotData.itemInstanceId ?? string.Empty;
            nextPlan.equipmentAssignments.Add(slotType, instanceId);
            if (string.IsNullOrWhiteSpace(instanceId))
            {
                continue;
            }

            if (!equippedIds.Add(instanceId))
            {
                error = $"Item '{instanceId}' occupies more than one equipment slot.";
                return false;
            }

            if (!playerItemsById.TryGetValue(instanceId, out ItemInstance item))
            {
                error = $"Equipment references item '{instanceId}' outside the player inventory.";
                return false;
            }

            if (!item.Definition.AcceptsSlot(slotType))
            {
                error = $"Item '{instanceId}' is incompatible with equipment slot '{slotType}'.";
                return false;
            }

            if ((slotType == EquipmentSlotType.MainHand || slotType == EquipmentSlotType.OffHand)
                && (item.Definition is not WeaponData weaponData || weaponData.equippedPrefab == null))
            {
                error = $"Equipped weapon '{instanceId}' has no visual prefab.";
                return false;
            }
        }

        if (!Enum.IsDefined(typeof(EquipmentSlotType), data.player.activeWeaponSlot))
        {
            error = $"Active weapon slot value '{data.player.activeWeaponSlot}' is unknown.";
            return false;
        }

        nextPlan.activeWeaponSlot = (EquipmentSlotType)data.player.activeWeaponSlot;
        if (nextPlan.activeWeaponSlot != EquipmentSlotType.MainHand
            && nextPlan.activeWeaponSlot != EquipmentSlotType.OffHand)
        {
            error = "Active weapon slot must be MainHand or OffHand.";
            return false;
        }

        string activeSlotItemId = nextPlan.equipmentAssignments[nextPlan.activeWeaponSlot];
        if (!string.Equals(activeSlotItemId, nextPlan.activeItemInstanceId, StringComparison.Ordinal))
        {
            error = "activeItemInstanceId does not match the item assigned to the active weapon slot.";
            return false;
        }

        if (!TryBuildContainerPlans(data.world.containers, world, nextPlan, globalInstanceIds, out error)
            || !TryBuildPickupPlans(data.world.pickups, world, nextPlan, out error)
            || !TryBuildDoorPlans(data.world.doors, world, nextPlan, out error))
        {
            return false;
        }

        if (!TryBuildQuestRestorePlan(
                data.questLog,
                player.questLog,
                playerItemsById,
                globalInstanceIds,
                out nextPlan.questLogRestore,
                out error))
        {
            return false;
        }

        if (!IsPlayerPositionSafe(
                player,
                world,
                nextPlan.doorStates,
                position,
                rotation,
                out error))
        {
            return false;
        }

        plan = nextPlan;
        return true;
    }

    private static bool TryBuildQuestRestorePlan(
        QuestLogSaveData source,
        PlayerQuestLog questLog,
        IReadOnlyDictionary<string, ItemInstance> plannedPlayerItems,
        HashSet<string> allPlannedItemIds,
        out QuestRestorePlan preparedRestore,
        out string error)
    {
        preparedRestore = null;
        error = string.Empty;
        if (questLog == null)
        {
            error = "Player quest log binding is missing.";
            return false;
        }

        if (!TryMapQuestSnapshot(source, out QuestLogSnapshot snapshot, out error))
        {
            return false;
        }

        if (!questLog.TryPrepareRestore(snapshot, out preparedRestore, out error))
        {
            error = $"Quest log validation failed: {error}";
            return false;
        }

        HashSet<string> rewardItemIds = new(StringComparer.Ordinal);
        for (int i = 0; i < snapshot.quests.Count; i++)
        {
            QuestProgressSnapshot quest = snapshot.quests[i];
            string rewardItemId = quest.rewardItemInstanceId ?? string.Empty;
            if (string.IsNullOrEmpty(rewardItemId))
            {
                continue;
            }

            if (!rewardItemIds.Add(rewardItemId))
            {
                error = $"Quest reward item reservation '{rewardItemId}' appears more than once.";
                return false;
            }

            QuestRewardStatus rewardStatus = (QuestRewardStatus)quest.rewardStatus;
            if (rewardStatus != QuestRewardStatus.None
                && (!string.Equals(rewardItemId, rewardItemId.Trim(), StringComparison.Ordinal)
                    || !string.Equals(
                        quest.rewardDefinitionId,
                        quest.rewardDefinitionId.Trim(),
                        StringComparison.Ordinal)))
            {
                error = $"Quest '{quest.questId}' has an untrimmed reward reservation.";
                return false;
            }

            if (rewardStatus == QuestRewardStatus.PendingDelivery
                && allPlannedItemIds.Contains(rewardItemId)
                && !plannedPlayerItems.ContainsKey(rewardItemId))
            {
                error = $"Pending quest reward item '{rewardItemId}' is owned by a world container.";
                return false;
            }

            if (plannedPlayerItems.TryGetValue(rewardItemId, out ItemInstance rewardItem)
                && !string.IsNullOrWhiteSpace(quest.rewardDefinitionId)
                && !string.Equals(
                    rewardItem.Definition.DefinitionId,
                    quest.rewardDefinitionId,
                    StringComparison.Ordinal))
            {
                error = $"Quest reward item '{rewardItemId}' has a different item definition.";
                return false;
            }
        }

        return true;
    }

    private static bool TryMapQuestSnapshot(
        QuestLogSaveData source,
        out QuestLogSnapshot snapshot,
        out string error)
    {
        snapshot = null;
        error = string.Empty;
        if (source == null || source.quests == null)
        {
            error = "Quest restore data is missing.";
            return false;
        }

        snapshot = new QuestLogSnapshot
        {
            trackedQuestId = source.trackedQuestId ?? string.Empty
        };
        for (int i = 0; i < source.quests.Count; i++)
        {
            QuestProgressSaveData questData = source.quests[i];
            if (questData == null || questData.objectives == null)
            {
                error = $"Quest record {i} is missing required data.";
                return false;
            }

            QuestProgressSnapshot quest = new()
            {
                questId = questData.questId ?? string.Empty,
                state = questData.state,
                currentObjectiveId = questData.currentObjectiveId ?? string.Empty,
                rewardStatus = questData.rewardStatus,
                rewardItemInstanceId = questData.rewardItemInstanceId ?? string.Empty,
                rewardDefinitionId = questData.rewardDefinitionId ?? string.Empty,
                rewardQuantity = questData.rewardQuantity
            };
            for (int objectiveIndex = 0; objectiveIndex < questData.objectives.Count; objectiveIndex++)
            {
                QuestObjectiveProgressSaveData objectiveData = questData.objectives[objectiveIndex];
                if (objectiveData == null || objectiveData.creditedTargetInstanceIds == null)
                {
                    error = $"Quest '{quest.questId}' objective record {objectiveIndex} is missing.";
                    return false;
                }

                quest.objectives.Add(new QuestObjectiveProgressSnapshot
                {
                    objectiveId = objectiveData.objectiveId ?? string.Empty,
                    progress = objectiveData.progress,
                    creditedTargetInstanceIds = new List<string>(objectiveData.creditedTargetInstanceIds)
                });
            }

            snapshot.quests.Add(quest);
        }

        return true;
    }
    private bool TryBuildItems(
        IReadOnlyList<ItemInstanceSaveData> source,
        List<ItemInstance> destination,
        Dictionary<string, ItemInstance> itemsById,
        HashSet<string> globalInstanceIds,
        string ownerName,
        out string error)
    {
        error = string.Empty;
        for (int i = 0; i < source.Count; i++)
        {
            ItemInstanceSaveData itemData = source[i];
            if (itemData == null)
            {
                error = $"{ownerName} item record {i} is missing.";
                return false;
            }

            if (string.IsNullOrWhiteSpace(itemData.instanceId)
                || !string.Equals(itemData.instanceId, itemData.instanceId.Trim(), StringComparison.Ordinal))
            {
                error = $"{ownerName} item record {i} has an empty or untrimmed instanceId.";
                return false;
            }

            if (!globalInstanceIds.Add(itemData.instanceId))
            {
                error = $"Duplicate global ItemInstance ID '{itemData.instanceId}'.";
                return false;
            }

            if (string.IsNullOrWhiteSpace(itemData.definitionId)
                || !itemDefinitionRegistry.TryResolve(itemData.definitionId, out ItemDefinition definition))
            {
                error = $"Item '{itemData.instanceId}' references unknown definition '{itemData.definitionId}'.";
                return false;
            }

            if (!ItemInstance.TryRestore(
                    itemData.instanceId,
                    definition,
                    itemData.quantity,
                    itemData.currentDurability,
                    itemData.maxDurability,
                    (ItemQuality)itemData.quality,
                    (ItemRarity)itemData.rarity,
                    itemData.creatorId,
                    itemData.affinityId,
                    itemData.isStolen,
                    itemData.originalOwnerId,
                    out ItemInstance item,
                    out error))
            {
                error = $"Invalid {ownerName} item: {error}";
                return false;
            }

            destination.Add(item);
            itemsById?.Add(item.InstanceId, item);
        }

        return true;
    }

    private bool TryBuildContainerPlans(
        IReadOnlyList<WorldContainerSaveData> source,
        WorldBindings world,
        RestorePlan plan,
        HashSet<string> globalInstanceIds,
        out string error)
    {
        error = string.Empty;
        if (source.Count != world.containers.Count)
        {
            error = $"Save has {source.Count} containers but the scene has {world.containers.Count}.";
            return false;
        }

        HashSet<string> seenIds = new(StringComparer.Ordinal);
        for (int i = 0; i < source.Count; i++)
        {
            WorldContainerSaveData containerData = source[i];
            if (containerData == null
                || string.IsNullOrWhiteSpace(containerData.worldId)
                || containerData.items == null
                || !containerData.initializedOrRestored
                || !seenIds.Add(containerData.worldId)
                || !world.containers.TryGetValue(containerData.worldId, out WorldItemContainer container))
            {
                error = $"Container record {i} is missing, duplicated, or unknown.";
                return false;
            }

            if (container.Capacity > 0 && containerData.items.Count > container.Capacity)
            {
                error = $"Container '{containerData.worldId}' exceeds capacity {container.Capacity}.";
                return false;
            }

            List<ItemInstance> items = new();
            if (!TryBuildItems(
                    containerData.items,
                    items,
                    null,
                    globalInstanceIds,
                    $"container '{containerData.worldId}'",
                    out error))
            {
                return false;
            }

            plan.containerItems.Add(containerData.worldId, items);
        }

        return true;
    }

    private static bool TryBuildPickupPlans(
        IReadOnlyList<WorldPickupSaveData> source,
        WorldBindings world,
        RestorePlan plan,
        out string error)
    {
        error = string.Empty;
        if (source.Count != world.pickups.Count)
        {
            error = $"Save has {source.Count} pickups but the scene has {world.pickups.Count}.";
            return false;
        }

        HashSet<string> seenIds = new(StringComparer.Ordinal);
        for (int i = 0; i < source.Count; i++)
        {
            WorldPickupSaveData pickupData = source[i];
            if (pickupData == null
                || string.IsNullOrWhiteSpace(pickupData.worldId)
                || !seenIds.Add(pickupData.worldId)
                || !world.pickups.ContainsKey(pickupData.worldId))
            {
                error = $"Pickup record {i} is missing, duplicated, or unknown.";
                return false;
            }

            plan.pickupStates.Add(pickupData.worldId, pickupData.collected);
        }

        return true;
    }

    private static bool TryBuildDoorPlans(
        IReadOnlyList<WorldDoorSaveData> source,
        WorldBindings world,
        RestorePlan plan,
        out string error)
    {
        error = string.Empty;
        if (source.Count != world.doors.Count)
        {
            error = $"Save has {source.Count} doors but the scene has {world.doors.Count}.";
            return false;
        }

        HashSet<string> seenIds = new(StringComparer.Ordinal);
        for (int i = 0; i < source.Count; i++)
        {
            WorldDoorSaveData doorData = source[i];
            if (doorData == null
                || string.IsNullOrWhiteSpace(doorData.worldId)
                || !seenIds.Add(doorData.worldId)
                || !world.doors.ContainsKey(doorData.worldId))
            {
                error = $"Door record {i} is missing, duplicated, or unknown.";
                return false;
            }

            plan.doorStates.Add(doorData.worldId, doorData.isOpen);
        }

        return true;
    }

    private bool TryApplyRestorePlan(
        RestorePlan plan,
        PlayerBindings player,
        WorldBindings world,
        out string error)
    {
        error = string.Empty;
        if (!player.inventory.TryRestoreItems(plan.playerItems, out error))
        {
            error = $"Player inventory restore failed: {error}";
            return false;
        }

        if (!player.equipment.TryRestoreState(
                plan.equipmentAssignments,
                plan.activeWeaponSlot,
                plan.activeItemInstanceId,
                out error))
        {
            error = $"Player equipment restore failed: {error}";
            return false;
        }

        foreach (KeyValuePair<string, List<ItemInstance>> entry in plan.containerItems)
        {
            if (!world.containers[entry.Key].TryRestoreContents(entry.Value, out error))
            {
                error = $"Container '{entry.Key}' restore failed: {error}";
                return false;
            }
        }

        foreach (KeyValuePair<string, bool> entry in plan.pickupStates)
        {
            world.pickups[entry.Key].SetCollectedState(entry.Value);
        }

        foreach (KeyValuePair<string, bool> entry in plan.doorStates)
        {
            world.doors[entry.Key].ApplyPersistentState(entry.Value);
        }

        bool controllerWasEnabled = player.characterController != null && player.characterController.enabled;
        if (controllerWasEnabled)
        {
            player.characterController.enabled = false;
        }

        player.transform.SetPositionAndRotation(plan.playerPosition, plan.playerRotation);
        if (controllerWasEnabled)
        {
            player.characterController.enabled = true;
        }

        Physics.SyncTransforms();
        player.health.RestoreHealth(plan.playerHealth);
        player.stamina.RestoreStamina(plan.playerStamina);
        CurrentSanctuaryId = plan.lastSanctuaryId;

        if (!player.questLog.TryApplyPreparedRestore(plan.questLogRestore, out error))
        {
            error = $"Quest log restore failed: {error}";
            return false;
        }

        player.inventory.NotifyStateRestored();
        return true;
    }

    private static void PrepareForLoad()
    {
        CharacterInventoryScreen inventoryScreen = FindFirstObjectByType<CharacterInventoryScreen>(FindObjectsInactive.Include);
        if (inventoryScreen != null)
        {
            inventoryScreen.CloseScreen();
            inventoryScreen.SelectItem(null);
        }

        FindFirstObjectByType<ContainerTransferScreen>(FindObjectsInactive.Include)?.Close();
        FindFirstObjectByType<DialoguePanel>(FindObjectsInactive.Include)?.Close();
        FindFirstObjectByType<QuestJournalScreen>(FindObjectsInactive.Include)?.CloseScreen();
        FindFirstObjectByType<DevConsole>(FindObjectsInactive.Include)?.CloseConsole();
        FindFirstObjectByType<PlayerInteractor>(FindObjectsInactive.Include)?.ClearCurrentTarget();
        FindFirstObjectByType<BasicMeleeAttack>(FindObjectsInactive.Include)?.CancelAttack();
        FindFirstObjectByType<PlayerCameraRelativeMovement>(FindObjectsInactive.Include)?.ResetTransientState();
        FindFirstObjectByType<PlayerLockOnController>(FindObjectsInactive.Include)?.ClearTarget();
        FindFirstObjectByType<PlayerAnimationDriver>(FindObjectsInactive.Include)?.ResetActionState();
        GameplayInputGate.ResetModalStateForRestore(2);
    }

    private bool TryResolveBindings(out PlayerBindings player, out WorldBindings world, out string error)
    {
        player = null;
        world = null;
        error = string.Empty;
        if (!ValidateRegistry(out error))
        {
            return false;
        }

        PlayerInventory[] inventories = FindObjectsByType<PlayerInventory>(FindObjectsInactive.Include, FindObjectsSortMode.None);
        if (inventories.Length != 1)
        {
            error = $"Expected exactly one PlayerInventory, found {inventories.Length}.";
            return false;
        }

        PlayerInventory inventory = inventories[0];
        PlayerEquipment equipment = inventory.Equipment != null
            ? inventory.Equipment
            : inventory.GetComponent<PlayerEquipment>();
        PlayerHealth health = inventory.GetComponent<PlayerHealth>();
        PlayerStamina stamina = inventory.GetComponent<PlayerStamina>();
        CharacterController characterController = inventory.GetComponent<CharacterController>();
        PlayerQuestLog questLog = inventory.GetComponent<PlayerQuestLog>();
        if (equipment == null
            || health == null
            || stamina == null
            || characterController == null
            || questLog == null)
        {
            error = "Player is missing PlayerEquipment, PlayerHealth, PlayerStamina, CharacterController, or PlayerQuestLog.";
            return false;
        }

        if (!ReferenceEquals(questLog.Registry, questDefinitionRegistry))
        {
            error = "PlayerQuestLog and SaveGameService must reference the same QuestDefinitionRegistry.";
            return false;
        }

        player = new PlayerBindings
        {
            transform = inventory.transform,
            inventory = inventory,
            equipment = equipment,
            health = health,
            stamina = stamina,
            characterController = characterController,
            questLog = questLog
        };

        if (!TryBuildWorldBindings(out world, out error))
        {
            player = null;
            return false;
        }

        return true;
    }

    private static bool TryBuildWorldBindings(out WorldBindings world, out string error)
    {
        world = new WorldBindings();
        error = string.Empty;
        HashSet<string> allIds = new(StringComparer.Ordinal);
        PersistentWorldId[] ids = FindObjectsByType<PersistentWorldId>(FindObjectsInactive.Include, FindObjectsSortMode.None);
        for (int i = 0; i < ids.Length; i++)
        {
            PersistentWorldId persistentId = ids[i];
            string worldId = persistentId.WorldId;
            if (string.IsNullOrWhiteSpace(worldId))
            {
                error = $"PersistentWorldId on '{persistentId.name}' is empty.";
                return false;
            }

            if (!allIds.Add(worldId))
            {
                error = $"Duplicate PersistentWorldId '{worldId}'.";
                return false;
            }

            WorldItemContainer container = persistentId.GetComponent<WorldItemContainer>();
            WeaponPickup pickup = persistentId.GetComponent<WeaponPickup>();
            SimpleDoorInteractable door = persistentId.GetComponent<SimpleDoorInteractable>();
            int participantCount = (container != null ? 1 : 0) + (pickup != null ? 1 : 0) + (door != null ? 1 : 0);
            if (participantCount != 1)
            {
                error = $"Persistent object '{worldId}' must have exactly one supported world-state component.";
                return false;
            }

            if (container != null)
            {
                world.containers.Add(worldId, container);
            }
            else if (pickup != null)
            {
                world.pickups.Add(worldId, pickup);
            }
            else
            {
                world.doors.Add(worldId, door);
            }
        }

        if (!ValidateParticipantIds<WorldItemContainer>(ids, out error)
            || !ValidateParticipantIds<WeaponPickup>(ids, out error)
            || !ValidateParticipantIds<SimpleDoorInteractable>(ids, out error))
        {
            return false;
        }

        return true;
    }

    private static bool ValidateParticipantIds<T>(PersistentWorldId[] ids, out string error)
        where T : Component
    {
        error = string.Empty;
        T[] participants = FindObjectsByType<T>(FindObjectsInactive.Include, FindObjectsSortMode.None);
        for (int i = 0; i < participants.Length; i++)
        {
            PersistentWorldId id = participants[i].GetComponent<PersistentWorldId>();
            if (id == null || !id.HasValidId || Array.IndexOf(ids, id) < 0)
            {
                error = $"Persistent participant '{participants[i].name}' ({typeof(T).Name}) has no valid PersistentWorldId.";
                return false;
            }
        }

        return true;
    }

    private static bool TryMigrateToCurrentSchema(SaveGameData data, out string error)
    {
        error = string.Empty;
        if (data.schemaVersion > CurrentSchemaVersion)
        {
            error = $"Save schema v{data.schemaVersion} is newer than supported v{CurrentSchemaVersion}.";
            return false;
        }

        while (data.schemaVersion < CurrentSchemaVersion)
        {
            switch (data.schemaVersion)
            {
                case 1:
                    data.questLog = new QuestLogSaveData();
                    data.schemaVersion = 2;
                    break;
                default:
                    error = $"No migration is registered from schema v{data.schemaVersion}.";
                    return false;
            }
        }

        return true;
    }

    private bool Fail(string operation, string error)
    {
        LastOperation = operation;
        LastError = string.IsNullOrWhiteSpace(error) ? "Unknown persistence failure." : error;
        LastStatus = $"{operation} failed.";
        Debug.Log($"[SaveGame] {operation} failed: {LastError}");
        return false;
    }

    private void TryDeleteTempFile()
    {
        try
        {
            DeleteIfPresent(TempPath);
        }
        catch
        {
        }
    }

    private static void DeleteIfPresent(string path)
    {
        if (File.Exists(path))
        {
            File.Delete(path);
        }
    }

    private static int CountEquipped(IReadOnlyList<EquipmentSlotSaveData> slots)
    {
        int count = 0;
        for (int i = 0; i < slots.Count; i++)
        {
            if (slots[i] != null && !string.IsNullOrWhiteSpace(slots[i].itemInstanceId))
            {
                count++;
            }
        }

        return count;
    }

    private static List<string> SortedKeys<T>(Dictionary<string, T> dictionary)
    {
        List<string> keys = new(dictionary.Keys);
        keys.Sort(StringComparer.Ordinal);
        return keys;
    }

    private static SerializableVector3Data FromVector3(Vector3 value)
    {
        return new SerializableVector3Data { x = value.x, y = value.y, z = value.z };
    }

    private static SerializableQuaternionData FromQuaternion(Quaternion value)
    {
        return new SerializableQuaternionData { x = value.x, y = value.y, z = value.z, w = value.w };
    }

    private static bool TryReadVector3(SerializableVector3Data data, out Vector3 value)
    {
        value = Vector3.zero;
        if (data == null || !IsFinite(data.x) || !IsFinite(data.y) || !IsFinite(data.z))
        {
            return false;
        }

        value = new Vector3(data.x, data.y, data.z);
        return true;
    }

    private static bool TryReadQuaternion(SerializableQuaternionData data, out Quaternion value)
    {
        value = Quaternion.identity;
        if (data == null
            || !IsFinite(data.x)
            || !IsFinite(data.y)
            || !IsFinite(data.z)
            || !IsFinite(data.w))
        {
            return false;
        }

        Quaternion candidate = new(data.x, data.y, data.z, data.w);
        float magnitudeSquared = candidate.x * candidate.x
            + candidate.y * candidate.y
            + candidate.z * candidate.z
            + candidate.w * candidate.w;
        if (!IsFinite(magnitudeSquared) || magnitudeSquared < 0.000001f)
        {
            return false;
        }

        float inverseMagnitude = 1f / Mathf.Sqrt(magnitudeSquared);
        value = new Quaternion(
            candidate.x * inverseMagnitude,
            candidate.y * inverseMagnitude,
            candidate.z * inverseMagnitude,
            candidate.w * inverseMagnitude);
        return true;
    }

    private static bool IsPlayerPositionSafe(
        PlayerBindings player,
        WorldBindings world,
        IReadOnlyDictionary<string, bool> plannedDoorStates,
        Vector3 position,
        Quaternion rotation,
        out string error)
    {
        error = string.Empty;
        CharacterController controller = player?.characterController;
        if (controller == null)
        {
            error = "Cannot validate player position without a CharacterController.";
            return false;
        }

        const float maxCoordinateMagnitude = 100000f;
        if (Mathf.Abs(position.x) > maxCoordinateMagnitude
            || Mathf.Abs(position.y) > maxCoordinateMagnitude
            || Mathf.Abs(position.z) > maxCoordinateMagnitude)
        {
            error = $"Player position exceeds the supported world bounds of +/-{maxCoordinateMagnitude:0}.";
            return false;
        }

        Vector3 scale = player.transform.lossyScale;
        float horizontalScale = Mathf.Max(Mathf.Abs(scale.x), Mathf.Abs(scale.z));
        float verticalScale = Mathf.Abs(scale.y);
        float radius = Mathf.Max(0.01f, controller.radius * horizontalScale);
        float height = Mathf.Max(controller.height * verticalScale, radius * 2f);
        Vector3 scaledCenter = new(
            controller.center.x * scale.x,
            controller.center.y * scale.y,
            controller.center.z * scale.z);
        Vector3 center = position + rotation * scaledCenter;
        Vector3 up = rotation * Vector3.up;
        float halfSegment = Mathf.Max(0f, height * 0.5f - radius);
        float queryRadius = Mathf.Max(0.01f, radius - controller.skinWidth * 0.5f);
        Collider[] overlaps = Physics.OverlapCapsule(
            center + up * halfSegment,
            center - up * halfSegment,
            queryRadius,
            ~0,
            QueryTriggerInteraction.Ignore);

        for (int i = 0; i < overlaps.Length; i++)
        {
            Collider candidate = overlaps[i];
            if (candidate == null
                || !candidate.enabled
                || candidate.isTrigger
                || candidate.transform.IsChildOf(player.transform)
                || Physics.GetIgnoreLayerCollision(controller.gameObject.layer, candidate.gameObject.layer))
            {
                continue;
            }


            SimpleDoorInteractable movingDoor = candidate.GetComponentInParent<SimpleDoorInteractable>();
            if (movingDoor != null && world.doors.ContainsValue(movingDoor))
            {
                continue;
            }

            if (Physics.ComputePenetration(
                    controller,
                    position,
                    rotation,
                    candidate,
                    candidate.transform.position,
                    candidate.transform.rotation,
                    out _,
                    out float distance)
                && distance > 0.001f)
            {
                error = $"Player position overlaps collider '{candidate.name}' by {distance:0.###} units.";
                return false;
            }
        }

        foreach (KeyValuePair<string, bool> doorState in plannedDoorStates)
        {
            SimpleDoorInteractable door = world.doors[doorState.Key];
            if (!door.gameObject.activeInHierarchy)
            {
                continue;
            }

            Collider[] doorColliders = door.GetComponentsInChildren<Collider>(true);
            for (int i = 0; i < doorColliders.Length; i++)
            {
                Collider doorCollider = doorColliders[i];
                if (doorCollider == null
                    || !doorCollider.enabled
                    || doorCollider.isTrigger
                    || !doorCollider.gameObject.activeInHierarchy
                    || Physics.GetIgnoreLayerCollision(
                        controller.gameObject.layer,
                        doorCollider.gameObject.layer))
                {
                    continue;
                }

                if (!door.TryGetPersistentColliderPose(
                        doorCollider,
                        doorState.Value,
                        out Vector3 colliderPosition,
                        out Quaternion colliderRotation))
                {
                    error = $"Could not predict collider pose for persistent door '{doorState.Key}'.";
                    return false;
                }

                if (Physics.ComputePenetration(
                        controller,
                        position,
                        rotation,
                        doorCollider,
                        colliderPosition,
                        colliderRotation,
                        out _,
                        out float distance)
                    && distance > 0.001f)
                {
                    error = $"Player position overlaps saved door collider '{doorCollider.name}' "
                        + $"by {distance:0.###} units.";
                    return false;
                }
            }
        }

        return true;
    }

    private static bool IsFinite(float value)
    {
        return !float.IsNaN(value) && !float.IsInfinity(value);
    }
}
