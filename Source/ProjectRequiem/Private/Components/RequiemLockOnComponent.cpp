// Copyright Project Requiem. All Rights Reserved.

#include "Components/RequiemLockOnComponent.h"

#include "Characters/RequiemCharacter.h"
#include "Combat/RequiemLockOnTargetInterface.h"
#include "Components/DecalComponent.h"
#include "Components/RequiemCombatComponent.h"
#include "Components/RequiemDodgeComponent.h"
#include "Components/RequiemHealthComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"

URequiemLockOnComponent::URequiemLockOnComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void URequiemLockOnComponent::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	ARequiemCharacter* Character = GetOwnerCharacter();
	AActor* Target = LockOnTarget.Get();
	const URequiemHealthComponent* HealthComponent = Character
		? Character->GetHealthComponent()
		: nullptr;
	if (!Character
		|| (HealthComponent && HealthComponent->IsDead())
		|| !IsTargetValid(Target))
	{
		ClearLockOn();
		return;
	}

	const float EffectiveBreakRange = FMath::Max(BreakRange, AcquisitionRange);
	if (FVector::DistSquared2D(Character->GetActorLocation(), GetLockOnFocusLocation())
		> FMath::Square(EffectiveBreakRange))
	{
		ClearLockOn();
		return;
	}

	UpdateGroundIndicator();
	if (HealthComponent && HealthComponent->IsDamageReactionActive())
	{
		// Hit reactions and authored knockback keep rotation ownership until presentation ends.
		return;
	}
	const URequiemCombatComponent* CombatComponent = Character->GetCombatComponent();
	if (CombatComponent
		&& (CombatComponent->IsUnarmedAttackActive()
			|| CombatComponent->IsUnarmedAttackMovementLocked()
			|| CombatComponent->HasPendingInitialUnarmedAttackRequest()))
	{
		// A strike already in progress keeps its authored facing and lunge commitment.
		return;
	}

	UpdateCameraTracking(FMath::Max(0.0f, DeltaTime));
}

void URequiemLockOnComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	HideGroundIndicator();
	LockOnTarget.Reset();
	bHasLockedCameraPitch = false;
	SetComponentTickEnabled(false);
	Super::EndPlay(EndPlayReason);
}

bool URequiemLockOnComponent::ToggleLockOn()
{
	if (IsLockOnActive())
	{
		// Manual disengage remains available even during reactions or death locks.
		ClearLockOn();
		return false;
	}

	return TryAcquireLockOn();
}

bool URequiemLockOnComponent::TryAcquireLockOn()
{
	ARequiemCharacter* Character = GetOwnerCharacter();
	const URequiemHealthComponent* HealthComponent = Character
		? Character->GetHealthComponent()
		: nullptr;
	if (!Character || (HealthComponent && HealthComponent->AreActionsLocked()))
	{
		return false;
	}

	AActor* Candidate = FindBestTarget();
	if (!Candidate)
	{
		return false;
	}

	BeginLockOn(Candidate);
	return true;
}

void URequiemLockOnComponent::ClearLockOn()
{
	AActor* PreviousTarget = LockOnTarget.Get();
	LockOnTarget.Reset();
	bHasLockedCameraPitch = false;
	SetComponentTickEnabled(false);
	HideGroundIndicator();
	if (PreviousTarget)
	{
		OnLockOnTargetChanged.Broadcast(PreviousTarget, nullptr);
	}
}

FVector URequiemLockOnComponent::GetLockOnFocusLocation() const
{
	AActor* Target = LockOnTarget.Get();
	if (!IsTargetValid(Target))
	{
		return FVector::ZeroVector;
	}

	return IRequiemLockOnTargetInterface::Execute_GetLockOnFocusLocation(Target);
}

ARequiemCharacter* URequiemLockOnComponent::GetOwnerCharacter() const
{
	return Cast<ARequiemCharacter>(GetOwner());
}

AActor* URequiemLockOnComponent::FindBestTarget() const
{
	const ARequiemCharacter* Character = GetOwnerCharacter();
	UWorld* World = GetWorld();
	if (!Character || !World)
	{
		return nullptr;
	}

	const FVector OwnerLocation = Character->GetActorLocation();
	FVector OwnerForward = Character->GetActorForwardVector().GetSafeNormal2D();
	if (OwnerForward.IsNearlyZero())
	{
		OwnerForward = FVector::ForwardVector;
	}

	const float MaximumDistanceSquared = FMath::Square(FMath::Max(0.0f, AcquisitionRange));
	const float MinimumForwardDot = FMath::Cos(FMath::DegreesToRadians(
		FMath::Clamp(AcquisitionConeHalfAngleDegrees, 0.0f, 89.0f)));
	float BestDistanceSquared = TNumericLimits<float>::Max();
	AActor* BestTarget = nullptr;

	for (TActorIterator<AActor> Iterator(World); Iterator; ++Iterator)
	{
		AActor* Candidate = *Iterator;
		if (!IsTargetValid(Candidate))
		{
			continue;
		}

		const FVector FocusLocation =
			IRequiemLockOnTargetInterface::Execute_GetLockOnFocusLocation(Candidate);
		const FVector ToCandidate = FocusLocation - OwnerLocation;
		const float DistanceSquared = ToCandidate.SizeSquared2D();
		if (DistanceSquared <= UE_KINDA_SMALL_NUMBER
			|| DistanceSquared > MaximumDistanceSquared
			|| DistanceSquared >= BestDistanceSquared)
		{
			continue;
		}

		const FVector DirectionToCandidate = ToCandidate.GetSafeNormal2D();
		if (FVector::DotProduct(OwnerForward, DirectionToCandidate) < MinimumForwardDot
			|| !HasClearAcquisitionPath(FocusLocation, Candidate))
		{
			continue;
		}

		BestDistanceSquared = DistanceSquared;
		BestTarget = Candidate;
	}

	return BestTarget;
}

bool URequiemLockOnComponent::IsTargetValid(const AActor* Candidate) const
{
	return IsValid(Candidate)
		&& Candidate != GetOwner()
		&& Candidate->GetClass()->ImplementsInterface(
			URequiemLockOnTargetInterface::StaticClass())
		&& IRequiemLockOnTargetInterface::Execute_IsValidLockOnTarget(
			const_cast<AActor*>(Candidate));
}

bool URequiemLockOnComponent::HasClearAcquisitionPath(
	const FVector& FocusLocation,
	const AActor* Candidate) const
{
	const ARequiemCharacter* Character = GetOwnerCharacter();
	UWorld* World = GetWorld();
	if (!Character || !World || !Candidate)
	{
		return false;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(RequiemLockOnAcquisition), false);
	QueryParams.AddIgnoredActor(Character);
	QueryParams.AddIgnoredActor(Candidate);
	return !World->LineTraceTestByChannel(
		Character->GetPawnViewLocation(),
		FocusLocation,
		ECC_Visibility,
		QueryParams);
}

void URequiemLockOnComponent::BeginLockOn(AActor* NewTarget)
{
	ARequiemCharacter* Character = GetOwnerCharacter();
	if (!Character || !IsTargetValid(NewTarget))
	{
		return;
	}

	AActor* PreviousTarget = LockOnTarget.Get();
	LockOnTarget = NewTarget;
	SetComponentTickEnabled(true);
	if (const AController* Controller = Character->GetController())
	{
		LockedCameraPitch = FRotator::NormalizeAxis(Controller->GetControlRotation().Pitch);
		bHasLockedCameraPitch = true;
	}
	else
	{
		bHasLockedCameraPitch = false;
	}

	if (URequiemCombatComponent* CombatComponent = Character->GetCombatComponent())
	{
		CombatComponent->EnterUnarmedCombat(ERequiemCombatEntryReason::LockOn);
	}

	const FVector TargetDirection =
		(GetLockOnFocusLocation() - Character->GetActorLocation()).GetSafeNormal2D();
	if (!TargetDirection.IsNearlyZero())
	{
		const URequiemDodgeComponent* DodgeComponent = Character->GetDodgeComponent();
		const URequiemHealthComponent* HealthComponent = Character->GetHealthComponent();
		const URequiemCombatComponent* CombatComponent = Character->GetCombatComponent();
		const bool bDodgeOwnsRotation = DodgeComponent && DodgeComponent->IsDodgeActive();
		const bool bDamageOwnsRotation = HealthComponent
			&& (HealthComponent->IsDamageReactionActive() || HealthComponent->IsDead());
		const bool bAttackOwnsRotation = CombatComponent
			&& (CombatComponent->IsUnarmedAttackActive()
				|| CombatComponent->IsUnarmedAttackMovementLocked()
				|| CombatComponent->HasPendingInitialUnarmedAttackRequest());
		if (!bAttackOwnsRotation)
		{
			if (AController* Controller = Character->GetController())
			{
				FRotator ControlRotation = Controller->GetControlRotation();
				ControlRotation.Yaw = TargetDirection.Rotation().Yaw;
				ControlRotation.Roll = 0.0f;
				Controller->SetControlRotation(ControlRotation);
			}
		}
		if (!bDodgeOwnsRotation && !bDamageOwnsRotation && !bAttackOwnsRotation)
		{
			Character->SetActorRotation(
				FRotator(0.0f, TargetDirection.Rotation().Yaw, 0.0f),
				ETeleportType::None);
		}
	}

	UpdateGroundIndicator();
	OnLockOnTargetChanged.Broadcast(PreviousTarget, NewTarget);
}

void URequiemLockOnComponent::UpdateCameraTracking(const float DeltaTime)
{
	ARequiemCharacter* Character = GetOwnerCharacter();
	AController* Controller = Character ? Character->GetController() : nullptr;
	if (!Character || !Controller)
	{
		return;
	}

	const FVector TargetDirection =
		(GetLockOnFocusLocation() - Character->GetActorLocation()).GetSafeNormal2D();
	if (TargetDirection.IsNearlyZero())
	{
		return;
	}

	FRotator DesiredRotation = Controller->GetControlRotation();
	DesiredRotation.Yaw = TargetDirection.Rotation().Yaw;
	if (bHasLockedCameraPitch)
	{
		DesiredRotation.Pitch = LockedCameraPitch;
	}
	DesiredRotation.Roll = 0.0f;

	const FRotator NewRotation = CameraTrackingInterpSpeed <= UE_KINDA_SMALL_NUMBER
		? DesiredRotation
		: FMath::RInterpTo(
			Controller->GetControlRotation(),
			DesiredRotation,
			DeltaTime,
			CameraTrackingInterpSpeed);
	Controller->SetControlRotation(NewRotation);
}

void URequiemLockOnComponent::UpdateGroundIndicator()
{
	ARequiemCharacter* Character = GetOwnerCharacter();
	AActor* Target = LockOnTarget.Get();
	UDecalComponent* Indicator = Character
		? Character->GetLockOnGroundIndicator()
		: nullptr;
	if (!Indicator || !IsTargetValid(Target))
	{
		HideGroundIndicator();
		return;
	}

	FVector BoundsOrigin = GetLockOnFocusLocation();
	FVector BoundsExtent = FVector::ZeroVector;
	Target->GetActorBounds(false, BoundsOrigin, BoundsExtent);
	const FVector DesiredDecalSize(
		FMath::Max(IndicatorProjectionDepth, 0.1f),
		FMath::Max(IndicatorRadius, 0.0f),
		FMath::Max(IndicatorRadius, 0.0f));
	if (!Indicator->DecalSize.Equals(DesiredDecalSize))
	{
		Indicator->DecalSize = DesiredDecalSize;
		Indicator->MarkRenderStateDirty();
	}
	const FVector GroundLocation(
		BoundsOrigin.X,
		BoundsOrigin.Y,
		BoundsOrigin.Z - BoundsExtent.Z + IndicatorGroundOffset);
	Indicator->SetWorldLocationAndRotation(
		GroundLocation,
		FRotator(-90.0f, 0.0f, 0.0f));
	Indicator->SetHiddenInGame(false, true);
	Indicator->SetVisibility(true, true);
}

void URequiemLockOnComponent::HideGroundIndicator()
{
	ARequiemCharacter* Character = GetOwnerCharacter();
	UDecalComponent* Indicator = Character
		? Character->GetLockOnGroundIndicator()
		: nullptr;
	if (Indicator)
	{
		Indicator->SetVisibility(false, true);
		Indicator->SetHiddenInGame(true, true);
	}
}
