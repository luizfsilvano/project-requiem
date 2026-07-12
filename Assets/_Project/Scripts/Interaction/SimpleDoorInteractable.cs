using System.Collections;
using UnityEngine;

public sealed class SimpleDoorInteractable : InteractableBehaviour
{
    [Header("Door")]
    [SerializeField] private Transform doorPivot;
    [SerializeField] private Vector3 openEulerOffset = new(0f, 100f, 0f);
    [SerializeField, Min(0.05f)] private float animationDuration = 0.45f;
    [SerializeField] private AnimationCurve animationCurve = AnimationCurve.EaseInOut(0f, 0f, 1f, 1f);
    [SerializeField] private bool startsOpen;

    private Quaternion closedLocalRotation;
    private Quaternion openLocalRotation;
    private Coroutine animationRoutine;
    private bool isOpen;
    private bool isAnimating;
    private bool rotationsInitialized;
    private bool doorStateInitialized;

    public override InteractionKind Kind => InteractionKind.Door;
    public override string ActionName => isOpen ? "Fechar Porta" : "Abrir Porta";
    public override bool IsAvailable => base.IsAvailable && doorPivot != null && !isAnimating;
    public bool IsOpen => isOpen;
    public bool IsAnimating => isAnimating;

    private void Awake()
    {
        EnsureRotationsInitialized();
        if (doorPivot == null)
        {
            return;
        }

        if (!doorStateInitialized)
        {
            isOpen = startsOpen;
            doorStateInitialized = true;
        }

        doorPivot.localRotation = isOpen ? openLocalRotation : closedLocalRotation;
    }

    private void OnDisable()
    {
        EnsureRotationsInitialized();
        if (animationRoutine != null)
        {
            StopCoroutine(animationRoutine);
            animationRoutine = null;
        }

        isAnimating = false;
        if (doorPivot != null)
        {
            doorPivot.localRotation = isOpen ? openLocalRotation : closedLocalRotation;
        }

        NotifyInteractionStateChanged();
    }

    public override bool CanInteract(InteractionContext context)
    {
        return !isAnimating && doorPivot != null && base.CanInteract(context);
    }

    public override InteractionPromptData GetPrompt(InteractionContext context)
    {
        string action = ActionName;
        return new InteractionPromptData(Kind, action, DisplayName, $"E — {action}");
    }

    public override bool TryInteract(InteractionContext context)
    {
        if (!CanInteract(context))
        {
            return false;
        }

        bool targetOpen = !isOpen;
        isAnimating = true;
        NotifyInteractionStateChanged();
        animationRoutine = StartCoroutine(AnimateDoor(targetOpen));
        return true;
    }

    public void ApplyPersistentState(bool open)
    {
        if (animationRoutine != null)
        {
            StopCoroutine(animationRoutine);
            animationRoutine = null;
        }

        EnsureRotationsInitialized();
        isOpen = open;
        isAnimating = false;
        doorStateInitialized = true;
        if (doorPivot != null)
        {
            doorPivot.localRotation = isOpen ? openLocalRotation : closedLocalRotation;
        }

        NotifyInteractionStateChanged();
    }

    public bool TryGetPersistentColliderPose(
        Collider collider,
        bool open,
        out Vector3 position,
        out Quaternion rotation)
    {
        position = collider != null ? collider.transform.position : Vector3.zero;
        rotation = collider != null ? collider.transform.rotation : Quaternion.identity;
        EnsureRotationsInitialized();
        if (collider == null || doorPivot == null || !collider.transform.IsChildOf(transform))
        {
            return false;
        }

        if (!collider.transform.IsChildOf(doorPivot))
        {
            return true;
        }

        Matrix4x4 parentToWorld = doorPivot.parent != null
            ? doorPivot.parent.localToWorldMatrix
            : Matrix4x4.identity;
        Matrix4x4 targetPivotMatrix = parentToWorld * Matrix4x4.TRS(
            doorPivot.localPosition,
            open ? openLocalRotation : closedLocalRotation,
            doorPivot.localScale);
        Matrix4x4 colliderRelativeToPivot = doorPivot.worldToLocalMatrix
            * collider.transform.localToWorldMatrix;
        Matrix4x4 targetColliderMatrix = targetPivotMatrix * colliderRelativeToPivot;
        position = targetColliderMatrix.GetColumn(3);
        rotation = targetColliderMatrix.rotation;
        return true;
    }

    private IEnumerator AnimateDoor(bool targetOpen)
    {
        Quaternion startRotation = doorPivot.localRotation;
        Quaternion targetRotation = targetOpen ? openLocalRotation : closedLocalRotation;
        float elapsed = 0f;

        while (elapsed < animationDuration)
        {
            if (doorPivot == null)
            {
                animationRoutine = null;
                isAnimating = false;
                NotifyInteractionStateChanged();
                yield break;
            }

            elapsed += Time.deltaTime;
            float normalizedTime = Mathf.Clamp01(elapsed / animationDuration);
            float easedTime = animationCurve != null
                ? animationCurve.Evaluate(normalizedTime)
                : normalizedTime;
            doorPivot.localRotation = Quaternion.SlerpUnclamped(startRotation, targetRotation, easedTime);
            yield return null;
        }

        if (doorPivot != null)
        {
            doorPivot.localRotation = targetRotation;
        }

        isOpen = targetOpen;
        isAnimating = false;
        animationRoutine = null;
        NotifyInteractionStateChanged();
    }

    private void ResolvePivot()
    {
        if (doorPivot != null)
        {
            return;
        }

        doorPivot = transform.Find("DoorPivot");
    }

    private void EnsureRotationsInitialized()
    {
        if (rotationsInitialized)
        {
            return;
        }

        ResolvePivot();
        if (doorPivot == null)
        {
            return;
        }

        closedLocalRotation = doorPivot.localRotation;
        openLocalRotation = closedLocalRotation * Quaternion.Euler(openEulerOffset);
        rotationsInitialized = true;
    }

    protected override void OnValidate()
    {
        base.OnValidate();
        animationDuration = Mathf.Max(0.05f, animationDuration);
    }
}
