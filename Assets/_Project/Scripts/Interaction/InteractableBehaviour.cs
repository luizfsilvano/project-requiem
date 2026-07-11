using System;
using UnityEngine;

public abstract class InteractableBehaviour : MonoBehaviour, IInteractable
{
    [Header("Interaction")]
    [SerializeField] private string displayName;
    [SerializeField, TextArea(2, 4)] private string description;
    [SerializeField] private int priority = 10;
    [SerializeField, Min(0.25f)] private float interactionRange = 3.5f;
    [SerializeField] private Transform interactionPoint;

    public event Action InteractionStateChanged;

    public abstract InteractionKind Kind { get; }
    public string DisplayName => string.IsNullOrWhiteSpace(displayName) ? gameObject.name : displayName;
    public string Description => description ?? string.Empty;
    public abstract string ActionName { get; }
    public int Priority => priority;
    public float InteractionRange => Mathf.Max(0.25f, interactionRange);
    public Transform InteractionPoint => interactionPoint != null ? interactionPoint : transform;
    public virtual bool IsAvailable => isActiveAndEnabled;

    public virtual bool CanInteract(InteractionContext context)
    {
        return IsAvailable && context.Actor != null;
    }

    public virtual InteractionPromptData GetPrompt(InteractionContext context)
    {
        string action = ActionName;
        return new InteractionPromptData(Kind, action, DisplayName, $"E — {action}");
    }

    public abstract bool TryInteract(InteractionContext context);

    protected void NotifyInteractionStateChanged()
    {
        InteractionStateChanged?.Invoke();
    }

    protected virtual void OnValidate()
    {
        interactionRange = Mathf.Max(0.25f, interactionRange);
    }
}
