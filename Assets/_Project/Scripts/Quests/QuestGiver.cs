using UnityEngine;

public enum QuestDialogueAction
{
    None,
    Accept,
    Decline,
    TurnIn
}

public readonly struct QuestDialogueViewModel
{
    public QuestDialogueViewModel(
        string line,
        QuestDialogueAction primaryAction,
        string primaryLabel,
        QuestDialogueAction secondaryAction,
        string secondaryLabel)
    {
        Line = line ?? string.Empty;
        PrimaryAction = primaryAction;
        PrimaryLabel = primaryLabel ?? string.Empty;
        SecondaryAction = secondaryAction;
        SecondaryLabel = secondaryLabel ?? string.Empty;
    }

    public string Line { get; }
    public QuestDialogueAction PrimaryAction { get; }
    public string PrimaryLabel { get; }
    public QuestDialogueAction SecondaryAction { get; }
    public string SecondaryLabel { get; }
}

[DisallowMultipleComponent]
public sealed class QuestGiver : MonoBehaviour
{
    [SerializeField] private string npcId;
    [SerializeField] private QuestDefinition questDefinition;

    public string NpcId => npcId != null ? npcId.Trim() : string.Empty;
    public QuestDefinition QuestDefinition => questDefinition;

    public QuestDialogueViewModel BuildDialogue(PlayerQuestLog questLog, string fallbackLine)
    {
        if (questLog == null
            || questDefinition == null
            || !questLog.TryGetQuest(questDefinition.QuestId, out QuestRuntimeState state))
        {
            return new QuestDialogueViewModel(
                fallbackLine,
                QuestDialogueAction.None,
                string.Empty,
                QuestDialogueAction.None,
                string.Empty);
        }

        return state.State switch
        {
            QuestState.Available => new QuestDialogueViewModel(
                questDefinition.OfferText,
                QuestDialogueAction.Accept,
                "Aceitar",
                QuestDialogueAction.Decline,
                "Recusar"),
            QuestState.Active => new QuestDialogueViewModel(
                questDefinition.ActiveText,
                QuestDialogueAction.None,
                string.Empty,
                QuestDialogueAction.None,
                string.Empty),
            QuestState.ReadyToTurnIn => new QuestDialogueViewModel(
                questDefinition.ReadyText,
                QuestDialogueAction.TurnIn,
                "Concluir",
                QuestDialogueAction.None,
                string.Empty),
            QuestState.Completed => new QuestDialogueViewModel(
                questDefinition.CompletedText,
                QuestDialogueAction.None,
                string.Empty,
                QuestDialogueAction.None,
                string.Empty),
            _ => new QuestDialogueViewModel(
                fallbackLine,
                QuestDialogueAction.None,
                string.Empty,
                QuestDialogueAction.None,
                string.Empty)
        };
    }

    public bool TryAccept(PlayerQuestLog questLog, out string feedback)
    {
        if (questLog == null || questDefinition == null)
        {
            feedback = "Missão indisponível.";
            return false;
        }

        return questLog.TryAcceptQuest(questDefinition.QuestId, out feedback);
    }

    public bool TryTurnIn(PlayerQuestLog questLog, out string feedback)
    {
        if (questLog == null
            || questDefinition == null
            || !string.Equals(NpcId, questDefinition.TurnInNpcId, System.StringComparison.Ordinal))
        {
            feedback = "Esta missão não pode ser entregue aqui.";
            return false;
        }

        return questLog.TryTurnInQuest(questDefinition.QuestId, out feedback);
    }

    public string GetDeclinedLine()
    {
        return questDefinition != null ? questDefinition.DeclinedText : string.Empty;
    }

    private void OnValidate()
    {
        npcId = npcId != null ? npcId.Trim() : string.Empty;
    }
}
