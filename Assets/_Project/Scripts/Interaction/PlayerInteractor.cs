using System;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.InputSystem;

[DefaultExecutionOrder(20)]
public sealed class PlayerInteractor : MonoBehaviour
{
    [Header("Detection")]
    [SerializeField] private Transform cameraTransform;
    [SerializeField, Min(0.5f)] private float searchRange = 4f;
    [SerializeField, Min(0.1f)] private float searchRadius = 1.35f;
    [SerializeField, Range(1f, 179f)] private float maxInteractionAngle = 75f;
    [SerializeField, Min(0f)] private float originHeight = 1f;
    [SerializeField, Min(0.02f)] private float searchInterval = 0.08f;
    [SerializeField] private LayerMask interactionMask = ~0;

    [Header("Action Locks")]
    [SerializeField] private BasicMeleeAttack meleeAttack;
    [SerializeField] private PlayerCameraRelativeMovement movement;

    [Header("Interaction UI")]
    [SerializeField] private InteractionPromptView promptView;
    [SerializeField] private ContainerTransferScreen containerScreen;
    [SerializeField] private DialoguePanel dialoguePanel;

    private readonly Collider[] overlapResults = new Collider[48];
    private readonly HashSet<int> evaluatedInteractables = new();
    private InteractableBehaviour currentTarget;
    private float nextSearchTime;

    public event Action<IInteractable> CurrentTargetChanged;

    public IInteractable CurrentTarget => currentTarget;
    public float SearchRange => searchRange;
    public bool IsActionLocked => (meleeAttack != null && meleeAttack.IsAttacking)
        || (movement != null && movement.IsDodging);

    private void Awake()
    {
        ResolveDependencies();
    }

    private void OnEnable()
    {
        GameplayInputGate.ModalChanged += HandleModalChanged;
    }

    private void OnDisable()
    {
        GameplayInputGate.ModalChanged -= HandleModalChanged;
        SetCurrentTarget(null);
    }

    private void Update()
    {
        if (GameplayInputGate.IsBlocked || IsActionLocked)
        {
            SetCurrentTarget(null);
            return;
        }

        InteractionContext context = new(this, gameObject);
        if (currentTarget != null && !IsCandidateValid(currentTarget, context, out _, out _))
        {
            SetCurrentTarget(null);
        }

        if (Time.unscaledTime >= nextSearchTime)
        {
            nextSearchTime = Time.unscaledTime + searchInterval;
            RefreshTarget(context);
        }

        if (currentTarget == null
            || Keyboard.current == null
            || !Keyboard.current.eKey.wasPressedThisFrame)
        {
            return;
        }

        InteractableBehaviour target = currentTarget;
        if (!IsCandidateValid(target, context, out _, out _))
        {
            SetCurrentTarget(null);
            return;
        }

        bool succeeded = target.TryInteract(context);
        nextSearchTime = Time.unscaledTime + searchInterval;
        if (succeeded && (GameplayInputGate.IsModalOpen || target == null || !target.IsAvailable))
        {
            SetCurrentTarget(null);
        }
        else
        {
            RefreshPrompt(context);
        }
    }

    public bool TryOpenContainer(WorldItemContainer container)
    {
        return container != null
            && containerScreen != null
            && containerScreen.Open(container, this);
    }

    public bool TryOpenDialogue(SimpleNpcInteractable npc)
    {
        return npc != null
            && dialoguePanel != null
            && dialoguePanel.Open(npc, this);
    }

    private void ResolveDependencies()
    {
        if (cameraTransform == null && Camera.main != null)
        {
            cameraTransform = Camera.main.transform;
        }

        if (meleeAttack == null)
        {
            meleeAttack = GetComponent<BasicMeleeAttack>();
        }

        if (movement == null)
        {
            movement = GetComponent<PlayerCameraRelativeMovement>();
        }

        if (promptView == null)
        {
            promptView = FindFirstObjectByType<InteractionPromptView>(FindObjectsInactive.Include);
        }

        if (containerScreen == null)
        {
            containerScreen = FindFirstObjectByType<ContainerTransferScreen>(FindObjectsInactive.Include);
        }

        if (dialoguePanel == null)
        {
            dialoguePanel = FindFirstObjectByType<DialoguePanel>(FindObjectsInactive.Include);
        }
    }

    private void RefreshTarget(InteractionContext context)
    {
        Vector3 origin = GetSearchOrigin();
        Vector3 forward = GetPlanarForward();
        Vector3 end = origin + forward * Mathf.Max(0f, searchRange - searchRadius);
        int hitCount = Physics.OverlapCapsuleNonAlloc(
            origin,
            end,
            searchRadius,
            overlapResults,
            interactionMask,
            QueryTriggerInteraction.Collide);

        evaluatedInteractables.Clear();
        InteractableBehaviour best = null;
        int bestPriority = int.MinValue;
        float bestAlignment = float.NegativeInfinity;
        float bestDistance = float.PositiveInfinity;
        int bestInstanceId = int.MaxValue;

        for (int i = 0; i < hitCount; i++)
        {
            Collider candidateCollider = overlapResults[i];
            if (candidateCollider == null)
            {
                continue;
            }

            InteractableBehaviour candidate = candidateCollider.GetComponentInParent<InteractableBehaviour>();
            if (candidate == null || !evaluatedInteractables.Add(candidate.GetInstanceID()))
            {
                continue;
            }

            if (!IsCandidateValid(candidate, context, out float alignment, out float distance))
            {
                continue;
            }

            int instanceId = candidate.GetInstanceID();
            bool isBetter = candidate.Priority > bestPriority
                || candidate.Priority == bestPriority && alignment > bestAlignment + 0.0001f
                || candidate.Priority == bestPriority
                    && Mathf.Abs(alignment - bestAlignment) <= 0.0001f
                    && distance < bestDistance - 0.0001f
                || candidate.Priority == bestPriority
                    && Mathf.Abs(alignment - bestAlignment) <= 0.0001f
                    && Mathf.Abs(distance - bestDistance) <= 0.0001f
                    && instanceId < bestInstanceId;

            if (!isBetter)
            {
                continue;
            }

            best = candidate;
            bestPriority = candidate.Priority;
            bestAlignment = alignment;
            bestDistance = distance;
            bestInstanceId = instanceId;
        }

        SetCurrentTarget(best);
    }

    private bool IsCandidateValid(
        InteractableBehaviour candidate,
        InteractionContext context,
        out float alignment,
        out float distance)
    {
        alignment = -1f;
        distance = float.PositiveInfinity;
        if (candidate == null || !candidate.IsAvailable || !candidate.CanInteract(context))
        {
            return false;
        }

        Transform point = candidate.InteractionPoint;
        if (point == null || !point.gameObject.activeInHierarchy)
        {
            return false;
        }

        Vector3 origin = GetSearchOrigin();
        Vector3 toTarget = point.position - origin;
        distance = toTarget.magnitude;
        float allowedRange = Mathf.Min(searchRange, candidate.InteractionRange);
        if (distance > allowedRange)
        {
            return false;
        }

        Vector3 planarTarget = Vector3.ProjectOnPlane(toTarget, Vector3.up);
        alignment = planarTarget.sqrMagnitude <= 0.0001f
            ? 1f
            : Vector3.Dot(GetPlanarForward(), planarTarget.normalized);
        float minimumAlignment = Mathf.Cos(maxInteractionAngle * Mathf.Deg2Rad);
        return alignment >= minimumAlignment;
    }

    private Vector3 GetSearchOrigin()
    {
        return transform.position + Vector3.up * originHeight;
    }

    private Vector3 GetPlanarForward()
    {
        Vector3 forward = cameraTransform != null ? cameraTransform.forward : transform.forward;
        forward = Vector3.ProjectOnPlane(forward, Vector3.up);
        return forward.sqrMagnitude > 0.0001f ? forward.normalized : transform.forward;
    }

    private void SetCurrentTarget(InteractableBehaviour target)
    {
        if (ReferenceEquals(currentTarget, target))
        {
            return;
        }

        if (currentTarget != null)
        {
            currentTarget.InteractionStateChanged -= HandleTargetStateChanged;
        }

        currentTarget = target;
        if (currentTarget != null)
        {
            currentTarget.InteractionStateChanged += HandleTargetStateChanged;
        }

        CurrentTargetChanged?.Invoke(currentTarget);
        RefreshPrompt(new InteractionContext(this, gameObject));
    }

    private void RefreshPrompt(InteractionContext context)
    {
        if (promptView == null)
        {
            return;
        }

        if (currentTarget == null || GameplayInputGate.IsModalOpen || !currentTarget.CanInteract(context))
        {
            promptView.Hide();
            return;
        }

        promptView.Show(currentTarget.GetPrompt(context));
    }

    private void HandleTargetStateChanged()
    {
        InteractionContext context = new(this, gameObject);
        if (currentTarget == null || !IsCandidateValid(currentTarget, context, out _, out _))
        {
            SetCurrentTarget(null);
            return;
        }

        RefreshPrompt(context);
    }

    private void HandleModalChanged(GameplayModalMode previousMode, GameplayModalMode mode)
    {
        if (mode != GameplayModalMode.None)
        {
            SetCurrentTarget(null);
        }
    }

    private void OnDrawGizmosSelected()
    {
        Vector3 origin = transform.position + Vector3.up * originHeight;
        Vector3 forward = cameraTransform != null ? cameraTransform.forward : transform.forward;
        forward = Vector3.ProjectOnPlane(forward, Vector3.up).normalized;
        Gizmos.color = new Color(0.82f, 0.63f, 0.22f, 0.7f);
        Gizmos.DrawWireSphere(origin, searchRadius);
        Gizmos.DrawWireSphere(origin + forward * Mathf.Max(0f, searchRange - searchRadius), searchRadius);
    }
}
