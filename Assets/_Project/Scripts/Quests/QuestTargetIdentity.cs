using System;
using UnityEngine;

public enum QuestTargetKind
{
    Other,
    Npc,
    Enemy,
    WorldObject
}

[DisallowMultipleComponent]
public sealed class QuestTargetIdentity : MonoBehaviour
{
    [SerializeField] private string targetId;
    [SerializeField] private string stableInstanceId;
    [SerializeField] private QuestTargetKind targetKind;
    [SerializeField] private TrainingDummyHealth enemyHealth;
    [SerializeField] private bool autoStartCombat;

    private string runtimeInstanceId;
    private bool defeatReported;

    public string TargetId => targetId != null ? targetId.Trim() : string.Empty;
    public string StableInstanceId => stableInstanceId != null ? stableInstanceId.Trim() : string.Empty;
    public string InstanceId
    {
        get
        {
            if (!string.IsNullOrWhiteSpace(StableInstanceId))
            {
                return StableInstanceId;
            }

            runtimeInstanceId ??= $"runtime.{Guid.NewGuid():N}";
            return runtimeInstanceId;
        }
    }
    public QuestTargetKind TargetKind => targetKind;

    private void Awake()
    {
        if (enemyHealth == null && targetKind == QuestTargetKind.Enemy)
        {
            enemyHealth = GetComponent<TrainingDummyHealth>();
        }
    }

    private void OnEnable()
    {
        if (enemyHealth != null)
        {
            enemyHealth.Died += HandleEnemyDied;
        }
    }

    private void Start()
    {
        if (autoStartCombat && targetKind == QuestTargetKind.Enemy)
        {
            GetComponent<EnemyPlaceholderAI>()?.SetCombatEnabled(true);
        }
    }

    private void OnDisable()
    {
        if (enemyHealth != null)
        {
            enemyHealth.Died -= HandleEnemyDied;
        }
    }

    private void HandleEnemyDied(TrainingDummyHealth _, Transform attacker)
    {
        if (defeatReported
            || targetKind != QuestTargetKind.Enemy
            || string.IsNullOrWhiteSpace(TargetId)
            || attacker == null
            || attacker.GetComponentInParent<PlayerInventory>() == null)
        {
            return;
        }

        defeatReported = true;
        QuestSignalHub.PublishTargetDefeated(new QuestDefeatSignal(TargetId, InstanceId, attacker));
    }

    private void OnValidate()
    {
        targetId = targetId != null ? targetId.Trim() : string.Empty;
        stableInstanceId = stableInstanceId != null ? stableInstanceId.Trim() : string.Empty;
        if (enemyHealth == null && targetKind == QuestTargetKind.Enemy)
        {
            enemyHealth = GetComponent<TrainingDummyHealth>();
        }
    }
}
