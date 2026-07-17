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
	return CombatState == ERequiemCombatState::CombatUnarmed
		? FName(TEXT("CombatUnarmed"))
		: FName(TEXT("Normal"));
}

void URequiemCombatComponent::ToggleUnarmedCombat()
{
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

ERequiemUnarmedAttackRequestResult URequiemCombatComponent::RequestUnarmedAttack()
{
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
