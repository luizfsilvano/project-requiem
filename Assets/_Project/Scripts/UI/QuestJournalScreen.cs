using System;
using System.Collections.Generic;
using System.Text;
using UnityEngine;
using UnityEngine.InputSystem;
using UnityEngine.UI;

[DefaultExecutionOrder(-5)]
public sealed class QuestJournalScreen : MonoBehaviour
{
    [Header("Screen")]
    [SerializeField] private GameObject screenRoot;
    [SerializeField] private PlayerQuestLog questLog;

    [Header("Quest Lists")]
    [SerializeField] private Transform activeQuestList;
    [SerializeField] private Transform completedQuestList;
    [SerializeField] private Button questEntryTemplate;
    [SerializeField] private Text emptyActiveText;
    [SerializeField] private Text emptyCompletedText;

    [Header("Details")]
    [SerializeField] private Text titleText;
    [SerializeField] private Text stateText;
    [SerializeField] private Text descriptionText;
    [SerializeField] private Text objectivesText;
    [SerializeField] private Text rewardText;
    [SerializeField] private Text feedbackText;

    [Header("Actions")]
    [SerializeField] private Button trackButton;
    [SerializeField] private Text trackButtonText;
    [SerializeField] private Button closeButton;

    private readonly List<Button> spawnedEntries = new();
    private string selectedQuestId = string.Empty;
    private bool listenersBound;

    public bool IsOpen { get; private set; }
    public string SelectedQuestId => selectedQuestId;

    private void Awake()
    {
        ResolveDependencies();
        BindButtonListeners();
        SetScreenVisible(false);
        if (questEntryTemplate != null)
        {
            questEntryTemplate.gameObject.SetActive(false);
        }
    }

    private void OnEnable()
    {
        ResolveDependencies();
        SubscribeToQuestEvents();
    }

    private void OnDisable()
    {
        UnsubscribeFromQuestEvents();
        if (Application.isPlaying && IsOpen)
        {
            CloseScreen();
        }
    }

    private void OnDestroy()
    {
        UnsubscribeFromQuestEvents();
        UnbindButtonListeners();
        ClearEntries();
        if (GameplayInputGate.IsOwnedBy(GameplayModalMode.QuestJournal, this))
        {
            GameplayInputGate.TryCloseModal(GameplayModalMode.QuestJournal, this);
        }
    }

    private void Update()
    {
        if (Keyboard.current == null)
        {
            return;
        }

        if (Keyboard.current.jKey.wasPressedThisFrame)
        {
            if (IsOpen)
            {
                CloseScreen();
            }
            else
            {
                OpenScreen();
            }

            return;
        }

        if (IsOpen && Keyboard.current.escapeKey.wasPressedThisFrame)
        {
            CloseScreen();
        }
    }

    public void OpenScreen()
    {
        ResolveDependencies();
        if (IsOpen
            || questLog == null
            || !GameplayInputGate.TryOpenModal(GameplayModalMode.QuestJournal, this))
        {
            return;
        }

        IsOpen = true;
        SetScreenVisible(true);
        GameplayInputGate.EnsureModalCursor();
        RefreshAll();
    }

    public void CloseScreen()
    {
        if (!IsOpen)
        {
            return;
        }

        IsOpen = false;
        SetScreenVisible(false);
        GameplayInputGate.TryCloseModal(GameplayModalMode.QuestJournal, this, 1);
    }

    public void SelectQuest(string questId)
    {
        selectedQuestId = questId != null ? questId.Trim() : string.Empty;
        RefreshAll();
    }

    public void ToggleTrackedQuest()
    {
        if (questLog == null || string.IsNullOrWhiteSpace(selectedQuestId))
        {
            SetFeedback("Selecione uma missao ativa para rastrear.");
            return;
        }

        if (string.Equals(questLog.TrackedQuestId, selectedQuestId, StringComparison.Ordinal))
        {
            questLog.TrySetTrackedQuest(string.Empty);
            SetFeedback("Rastreamento removido.");
        }
        else if (questLog.TrySetTrackedQuest(selectedQuestId))
        {
            SetFeedback("Missao rastreada.");
        }
        else
        {
            SetFeedback("Apenas missoes ativas ou prontas para entrega podem ser rastreadas.");
        }

        RefreshAll();
    }

    private void RefreshAll()
    {
        if (!IsOpen || questLog == null)
        {
            return;
        }

        ResolveSelection();
        RebuildQuestLists();
        RefreshDetails();
    }

    private void ResolveSelection()
    {
        if (!string.IsNullOrWhiteSpace(selectedQuestId)
            && questLog.TryGetQuest(selectedQuestId, out QuestRuntimeState selected)
            && QuestUiFormatting.IsJournalVisible(selected))
        {
            return;
        }

        selectedQuestId = string.Empty;
        IReadOnlyList<QuestRuntimeState> quests = questLog.Quests;
        for (int i = 0; i < quests.Count; i++)
        {
            if (QuestUiFormatting.IsJournalVisible(quests[i]))
            {
                selectedQuestId = quests[i].QuestId;
                return;
            }
        }
    }

    private void RebuildQuestLists()
    {
        ClearEntries();
        int activeCount = 0;
        int completedCount = 0;
        IReadOnlyList<QuestRuntimeState> quests = questLog.Quests;
        for (int i = 0; i < quests.Count; i++)
        {
            QuestRuntimeState quest = quests[i];
            if (quest == null || quest.Definition == null)
            {
                continue;
            }

            if (quest.State == QuestState.Active || quest.State == QuestState.ReadyToTurnIn)
            {
                CreateQuestEntry(quest, activeQuestList);
                activeCount++;
            }
            else if (quest.State == QuestState.Completed)
            {
                CreateQuestEntry(quest, completedQuestList);
                completedCount++;
            }
        }

        SetVisible(emptyActiveText, activeCount == 0);
        SetVisible(emptyCompletedText, completedCount == 0);
    }

    private void CreateQuestEntry(QuestRuntimeState quest, Transform parent)
    {
        if (questEntryTemplate == null || parent == null)
        {
            return;
        }

        Button entry = Instantiate(questEntryTemplate, parent);
        entry.gameObject.name = $"QuestEntry_{quest.QuestId}";
        entry.gameObject.SetActive(true);
        string questId = quest.QuestId;
        entry.onClick.AddListener(() => SelectQuest(questId));
        Text label = entry.GetComponentInChildren<Text>(true);
        if (label != null)
        {
            string tracked = string.Equals(questLog.TrackedQuestId, questId, StringComparison.Ordinal)
                ? "  [RASTREADA]"
                : string.Empty;
            label.text = $"{quest.Definition.Title}{tracked}\n{QuestUiFormatting.GetCompactProgress(quest)}";
            label.color = string.Equals(selectedQuestId, questId, StringComparison.Ordinal)
                ? new Color(0.95f, 0.78f, 0.4f, 1f)
                : new Color(0.9f, 0.86f, 0.76f, 1f);
        }

        spawnedEntries.Add(entry);
    }

    private void RefreshDetails()
    {
        QuestRuntimeState selected = null;
        if (!string.IsNullOrWhiteSpace(selectedQuestId))
        {
            questLog.TryGetQuest(selectedQuestId, out selected);
        }

        bool hasQuest = selected?.Definition != null;
        SetText(titleText, hasQuest ? selected.Definition.Title : "Selecione uma missao");
        SetText(stateText, hasQuest ? QuestUiFormatting.GetStateLabel(selected.State) : string.Empty);
        SetText(descriptionText, hasQuest ? selected.Definition.Description : "Missoes ativas e concluidas aparecem aqui.");
        SetText(objectivesText, hasQuest ? QuestUiFormatting.GetObjectivesText(selected) : string.Empty);
        SetText(rewardText, hasQuest ? QuestUiFormatting.GetRewardText(selected) : string.Empty);

        bool canTrack = hasQuest
            && (selected.State == QuestState.Active || selected.State == QuestState.ReadyToTurnIn);
        if (trackButton != null)
        {
            trackButton.interactable = canTrack;
        }

        if (trackButtonText != null)
        {
            trackButtonText.text = canTrack
                && string.Equals(questLog.TrackedQuestId, selected.QuestId, StringComparison.Ordinal)
                    ? "PARAR DE RASTREAR"
                    : "RASTREAR";
        }
    }

    private void ResolveDependencies()
    {
        if (questLog == null)
        {
            questLog = FindFirstObjectByType<PlayerQuestLog>(FindObjectsInactive.Include);
        }
    }

    private void SubscribeToQuestEvents()
    {
        if (questLog == null)
        {
            return;
        }

        questLog.QuestLogChanged -= HandleQuestLogChanged;
        questLog.QuestLogChanged += HandleQuestLogChanged;
    }

    private void UnsubscribeFromQuestEvents()
    {
        if (questLog == null)
        {
            return;
        }

        questLog.QuestLogChanged -= HandleQuestLogChanged;
    }

    private void HandleQuestLogChanged()
    {
        if (IsOpen)
        {
            RefreshAll();
        }
    }

    private void BindButtonListeners()
    {
        if (listenersBound)
        {
            return;
        }

        trackButton?.onClick.AddListener(ToggleTrackedQuest);
        closeButton?.onClick.AddListener(CloseScreen);
        listenersBound = true;
    }

    private void UnbindButtonListeners()
    {
        if (!listenersBound)
        {
            return;
        }

        trackButton?.onClick.RemoveListener(ToggleTrackedQuest);
        closeButton?.onClick.RemoveListener(CloseScreen);
        listenersBound = false;
    }

    private void ClearEntries()
    {
        for (int i = 0; i < spawnedEntries.Count; i++)
        {
            if (spawnedEntries[i] != null)
            {
                Destroy(spawnedEntries[i].gameObject);
            }
        }

        spawnedEntries.Clear();
    }

    private void SetScreenVisible(bool visible)
    {
        if (screenRoot != null && screenRoot.activeSelf != visible)
        {
            screenRoot.SetActive(visible);
        }
    }

    private void SetFeedback(string value)
    {
        SetText(feedbackText, value);
    }

    private static void SetVisible(Graphic graphic, bool visible)
    {
        if (graphic != null && graphic.gameObject.activeSelf != visible)
        {
            graphic.gameObject.SetActive(visible);
        }
    }

    private static void SetText(Text text, string value)
    {
        if (text != null)
        {
            text.text = value ?? string.Empty;
        }
    }
}

public static class QuestUiFormatting
{
    public static bool IsJournalVisible(QuestRuntimeState quest)
    {
        return quest != null
            && (quest.State == QuestState.Active
                || quest.State == QuestState.ReadyToTurnIn
                || quest.State == QuestState.Completed);
    }

    public static string GetStateLabel(QuestState state)
    {
        return state switch
        {
            QuestState.Available => "DISPONIVEL",
            QuestState.Active => "ATIVA",
            QuestState.ReadyToTurnIn => "PRONTA PARA ENTREGA",
            QuestState.Completed => "CONCLUIDA",
            _ => "INDISPONIVEL"
        };
    }

    public static string GetCompactProgress(QuestRuntimeState quest)
    {
        if (quest == null || quest.Definition == null)
        {
            return string.Empty;
        }

        if (quest.State == QuestState.Completed)
        {
            return "Concluida";
        }

        if (quest.State == QuestState.ReadyToTurnIn)
        {
            return "Retorne ao NPC";
        }

        QuestObjectiveDefinition objective = quest.CurrentObjective;
        if (objective == null)
        {
            return string.Empty;
        }

        int progress = quest.GetProgress(objective.ObjectiveId);
        return $"{objective.Description}  {progress}/{objective.RequiredAmount}";
    }

    public static string GetObjectivesText(QuestRuntimeState quest)
    {
        if (quest?.Definition?.Objectives == null)
        {
            return string.Empty;
        }

        StringBuilder builder = new();
        IReadOnlyList<QuestObjectiveDefinition> objectives = quest.Definition.Objectives;
        for (int i = 0; i < objectives.Count; i++)
        {
            QuestObjectiveDefinition objective = objectives[i];
            if (objective == null)
            {
                continue;
            }

            int progress = quest.GetProgress(objective.ObjectiveId);
            bool complete = progress >= objective.RequiredAmount || quest.State == QuestState.Completed;
            bool current = string.Equals(
                objective.ObjectiveId,
                quest.CurrentObjectiveId,
                StringComparison.Ordinal);
            string marker = complete ? "[x]" : current ? "[>]" : "[ ]";
            builder.Append(marker)
                .Append(' ')
                .Append(string.IsNullOrWhiteSpace(objective.Description) ? objective.ObjectiveId : objective.Description)
                .Append("  ")
                .Append(Mathf.Clamp(progress, 0, objective.RequiredAmount))
                .Append('/')
                .Append(objective.RequiredAmount);
            if (i < objectives.Count - 1)
            {
                builder.AppendLine();
            }
        }

        return builder.ToString();
    }

    public static string GetRewardText(QuestRuntimeState quest)
    {
        if (quest?.Definition?.RewardItem == null)
        {
            return "RECOMPENSA  -";
        }

        string suffix = quest.RewardStatus == QuestRewardStatus.Granted
            ? "  [RECEBIDA]"
            : quest.RewardStatus == QuestRewardStatus.PendingDelivery ? "  [PENDENTE]" : string.Empty;
        return $"RECOMPENSA  {quest.Definition.RewardQuantity}x {quest.Definition.RewardItem.DisplayName}{suffix}";
    }
}
