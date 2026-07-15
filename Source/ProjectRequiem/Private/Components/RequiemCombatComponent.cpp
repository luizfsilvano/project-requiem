// Copyright Project Requiem. All Rights Reserved.

#include "Components/RequiemCombatComponent.h"

#include "Components/RequiemDodgeComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace
{
void SetPlanarVelocity(
	UCharacterMovementComponent* MovementComponent,
	const FVector& NewPlanarVelocity)
{
	if (!MovementComponent)
	{
		return;
	}

	// Write only the planar portion of CharacterMovement's runtime velocity.
	// Its normal walking tick still owns collision, braking and the resulting displacement.
	MovementComponent->Velocity.X = NewPlanarVelocity.X;
	MovementComponent->Velocity.Y = NewPlanarVelocity.Y;
}
}

URequiemCombatComponent::URequiemCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

FName URequiemCombatComponent::GetCombatStateName() const
{
	return CombatState == ERequiemCombatState::CombatUnarmed
		? FName(TEXT("CombatUnarmed"))
		: FName(TEXT("Normal"));
}

void URequiemCombatComponent::ToggleUnarmedCombat()
{
	if (const URequiemDodgeComponent* DodgeComponent =
		GetOwner() ? GetOwner()->FindComponentByClass<URequiemDodgeComponent>() : nullptr;
		DodgeComponent && DodgeComponent->AreDodgeRestrictedActionsLocked())
	{
		return;
	}

	if (CombatState == ERequiemCombatState::CombatUnarmed)
	{
		ExitCombat();
		return;
	}

	EnterUnarmedCombat(ERequiemCombatEntryReason::Manual);
}

ERequiemUnarmedAttackRequestResult URequiemCombatComponent::RequestUnarmedAttack()
{
	if (const URequiemDodgeComponent* DodgeComponent =
		GetOwner() ? GetOwner()->FindComponentByClass<URequiemDodgeComponent>() : nullptr;
		DodgeComponent && DodgeComponent->AreDodgeRestrictedActionsLocked())
	{
		return ERequiemUnarmedAttackRequestResult::Rejected;
	}

	const bool bEnteredCombat = CombatState != ERequiemCombatState::CombatUnarmed;
	if (CombatState != ERequiemCombatState::CombatUnarmed)
	{
		EnterUnarmedCombat(ERequiemCombatEntryReason::Attack);
	}

	AttackRequestSerial = AttackRequestSerial == MAX_int32
		? 1
		: AttackRequestSerial + 1;

	if (!bUnarmedAttackActive)
	{
		if (bInitialUnarmedAttackRequested)
		{
			return ERequiemUnarmedAttackRequestResult::Rejected;
		}

		// Enter/idle accepts one initial attack only. Repeated clicks before the
		// first authored strike starts deliberately do not become follow-ups.
		bInitialUnarmedAttackRequested = true;
		ACharacter* Character = Cast<ACharacter>(GetOwner());
		UCharacterMovementComponent* MovementComponent = Character
			? Character->GetCharacterMovement()
			: nullptr;
		if (bEnteredCombat
			&& Character
			&& MovementComponent
			&& !Character->IsCrouched()
			&& !MovementComponent->IsFalling())
		{
			bUnarmedAttackMovementLocked = true;
			// A moving auto-entry keeps its current CharacterMovement velocity and
			// brakes naturally, but no new Move input can postpone the stationary Enter.
			Character->ConsumeMovementInputVector();
		}
		if (!bEnteredCombat)
		{
			MarkCombatActivity();
		}
		return ERequiemUnarmedAttackRequestResult::InitialAccepted;
	}

	if (IsUnarmedAttackInputWindowOpen())
	{
		// A combo step owns one follow-up slot. Further clicks in the same window
		// are ignored instead of creating a backlog of future attacks.
		bQueuedUnarmedFollowUp = true;
		MarkCombatActivity();
		return ERequiemUnarmedAttackRequestResult::FollowUpBuffered;
	}

	return ERequiemUnarmedAttackRequestResult::Rejected;
}

void URequiemCombatComponent::EnterUnarmedCombat(const ERequiemCombatEntryReason EntryReason)
{
	if (CombatState == ERequiemCombatState::CombatUnarmed)
	{
		return;
	}

	CombatState = ERequiemCombatState::CombatUnarmed;
	MarkCombatActivity();

	// The reason is intentionally accepted by the stable contract so future
	// lock-on and damage callers do not require a second entry path.
	(void)EntryReason;
}

void URequiemCombatComponent::ExitCombat()
{
	const bool bCancelPendingInitialAttack =
		bInitialUnarmedAttackRequested && !bUnarmedAttackActive;
	CombatState = ERequiemCombatState::Normal;
	bInitialUnarmedAttackRequested = false;
	bUnarmedAttackInputWindowOpen = false;
	bQueuedUnarmedFollowUp = false;
	if (bCancelPendingInitialAttack)
	{
		// No authored strike committed yet, so release only the input lock and let
		// CharacterMovement preserve its current braking velocity.
		bUnarmedAttackMovementLocked = false;
	}
}

bool URequiemCombatComponent::ConsumeInitialUnarmedAttackRequest()
{
	if (!bInitialUnarmedAttackRequested
		|| CombatState != ERequiemCombatState::CombatUnarmed)
	{
		return false;
	}

	bInitialUnarmedAttackRequested = false;
	return true;
}

void URequiemCombatComponent::BeginUnarmedAttackStep(const bool bApplyForwardLunge)
{
	bUnarmedAttackActive = true;
	bUnarmedAttackMovementLocked = true;
	bUnarmedAttackInputWindowOpen = false;
	bQueuedUnarmedFollowUp = false;

	ACharacter* Character = Cast<ACharacter>(GetOwner());
	UCharacterMovementComponent* MovementComponent = Character
		? Character->GetCharacterMovement()
		: nullptr;
	if (!Character || !MovementComponent)
	{
		return;
	}

	// Discard movement input already collected in this frame. The planar velocity
	// replaces inherited Jog velocity and is then resolved by CharacterMovement's
	// normal walking, braking and collision code.
	Character->ConsumeMovementInputVector();
	if (!bApplyForwardLunge || UnarmedAttackLungeSpeed <= UE_KINDA_SMALL_NUMBER)
	{
		SetPlanarVelocity(MovementComponent, FVector::ZeroVector);
		return;
	}

	const FVector ForwardDirection = Character->GetActorForwardVector().GetSafeNormal2D();
	const FVector TargetPlanarVelocity = ForwardDirection * UnarmedAttackLungeSpeed;
	SetPlanarVelocity(MovementComponent, TargetPlanarVelocity);
}

void URequiemCombatComponent::SetUnarmedAttackInputWindowOpen(const bool bOpen)
{
	bUnarmedAttackInputWindowOpen = bOpen
		&& bUnarmedAttackActive
		&& CombatState == ERequiemCombatState::CombatUnarmed;
}

bool URequiemCombatComponent::ConsumeQueuedUnarmedFollowUp()
{
	if (!bQueuedUnarmedFollowUp)
	{
		return false;
	}

	bQueuedUnarmedFollowUp = false;
	bUnarmedAttackInputWindowOpen = false;
	return true;
}

void URequiemCombatComponent::EndUnarmedAttackSequence()
{
	const bool bWasMovementLocked = bUnarmedAttackMovementLocked;
	bUnarmedAttackActive = false;
	bUnarmedAttackMovementLocked = false;
	bUnarmedAttackInputWindowOpen = false;
	bQueuedUnarmedFollowUp = false;

	ACharacter* Character = Cast<ACharacter>(GetOwner());
	UCharacterMovementComponent* MovementComponent = Character
		? Character->GetCharacterMovement()
		: nullptr;
	if (bWasMovementLocked && MovementComponent)
	{
		// Release only the planar commitment. Jump/fall velocity remains owned by
		// CharacterMovement when an airborne transition interrupts the attack.
		SetPlanarVelocity(MovementComponent, FVector::ZeroVector);
	}
}

bool URequiemCombatComponent::CanAutoExit(const bool bEnemyNearby) const
{
	if (CombatState != ERequiemCombatState::CombatUnarmed
		|| bEnemyNearby
		|| LastCombatActivityTimeSeconds < 0.0f)
	{
		return false;
	}

	const UWorld* World = GetWorld();
	return World
		&& World->GetTimeSeconds() - LastCombatActivityTimeSeconds >= AutoExitDelay;
}

void URequiemCombatComponent::MarkCombatActivity()
{
	if (const UWorld* World = GetWorld())
	{
		LastCombatActivityTimeSeconds = World->GetTimeSeconds();
	}
}
