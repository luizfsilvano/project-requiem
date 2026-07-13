using System;
using System.Collections.Generic;

[Serializable]
public sealed class QuestObjectiveProgressSnapshot
{
    public string objectiveId;
    public int progress;
    public List<string> creditedTargetInstanceIds = new();

    public QuestObjectiveProgressSnapshot DeepClone()
    {
        return new QuestObjectiveProgressSnapshot
        {
            objectiveId = objectiveId ?? string.Empty,
            progress = progress,
            creditedTargetInstanceIds = creditedTargetInstanceIds != null
                ? new List<string>(creditedTargetInstanceIds)
                : new List<string>()
        };
    }
}

[Serializable]
public sealed class QuestProgressSnapshot
{
    public string questId;
    public int state;
    public string currentObjectiveId;
    public List<QuestObjectiveProgressSnapshot> objectives = new();
    public int rewardStatus;
    public string rewardItemInstanceId;
    public string rewardDefinitionId;
    public int rewardQuantity;

    public QuestProgressSnapshot DeepClone()
    {
        QuestProgressSnapshot clone = new()
        {
            questId = questId ?? string.Empty,
            state = state,
            currentObjectiveId = currentObjectiveId ?? string.Empty,
            rewardStatus = rewardStatus,
            rewardItemInstanceId = rewardItemInstanceId ?? string.Empty,
            rewardDefinitionId = rewardDefinitionId ?? string.Empty,
            rewardQuantity = rewardQuantity
        };

        if (objectives != null)
        {
            for (int i = 0; i < objectives.Count; i++)
            {
                clone.objectives.Add(objectives[i]?.DeepClone());
            }
        }

        return clone;
    }
}

[Serializable]
public sealed class QuestLogSnapshot
{
    public string trackedQuestId;
    public List<QuestProgressSnapshot> quests = new();

    public QuestLogSnapshot DeepClone()
    {
        QuestLogSnapshot clone = new()
        {
            trackedQuestId = trackedQuestId ?? string.Empty
        };

        if (quests != null)
        {
            for (int i = 0; i < quests.Count; i++)
            {
                clone.quests.Add(quests[i]?.DeepClone());
            }
        }

        return clone;
    }
}

public sealed class QuestRestorePlan
{
    internal QuestRestorePlan(QuestLogSnapshot snapshot, int ignoredQuestCount)
    {
        Snapshot = snapshot;
        IgnoredQuestCount = ignoredQuestCount;
    }

    internal QuestLogSnapshot Snapshot { get; }
    public int IgnoredQuestCount { get; }
}

public sealed class QuestRuntimeState
{
    internal QuestRuntimeState(QuestDefinition definition, QuestProgressSnapshot data)
    {
        Definition = definition;
        Data = data;
    }

    internal QuestProgressSnapshot Data { get; }
    public QuestDefinition Definition { get; }
    public string QuestId => Definition != null ? Definition.QuestId : Data?.questId ?? string.Empty;
    public QuestState State => Data != null && Enum.IsDefined(typeof(QuestState), Data.state)
        ? (QuestState)Data.state
        : QuestState.Unavailable;
    public QuestRewardStatus RewardStatus => Data != null && Enum.IsDefined(typeof(QuestRewardStatus), Data.rewardStatus)
        ? (QuestRewardStatus)Data.rewardStatus
        : QuestRewardStatus.None;
    public string CurrentObjectiveId => Data?.currentObjectiveId ?? string.Empty;
    public string RewardItemInstanceId => Data?.rewardItemInstanceId ?? string.Empty;
    public bool IsTrackable => State == QuestState.Active || State == QuestState.ReadyToTurnIn;

    public QuestObjectiveDefinition CurrentObjective => Definition != null
        && Definition.TryGetObjective(CurrentObjectiveId, out QuestObjectiveDefinition objective)
            ? objective
            : null;

    public int GetProgress(string objectiveId)
    {
        QuestObjectiveProgressSnapshot progress = GetObjectiveProgress(objectiveId);
        return progress != null ? progress.progress : 0;
    }

    public QuestObjectiveProgressSnapshot GetObjectiveProgress(string objectiveId)
    {
        if (Data?.objectives == null || string.IsNullOrWhiteSpace(objectiveId))
        {
            return null;
        }

        for (int i = 0; i < Data.objectives.Count; i++)
        {
            QuestObjectiveProgressSnapshot progress = Data.objectives[i];
            if (progress != null && string.Equals(progress.objectiveId, objectiveId, StringComparison.Ordinal))
            {
                return progress;
            }
        }

        return null;
    }
}
