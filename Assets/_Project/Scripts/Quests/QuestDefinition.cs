using System;
using System.Collections.Generic;
using UnityEngine;

public enum QuestState
{
    Unavailable,
    Available,
    Active,
    ReadyToTurnIn,
    Completed
}

public enum QuestObjectiveType
{
    TalkToNpc,
    DefeatTarget,
    OwnItem,
    InteractWithObject
}

public enum QuestRewardStatus
{
    None,
    PendingDelivery,
    Granted
}

[Serializable]
public sealed class QuestObjectiveDefinition
{
    [SerializeField] private string objectiveId;
    [SerializeField] private QuestObjectiveType objectiveType;
    [SerializeField] private string targetId;
    [SerializeField, Min(1)] private int requiredAmount = 1;
    [SerializeField, TextArea(1, 4)] private string description;

    public string ObjectiveId => objectiveId != null ? objectiveId.Trim() : string.Empty;
    public QuestObjectiveType ObjectiveType => objectiveType;
    public string TargetId => targetId != null ? targetId.Trim() : string.Empty;
    public int RequiredAmount => Mathf.Max(1, requiredAmount);
    public string Description => description ?? string.Empty;
}

[CreateAssetMenu(fileName = "QuestDefinition", menuName = "Combat Sandbox/Quests/Quest Definition")]
public sealed class QuestDefinition : ScriptableObject
{
    [Header("Identity")]
    [SerializeField] private string questId;
    [SerializeField] private string title;
    [SerializeField, TextArea(3, 7)] private string description;
    [SerializeField] private bool availableByDefault = true;
    [SerializeField] private string giverNpcId;
    [SerializeField] private string turnInNpcId;

    [Header("Ordered Objectives")]
    [SerializeField] private List<QuestObjectiveDefinition> objectives = new();

    [Header("Reward")]
    [SerializeField] private ItemDefinition rewardItem;
    [SerializeField, Min(1)] private int rewardQuantity = 1;

    [Header("Dialogue")]
    [SerializeField, TextArea(2, 6)] private string offerText;
    [SerializeField, TextArea(2, 6)] private string declinedText;
    [SerializeField, TextArea(2, 6)] private string activeText;
    [SerializeField, TextArea(2, 6)] private string readyText;
    [SerializeField, TextArea(2, 6)] private string completedText;

    public string QuestId => questId != null ? questId.Trim() : string.Empty;
    public string Title => string.IsNullOrWhiteSpace(title) ? name : title;
    public string Description => description ?? string.Empty;
    public bool AvailableByDefault => availableByDefault;
    public string GiverNpcId => giverNpcId != null ? giverNpcId.Trim() : string.Empty;
    public string TurnInNpcId => turnInNpcId != null ? turnInNpcId.Trim() : string.Empty;
    public IReadOnlyList<QuestObjectiveDefinition> Objectives => objectives;
    public ItemDefinition RewardItem => rewardItem;
    public int RewardQuantity => Mathf.Max(1, rewardQuantity);
    public string OfferText => offerText ?? string.Empty;
    public string DeclinedText => declinedText ?? string.Empty;
    public string ActiveText => activeText ?? string.Empty;
    public string ReadyText => readyText ?? string.Empty;
    public string CompletedText => completedText ?? string.Empty;

    public bool TryGetObjective(string objectiveId, out QuestObjectiveDefinition objective)
    {
        objective = null;
        if (string.IsNullOrWhiteSpace(objectiveId) || objectives == null)
        {
            return false;
        }

        for (int i = 0; i < objectives.Count; i++)
        {
            QuestObjectiveDefinition candidate = objectives[i];
            if (candidate != null
                && string.Equals(candidate.ObjectiveId, objectiveId, StringComparison.Ordinal))
            {
                objective = candidate;
                return true;
            }
        }

        return false;
    }

    public int GetObjectiveIndex(string objectiveId)
    {
        if (string.IsNullOrWhiteSpace(objectiveId) || objectives == null)
        {
            return -1;
        }

        for (int i = 0; i < objectives.Count; i++)
        {
            if (objectives[i] != null
                && string.Equals(objectives[i].ObjectiveId, objectiveId, StringComparison.Ordinal))
            {
                return i;
            }
        }

        return -1;
    }

    public bool Validate(out string error)
    {
        error = string.Empty;
        if (string.IsNullOrWhiteSpace(QuestId))
        {
            error = $"Quest definition '{name}' has an empty questId.";
            return false;
        }

        if (string.IsNullOrWhiteSpace(GiverNpcId) || string.IsNullOrWhiteSpace(TurnInNpcId))
        {
            error = $"Quest '{QuestId}' has an empty giver or turn-in NPC ID.";
            return false;
        }

        if (objectives == null || objectives.Count == 0)
        {
            error = $"Quest '{QuestId}' has no objectives.";
            return false;
        }

        HashSet<string> objectiveIds = new(StringComparer.Ordinal);
        for (int i = 0; i < objectives.Count; i++)
        {
            QuestObjectiveDefinition objective = objectives[i];
            if (objective == null
                || string.IsNullOrWhiteSpace(objective.ObjectiveId)
                || string.IsNullOrWhiteSpace(objective.TargetId)
                || !Enum.IsDefined(typeof(QuestObjectiveType), objective.ObjectiveType))
            {
                error = $"Quest '{QuestId}' objective {i} is invalid.";
                return false;
            }

            if (!objectiveIds.Add(objective.ObjectiveId))
            {
                error = $"Quest '{QuestId}' has duplicate objectiveId '{objective.ObjectiveId}'.";
                return false;
            }
        }

        if (rewardItem != null && RewardQuantity > rewardItem.MaxStackSize)
        {
            error = $"Quest '{QuestId}' reward quantity exceeds item stack size.";
            return false;
        }

        return true;
    }

    private void OnValidate()
    {
        questId = questId != null ? questId.Trim() : string.Empty;
        title = title != null ? title.Trim() : string.Empty;
        giverNpcId = giverNpcId != null ? giverNpcId.Trim() : string.Empty;
        turnInNpcId = turnInNpcId != null ? turnInNpcId.Trim() : string.Empty;
        objectives ??= new List<QuestObjectiveDefinition>();
        rewardQuantity = Mathf.Max(1, rewardQuantity);
    }
}
