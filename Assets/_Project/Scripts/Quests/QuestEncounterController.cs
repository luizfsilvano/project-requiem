using System;
using System.Collections;
using UnityEngine;

[DisallowMultipleComponent]
public sealed class QuestEncounterController : MonoBehaviour
{
    [SerializeField] private PlayerQuestLog questLog;
    [SerializeField] private string questId;
    [SerializeField] private string objectiveId;
    [SerializeField] private GameObject[] encounterObjects = Array.Empty<GameObject>();

    private bool subscribed;
    private Coroutine deactivateRoutine;

    public string QuestId => questId != null ? questId.Trim() : string.Empty;
    public string ObjectiveId => objectiveId != null ? objectiveId.Trim() : string.Empty;
    public int EncounterObjectCount => encounterObjects?.Length ?? 0;

    private void Awake()
    {
        ResolveDependencies();
        ApplyCurrentState();
    }

    private void OnEnable()
    {
        ResolveDependencies();
        Subscribe();
        ApplyCurrentState();
    }

    private void Start()
    {
        ApplyCurrentState();
    }

    private void OnDisable()
    {
        CancelDeferredDeactivation();
        Unsubscribe();
    }

    private void OnDestroy()
    {
        Unsubscribe();
    }

    private void ApplyCurrentState()
    {
        QuestRuntimeState state = null;
        bool hasQuest = questLog != null
            && !string.IsNullOrWhiteSpace(QuestId)
            && questLog.TryGetQuest(QuestId, out state);
        ApplyState(hasQuest ? state : null);
    }

    private void ApplyState(QuestRuntimeState quest)
    {
        bool shouldBeActive = quest != null
            && quest.State == QuestState.Active
            && quest.CurrentObjective != null
            && string.Equals(quest.CurrentObjective.ObjectiveId, ObjectiveId, StringComparison.Ordinal);

        if (encounterObjects == null)
        {
            return;
        }

        if (shouldBeActive)
        {
            CancelDeferredDeactivation();
            SetEncounterObjectsActive(true);
            return;
        }

        if (!Application.isPlaying || !isActiveAndEnabled)
        {
            SetEncounterObjectsActive(false);
            return;
        }

        if (deactivateRoutine == null && HasActiveEncounterObject())
        {
            deactivateRoutine = StartCoroutine(DeactivateAfterCurrentFrame());
        }
    }

    private IEnumerator DeactivateAfterCurrentFrame()
    {
        yield return null;
        deactivateRoutine = null;
        SetEncounterObjectsActive(false);
    }

    private void SetEncounterObjectsActive(bool active)
    {
        for (int i = 0; i < encounterObjects.Length; i++)
        {
            GameObject encounterObject = encounterObjects[i];
            if (encounterObject != null && encounterObject.activeSelf != active)
            {
                encounterObject.SetActive(active);
            }
        }
    }

    private bool HasActiveEncounterObject()
    {
        for (int i = 0; i < encounterObjects.Length; i++)
        {
            if (encounterObjects[i] != null && encounterObjects[i].activeSelf)
            {
                return true;
            }
        }

        return false;
    }

    private void CancelDeferredDeactivation()
    {
        if (deactivateRoutine == null)
        {
            return;
        }

        StopCoroutine(deactivateRoutine);
        deactivateRoutine = null;
    }

    private void ResolveDependencies()
    {
        if (questLog == null)
        {
            questLog = FindFirstObjectByType<PlayerQuestLog>(FindObjectsInactive.Include);
        }
    }

    private void Subscribe()
    {
        if (subscribed || questLog == null)
        {
            return;
        }

        questLog.QuestLogChanged += ApplyCurrentState;
        subscribed = true;
    }

    private void Unsubscribe()
    {
        if (!subscribed)
        {
            return;
        }

        if (questLog != null)
        {
            questLog.QuestLogChanged -= ApplyCurrentState;
        }

        subscribed = false;
    }

    private void OnValidate()
    {
        questId = questId != null ? questId.Trim() : string.Empty;
        objectiveId = objectiveId != null ? objectiveId.Trim() : string.Empty;
    }
}
