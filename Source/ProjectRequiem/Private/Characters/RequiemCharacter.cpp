// Copyright Project Requiem. All Rights Reserved.

#include "Characters/RequiemCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/RequiemCombatComponent.h"
#include "Components/RequiemDodgeComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedPlayerInput.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputActionValue.h"
#include "Math/RotationMatrix.h"

ARequiemCharacter::ARequiemCharacter()
{
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Keep facing tied to the camera so forward, backward, lateral and diagonal
	// movement remain distinct inputs for the eight-direction animation set.
	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->bUseControllerDesiredRotation = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0, 500.0, 0.0);
	GetCharacterMovement()->JumpZVelocity = 500.0f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.0f;
	GetCharacterMovement()->MaxWalkSpeedCrouched = 220.0f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.0f;
	GetCharacterMovement()->MaxAcceleration = 2000.0f;
	GetCharacterMovement()->GroundFriction = 6.0f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.0f;
	GetCharacterMovement()->bUseSeparateBrakingFriction = false;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;
	GetCharacterMovement()->GetNavAgentPropertiesRef().bCanCrouch = true;

	CombatComponent = CreateDefaultSubobject<URequiemCombatComponent>(TEXT("CombatComponent"));
	DodgeComponent = CreateDefaultSubobject<URequiemDodgeComponent>(TEXT("DodgeComponent"));

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
}

void ARequiemCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EnhancedInputComponent)
	{
		return;
	}

	if (MoveAction)
	{
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ARequiemCharacter::Move);
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Completed, this, &ARequiemCharacter::StopMove);
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Canceled, this, &ARequiemCharacter::StopMove);
	}

	if (LookAction)
	{
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ARequiemCharacter::Look);
	}

	if (JumpAction)
	{
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ARequiemCharacter::StartJump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ARequiemCharacter::StopJump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Canceled, this, &ARequiemCharacter::StopJump);
	}

	if (CrouchAction)
	{
		EnhancedInputComponent->BindAction(CrouchAction, ETriggerEvent::Started, this, &ARequiemCharacter::StartCrouch);
		EnhancedInputComponent->BindAction(CrouchAction, ETriggerEvent::Completed, this, &ARequiemCharacter::StopCrouch);
		EnhancedInputComponent->BindAction(CrouchAction, ETriggerEvent::Canceled, this, &ARequiemCharacter::StopCrouch);
	}

	if (RollAction)
	{
		EnhancedInputComponent->BindAction(
			RollAction,
			ETriggerEvent::Started,
			this,
			&ARequiemCharacter::StartDodge);
	}

	if (ToggleCombatAction)
	{
		EnhancedInputComponent->BindAction(
			ToggleCombatAction,
			ETriggerEvent::Started,
			this,
			&ARequiemCharacter::ToggleCombat);
	}

	if (PrimaryAttackAction)
	{
		EnhancedInputComponent->BindAction(
			PrimaryAttackAction,
			ETriggerEvent::Started,
			this,
			&ARequiemCharacter::PrimaryAttack);
	}
}

float ARequiemCharacter::TakeDamage(
	const float DamageAmount,
	const FDamageEvent& DamageEvent,
	AController* EventInstigator,
	AActor* DamageCauser)
{
	if (DodgeComponent && DodgeComponent->ShouldIgnoreIncomingDamage())
	{
		return 0.0f;
	}

	return Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
}

bool ARequiemCharacter::IsDodgeInvulnerable() const
{
	return DodgeComponent && DodgeComponent->IsDodgeInvulnerable();
}

void ARequiemCharacter::OnStartCrouch(const float HalfHeightAdjust, const float ScaledHalfHeightAdjust)
{
	Super::OnStartCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);

	// Crouching uses camera-relative strafing so all eight directional clips remain meaningful.
	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->bUseControllerDesiredRotation = true;
}

void ARequiemCharacter::OnEndCrouch(const float HalfHeightAdjust, const float ScaledHalfHeightAdjust)
{
	Super::OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);

	GetCharacterMovement()->bUseControllerDesiredRotation = true;
	GetCharacterMovement()->bOrientRotationToMovement = false;
}

void ARequiemCharacter::Move(const FInputActionValue& Value)
{
	const FVector2D MovementVector = Value.Get<FVector2D>();
	if (!Controller)
	{
		return;
	}

	const FRotator ControlRotation = Controller->GetControlRotation();
	const FRotator YawRotation(0.0, ControlRotation.Yaw, 0.0);
	const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
	CurrentMovementInputDirection = (
		ForwardDirection * MovementVector.Y
		+ RightDirection * MovementVector.X).GetSafeNormal2D();

	if ((CombatComponent && CombatComponent->IsUnarmedAttackMovementLocked())
		|| (DodgeComponent && DodgeComponent->IsDodgeMovementLocked()))
	{
		return;
	}

	AddMovementInput(ForwardDirection, MovementVector.Y);
	AddMovementInput(RightDirection, MovementVector.X);
}

void ARequiemCharacter::StopMove()
{
	CurrentMovementInputDirection = FVector::ZeroVector;
}

void ARequiemCharacter::Look(const FInputActionValue& Value)
{
	const FVector2D LookAxisVector = Value.Get<FVector2D>();
	AddControllerYawInput(LookAxisVector.X);
	AddControllerPitchInput(LookAxisVector.Y);
}

void ARequiemCharacter::StartJump()
{
	if (!DodgeComponent || !DodgeComponent->AreDodgeRestrictedActionsLocked())
	{
		Jump();
	}
}

void ARequiemCharacter::StopJump()
{
	StopJumping();
}

void ARequiemCharacter::StartCrouch()
{
	if (!DodgeComponent || !DodgeComponent->AreDodgeRestrictedActionsLocked())
	{
		Crouch();
	}
}

void ARequiemCharacter::StopCrouch()
{
	UnCrouch();
}

void ARequiemCharacter::StartDodge()
{
	if (!DodgeComponent)
	{
		return;
	}

	FVector CapturedDirection = GetCurrentDodgeInputDirection();
	if (CapturedDirection.IsNearlyZero())
	{
		CapturedDirection = CurrentMovementInputDirection.GetSafeNormal2D();
	}
	if (CapturedDirection.IsNearlyZero())
	{
		CapturedDirection = GetCharacterMovement()->GetCurrentAcceleration().GetSafeNormal2D();
	}
	if (CapturedDirection.IsNearlyZero())
	{
		CapturedDirection = GetActorForwardVector().GetSafeNormal2D();
	}

	DodgeComponent->RequestDodge(CapturedDirection);
}

FVector ARequiemCharacter::GetCurrentDodgeInputDirection() const
{
	const APlayerController* PlayerController = Cast<APlayerController>(Controller);
	const UEnhancedPlayerInput* EnhancedPlayerInput = PlayerController
		? Cast<UEnhancedPlayerInput>(PlayerController->PlayerInput)
		: nullptr;
	if (!EnhancedPlayerInput || !MoveAction || !Controller)
	{
		return FVector::ZeroVector;
	}

	const FVector2D MovementVector =
		EnhancedPlayerInput->GetActionValue(MoveAction).Get<FVector2D>();
	if (MovementVector.IsNearlyZero())
	{
		return FVector::ZeroVector;
	}

	const FRotator ControlRotation = Controller->GetControlRotation();
	const FRotator YawRotation(0.0, ControlRotation.Yaw, 0.0);
	const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
	return (ForwardDirection * MovementVector.Y + RightDirection * MovementVector.X)
		.GetSafeNormal2D();
}

void ARequiemCharacter::ToggleCombat()
{
	if (CombatComponent
		&& (!DodgeComponent || !DodgeComponent->AreDodgeRestrictedActionsLocked()))
	{
		CombatComponent->ToggleUnarmedCombat();
	}
}

void ARequiemCharacter::PrimaryAttack()
{
	if (CombatComponent
		&& (!DodgeComponent || !DodgeComponent->AreDodgeRestrictedActionsLocked()))
	{
		CombatComponent->RequestUnarmedAttack();
	}
}
