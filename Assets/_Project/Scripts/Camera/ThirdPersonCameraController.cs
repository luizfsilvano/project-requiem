using UnityEngine;
using UnityEngine.InputSystem;

public sealed class ThirdPersonCameraController : MonoBehaviour
{
    [SerializeField] private Transform target;
    [SerializeField] private Camera controlledCamera;
    [SerializeField] private PlayerLockOnController lockOnController;
    [SerializeField] private float distance = 5.5f;
    [SerializeField] private float targetHeight = 1.35f;
    [SerializeField] private float rightOffset = 0.65f;
    [SerializeField] private float mouseSensitivity = 0.12f;
    [SerializeField] private float minPitch = -25f;
    [SerializeField] private float maxPitch = 55f;
    [SerializeField] private float positionSharpness = 18f;
    [SerializeField] private float rotationSharpness = 20f;
    [SerializeField] private float shakeSharpness = 28f;
    [SerializeField] private float lockOnYawSharpness = 10f;
    [SerializeField] private float lockOnPitch = 14f;

    private float yaw;
    private float pitch = 18f;
    private float shakeStrength;
    private float shakeTimer;

    public Transform Target
    {
        get => target;
        set
        {
            target = value;
            if (target != null)
            {
                lockOnController = target.GetComponent<PlayerLockOnController>();
            }
        }
    }

    public Camera ControlledCamera => controlledCamera;

    public Vector3 PlanarForward => Quaternion.Euler(0f, yaw, 0f) * Vector3.forward;
    public Vector3 PlanarRight => Quaternion.Euler(0f, yaw, 0f) * Vector3.right;

    private void Awake()
    {
        if (controlledCamera == null)
        {
            controlledCamera = GetComponentInChildren<Camera>();
        }

        if (lockOnController == null && target != null)
        {
            lockOnController = target.GetComponent<PlayerLockOnController>();
        }

        yaw = transform.eulerAngles.y;
    }

    private void OnEnable()
    {
        LockCursor();
    }

    private void Update()
    {
        if (DevSettings.ConsoleOpen)
        {
            Cursor.lockState = CursorLockMode.None;
            Cursor.visible = true;
            return;
        }

        if (Keyboard.current != null && Keyboard.current.escapeKey.wasPressedThisFrame)
        {
            Cursor.lockState = CursorLockMode.None;
            Cursor.visible = true;
        }

        if (Mouse.current != null && Mouse.current.leftButton.wasPressedThisFrame)
        {
            LockCursor();
        }

        if (lockOnController != null && lockOnController.HasTarget)
        {
            return;
        }

        if (Mouse.current == null || Cursor.lockState != CursorLockMode.Locked)
        {
            return;
        }

        Vector2 mouseDelta = Mouse.current.delta.ReadValue();
        yaw += mouseDelta.x * mouseSensitivity;
        pitch = Mathf.Clamp(pitch - mouseDelta.y * mouseSensitivity, minPitch, maxPitch);
    }

    private void LateUpdate()
    {
        if (target == null || controlledCamera == null)
        {
            return;
        }

        UpdateLockOnOrbit();

        Vector3 targetPoint = target.position + Vector3.up * targetHeight;
        Quaternion orbitRotation = Quaternion.Euler(pitch, yaw, 0f);
        Vector3 desiredPosition = targetPoint - orbitRotation * Vector3.forward * distance + orbitRotation * Vector3.right * rightOffset;
        desiredPosition += GetShakeOffset();
        Quaternion desiredRotation = Quaternion.LookRotation(targetPoint - desiredPosition, Vector3.up);

        controlledCamera.transform.SetPositionAndRotation(
            Vector3.Lerp(controlledCamera.transform.position, desiredPosition, positionSharpness * Time.deltaTime),
            Quaternion.Slerp(controlledCamera.transform.rotation, desiredRotation, rotationSharpness * Time.deltaTime));
    }

    public void AddShake(float strength, float duration)
    {
        shakeStrength = Mathf.Max(shakeStrength, strength);
        shakeTimer = Mathf.Max(shakeTimer, duration);
    }

    public void SetLockOnController(PlayerLockOnController controller)
    {
        lockOnController = controller;
    }

    private void UpdateLockOnOrbit()
    {
        if (lockOnController == null || !lockOnController.HasTarget)
        {
            return;
        }

        Vector3 toTarget = Vector3.ProjectOnPlane(lockOnController.CurrentTargetPoint - target.position, Vector3.up);
        if (toTarget.sqrMagnitude <= 0.001f)
        {
            return;
        }

        float desiredYaw = Quaternion.LookRotation(toTarget.normalized, Vector3.up).eulerAngles.y;
        yaw = Mathf.LerpAngle(yaw, desiredYaw, lockOnYawSharpness * Time.deltaTime);
        pitch = Mathf.Lerp(pitch, lockOnPitch, lockOnYawSharpness * Time.deltaTime);
    }

    private Vector3 GetShakeOffset()
    {
        if (shakeTimer <= 0f || shakeStrength <= 0f)
        {
            return Vector3.zero;
        }

        shakeTimer -= Time.unscaledDeltaTime;
        float fade = Mathf.Clamp01(shakeTimer * shakeSharpness);
        Vector2 randomOffset = Random.insideUnitCircle * shakeStrength * fade;
        return controlledCamera.transform.right * randomOffset.x + controlledCamera.transform.up * randomOffset.y;
    }

    private static void LockCursor()
    {
        Cursor.lockState = CursorLockMode.Locked;
        Cursor.visible = false;
    }
}
