using UnityEngine;
using UnityEngine.InputSystem;
using UnityEngine.Serialization;
using System;

[DefaultExecutionOrder(-20)]
public sealed class PlayerCameraRelativeMovement : MonoBehaviour
{
    [SerializeField] private CharacterController characterController;
    [SerializeField] private Transform cameraTransform;
    [SerializeField] private ThirdPersonCameraController cameraController;
    [SerializeField] private BasicMeleeAttack meleeAttack;
    [SerializeField] private PlayerLockOnController lockOnController;
    [SerializeField] private PlayerStamina stamina;
    [SerializeField] private float walkSpeed = 3.5f;
    [FormerlySerializedAs("moveSpeed")]
    [SerializeField] private float runSpeed = 5f;
    [SerializeField] private float rotationSpeed = 14f;
    [SerializeField] private float gravity = -20f;
    [SerializeField] private float dodgeSpeed = 12f;
    [SerializeField] private float dodgeDuration = 0.2f;
    [SerializeField] private float dodgeCooldown = 0.45f;
    [FormerlySerializedAs("dodgeTapMaxDuration")]
    [SerializeField] private float runHoldMinDuration = 0.22f;

    private float verticalVelocity;
    private float dodgeTimer;
    private float dodgeCooldownTimer;
    private float spaceHeldTime;
    private bool isTrackingSpaceHold;
    private Vector3 dodgeDirection;

    public event Action DodgeStarted;

    public bool IsDodging => dodgeTimer > 0f;
    public bool IsRunning { get; private set; }
    public float CurrentPlanarSpeed { get; private set; }
    public bool IsActionLocked => meleeAttack != null && meleeAttack.IsAttacking;

    public Transform CameraTransform
    {
        get => cameraTransform;
        set
        {
            cameraTransform = value;
            cameraController = value != null ? value.GetComponentInParent<ThirdPersonCameraController>() : null;
            if (lockOnController != null)
            {
                lockOnController.CameraTransform = value;
            }
        }
    }

    private void Awake()
    {
        if (characterController == null)
        {
            characterController = GetComponent<CharacterController>();
        }

        if (meleeAttack == null)
        {
            meleeAttack = GetComponent<BasicMeleeAttack>();
        }

        if (lockOnController == null)
        {
            lockOnController = GetComponent<PlayerLockOnController>();
        }

        if (stamina == null)
        {
            stamina = GetComponent<PlayerStamina>();
        }

        if (cameraController == null && cameraTransform != null)
        {
            cameraController = cameraTransform.GetComponentInParent<ThirdPersonCameraController>();
        }
    }

    private void Update()
    {
        if (characterController == null)
        {
            return;
        }

        if (dodgeCooldownTimer > 0f)
        {
            dodgeCooldownTimer -= Time.deltaTime;
        }

        if (dodgeTimer > 0f)
        {
            dodgeTimer -= Time.deltaTime;
        }

        Vector2 input = ReadMovementInput();
        Vector3 moveDirection = GetCameraRelativeDirection(input);
        UpdateSpaceRun();
        UpdateDodgeInput(moveDirection);

        if (characterController.isGrounded && verticalVelocity < 0f)
        {
            verticalVelocity = -2f;
        }

        verticalVelocity += gravity * Time.deltaTime;

        Vector3 horizontalVelocity = GetHorizontalVelocity(moveDirection);
        CurrentPlanarSpeed = horizontalVelocity.magnitude;
        Vector3 velocity = horizontalVelocity;
        velocity.y = verticalVelocity;
        characterController.Move(velocity * Time.deltaTime);

        bool canRotateWhileLocked = meleeAttack != null && meleeAttack.CanRotateDuringAttack;
        Vector3 facingDirection = GetFacingDirection(horizontalVelocity, moveDirection, canRotateWhileLocked);
        if (facingDirection.sqrMagnitude > 0.001f)
        {
            Quaternion targetRotation = Quaternion.LookRotation(facingDirection, Vector3.up);
            transform.rotation = Quaternion.Slerp(transform.rotation, targetRotation, rotationSpeed * Time.deltaTime);
        }
    }

    private static Vector2 ReadMovementInput()
    {
        if (Keyboard.current == null)
        {
            return Vector2.zero;
        }

        Vector2 input = Vector2.zero;

        if (Keyboard.current.wKey.isPressed)
        {
            input.y += 1f;
        }

        if (Keyboard.current.sKey.isPressed)
        {
            input.y -= 1f;
        }

        if (Keyboard.current.dKey.isPressed)
        {
            input.x += 1f;
        }

        if (Keyboard.current.aKey.isPressed)
        {
            input.x -= 1f;
        }

        return input.sqrMagnitude > 1f ? input.normalized : input;
    }

    private Vector3 GetCameraRelativeDirection(Vector2 input)
    {
        if (input.sqrMagnitude <= 0.001f)
        {
            return Vector3.zero;
        }

        Vector3 forward;
        Vector3 right;

        if (cameraController != null)
        {
            forward = cameraController.PlanarForward;
            right = cameraController.PlanarRight;
        }
        else
        {
            Transform referenceTransform = cameraTransform != null ? cameraTransform : transform;
            forward = Vector3.ProjectOnPlane(referenceTransform.forward, Vector3.up).normalized;
            right = Vector3.ProjectOnPlane(referenceTransform.right, Vector3.up).normalized;
        }

        return (forward * input.y + right * input.x).normalized;
    }

    private void UpdateSpaceRun()
    {
        IsRunning = false;

        if (Keyboard.current == null)
        {
            isTrackingSpaceHold = false;
            spaceHeldTime = 0f;
            return;
        }

        var spaceKey = Keyboard.current.spaceKey;
        if (!spaceKey.isPressed)
        {
            isTrackingSpaceHold = false;
            spaceHeldTime = 0f;
            return;
        }

        if (!isTrackingSpaceHold)
        {
            isTrackingSpaceHold = true;
            spaceHeldTime = 0f;
        }

        spaceHeldTime += Time.deltaTime > 0f ? Time.deltaTime : 1f / 60f;
        IsRunning = spaceHeldTime >= runHoldMinDuration && !IsActionLocked && !IsDodging;
    }

    private void UpdateDodgeInput(Vector3 moveDirection)
    {
        if (Keyboard.current == null)
        {
            return;
        }

        if (Keyboard.current.leftShiftKey.wasPressedThisFrame)
        {
            TryStartDodge(moveDirection);
        }
    }

    public bool TryStartDodge(Vector3 moveDirection)
    {
        if (dodgeCooldownTimer <= 0f && dodgeTimer <= 0f && !IsActionLocked && CanSpendDodge())
        {
            stamina?.TrySpendDodge();
            dodgeDirection = moveDirection.sqrMagnitude > 0.001f ? moveDirection : transform.forward;
            dodgeTimer = dodgeDuration;
            dodgeCooldownTimer = dodgeCooldown;
            DodgeStarted?.Invoke();
            return true;
        }

        return false;
    }

    private bool CanSpendDodge()
    {
        return stamina == null || stamina.CanSpend(stamina.DodgeCost);
    }

    private Vector3 GetHorizontalVelocity(Vector3 moveDirection)
    {
        if (IsActionLocked)
        {
            return Vector3.zero;
        }

        if (dodgeTimer <= 0f)
        {
            return moveDirection * ((IsRunning ? runSpeed : walkSpeed) * DevSettings.PlayerSpeedMultiplier);
        }

        return dodgeDirection * (dodgeSpeed * DevSettings.PlayerSpeedMultiplier);
    }

    private Vector3 GetFacingDirection(Vector3 horizontalVelocity, Vector3 moveDirection, bool canRotateWhileLocked)
    {
        if (IsActionLocked)
        {
            if (lockOnController != null && lockOnController.TryGetDirectionToTarget(out Vector3 lockOnDirection))
            {
                return lockOnDirection;
            }

            return canRotateWhileLocked ? moveDirection : Vector3.zero;
        }

        return horizontalVelocity.sqrMagnitude > 0.001f
            ? horizontalVelocity.normalized
            : moveDirection;
    }
}
