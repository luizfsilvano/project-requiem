// Copyright Project Requiem. All Rights Reserved.

#include "Components/RequiemDodgeComponent.h"

#include "Components/RequiemCombatComponent.h"
#include "Components/RequiemHealthComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

URequiemDodgeComponent::URequiemDodgeComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void URequiemDodgeComponent::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bDodgeActive)
	{
		SetComponentTickEnabled(false);
		return;
	}

	DodgeElapsedSeconds += FMath::Max(0.0f, DeltaTime);
	ApplyMovementRecoveryIfNeeded();
	if (GetDodgeNormalizedTime() >= 1.0f)
	{
		FinishDodge();
	}
}

void URequiemDodgeComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bDodgeActive)
	{
		RestoreCharacterRotationPolicy();
	}

	Super::EndPlay(EndPlayReason);
}

bool URequiemDodgeComponent::RequestDodge(const FVector& Direction)
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	UCharacterMovementComponent* MovementComponent = Character
		? Character->GetCharacterMovement()
		: nullptr;
	const URequiemCombatComponent* CombatComponent = Character
		? Character->FindComponentByClass<URequiemCombatComponent>()
		: nullptr;
	const URequiemHealthComponent* HealthComponent = Character
		? Character->FindComponentByClass<URequiemHealthComponent>()
		: nullptr;

	if (bDodgeActive
		|| !Character
		|| !MovementComponent
		|| (HealthComponent && HealthComponent->AreActionsLocked())
		|| Character->IsCrouched()
		|| MovementComponent->IsFalling()
		|| !MovementComponent->IsMovingOnGround()
		|| (CombatComponent
			&& (CombatComponent->IsUnarmedAttackActive()
				|| CombatComponent->IsUnarmedAttackMovementLocked()
				|| CombatComponent->HasPendingInitialUnarmedAttackRequest())))
	{
		return false;
	}

	FVector CapturedDirection = Direction.GetSafeNormal2D();
	if (CapturedDirection.IsNearlyZero())
	{
		CapturedDirection = Character->GetActorForwardVector().GetSafeNormal2D();
	}
	if (CapturedDirection.IsNearlyZero())
	{
		CapturedDirection = FVector::ForwardVector;
	}

	DodgeDirection = CapturedDirection;
	DodgeElapsedSeconds = 0.0f;
	ActiveDodgeDurationSeconds = FMath::Max(DefaultDodgeDurationSeconds, 0.05f);
	bDodgeActive = true;
	bMovementRecoveryApplied = false;
	DodgeRequestSerial = DodgeRequestSerial == MAX_int32 ? 1 : DodgeRequestSerial + 1;

	// If jump or crouch was requested earlier in this same input frame, the
	// accepted dodge wins and clears those pending intents before movement ticks.
	Character->StopJumping();
	Character->UnCrouch();

	bSavedUseControllerDesiredRotation = MovementComponent->bUseControllerDesiredRotation;
	bSavedOrientRotationToMovement = MovementComponent->bOrientRotationToMovement;
	bSavedUseControllerRotationYaw = Character->bUseControllerRotationYaw;
	MovementComponent->bUseControllerDesiredRotation = false;
	MovementComponent->bOrientRotationToMovement = false;
	Character->bUseControllerRotationYaw = false;

	Character->ConsumeMovementInputVector();
	MovementComponent->StopMovementImmediately();
	Character->SetActorRotation(
		FRotator(0.0f, DodgeDirection.Rotation().Yaw, 0.0f),
		ETeleportType::None);
	SetComponentTickEnabled(true);
	return true;
}

void URequiemDodgeComponent::SynchronizeDodgePresentation(
	const float DurationSeconds,
	const float NormalizedTime)
{
	if (!bDodgeActive || DurationSeconds <= UE_KINDA_SMALL_NUMBER)
	{
		return;
	}

	ActiveDodgeDurationSeconds = FMath::Max(DurationSeconds, 0.05f);
	DodgeElapsedSeconds = FMath::Clamp(NormalizedTime, 0.0f, 1.0f)
		* ActiveDodgeDurationSeconds;
	ApplyMovementRecoveryIfNeeded();
}

void URequiemDodgeComponent::FinishDodge()
{
	if (!bDodgeActive)
	{
		return;
	}

	DodgeElapsedSeconds = ActiveDodgeDurationSeconds;
	ApplyMovementRecoveryIfNeeded();
	bDodgeActive = false;
	RestoreCharacterRotationPolicy();
	SetComponentTickEnabled(false);
}

bool URequiemDodgeComponent::IsDodgeMovementLocked() const
{
	return bDodgeActive
		&& GetDodgeNormalizedTime() < MovementControlRecoveryNormalized;
}

bool URequiemDodgeComponent::IsDodgeInvulnerable() const
{
	if (!bDodgeActive)
	{
		return false;
	}

	const float NormalizedTime = GetDodgeNormalizedTime();
	const float WindowStart = FMath::Clamp(IFrameStartNormalized, 0.0f, 1.0f);
	const float WindowEnd = FMath::Clamp(
		FMath::Max(WindowStart, IFrameEndNormalized),
		0.0f,
		1.0f);
	return NormalizedTime >= WindowStart && NormalizedTime <= WindowEnd;
}

float URequiemDodgeComponent::GetDodgeNormalizedTime() const
{
	if (ActiveDodgeDurationSeconds <= UE_KINDA_SMALL_NUMBER)
	{
		return bDodgeActive ? 0.0f : 1.0f;
	}

	return FMath::Clamp(
		DodgeElapsedSeconds / ActiveDodgeDurationSeconds,
		0.0f,
		1.0f);
}

void URequiemDodgeComponent::ApplyMovementRecoveryIfNeeded()
{
	if (!bDodgeActive
		|| bMovementRecoveryApplied
		|| GetDodgeNormalizedTime() < MovementControlRecoveryNormalized)
	{
		return;
	}

	bMovementRecoveryApplied = true;
	// Preserve the velocity produced by the final moving root key. Once animation
	// root motion is ignored, CharacterMovement blends from it using its normal
	// acceleration and braking instead of introducing a forced stop.
}

void URequiemDodgeComponent::RestoreCharacterRotationPolicy()
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	UCharacterMovementComponent* MovementComponent = Character
		? Character->GetCharacterMovement()
		: nullptr;
	if (!Character || !MovementComponent)
	{
		return;
	}

	MovementComponent->bUseControllerDesiredRotation = bSavedUseControllerDesiredRotation;
	MovementComponent->bOrientRotationToMovement = bSavedOrientRotationToMovement;
	Character->bUseControllerRotationYaw = bSavedUseControllerRotationYaw;
}
