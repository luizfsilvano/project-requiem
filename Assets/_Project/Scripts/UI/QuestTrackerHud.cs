using UnityEngine;
using UnityEngine.UI;

public sealed class QuestTrackerHud : MonoBehaviour
{
    [SerializeField] private GameObject trackerRoot;
    [SerializeField] private PlayerQuestLog questLog;
    [SerializeField] private Text titleText;
    [SerializeField] private Text objectiveText;
    [SerializeField] private Text stateText;

    private void Awake()
    {
        ResolveDependencies();
        Refresh();
    }

    private void OnEnable()
    {
        ResolveDependencies();
        Subscribe();
        Refresh();
    }

    private void OnDisable()
    {
        Unsubscribe();
    }

    private void OnDestroy()
    {
        Unsubscribe();
    }

    public void Refresh()
    {
        QuestRuntimeState tracked = null;
        if (questLog != null && !string.IsNullOrWhiteSpace(questLog.TrackedQuestId))
        {
            questLog.TryGetQuest(questLog.TrackedQuestId, out tracked);
        }
        bool visible = tracked?.Definition != null
            && (tracked.State == QuestState.Active || tracked.State == QuestState.ReadyToTurnIn);
        if (trackerRoot != null && trackerRoot.activeSelf != visible)
        {
            trackerRoot.SetActive(visible);
        }

        if (!visible)
        {
            return;
        }

        SetText(titleText, tracked.Definition.Title);
        SetText(objectiveText, QuestUiFormatting.GetCompactProgress(tracked));
        SetText(stateText, tracked.State == QuestState.ReadyToTurnIn
            ? "PRONTA PARA ENTREGA"
            : string.Empty);
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
        if (questLog == null)
        {
            return;
        }

        questLog.QuestLogChanged -= HandleQuestLogChanged;
        questLog.QuestLogChanged += HandleQuestLogChanged;
    }

    private void Unsubscribe()
    {
        if (questLog == null)
        {
            return;
        }

        questLog.QuestLogChanged -= HandleQuestLogChanged;
    }

    private void HandleQuestLogChanged()
    {
        Refresh();
    }

    private static void SetText(Text text, string value)
    {
        if (text != null)
        {
            text.text = value ?? string.Empty;
            text.gameObject.SetActive(!string.IsNullOrWhiteSpace(value));
        }
    }
}
