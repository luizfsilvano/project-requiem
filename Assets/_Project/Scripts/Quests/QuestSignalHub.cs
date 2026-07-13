using System;
using UnityEngine;

public readonly struct QuestDefeatSignal
{
    public QuestDefeatSignal(string targetId, string targetInstanceId, Transform attacker)
    {
        TargetId = targetId ?? string.Empty;
        TargetInstanceId = targetInstanceId ?? string.Empty;
        Attacker = attacker;
    }

    public string TargetId { get; }
    public string TargetInstanceId { get; }
    public Transform Attacker { get; }
}

public static class QuestSignalHub
{
    public static event Action<QuestDefeatSignal> TargetDefeated;

    [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
    private static void ResetBeforeSceneLoad()
    {
        TargetDefeated = null;
    }

    public static void PublishTargetDefeated(QuestDefeatSignal signal)
    {
        if (!string.IsNullOrWhiteSpace(signal.TargetId)
            && !string.IsNullOrWhiteSpace(signal.TargetInstanceId))
        {
            TargetDefeated?.Invoke(signal);
        }
    }
}
