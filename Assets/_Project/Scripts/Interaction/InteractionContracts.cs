using System;
using UnityEngine;

public enum InteractionKind
{
    Pickup,
    ItemContainer,
    Door,
    Dialogue,
    Other
}

public readonly struct InteractionContext
{
    public InteractionContext(PlayerInteractor interactor, GameObject actor)
    {
        Interactor = interactor;
        Actor = actor;
    }

    public PlayerInteractor Interactor { get; }
    public GameObject Actor { get; }
    public Transform ActorTransform => Actor != null ? Actor.transform : null;

    public T GetActorComponent<T>() where T : Component
    {
        return Actor != null ? Actor.GetComponent<T>() : null;
    }
}

public readonly struct InteractionPromptData
{
    public InteractionPromptData(
        InteractionKind kind,
        string actionName,
        string targetName,
        string text)
    {
        Kind = kind;
        ActionName = actionName ?? string.Empty;
        TargetName = targetName ?? string.Empty;
        Text = text ?? string.Empty;
    }

    public InteractionKind Kind { get; }
    public string ActionName { get; }
    public string TargetName { get; }
    public string Text { get; }
}

public interface IInteractable
{
    event Action InteractionStateChanged;

    InteractionKind Kind { get; }
    string DisplayName { get; }
    string Description { get; }
    string ActionName { get; }
    int Priority { get; }
    float InteractionRange { get; }
    Transform InteractionPoint { get; }
    bool IsAvailable { get; }

    bool CanInteract(InteractionContext context);
    InteractionPromptData GetPrompt(InteractionContext context);
    bool TryInteract(InteractionContext context);
}
