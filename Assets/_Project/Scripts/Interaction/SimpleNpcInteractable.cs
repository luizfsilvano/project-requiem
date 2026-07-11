using UnityEngine;

public sealed class SimpleNpcInteractable : InteractableBehaviour
{
    [Header("Dialogue")]
    [SerializeField] private string speakerName = "Aldeão";
    [SerializeField, TextArea(2, 6)] private string dialogueLine =
        "A floresta está perigosa desde que os esqueletos apareceram.";

    public override InteractionKind Kind => InteractionKind.Dialogue;
    public override string ActionName => $"Falar com {SpeakerName}";
    public string SpeakerName => string.IsNullOrWhiteSpace(speakerName) ? DisplayName : speakerName;
    public string DialogueLine => dialogueLine ?? string.Empty;

    public override bool CanInteract(InteractionContext context)
    {
        return context.Interactor != null && base.CanInteract(context);
    }

    public override InteractionPromptData GetPrompt(InteractionContext context)
    {
        string action = ActionName;
        return new InteractionPromptData(Kind, action, SpeakerName, $"E — {action}");
    }

    public override bool TryInteract(InteractionContext context)
    {
        return CanInteract(context) && context.Interactor.TryOpenDialogue(this);
    }
}
