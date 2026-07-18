// Copyright Project Requiem. All Rights Reserved.

#include "Components/RequiemCombatComponent.h"

#include "Components/RequiemDodgeComponent.h"
#include "Components/RequiemHealthComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"

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

bool HasClearDamagePath(
	UWorld* World,
	const FVector& TraceStart,
	const FVector& TraceEnd,
	const AActor* SourceActor,
	const AActor* TargetActor)
{
	if (!World)
	{
		return false;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(RequiemUnarmedObstruction), false);
	QueryParams.AddIgnoredActor(SourceActor);
	QueryParams.AddIgnoredActor(TargetActor);
	return !World->LineTraceTestByChannel(
		TraceStart,
		TraceEnd,
		ECC_Visibility,
		QueryParams);
}
}

URequiemCombatComponent::URequiemCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

FName URequiemCombatComponent::GetCombatStateName() const
{
	switch (CombatState)
	{
	case ERequiemCombatState::CombatUnarmed:
		return FName(TEXT("CombatUnarmed"));
	case ERequiemCombatState::CombatSword:
		return FName(TEXT("CombatSword"));
	default:
		return FName(TEXT("Normal"));
	}
}

void URequiemCombatComponent::ToggleUnarmedCombat()
{
	if (IsSwordHeavyAttackCommitted())
	{
		return;
	}

	if (const URequiemHealthComponent* HealthComponent =
		GetOwner() ? GetOwner()->FindComponentByClass<URequiemHealthComponent>() : nullptr;
		HealthComponent && HealthComponent->AreActionsLocked())
	{
		return;
	}

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

bool URequiemCombatComponent::ToggleSwordCombat()
{
	if (const URequiemHealthComponent* HealthComponent =
		GetOwner() ? GetOwner()->FindComponentByClass<URequiemHealthComponent>() : nullptr;
		HealthComponent && HealthComponent->AreActionsLocked())
	{
		return false;
	}

	if (const URequiemDodgeComponent* DodgeComponent =
		GetOwner() ? GetOwner()->FindComponentByClass<URequiemDodgeComponent>() : nullptr;
		DodgeComponent && DodgeComponent->AreDodgeRestrictedActionsLocked())
	{
		return false;
	}

	if (IsSwordHeavyAttackCommitted())
	{
		return false;
	}

	CancelActiveAttackForExternalReaction();
	if (CombatState == ERequiemCombatState::CombatSword)
	{
		EnterUnarmedCombat(ERequiemCombatEntryReason::Manual);
	}
	else
	{
		EnterSwordCombat(ERequiemCombatEntryReason::Manual);
	}

	SwordToggleSerial = AdvanceSerial(SwordToggleSerial);
	return true;
}

ERequiemUnarmedAttackRequestResult URequiemCombatComponent::RequestUnarmedAttack()
{
	// The equipped style owns primary attack. Blueprint callers must not bypass
	// the Character input routing and silently replace it with the unarmed style.
	if (CombatState == ERequiemCombatState::CombatSword)
	{
		return ERequiemUnarmedAttackRequestResult::Rejected;
	}

	if (const URequiemHealthComponent* HealthComponent =
		GetOwner() ? GetOwner()->FindComponentByClass<URequiemHealthComponent>() : nullptr;
		HealthComponent && HealthComponent->AreActionsLocked())
	{
		return ERequiemUnarmedAttackRequestResult::Rejected;
	}

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
			// A grounded auto-entry claims movement before the presentation update;
			// the AnimInstance may skip stance Enter and commit the first strike directly.
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
	// A committed heavy attack is never canceled by a public style-change call.
	if (IsSwordHeavyAttackCommitted())
	{
		return;
	}

	// Keep the Blueprint-callable contract coherent even when callers skip the
	// normal toggle path. No pending sword request or movement lock may survive.
	CancelSwordAttack();
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

void URequiemCombatComponent::EnterSwordCombat(const ERequiemCombatEntryReason EntryReason)
{
	// The sword style supersedes every unarmed request/step, including calls made
	// directly from Blueprint instead of through ToggleSwordCombat.
	CancelUnarmedAttackForExternalReaction();
	if (CombatState == ERequiemCombatState::CombatSword)
	{
		return;
	}

	CombatState = ERequiemCombatState::CombatSword;
	MarkCombatActivity();
	(void)EntryReason;
}

void URequiemCombatComponent::EnterCurrentCombat(const ERequiemCombatEntryReason EntryReason)
{
	if (CombatState == ERequiemCombatState::CombatSword)
	{
		MarkCombatActivity();
		return;
	}

	EnterUnarmedCombat(EntryReason);
}

void URequiemCombatComponent::ExitCombat()
{
	const bool bCancelPendingInitialAttack =
		bInitialUnarmedAttackRequested && !bUnarmedAttackActive;
	const bool bCancelPendingSwordAttack =
		PendingSwordAttackType != ERequiemSwordAttackType::None
		&& !bSwordAttackActive;
	CombatState = ERequiemCombatState::Normal;
	bInitialUnarmedAttackRequested = false;
	bUnarmedAttackInputWindowOpen = false;
	bQueuedUnarmedFollowUp = false;
	CancelSwordCharge();
	PendingSwordAttackType = ERequiemSwordAttackType::None;
	bSwordAttackInputWindowOpen = false;
	bQueuedSwordLightFollowUp = false;
	if (bCancelPendingInitialAttack)
	{
		// No authored strike committed yet, so release only the input lock and let
		// CharacterMovement preserve its current braking velocity.
		bUnarmedAttackMovementLocked = false;
	}
	if (bCancelPendingSwordAttack)
	{
		bSwordAttackMovementLocked = false;
	}
}

bool URequiemCombatComponent::ConsumeInitialUnarmedAttackRequest()
{
	if (const URequiemHealthComponent* HealthComponent =
		GetOwner() ? GetOwner()->FindComponentByClass<URequiemHealthComponent>() : nullptr;
		HealthComponent && HealthComponent->AreActionsLocked())
	{
		bInitialUnarmedAttackRequested = false;
		return false;
	}

	if (!bInitialUnarmedAttackRequested
		|| CombatState != ERequiemCombatState::CombatUnarmed)
	{
		return false;
	}

	bInitialUnarmedAttackRequested = false;
	return true;
}

void URequiemCombatComponent::BeginUnarmedAttackStep(
	const bool bApplyForwardLunge,
	const int32 ComboAnimationIndex)
{
	if (const URequiemHealthComponent* HealthComponent =
		GetOwner() ? GetOwner()->FindComponentByClass<URequiemHealthComponent>() : nullptr;
		HealthComponent && HealthComponent->AreActionsLocked())
	{
		CancelUnarmedAttackForExternalReaction();
		return;
	}

	bUnarmedAttackActive = true;
	bUnarmedAttackMovementLocked = true;
	bUnarmedAttackInputWindowOpen = false;
	bQueuedUnarmedFollowUp = false;
	bUnarmedHitAttempted = false;
	ActiveUnarmedAttackIndex = ComboAnimationIndex;
	LastUnarmedHitActor.Reset();

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

void URequiemCombatComponent::UpdateUnarmedAttackHit(const float NormalizedTime)
{
	if (!bUnarmedAttackActive
		|| bUnarmedHitAttempted
		|| ActiveUnarmedAttackIndex == INDEX_NONE
		|| NormalizedTime < FMath::Clamp(UnarmedHitMomentNormalized, 0.0f, 0.7f))
	{
		return;
	}

	bUnarmedHitAttempted = true;
	UnarmedHitAttemptSerial = UnarmedHitAttemptSerial == MAX_int32
		? 1
		: UnarmedHitAttemptSerial + 1;
	ResolveUnarmedAttackHit();
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

void URequiemCombatComponent::ReleaseUnarmedAttackMovementLock()
{
	if (!bUnarmedAttackMovementLocked)
	{
		return;
	}

	bUnarmedAttackMovementLocked = false;
	// CharacterMovement keeps the current planar velocity and blends it into the
	// held direction using its configured acceleration/braking. The active combo
	// step, input window and single buffered follow-up remain untouched.
}

void URequiemCombatComponent::EndUnarmedAttackSequence()
{
	const bool bWasMovementLocked = bUnarmedAttackMovementLocked;
	ReleaseUnarmedAttackMovementLock();
	bUnarmedAttackActive = false;
	bUnarmedAttackInputWindowOpen = false;
	bQueuedUnarmedFollowUp = false;
	bUnarmedHitAttempted = false;
	ActiveUnarmedAttackIndex = INDEX_NONE;

	ACharacter* Character = Cast<ACharacter>(GetOwner());
	UCharacterMovementComponent* MovementComponent = Character
		? Character->GetCharacterMovement()
		: nullptr;
	if (bWasMovementLocked && MovementComponent)
	{
		// An interruption before normal movement recovery still cancels the
		// authored lunge. Normal completion never stops movement a second time.
		SetPlanarVelocity(MovementComponent, FVector::ZeroVector);
	}
}

void URequiemCombatComponent::CancelUnarmedAttackForExternalReaction()
{
	bInitialUnarmedAttackRequested = false;
	EndUnarmedAttackSequence();
}

bool URequiemCombatComponent::BeginSwordCharge()
{
	if (CombatState != ERequiemCombatState::CombatSword
		|| bSwordChargeActive
		|| IsSwordHeavyAttackCommitted()
		|| PendingSwordAttackType != ERequiemSwordAttackType::None
		|| bQueuedSwordLightFollowUp
		|| bUnarmedAttackActive
		|| bInitialUnarmedAttackRequested
		|| (bSwordAttackActive && !IsSwordAttackInputWindowOpen()))
	{
		return false;
	}

	ACharacter* Character = Cast<ACharacter>(GetOwner());
	UCharacterMovementComponent* MovementComponent = Character
		? Character->GetCharacterMovement()
		: nullptr;
	if (!Character
		|| !MovementComponent
		|| Character->IsCrouched()
		|| MovementComponent->IsFalling()
		|| !MovementComponent->IsMovingOnGround())
	{
		return false;
	}

	if (const URequiemHealthComponent* HealthComponent =
		GetOwner() ? GetOwner()->FindComponentByClass<URequiemHealthComponent>() : nullptr;
		HealthComponent && HealthComponent->AreActionsLocked())
	{
		return false;
	}

	if (const URequiemDodgeComponent* DodgeComponent =
		GetOwner() ? GetOwner()->FindComponentByClass<URequiemDodgeComponent>() : nullptr;
		DodgeComponent && DodgeComponent->AreDodgeRestrictedActionsLocked())
	{
		return false;
	}

	bSwordChargeActive = true;
	SwordChargeStartTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	SwordChargeRequestSerial = AdvanceSerial(SwordChargeRequestSerial);
	return true;
}

ERequiemSwordAttackRequestResult URequiemCombatComponent::ReleaseSwordCharge()
{
	if (!bSwordChargeActive)
	{
		return ERequiemSwordAttackRequestResult::Rejected;
	}

	const float HeldDurationSeconds = GetSwordChargeElapsedSeconds();
	bSwordChargeActive = false;
	SwordChargeStartTimeSeconds = -1.0f;
	const ERequiemSwordAttackType RequestedType =
		HeldDurationSeconds >= FMath::Max(SwordChargeThresholdSeconds, 0.05f)
		? ERequiemSwordAttackType::Heavy
		: ERequiemSwordAttackType::Light;
	return RequestSwordAttack(RequestedType);
}

void URequiemCombatComponent::CancelSwordCharge()
{
	bSwordChargeActive = false;
	SwordChargeStartTimeSeconds = -1.0f;
}

float URequiemCombatComponent::GetSwordChargeElapsedSeconds() const
{
	if (!bSwordChargeActive)
	{
		return 0.0f;
	}

	const UWorld* World = GetWorld();
	return World
		? FMath::Max(0.0f, World->GetTimeSeconds() - SwordChargeStartTimeSeconds)
		: 0.0f;
}

ERequiemSwordAttackRequestResult URequiemCombatComponent::RequestSwordAttack(
	const ERequiemSwordAttackType AttackType)
{
	if (AttackType == ERequiemSwordAttackType::None
		|| CombatState != ERequiemCombatState::CombatSword)
	{
		return ERequiemSwordAttackRequestResult::Rejected;
	}

	if (const URequiemHealthComponent* HealthComponent =
		GetOwner() ? GetOwner()->FindComponentByClass<URequiemHealthComponent>() : nullptr;
		HealthComponent && HealthComponent->AreActionsLocked())
	{
		return ERequiemSwordAttackRequestResult::Rejected;
	}

	if (const URequiemDodgeComponent* DodgeComponent =
		GetOwner() ? GetOwner()->FindComponentByClass<URequiemDodgeComponent>() : nullptr;
		DodgeComponent && DodgeComponent->AreDodgeRestrictedActionsLocked())
	{
		return ERequiemSwordAttackRequestResult::Rejected;
	}

	ACharacter* Character = Cast<ACharacter>(GetOwner());
	UCharacterMovementComponent* MovementComponent = Character
		? Character->GetCharacterMovement()
		: nullptr;
	if (!Character
		|| !MovementComponent
		|| Character->IsCrouched()
		|| MovementComponent->IsFalling()
		|| !MovementComponent->IsMovingOnGround())
	{
		return ERequiemSwordAttackRequestResult::Rejected;
	}

	AttackRequestSerial = AdvanceSerial(AttackRequestSerial);
	if (!bSwordAttackActive)
	{
		if (PendingSwordAttackType != ERequiemSwordAttackType::None)
		{
			return ERequiemSwordAttackRequestResult::Rejected;
		}

		PendingSwordAttackType = AttackType;
		bSwordAttackMovementLocked = true;
		Character->ConsumeMovementInputVector();
		MarkCombatActivity();
		if (AttackType == ERequiemSwordAttackType::Heavy)
		{
			SwordHeavyRequestSerial = AdvanceSerial(SwordHeavyRequestSerial);
			return ERequiemSwordAttackRequestResult::InitialHeavyAccepted;
		}

		SwordLightRequestSerial = AdvanceSerial(SwordLightRequestSerial);
		return ERequiemSwordAttackRequestResult::InitialLightAccepted;
	}

	if (AttackType == ERequiemSwordAttackType::Light
		&& ActiveSwordAttackType == ERequiemSwordAttackType::Light
		&& IsSwordAttackInputWindowOpen())
	{
		bQueuedSwordLightFollowUp = true;
		SwordLightRequestSerial = AdvanceSerial(SwordLightRequestSerial);
		MarkCombatActivity();
		return ERequiemSwordAttackRequestResult::FollowUpBuffered;
	}

	return ERequiemSwordAttackRequestResult::Rejected;
}

ERequiemSwordAttackType URequiemCombatComponent::ConsumeInitialSwordAttackRequest()
{
	if (const URequiemHealthComponent* HealthComponent =
		GetOwner() ? GetOwner()->FindComponentByClass<URequiemHealthComponent>() : nullptr;
		HealthComponent && HealthComponent->AreActionsLocked())
	{
		PendingSwordAttackType = ERequiemSwordAttackType::None;
		bSwordAttackMovementLocked = false;
		return ERequiemSwordAttackType::None;
	}

	if (CombatState != ERequiemCombatState::CombatSword)
	{
		CancelSwordAttack();
		return ERequiemSwordAttackType::None;
	}

	const ERequiemSwordAttackType Result = PendingSwordAttackType;
	PendingSwordAttackType = ERequiemSwordAttackType::None;
	return Result;
}

void URequiemCombatComponent::BeginSwordAttackStep(
	const ERequiemSwordAttackType AttackType,
	const bool bApplyForwardLunge,
	const int32 ComboAnimationIndex)
{
	if (AttackType == ERequiemSwordAttackType::None
		|| CombatState != ERequiemCombatState::CombatSword)
	{
		CancelSwordAttack();
		return;
	}

	if (const URequiemHealthComponent* HealthComponent =
		GetOwner() ? GetOwner()->FindComponentByClass<URequiemHealthComponent>() : nullptr;
		HealthComponent && HealthComponent->AreActionsLocked())
	{
		CancelSwordAttack();
		return;
	}

	bSwordAttackActive = true;
	bSwordAttackMovementLocked = true;
	bSwordAttackInputWindowOpen = false;
	bQueuedSwordLightFollowUp = false;
	bSwordHitAttempted = false;
	ActiveSwordAttackType = AttackType;
	ActiveSwordAttackIndex = ComboAnimationIndex;
	LastSwordHitActor.Reset();

	ACharacter* Character = Cast<ACharacter>(GetOwner());
	UCharacterMovementComponent* MovementComponent = Character
		? Character->GetCharacterMovement()
		: nullptr;
	if (!Character || !MovementComponent)
	{
		return;
	}

	Character->ConsumeMovementInputVector();
	if (AttackType == ERequiemSwordAttackType::Heavy
		|| !bApplyForwardLunge
		|| SwordLightAttackLungeSpeed <= UE_KINDA_SMALL_NUMBER)
	{
		// Heavy displacement is supplied only by its authored root-motion presentation.
		SetPlanarVelocity(MovementComponent, FVector::ZeroVector);
		return;
	}

	const FVector ForwardDirection = Character->GetActorForwardVector().GetSafeNormal2D();
	SetPlanarVelocity(MovementComponent, ForwardDirection * SwordLightAttackLungeSpeed);
}

void URequiemCombatComponent::UpdateSwordAttackHit(const float NormalizedTime)
{
	if (!bSwordAttackActive
		|| bSwordHitAttempted
		|| ActiveSwordAttackType == ERequiemSwordAttackType::None
		|| ActiveSwordAttackIndex == INDEX_NONE)
	{
		return;
	}

	const float HitMoment = ActiveSwordAttackType == ERequiemSwordAttackType::Heavy
		? SwordHeavyHitMomentNormalized
		: SwordLightHitMomentNormalized;
	if (NormalizedTime < FMath::Clamp(HitMoment, 0.0f, 1.0f))
	{
		return;
	}

	bSwordHitAttempted = true;
	SwordHitAttemptSerial = AdvanceSerial(SwordHitAttemptSerial);
	ResolveSwordAttackHit();
}

void URequiemCombatComponent::SetSwordAttackInputWindowOpen(const bool bOpen)
{
	bSwordAttackInputWindowOpen = bOpen
		&& bSwordAttackActive
		&& ActiveSwordAttackType == ERequiemSwordAttackType::Light
		&& CombatState == ERequiemCombatState::CombatSword;
}

bool URequiemCombatComponent::ConsumeQueuedSwordLightFollowUp()
{
	if (!bQueuedSwordLightFollowUp)
	{
		return false;
	}

	bQueuedSwordLightFollowUp = false;
	bSwordAttackInputWindowOpen = false;
	return true;
}

void URequiemCombatComponent::ReleaseSwordAttackMovementLock()
{
	if (!bSwordAttackMovementLocked
		|| ActiveSwordAttackType == ERequiemSwordAttackType::Heavy)
	{
		return;
	}

	bSwordAttackMovementLocked = false;
}

void URequiemCombatComponent::EndSwordAttackSequence()
{
	const bool bWasMovementLocked = bSwordAttackMovementLocked;
	bSwordAttackMovementLocked = false;
	bSwordAttackActive = false;
	bSwordAttackInputWindowOpen = false;
	bQueuedSwordLightFollowUp = false;
	bSwordHitAttempted = false;
	ActiveSwordAttackType = ERequiemSwordAttackType::None;
	ActiveSwordAttackIndex = INDEX_NONE;

	ACharacter* Character = Cast<ACharacter>(GetOwner());
	UCharacterMovementComponent* MovementComponent = Character
		? Character->GetCharacterMovement()
		: nullptr;
	if (bWasMovementLocked && MovementComponent)
	{
		SetPlanarVelocity(MovementComponent, FVector::ZeroVector);
	}
}

void URequiemCombatComponent::CancelSwordAttack()
{
	CancelSwordCharge();
	PendingSwordAttackType = ERequiemSwordAttackType::None;
	EndSwordAttackSequence();
}

void URequiemCombatComponent::CancelActiveAttackForExternalReaction()
{
	CancelUnarmedAttackForExternalReaction();
	CancelSwordAttack();
}

bool URequiemCombatComponent::CanAutoExit(const bool bEnemyNearby) const
{
	if (CombatState == ERequiemCombatState::Normal
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

int32 URequiemCombatComponent::AdvanceSerial(const int32 Serial)
{
	return Serial == MAX_int32 ? 1 : Serial + 1;
}

void URequiemCombatComponent::ResolveUnarmedAttackHit()
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	UWorld* World = GetWorld();
	if (!Character
		|| !World
		|| UnarmedHitDamage <= UE_KINDA_SMALL_NUMBER
		|| UnarmedHitRadius <= UE_KINDA_SMALL_NUMBER)
	{
		return;
	}

	const FVector ForwardDirection = Character->GetActorForwardVector().GetSafeNormal2D();
	if (ForwardDirection.IsNearlyZero())
	{
		return;
	}

	const FVector TraceStart = Character->GetActorLocation() + FVector::UpVector * UnarmedHitHeight;
	const FVector TraceEnd = TraceStart + ForwardDirection * FMath::Max(0.0f, UnarmedHitRange);
	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(RequiemUnarmedHit), false, Character);
	TArray<FHitResult> HitResults;
	if (!World->SweepMultiByObjectType(
		HitResults,
		TraceStart,
		TraceEnd,
		FQuat::Identity,
		ObjectQueryParams,
		FCollisionShape::MakeSphere(UnarmedHitRadius),
		QueryParams))
	{
		return;
	}

	HitResults.Sort([](const FHitResult& Left, const FHitResult& Right)
	{
		return Left.Time < Right.Time;
	});

	for (const FHitResult& HitResult : HitResults)
	{
		AActor* HitActor = HitResult.GetActor();
		if (!HitActor || HitActor == Character || !HitActor->CanBeDamaged())
		{
			continue;
		}
		if (!HasClearDamagePath(
			World,
			TraceStart,
			HitResult.ImpactPoint,
			Character,
			HitActor))
		{
			break;
		}

		const float AppliedDamage = UGameplayStatics::ApplyPointDamage(
			HitActor,
			UnarmedHitDamage,
			ForwardDirection,
			HitResult,
			Character->GetController(),
			Character,
			nullptr);
		if (AppliedDamage > UE_KINDA_SMALL_NUMBER)
		{
			LastUnarmedHitActor = HitActor;
			UnarmedHitConfirmSerial = UnarmedHitConfirmSerial == MAX_int32
				? 1
				: UnarmedHitConfirmSerial + 1;
			MarkCombatActivity();
		}
		break;
	}
}

void URequiemCombatComponent::ResolveSwordAttackHit()
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	UWorld* World = GetWorld();
	const float HitDamage = ActiveSwordAttackType == ERequiemSwordAttackType::Heavy
		? SwordHeavyHitDamage
		: SwordLightHitDamage;
	if (!Character
		|| !World
		|| HitDamage <= UE_KINDA_SMALL_NUMBER
		|| SwordHitRadius <= UE_KINDA_SMALL_NUMBER)
	{
		return;
	}

	const FVector ForwardDirection = Character->GetActorForwardVector().GetSafeNormal2D();
	if (ForwardDirection.IsNearlyZero())
	{
		return;
	}

	const FVector TraceStart = Character->GetActorLocation() + FVector::UpVector * SwordHitHeight;
	const FVector TraceEnd = TraceStart + ForwardDirection * FMath::Max(0.0f, SwordHitRange);
	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(RequiemSwordHit), false, Character);
	TArray<FHitResult> HitResults;
	if (!World->SweepMultiByObjectType(
		HitResults,
		TraceStart,
		TraceEnd,
		FQuat::Identity,
		ObjectQueryParams,
		FCollisionShape::MakeSphere(SwordHitRadius),
		QueryParams))
	{
		return;
	}

	HitResults.Sort([](const FHitResult& Left, const FHitResult& Right)
	{
		return Left.Time < Right.Time;
	});

	for (const FHitResult& HitResult : HitResults)
	{
		AActor* HitActor = HitResult.GetActor();
		if (!HitActor || HitActor == Character || !HitActor->CanBeDamaged())
		{
			continue;
		}
		if (!HasClearDamagePath(
			World,
			TraceStart,
			HitResult.ImpactPoint,
			Character,
			HitActor))
		{
			break;
		}

		const float AppliedDamage = UGameplayStatics::ApplyPointDamage(
			HitActor,
			HitDamage,
			ForwardDirection,
			HitResult,
			Character->GetController(),
			Character,
			nullptr);
		if (AppliedDamage > UE_KINDA_SMALL_NUMBER)
		{
			LastSwordHitActor = HitActor;
			SwordHitConfirmSerial = AdvanceSerial(SwordHitConfirmSerial);
			MarkCombatActivity();
		}
		break;
	}
}
