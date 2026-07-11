using UnityEngine;
using UnityEngine.InputSystem;

[DefaultExecutionOrder(-30)]
public sealed class PlayerLockOnController : MonoBehaviour
{
    [SerializeField] private Transform cameraTransform;
    [SerializeField] private float searchRadius = 12f;
    [SerializeField] private float maxLockAngle = 85f;
    [SerializeField] private float targetHeightOffset = 1.25f;
    [SerializeField] private float retargetGraceDistance = 15f;
    [SerializeField] private float mouseRetargetThreshold = 45f;
    [SerializeField] private float mouseRetargetCooldown = 0.28f;
    [SerializeField] private float sideSwitchDeadZone = 0.08f;
    [SerializeField] private Color indicatorColor = new(1f, 0.82f, 0.22f, 0.85f);
    [SerializeField] private float indicatorRadius = 0.62f;
    [SerializeField] private float indicatorHeightOffset = 0.08f;

    private TrainingDummyHealth currentTargetHealth;
    private Transform currentTarget;
    private LineRenderer indicator;
    private Material indicatorMaterial;
    private float retargetCooldownTimer;
    private float accumulatedMouseRetargetX;

    public bool HasTarget => currentTarget != null && currentTargetHealth != null && !currentTargetHealth.IsDead;
    public Transform CurrentTarget => HasTarget ? currentTarget : null;
    public Vector3 CurrentTargetPoint => HasTarget
        ? currentTargetHealth.GetLockOnPoint(targetHeightOffset)
        : transform.position + transform.forward;

    public Transform CameraTransform
    {
        get => cameraTransform;
        set => cameraTransform = value;
    }

    private void Update()
    {
        bool inputBlocked = GameplayInputGate.IsBlocked;
        if (!inputBlocked && Keyboard.current != null && Keyboard.current.tabKey.wasPressedThisFrame)
        {
            ToggleLockOn();
        }

        if (!inputBlocked)
        {
            UpdateMouseRetarget();
        }

        if (HasTarget && Vector3.Distance(transform.position, currentTarget.position) > retargetGraceDistance)
        {
            ClearTarget();
        }

        UpdateIndicator();
    }

    public bool TryGetDirectionToTarget(out Vector3 direction)
    {
        direction = Vector3.zero;
        if (!HasTarget)
        {
            return false;
        }

        direction = Vector3.ProjectOnPlane(CurrentTargetPoint - transform.position, Vector3.up);
        if (direction.sqrMagnitude <= 0.001f)
        {
            return false;
        }

        direction.Normalize();
        return true;
    }

    public void ToggleLockOn()
    {
        if (HasTarget)
        {
            ClearTarget();
            return;
        }

        TryLockNearestTarget();
    }

    public bool TryLockNearestTarget()
    {
        TrainingDummyHealth bestTarget = null;
        float bestScore = float.MaxValue;
        Vector3 cameraForward = GetCameraPlanarForward();

        foreach (TrainingDummyHealth candidate in FindObjectsByType<TrainingDummyHealth>(FindObjectsInactive.Exclude, FindObjectsSortMode.None))
        {
            if (candidate == null || candidate.IsDead || candidate.transform == transform)
            {
                continue;
            }

            Vector3 toCandidate = candidate.transform.position - transform.position;
            float distance = toCandidate.magnitude;
            if (distance > searchRadius || distance <= 0.01f)
            {
                continue;
            }

            Vector3 planarDirection = Vector3.ProjectOnPlane(toCandidate, Vector3.up).normalized;
            float angle = Vector3.Angle(cameraForward, planarDirection);
            if (angle > maxLockAngle)
            {
                continue;
            }

            float score = distance + angle * 0.05f;
            if (score < bestScore)
            {
                bestScore = score;
                bestTarget = candidate;
            }
        }

        if (bestTarget == null)
        {
            ClearTarget();
            return false;
        }

        return SetTarget(bestTarget);
    }

    public bool TrySwitchTarget(int side)
    {
        if (!HasTarget)
        {
            return TryLockNearestTarget();
        }

        int sideDirection = side >= 0 ? 1 : -1;
        Vector3 cameraForward = GetCameraPlanarForward();
        Vector3 cameraRight = GetCameraPlanarRight();
        Vector3 currentDirection = Vector3.ProjectOnPlane(currentTarget.position - transform.position, Vector3.up);
        float currentSide = currentDirection.sqrMagnitude > 0.001f
            ? Vector3.Dot(currentDirection.normalized, cameraRight)
            : 0f;
        TrainingDummyHealth bestTarget = null;
        float bestScore = float.MaxValue;

        foreach (TrainingDummyHealth candidate in FindObjectsByType<TrainingDummyHealth>(FindObjectsInactive.Exclude, FindObjectsSortMode.None))
        {
            if (!IsValidCandidate(candidate) || candidate == currentTargetHealth)
            {
                continue;
            }

            Vector3 toCandidate = candidate.transform.position - transform.position;
            float distance = toCandidate.magnitude;
            if (distance > searchRadius || distance <= 0.01f)
            {
                continue;
            }

            Vector3 planarDirection = Vector3.ProjectOnPlane(toCandidate, Vector3.up).normalized;
            float angle = Vector3.Angle(cameraForward, planarDirection);
            if (angle > Mathf.Max(maxLockAngle, 110f))
            {
                continue;
            }

            float candidateSide = Vector3.Dot(planarDirection, cameraRight);
            float sideDelta = (candidateSide - currentSide) * sideDirection;
            if (sideDelta <= sideSwitchDeadZone)
            {
                continue;
            }

            float score = sideDelta * 2f + angle * 0.04f + distance * 0.08f;
            if (score < bestScore)
            {
                bestScore = score;
                bestTarget = candidate;
            }
        }

        return bestTarget != null && SetTarget(bestTarget);
    }

    public void ClearTarget()
    {
        currentTargetHealth = null;
        currentTarget = null;
        accumulatedMouseRetargetX = 0f;
        if (indicator != null)
        {
            indicator.enabled = false;
        }
    }

    private Vector3 GetCameraPlanarForward()
    {
        Transform referenceTransform = cameraTransform != null ? cameraTransform : transform;
        Vector3 forward = Vector3.ProjectOnPlane(referenceTransform.forward, Vector3.up);
        return forward.sqrMagnitude > 0.001f ? forward.normalized : transform.forward;
    }

    private Vector3 GetCameraPlanarRight()
    {
        Transform referenceTransform = cameraTransform != null ? cameraTransform : transform;
        Vector3 right = Vector3.ProjectOnPlane(referenceTransform.right, Vector3.up);
        return right.sqrMagnitude > 0.001f ? right.normalized : transform.right;
    }

    private void UpdateMouseRetarget()
    {
        if (retargetCooldownTimer > 0f)
        {
            retargetCooldownTimer -= Time.deltaTime;
        }

        if (!HasTarget || Mouse.current == null)
        {
            accumulatedMouseRetargetX = 0f;
            return;
        }

        Vector2 mouseDelta = Mouse.current.delta.ReadValue();
        if (Mathf.Abs(mouseDelta.x) < 0.01f)
        {
            accumulatedMouseRetargetX = Mathf.MoveTowards(accumulatedMouseRetargetX, 0f, Time.deltaTime * mouseRetargetThreshold);
            return;
        }

        if (Mathf.Sign(mouseDelta.x) != Mathf.Sign(accumulatedMouseRetargetX) && Mathf.Abs(accumulatedMouseRetargetX) > 0.01f)
        {
            accumulatedMouseRetargetX = 0f;
        }

        accumulatedMouseRetargetX += mouseDelta.x;
        if (retargetCooldownTimer > 0f || Mathf.Abs(accumulatedMouseRetargetX) < mouseRetargetThreshold)
        {
            return;
        }

        if (TrySwitchTarget(accumulatedMouseRetargetX > 0f ? 1 : -1))
        {
            retargetCooldownTimer = mouseRetargetCooldown;
        }

        accumulatedMouseRetargetX = 0f;
    }

    private bool SetTarget(TrainingDummyHealth targetHealth)
    {
        if (!IsValidCandidate(targetHealth))
        {
            return false;
        }

        currentTargetHealth = targetHealth;
        currentTarget = targetHealth.transform;
        accumulatedMouseRetargetX = 0f;
        EnsureIndicator();
        indicator.enabled = true;
        return true;
    }

    private bool IsValidCandidate(TrainingDummyHealth candidate)
    {
        return candidate != null && !candidate.IsDead && candidate.transform != transform;
    }

    private void EnsureIndicator()
    {
        if (indicator != null)
        {
            return;
        }

        GameObject indicatorObject = new("LockOnIndicator");
        indicatorObject.transform.SetParent(transform, false);
        indicator = indicatorObject.AddComponent<LineRenderer>();
        indicator.useWorldSpace = true;
        indicator.loop = true;
        indicator.positionCount = 48;
        indicator.startWidth = 0.035f;
        indicator.endWidth = 0.035f;
        indicator.shadowCastingMode = UnityEngine.Rendering.ShadowCastingMode.Off;
        indicator.receiveShadows = false;

        indicatorMaterial = new Material(Shader.Find("Universal Render Pipeline/Unlit"));
        indicatorMaterial.color = indicatorColor;
        indicator.material = indicatorMaterial;
        indicator.enabled = false;
    }

    private void UpdateIndicator()
    {
        if (!HasTarget)
        {
            if (indicator != null)
            {
                indicator.enabled = false;
            }

            return;
        }

        EnsureIndicator();
        indicator.enabled = true;

        Vector3 center = currentTargetHealth.GetLockOnIndicatorCenter(indicatorHeightOffset);
        float radius = Mathf.Max(0.1f, currentTargetHealth.GetLockOnIndicatorRadius(indicatorRadius));
        for (int i = 0; i < indicator.positionCount; i++)
        {
            float t = i / (float)indicator.positionCount * Mathf.PI * 2f;
            indicator.SetPosition(i, center + new Vector3(Mathf.Cos(t) * radius, 0f, Mathf.Sin(t) * radius));
        }
    }

    private void OnDisable()
    {
        ClearTarget();
    }
}
