// Copyright Project Requiem. All Rights Reserved.

#include "Characters/RequiemCharacter.h"

#include "Camera/CameraComponent.h"
#include "Combat/RequiemCombatDummy.h"
#include "Components/DecalComponent.h"
#include "Components/RequiemCombatComponent.h"
#include "Components/RequiemDodgeComponent.h"
#include "Components/RequiemHealthComponent.h"
#include "Components/RequiemLockOnComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DamageEvents.h"
#include "EnhancedInputComponent.h"
#include "EnhancedPlayerInput.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputActionValue.h"
#include "Math/RotationMatrix.h"
#include "ProjectRequiem.h"

namespace
{
ERequiemHitRegion ParseTestHitRegion(const FString& RegionName)
{
	if (RegionName.Equals(TEXT("Head"), ESearchCase::IgnoreCase))
	{
		return ERequiemHitRegion::Head;
	}
	if (RegionName.Equals(TEXT("Stomach"), ESearchCase::IgnoreCase))
	{
		return ERequiemHitRegion::Stomach;
	}
	if (RegionName.Equals(TEXT("Shoulder_L"), ESearchCase::IgnoreCase)
		|| RegionName.Equals(TEXT("ShoulderLeft"), ESearchCase::IgnoreCase))
	{
		return ERequiemHitRegion::ShoulderLeft;
	}
	if (RegionName.Equals(TEXT("Shoulder_R"), ESearchCase::IgnoreCase)
		|| RegionName.Equals(TEXT("ShoulderRight"), ESearchCase::IgnoreCase))
	{
		return ERequiemHitRegion::ShoulderRight;
	}
	return ERequiemHitRegion::Chest;
}
}

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
	HealthComponent = CreateDefaultSubobject<URequiemHealthComponent>(TEXT("HealthComponent"));
	LockOnComponent = CreateDefaultSubobject<URequiemLockOnComponent>(TEXT("LockOnComponent"));
	SwordMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SwordMesh"));
	SwordMesh->SetupAttachment(GetMesh(), SwordBackSocketName);
	SwordMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SwordMesh->SetGenerateOverlapEvents(false);
	SwordMesh->SetCanEverAffectNavigation(false);
	SwordMesh->SetHiddenInGame(false, true);
	SwordMesh->SetVisibility(true, true);

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	LockOnGroundIndicator = CreateDefaultSubobject<UDecalComponent>(TEXT("LockOnGroundIndicator"));
	LockOnGroundIndicator->SetupAttachment(RootComponent);
	LockOnGroundIndicator->SetAbsolute(true, true, true);
	LockOnGroundIndicator->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));
	LockOnGroundIndicator->DecalSize = FVector(8.0f, 90.0f, 90.0f);
	LockOnGroundIndicator->SetDecalColor(FLinearColor(1.0f, 0.8f, 0.0f, 1.0f));
	LockOnGroundIndicator->SetFadeScreenSize(0.001f);
	LockOnGroundIndicator->SetSortOrder(100);
	LockOnGroundIndicator->SetCanEverAffectNavigation(false);
	LockOnGroundIndicator->SetHiddenInGame(true);
	LockOnGroundIndicator->SetVisibility(false);
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
			&ARequiemCharacter::ToggleSwordCombat);
	}

	if (PrimaryAttackAction)
	{
		EnhancedInputComponent->BindAction(
			PrimaryAttackAction,
			ETriggerEvent::Started,
			this,
			&ARequiemCharacter::BeginPrimaryAttack);
		EnhancedInputComponent->BindAction(
			PrimaryAttackAction,
			ETriggerEvent::Completed,
			this,
			&ARequiemCharacter::ReleasePrimaryAttack);
		EnhancedInputComponent->BindAction(
			PrimaryAttackAction,
			ETriggerEvent::Canceled,
			this,
			&ARequiemCharacter::ReleasePrimaryAttack);
	}

	if (LockOnAction)
	{
		EnhancedInputComponent->BindAction(
			LockOnAction,
			ETriggerEvent::Started,
			this,
			&ARequiemCharacter::ToggleLockOn);
	}
}

float ARequiemCharacter::TakeDamage(
	const float DamageAmount,
	const FDamageEvent& DamageEvent,
	AController* EventInstigator,
	AActor* DamageCauser)
{
	if (!HealthComponent)
	{
		return 0.0f;
	}

	FRequiemDamageRequest Request;
	Request.DamageAmount = DamageAmount;
	Request.HitRegion = ERequiemHitRegion::Chest;
	Request.Strength = DamageAmount >= HealthComponent->GetStrongDamageThreshold()
		? ERequiemDamageStrength::Strong
		: ERequiemDamageStrength::Light;
	Request.ImpactDirection = DamageCauser
		? (GetActorLocation() - DamageCauser->GetActorLocation()).GetSafeNormal2D()
		: FVector::ZeroVector;
	Request.DamageTypeClass = DamageEvent.DamageTypeClass;

	const ERequiemDamageOutcome Outcome = ApplyRequiemDamage(Request);
	if (Outcome != ERequiemDamageOutcome::Applied
		&& Outcome != ERequiemDamageOutcome::Killed)
	{
		return 0.0f;
	}

	const float AppliedDamage = HealthComponent->GetLastAppliedDamage();
	return Super::TakeDamage(AppliedDamage, DamageEvent, EventInstigator, DamageCauser);
}

ERequiemDamageOutcome ARequiemCharacter::ApplyRequiemDamage(
	const FRequiemDamageRequest& Request)
{
	const ERequiemDamageOutcome Outcome = HealthComponent
		? HealthComponent->ApplyDamage(Request)
		: ERequiemDamageOutcome::IgnoredInvalid;
	if (Outcome == ERequiemDamageOutcome::Applied
		|| Outcome == ERequiemDamageOutcome::Killed)
	{
		CurrentMovementInputDirection = FVector::ZeroVector;
	}
	if (Outcome == ERequiemDamageOutcome::Killed && LockOnComponent)
	{
		LockOnComponent->ClearLockOn();
	}
	return Outcome;
}

bool ARequiemCharacter::CanCrouch() const
{
	return !AreDamageActionsLocked()
		&& (!CombatComponent || !CombatComponent->IsSwordAttackMovementLocked())
		&& Super::CanCrouch();
}

bool ARequiemCharacter::CanJumpInternal_Implementation() const
{
	return !AreDamageActionsLocked()
		&& (!CombatComponent || !CombatComponent->IsSwordAttackMovementLocked())
		&& Super::CanJumpInternal_Implementation();
}

bool ARequiemCharacter::IsDodgeInvulnerable() const
{
	return DodgeComponent && DodgeComponent->IsDodgeInvulnerable();
}

void ARequiemCharacter::SetSwordVisualVisible(const bool bVisible)
{
	if (!SwordMesh)
	{
		return;
	}

	SwordMesh->SetHiddenInGame(!bVisible, true);
	SwordMesh->SetVisibility(bVisible, true);
}

void ARequiemCharacter::SetSwordEquippedPresentation(const bool bEquipped)
{
	AttachSwordToPresentationSocket(
		bEquipped ? SwordHandSocketName : SwordBackSocketName);
	SetSwordVisualVisible(true);
}

void ARequiemCharacter::AttachSwordToPresentationSocket(const FName SocketName)
{
	USkeletalMeshComponent* CharacterMesh = GetMesh();
	if (!SwordMesh || !CharacterMesh || SocketName.IsNone())
	{
		return;
	}

	if (SwordMesh->GetAttachParent() == CharacterMesh
		&& SwordMesh->GetAttachSocketName() == SocketName)
	{
		return;
	}

	if (!CharacterMesh->DoesSocketExist(SocketName))
	{
		UE_LOG(
			LogProjectRequiem,
			Error,
			TEXT("Sword presentation socket %s is missing from %s"),
			*SocketName.ToString(),
			*GetNameSafe(CharacterMesh->GetSkeletalMeshAsset()));
		return;
	}

	SwordMesh->AttachToComponent(
		CharacterMesh,
		FAttachmentTransformRules::SnapToTargetNotIncludingScale,
		SocketName);
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
	if (!Controller || AreDamageActionsLocked())
	{
		CurrentMovementInputDirection = FVector::ZeroVector;
		return;
	}

	const FRotator ControlRotation = Controller->GetControlRotation();
	const FRotator YawRotation(0.0, ControlRotation.Yaw, 0.0);
	const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
	CurrentMovementInputDirection = (
		ForwardDirection * MovementVector.Y
		+ RightDirection * MovementVector.X).GetSafeNormal2D();

	if ((CombatComponent && CombatComponent->IsAnyAttackMovementLocked())
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
	if (LockOnComponent && LockOnComponent->IsLockOnActive())
	{
		return;
	}

	const FVector2D LookAxisVector = Value.Get<FVector2D>();
	AddControllerYawInput(LookAxisVector.X);
	AddControllerPitchInput(LookAxisVector.Y);
}

void ARequiemCharacter::StartJump()
{
	if (!AreDamageActionsLocked()
		&& (!CombatComponent || !CombatComponent->IsSwordAttackMovementLocked())
		&& (!DodgeComponent || !DodgeComponent->AreDodgeRestrictedActionsLocked()))
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
	if (!AreDamageActionsLocked()
		&& (!CombatComponent || !CombatComponent->IsSwordAttackMovementLocked())
		&& (!DodgeComponent || !DodgeComponent->AreDodgeRestrictedActionsLocked()))
	{
		Crouch();
	}
}

void ARequiemCharacter::StopCrouch()
{
	if (!AreDamageActionsLocked()
		&& (!CombatComponent || !CombatComponent->IsSwordHeavyAttackCommitted()))
	{
		UnCrouch();
	}
}

void ARequiemCharacter::StartDodge()
{
	if (!DodgeComponent || AreDamageActionsLocked())
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

void ARequiemCharacter::ToggleSwordCombat()
{
	if (CombatComponent
		&& !AreDamageActionsLocked()
		&& (!DodgeComponent || !DodgeComponent->AreDodgeRestrictedActionsLocked()))
	{
		CombatComponent->ToggleSwordCombat();
	}
}

void ARequiemCharacter::BeginPrimaryAttack()
{
	if (CombatComponent
		&& !AreDamageActionsLocked()
		&& !CombatComponent->IsSwordHeavyAttackCommitted()
		&& (!DodgeComponent || !DodgeComponent->AreDodgeRestrictedActionsLocked()))
	{
		if (CombatComponent->IsSwordEquipped())
		{
			CombatComponent->BeginSwordCharge();
		}
		else
		{
			// Preserve the validated unarmed contract: the press itself requests the strike.
			CombatComponent->RequestUnarmedAttack();
		}
	}
}

void ARequiemCharacter::ReleasePrimaryAttack()
{
	if (CombatComponent)
	{
		// This is deliberately a no-op when no sword charge was started.
		CombatComponent->ReleaseSwordCharge();
	}
}

void ARequiemCharacter::ToggleLockOn()
{
	if (LockOnComponent)
	{
		LockOnComponent->ToggleLockOn();
	}
}

bool ARequiemCharacter::AreDamageActionsLocked() const
{
	return HealthComponent && HealthComponent->AreActionsLocked();
}

void ARequiemCharacter::RequiemTestDamage(
	const FString& RegionName,
	const float DamageAmount,
	const bool bStrong)
{
#if !UE_BUILD_SHIPPING
	FRequiemDamageRequest Request;
	Request.DamageAmount = DamageAmount;
	Request.HitRegion = ParseTestHitRegion(RegionName);
	Request.Strength = bStrong
		? ERequiemDamageStrength::Strong
		: ERequiemDamageStrength::Light;
	Request.ImpactDirection = -GetActorForwardVector();
	ApplyRequiemDamage(Request);
#endif
}

void ARequiemCharacter::RequiemTestKill(const int32 DeathVariant)
{
#if !UE_BUILD_SHIPPING
	if (!HealthComponent)
	{
		return;
	}

	FRequiemDamageRequest Request;
	Request.DamageAmount = FMath::Max(HealthComponent->GetCurrentHealth(), 1.0f);
	Request.HitRegion = ERequiemHitRegion::Chest;
	Request.Strength = ERequiemDamageStrength::Strong;
	Request.DeathAnimation = DeathVariant == 2
		? ERequiemDeathAnimation::Death02
		: ERequiemDeathAnimation::Death01;
	ApplyRequiemDamage(Request);
#endif
}

void ARequiemCharacter::RequiemTestReset()
{
#if !UE_BUILD_SHIPPING
	if (HealthComponent)
	{
		HealthComponent->ResetForTesting();
	}
#endif
}

void ARequiemCharacter::RequiemTestDummyAttack()
{
#if !UE_BUILD_SHIPPING
	ARequiemCombatDummy* NearestDummy = nullptr;
	float NearestDistanceSquared = TNumericLimits<float>::Max();
	for (TActorIterator<ARequiemCombatDummy> Iterator(GetWorld()); Iterator; ++Iterator)
	{
		ARequiemCombatDummy* Dummy = *Iterator;
		if (!IsValid(Dummy) || Dummy->IsDefeated())
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared2D(
			GetActorLocation(),
			Dummy->GetActorLocation());
		if (DistanceSquared < NearestDistanceSquared)
		{
			NearestDistanceSquared = DistanceSquared;
			NearestDummy = Dummy;
		}
	}

	if (NearestDummy)
	{
		NearestDummy->RequestTestAttack(this);
	}
#endif
}

void ARequiemCharacter::RequiemTestDummyReset()
{
#if !UE_BUILD_SHIPPING
	for (TActorIterator<ARequiemCombatDummy> Iterator(GetWorld()); Iterator; ++Iterator)
	{
		if (ARequiemCombatDummy* Dummy = *Iterator; IsValid(Dummy))
		{
			Dummy->ResetForTesting();
		}
	}
#endif
}
