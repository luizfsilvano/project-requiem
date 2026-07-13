using System;
using System.Collections.Generic;
using UnityEngine;

[DefaultExecutionOrder(12)]
[DisallowMultipleComponent]
public sealed class PlayerQuestLog : MonoBehaviour
{
    [SerializeField] private QuestDefinitionRegistry registry;
    [SerializeField] private PlayerInventory inventory;
    [SerializeField] private PlayerInteractor interactor;

    private readonly Dictionary<string, QuestRuntimeState> states = new(StringComparer.Ordinal);
    private readonly List<QuestRuntimeState> orderedStates = new();
    private readonly List<QuestProgressSnapshot> orphanedStates = new();
    private bool initialized;
    private bool subscribed;
    private string trackedQuestId = string.Empty;

    public event Action QuestLogChanged;
    public event Action<QuestRuntimeState> QuestChanged;
    public event Action<QuestRuntimeState> TrackedQuestChanged;

    public QuestDefinitionRegistry Registry => registry;
    public IReadOnlyList<QuestRuntimeState> Quests
    {
        get
        {
            EnsureInitialized();
            return orderedStates;
        }
    }
    public int Count
    {
        get
        {
            EnsureInitialized();
            return orderedStates.Count;
        }
    }
    public string TrackedQuestId => trackedQuestId;
    public QuestRuntimeState TrackedQuest => !string.IsNullOrWhiteSpace(trackedQuestId)
        && TryGetQuest(trackedQuestId, out QuestRuntimeState tracked)
            ? tracked
            : null;

    private void Awake()
    {
        ResolveDependencies();
        EnsureInitialized();
    }

    private void OnEnable()
    {
        ResolveDependencies();
        EnsureInitialized();
        Subscribe();
    }

    private void OnDisable()
    {
        Unsubscribe();
    }

    public bool TryGetQuest(string questId, out QuestRuntimeState state)
    {
        EnsureInitialized();
        state = null;
        return !string.IsNullOrWhiteSpace(questId) && states.TryGetValue(questId, out state);
    }

    public bool TryAcceptQuest(string questId, out string feedback)
    {
        feedback = string.Empty;
        if (!TryGetQuest(questId, out QuestRuntimeState state)
            || state.State != QuestState.Available)
        {
            feedback = "Esta missão não está disponível para aceite.";
            return false;
        }

        ResetProgress(state.Data);
        state.Data.state = (int)QuestState.Active;
        state.Data.currentObjectiveId = state.Definition.Objectives[0].ObjectiveId;
        if (string.IsNullOrWhiteSpace(trackedQuestId))
        {
            trackedQuestId = questId;
            TrackedQuestChanged?.Invoke(state);
        }

        RefreshOwnedItemObjective(state);
        feedback = $"Missão aceita: {state.Definition.Title}.";
        NotifyQuestChanged(questId);
        return true;
    }

    public bool TryTurnInQuest(string questId, out string feedback)
    {
        feedback = string.Empty;
        if (!TryGetQuest(questId, out QuestRuntimeState state)
            || state.State != QuestState.ReadyToTurnIn)
        {
            feedback = "A missão ainda não está pronta para entrega.";
            return false;
        }

        ItemDefinition rewardDefinition = state.Definition.RewardItem;
        if (rewardDefinition == null)
        {
            CompleteQuest(state, QuestRewardStatus.None);
            feedback = "Missão concluída.";
            return true;
        }

        if (state.RewardStatus == QuestRewardStatus.None)
        {
            state.Data.rewardStatus = (int)QuestRewardStatus.PendingDelivery;
            state.Data.rewardItemInstanceId = Guid.NewGuid().ToString("N");
            state.Data.rewardDefinitionId = rewardDefinition.DefinitionId;
            state.Data.rewardQuantity = state.Definition.RewardQuantity;
        }

        if (string.IsNullOrWhiteSpace(state.Data.rewardItemInstanceId))
        {
            feedback = "A reserva da recompensa está inválida.";
            NotifyQuestChanged(questId);
            return false;
        }

        if (inventory != null
            && inventory.TryGetItem(state.Data.rewardItemInstanceId, out ItemInstance existingReward))
        {
            if (existingReward.Definition == rewardDefinition
                && existingReward.Quantity == state.Data.rewardQuantity)
            {
                CompleteQuest(state, QuestRewardStatus.Granted);
                feedback = $"Missão concluída. Recompensa: {rewardDefinition.DisplayName}.";
                return true;
            }

            feedback = "O ID reservado da recompensa pertence a outro item.";
            NotifyQuestChanged(questId);
            return false;
        }

        string rewardError = string.Empty;
        if (inventory == null)
        {
            feedback = "O inventário do jogador não está disponível.";
            NotifyQuestChanged(questId);
            return false;
        }

        if (!ItemInstance.TryRestore(
                state.Data.rewardItemInstanceId,
                rewardDefinition,
                state.Data.rewardQuantity,
                rewardDefinition.DefaultMaxDurability,
                rewardDefinition.DefaultMaxDurability,
                ItemQuality.Standard,
                ItemRarity.Common,
                string.Empty,
                string.Empty,
                false,
                string.Empty,
                out ItemInstance reward,
                out rewardError))
        {
            feedback = string.IsNullOrWhiteSpace(rewardError)
                ? "Não foi possível preparar a recompensa."
                : rewardError;
            NotifyQuestChanged(questId);
            return false;
        }

        if (!inventory.CanAdd(reward) || !inventory.TryAdd(reward))
        {
            feedback = "Inventário cheio. Libere um espaço e fale novamente com o NPC.";
            NotifyQuestChanged(questId);
            return false;
        }

        CompleteQuest(state, QuestRewardStatus.Granted);
        feedback = $"Missão concluída. Recompensa: {rewardDefinition.DisplayName} x{reward.Quantity}.";
        return true;
    }

    public bool TrySetTrackedQuest(string questId)
    {
        EnsureInitialized();
        string nextTrackedId = questId ?? string.Empty;
        if (!string.IsNullOrWhiteSpace(nextTrackedId)
            && (!states.TryGetValue(nextTrackedId, out QuestRuntimeState state) || !state.IsTrackable))
        {
            return false;
        }

        if (string.Equals(trackedQuestId, nextTrackedId, StringComparison.Ordinal))
        {
            return true;
        }

        trackedQuestId = nextTrackedId;
        TrackedQuestChanged?.Invoke(TrackedQuest);
        QuestLogChanged?.Invoke();
        return true;
    }

    public bool DebugStartQuest(string questId, out string feedback)
    {
        return TryAcceptQuest(questId, out feedback);
    }

    public bool DevStartQuest(string questId, out string feedback)
    {
        return DebugStartQuest(questId, out feedback);
    }

    public bool DebugAdvanceCurrentObjective(string questId, out string feedback)
    {
        feedback = string.Empty;
        if (!TryGetQuest(questId, out QuestRuntimeState state)
            || state.State != QuestState.Active
            || state.CurrentObjective == null)
        {
            feedback = "A missão não possui um objetivo ativo.";
            return false;
        }

        QuestObjectiveDefinition objective = state.CurrentObjective;
        QuestObjectiveProgressSnapshot progress = state.GetObjectiveProgress(objective.ObjectiveId);
        progress.progress = objective.RequiredAmount;
        if (objective.ObjectiveType == QuestObjectiveType.DefeatTarget)
        {
            progress.creditedTargetInstanceIds.Clear();
            for (int i = 0; i < objective.RequiredAmount; i++)
            {
                progress.creditedTargetInstanceIds.Add($"debug.{questId}.{objective.ObjectiveId}.{i}");
            }
        }

        AdvanceAfterObjectiveCompletion(state);
        feedback = "Objetivo atual concluído pela ferramenta de desenvolvimento.";
        NotifyQuestChanged(questId);
        return true;
    }

    public bool DevAdvanceCurrentObjective(string questId, out string feedback)
    {
        return DebugAdvanceCurrentObjective(questId, out feedback);
    }

    public void DebugResetAllQuestProgress()
    {
        initialized = false;
        states.Clear();
        orderedStates.Clear();
        orphanedStates.Clear();
        trackedQuestId = string.Empty;
        EnsureInitialized();
        TrackedQuestChanged?.Invoke(null);
        QuestLogChanged?.Invoke();
    }

    public void DevResetAllQuests()
    {
        DebugResetAllQuestProgress();
    }

    public QuestLogSnapshot CaptureSnapshot()
    {
        EnsureInitialized();
        QuestLogSnapshot snapshot = new()
        {
            trackedQuestId = trackedQuestId ?? string.Empty
        };

        for (int i = 0; i < orderedStates.Count; i++)
        {
            snapshot.quests.Add(orderedStates[i].Data.DeepClone());
        }

        for (int i = 0; i < orphanedStates.Count; i++)
        {
            snapshot.quests.Add(orphanedStates[i].DeepClone());
        }

        return snapshot;
    }

    public bool TryPrepareRestore(
        QuestLogSnapshot snapshot,
        out QuestRestorePlan plan,
        out string error)
    {
        EnsureInitialized();
        plan = null;
        error = string.Empty;
        if (snapshot == null || snapshot.quests == null || registry == null || !registry.Validate(out error))
        {
            error = string.IsNullOrWhiteSpace(error) ? "Quest restore snapshot is missing." : error;
            return false;
        }

        Dictionary<string, QuestProgressSnapshot> savedById = new(StringComparer.Ordinal);
        List<QuestProgressSnapshot> orphans = new();
        for (int i = 0; i < snapshot.quests.Count; i++)
        {
            QuestProgressSnapshot record = snapshot.quests[i];
            if (!ValidateStructuralRecord(record, out error)
                || !savedById.TryAdd(record.questId, record))
            {
                if (string.IsNullOrWhiteSpace(error))
                {
                    error = $"Quest '{record?.questId}' appears more than once.";
                }
                return false;
            }

            if (!registry.TryResolve(record.questId, out _))
            {
                orphans.Add(record.DeepClone());
            }
        }

        QuestLogSnapshot normalized = new();
        for (int i = 0; i < registry.Definitions.Count; i++)
        {
            QuestDefinition definition = registry.Definitions[i];
            QuestProgressSnapshot record = savedById.TryGetValue(definition.QuestId, out QuestProgressSnapshot saved)
                ? saved.DeepClone()
                : CreateDefaultSnapshot(definition);
            if (!ValidateKnownRecord(definition, record, out error))
            {
                return false;
            }

            normalized.quests.Add(record);
        }

        normalized.quests.AddRange(orphans);
        string requestedTrackedId = snapshot.trackedQuestId ?? string.Empty;
        normalized.trackedQuestId = string.Empty;
        for (int i = 0; i < normalized.quests.Count; i++)
        {
            QuestProgressSnapshot record = normalized.quests[i];
            if (string.Equals(record.questId, requestedTrackedId, StringComparison.Ordinal)
                && registry.TryResolve(record.questId, out _)
                && ((QuestState)record.state == QuestState.Active
                    || (QuestState)record.state == QuestState.ReadyToTurnIn))
            {
                normalized.trackedQuestId = requestedTrackedId;
                break;
            }
        }

        plan = new QuestRestorePlan(normalized, orphans.Count);
        return true;
    }

    public bool TryApplyPreparedRestore(QuestRestorePlan plan, out string error)
    {
        error = string.Empty;
        if (plan?.Snapshot == null
            || !TryPrepareRestore(plan.Snapshot, out QuestRestorePlan validatedPlan, out error))
        {
            return false;
        }

        Dictionary<string, QuestRuntimeState> nextStates = new(StringComparer.Ordinal);
        List<QuestRuntimeState> nextOrdered = new();
        List<QuestProgressSnapshot> nextOrphans = new();
        for (int i = 0; i < validatedPlan.Snapshot.quests.Count; i++)
        {
            QuestProgressSnapshot record = validatedPlan.Snapshot.quests[i];
            if (!registry.TryResolve(record.questId, out QuestDefinition definition))
            {
                nextOrphans.Add(record.DeepClone());
                continue;
            }

            QuestRuntimeState state = new(definition, record.DeepClone());
            nextStates.Add(definition.QuestId, state);
            nextOrdered.Add(state);
        }

        states.Clear();
        foreach (KeyValuePair<string, QuestRuntimeState> entry in nextStates)
        {
            states.Add(entry.Key, entry.Value);
        }

        orderedStates.Clear();
        orderedStates.AddRange(nextOrdered);
        orphanedStates.Clear();
        orphanedStates.AddRange(nextOrphans);
        trackedQuestId = validatedPlan.Snapshot.trackedQuestId ?? string.Empty;
        initialized = true;
        TrackedQuestChanged?.Invoke(TrackedQuest);
        QuestLogChanged?.Invoke();
        return true;
    }

    private void HandleTargetDefeated(QuestDefeatSignal signal)
    {
        if (inventory == null
            || signal.Attacker == null
            || signal.Attacker.GetComponentInParent<PlayerInventory>() != inventory)
        {
            return;
        }

        ReportProgress(
            QuestObjectiveType.DefeatTarget,
            signal.TargetId,
            signal.TargetInstanceId,
            1);
    }

    private void HandleInteractionSucceeded(IInteractable interactable)
    {
        if (interactable is not Component component)
        {
            return;
        }

        QuestTargetIdentity identity = component.GetComponent<QuestTargetIdentity>();
        if (identity == null || string.IsNullOrWhiteSpace(identity.TargetId))
        {
            return;
        }

        if (identity.TargetKind == QuestTargetKind.Npc)
        {
            ReportProgress(QuestObjectiveType.TalkToNpc, identity.TargetId, identity.InstanceId, 1);
        }

        ReportProgress(QuestObjectiveType.InteractWithObject, identity.TargetId, identity.InstanceId, 1);
    }

    private void HandleInventoryChanged()
    {
        EnsureInitialized();
        for (int i = 0; i < orderedStates.Count; i++)
        {
            QuestRuntimeState state = orderedStates[i];
            if (state.State == QuestState.Active
                && state.CurrentObjective?.ObjectiveType == QuestObjectiveType.OwnItem
                && RefreshOwnedItemObjective(state))
            {
                NotifyQuestChanged(state.QuestId);
            }
        }
    }

    private void ReportProgress(
        QuestObjectiveType objectiveType,
        string targetId,
        string targetInstanceId,
        int amount)
    {
        EnsureInitialized();
        if (string.IsNullOrWhiteSpace(targetId) || amount <= 0)
        {
            return;
        }

        for (int i = 0; i < orderedStates.Count; i++)
        {
            QuestRuntimeState state = orderedStates[i];
            QuestObjectiveDefinition objective = state.CurrentObjective;
            if (state.State != QuestState.Active
                || objective == null
                || objective.ObjectiveType != objectiveType
                || !string.Equals(objective.TargetId, targetId, StringComparison.Ordinal))
            {
                continue;
            }

            QuestObjectiveProgressSnapshot progress = state.GetObjectiveProgress(objective.ObjectiveId);
            if (objectiveType == QuestObjectiveType.DefeatTarget)
            {
                if (string.IsNullOrWhiteSpace(targetInstanceId)
                    || progress.creditedTargetInstanceIds.Contains(targetInstanceId))
                {
                    continue;
                }

                progress.creditedTargetInstanceIds.Add(targetInstanceId);
            }

            progress.progress = Math.Min(objective.RequiredAmount, progress.progress + amount);
            if (progress.progress >= objective.RequiredAmount)
            {
                AdvanceAfterObjectiveCompletion(state);
            }

            NotifyQuestChanged(state.QuestId);
        }
    }

    private bool RefreshOwnedItemObjective(QuestRuntimeState state)
    {
        QuestObjectiveDefinition objective = state?.CurrentObjective;
        if (state == null
            || state.State != QuestState.Active
            || objective?.ObjectiveType != QuestObjectiveType.OwnItem
            || inventory == null)
        {
            return false;
        }

        int ownedQuantity = 0;
        for (int i = 0; i < inventory.Items.Count; i++)
        {
            ItemInstance item = inventory.Items[i];
            if (item?.Definition != null
                && string.Equals(item.Definition.DefinitionId, objective.TargetId, StringComparison.Ordinal))
            {
                ownedQuantity += item.Quantity;
            }
        }

        QuestObjectiveProgressSnapshot progress = state.GetObjectiveProgress(objective.ObjectiveId);
        int nextProgress = Math.Min(objective.RequiredAmount, ownedQuantity);
        if (progress.progress == nextProgress)
        {
            return false;
        }

        progress.progress = nextProgress;
        if (nextProgress >= objective.RequiredAmount)
        {
            AdvanceAfterObjectiveCompletion(state);
        }

        return true;
    }

    private static void AdvanceAfterObjectiveCompletion(QuestRuntimeState state)
    {
        int currentIndex = state.Definition.GetObjectiveIndex(state.Data.currentObjectiveId);
        int nextIndex = currentIndex + 1;
        if (nextIndex >= state.Definition.Objectives.Count)
        {
            state.Data.currentObjectiveId = string.Empty;
            state.Data.state = (int)QuestState.ReadyToTurnIn;
            return;
        }

        state.Data.currentObjectiveId = state.Definition.Objectives[nextIndex].ObjectiveId;
    }

    private void CompleteQuest(QuestRuntimeState state, QuestRewardStatus rewardStatus)
    {
        state.Data.state = (int)QuestState.Completed;
        state.Data.currentObjectiveId = string.Empty;
        state.Data.rewardStatus = (int)rewardStatus;
        if (string.Equals(trackedQuestId, state.QuestId, StringComparison.Ordinal))
        {
            trackedQuestId = string.Empty;
            TrackedQuestChanged?.Invoke(null);
        }

        NotifyQuestChanged(state.QuestId);
    }

    private void ResolveDependencies()
    {
        inventory ??= GetComponent<PlayerInventory>();
        interactor ??= GetComponent<PlayerInteractor>();
    }

    private void EnsureInitialized()
    {
        if (initialized)
        {
            return;
        }

        states.Clear();
        orderedStates.Clear();
        orphanedStates.Clear();
        trackedQuestId = string.Empty;
        if (registry != null && registry.Validate(out _))
        {
            for (int i = 0; i < registry.Definitions.Count; i++)
            {
                QuestDefinition definition = registry.Definitions[i];
                QuestProgressSnapshot snapshot = CreateDefaultSnapshot(definition);
                QuestRuntimeState state = new(definition, snapshot);
                states.Add(definition.QuestId, state);
                orderedStates.Add(state);
            }
        }

        initialized = true;
    }

    private static QuestProgressSnapshot CreateDefaultSnapshot(QuestDefinition definition)
    {
        QuestProgressSnapshot snapshot = new()
        {
            questId = definition.QuestId,
            state = (int)(definition.AvailableByDefault ? QuestState.Available : QuestState.Unavailable),
            currentObjectiveId = string.Empty,
            rewardStatus = (int)QuestRewardStatus.None,
            rewardItemInstanceId = string.Empty,
            rewardDefinitionId = string.Empty,
            rewardQuantity = 0
        };

        for (int i = 0; i < definition.Objectives.Count; i++)
        {
            snapshot.objectives.Add(new QuestObjectiveProgressSnapshot
            {
                objectiveId = definition.Objectives[i].ObjectiveId,
                progress = 0
            });
        }

        return snapshot;
    }

    private static void ResetProgress(QuestProgressSnapshot snapshot)
    {
        snapshot.currentObjectiveId = string.Empty;
        snapshot.rewardStatus = (int)QuestRewardStatus.None;
        snapshot.rewardItemInstanceId = string.Empty;
        snapshot.rewardDefinitionId = string.Empty;
        snapshot.rewardQuantity = 0;
        for (int i = 0; i < snapshot.objectives.Count; i++)
        {
            snapshot.objectives[i].progress = 0;
            snapshot.objectives[i].creditedTargetInstanceIds.Clear();
        }
    }

    private static bool ValidateStructuralRecord(QuestProgressSnapshot record, out string error)
    {
        error = string.Empty;
        if (record == null
            || string.IsNullOrWhiteSpace(record.questId)
            || !string.Equals(record.questId, record.questId.Trim(), StringComparison.Ordinal)
            || record.objectives == null
            || !Enum.IsDefined(typeof(QuestState), record.state)
            || !Enum.IsDefined(typeof(QuestRewardStatus), record.rewardStatus))
        {
            error = "Quest record is missing required data or contains an unknown enum.";
            return false;
        }

        HashSet<string> objectiveIds = new(StringComparer.Ordinal);
        for (int i = 0; i < record.objectives.Count; i++)
        {
            QuestObjectiveProgressSnapshot objective = record.objectives[i];
            if (objective == null
                || string.IsNullOrWhiteSpace(objective.objectiveId)
                || objective.progress < 0
                || objective.creditedTargetInstanceIds == null
                || !objectiveIds.Add(objective.objectiveId))
            {
                error = $"Quest '{record.questId}' has an invalid objective record.";
                return false;
            }

            HashSet<string> creditedIds = new(StringComparer.Ordinal);
            for (int creditedIndex = 0; creditedIndex < objective.creditedTargetInstanceIds.Count; creditedIndex++)
            {
                string creditedId = objective.creditedTargetInstanceIds[creditedIndex];
                if (string.IsNullOrWhiteSpace(creditedId) || !creditedIds.Add(creditedId))
                {
                    error = $"Quest '{record.questId}' has an invalid credited target instance ID.";
                    return false;
                }
            }
        }

        QuestRewardStatus rewardStatus = (QuestRewardStatus)record.rewardStatus;
        if (rewardStatus != QuestRewardStatus.None
            && (string.IsNullOrWhiteSpace(record.rewardItemInstanceId)
                || string.IsNullOrWhiteSpace(record.rewardDefinitionId)
                || record.rewardQuantity < 1))
        {
            error = $"Quest '{record.questId}' has incomplete reward reservation data.";
            return false;
        }

        return true;
    }

    private static bool ValidateKnownRecord(
        QuestDefinition definition,
        QuestProgressSnapshot record,
        out string error)
    {
        error = string.Empty;
        if (record.objectives.Count != definition.Objectives.Count)
        {
            error = $"Quest '{definition.QuestId}' objective count does not match its definition.";
            return false;
        }

        bool allComplete = true;
        string firstIncompleteId = string.Empty;
        int firstIncompleteIndex = -1;
        for (int i = 0; i < definition.Objectives.Count; i++)
        {
            QuestObjectiveDefinition objective = definition.Objectives[i];
            QuestObjectiveProgressSnapshot progress = record.objectives[i];
            if (!string.Equals(progress.objectiveId, objective.ObjectiveId, StringComparison.Ordinal)
                || progress.progress > objective.RequiredAmount)
            {
                error = $"Quest '{definition.QuestId}' objective '{objective.ObjectiveId}' is incompatible with its definition.";
                return false;
            }

            if (objective.ObjectiveType == QuestObjectiveType.DefeatTarget
                && progress.creditedTargetInstanceIds.Count != progress.progress)
            {
                error = $"Quest '{definition.QuestId}' defeat objective credit IDs do not match its progress.";
                return false;
            }

            if (progress.progress < objective.RequiredAmount && string.IsNullOrEmpty(firstIncompleteId))
            {
                firstIncompleteId = objective.ObjectiveId;
                firstIncompleteIndex = i;
                allComplete = false;
            }
        }

        bool hasAnyProgress = record.objectives.Exists(item => item.progress != 0);
        QuestState state = (QuestState)record.state;
        if ((state == QuestState.Available || state == QuestState.Unavailable)
            && (!string.IsNullOrEmpty(record.currentObjectiveId) || hasAnyProgress))
        {
            error = $"Quest '{definition.QuestId}' has progress before it is active.";
            return false;
        }

        if (state == QuestState.Active
            && (allComplete || !string.Equals(record.currentObjectiveId, firstIncompleteId, StringComparison.Ordinal)))
        {
            error = $"Quest '{definition.QuestId}' active objective is inconsistent.";
            return false;
        }

        if (state == QuestState.Active && firstIncompleteIndex >= 0)
        {
            for (int i = firstIncompleteIndex + 1; i < record.objectives.Count; i++)
            {
                if (record.objectives[i].progress != 0)
                {
                    error = $"Quest '{definition.QuestId}' has progress in a future objective.";
                    return false;
                }
            }
        }

        if ((state == QuestState.ReadyToTurnIn || state == QuestState.Completed)
            && (!allComplete || !string.IsNullOrEmpty(record.currentObjectiveId)))
        {
            error = $"Quest '{definition.QuestId}' completion state is inconsistent.";
            return false;
        }

        QuestRewardStatus rewardStatus = (QuestRewardStatus)record.rewardStatus;
        if (rewardStatus == QuestRewardStatus.None
            && (!string.IsNullOrEmpty(record.rewardItemInstanceId)
                || !string.IsNullOrEmpty(record.rewardDefinitionId)
                || record.rewardQuantity != 0))
        {
            error = $"Quest '{definition.QuestId}' has reward data without a reward status.";
            return false;
        }

        if (rewardStatus != QuestRewardStatus.None
            && (definition.RewardItem == null
                || !string.Equals(record.rewardDefinitionId, definition.RewardItem.DefinitionId, StringComparison.Ordinal)
                || record.rewardQuantity != definition.RewardQuantity))
        {
            error = $"Quest '{definition.QuestId}' reward reservation does not match its definition.";
            return false;
        }

        if (rewardStatus == QuestRewardStatus.PendingDelivery && state != QuestState.ReadyToTurnIn
            || rewardStatus == QuestRewardStatus.Granted && state != QuestState.Completed
            || state == QuestState.Completed && definition.RewardItem != null && rewardStatus != QuestRewardStatus.Granted)
        {
            error = $"Quest '{definition.QuestId}' reward status is inconsistent.";
            return false;
        }

        return true;
    }

    private void Subscribe()
    {
        if (subscribed)
        {
            return;
        }

        QuestSignalHub.TargetDefeated += HandleTargetDefeated;
        if (interactor != null)
        {
            interactor.InteractionSucceeded += HandleInteractionSucceeded;
        }

        if (inventory != null)
        {
            inventory.InventoryChanged += HandleInventoryChanged;
        }

        subscribed = true;
    }

    private void Unsubscribe()
    {
        if (!subscribed)
        {
            return;
        }

        QuestSignalHub.TargetDefeated -= HandleTargetDefeated;
        if (interactor != null)
        {
            interactor.InteractionSucceeded -= HandleInteractionSucceeded;
        }

        if (inventory != null)
        {
            inventory.InventoryChanged -= HandleInventoryChanged;
        }

        subscribed = false;
    }

    private void NotifyQuestChanged(string questId)
    {
        states.TryGetValue(questId, out QuestRuntimeState state);
        QuestChanged?.Invoke(state);
        QuestLogChanged?.Invoke();
    }
}
