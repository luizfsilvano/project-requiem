// Copyright Project Requiem. All Rights Reserved.

#include "Components/RequiemLockOnComponent.h"

#include "Characters/RequiemCharacter.h"
#include "Combat/RequiemLockOnTargetInterface.h"
#include "Components/BillboardComponent.h"
#include "Components/RequiemCombatComponent.h"
#include "Components/RequiemDodgeComponent.h"
#include "Components/RequiemHealthComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"

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

	UpdateIndicator();
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
	HideIndicator();
	LockOnTarget.Reset();
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
	SetComponentTickEnabled(false);
	HideIndicator();
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

	UpdateIndicator();
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

	FVector ViewLocation = Character->GetPawnViewLocation();
	FRotator UnusedViewRotation = Controller->GetControlRotation();
	if (const APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		PlayerController->GetPlayerViewPoint(ViewLocation, UnusedViewRotation);
	}

	const FVector FocusLocation = GetLockOnFocusLocation();
	const FVector TargetDirection =
		(FocusLocation - Character->GetActorLocation()).GetSafeNormal2D();
	if (TargetDirection.IsNearlyZero())
	{
		return;
	}

	FRotator DesiredRotation = (FocusLocation - ViewLocation).Rotation();
	DesiredRotation.Yaw = TargetDirection.Rotation().Yaw;
	DesiredRotation.Pitch = FMath::Clamp(
		FRotator::NormalizeAxis(DesiredRotation.Pitch),
		FMath::Min(MinimumCameraPitch, MaximumCameraPitch),
		FMath::Max(MinimumCameraPitch, MaximumCameraPitch));
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

void URequiemLockOnComponent::UpdateIndicator()
{
	ARequiemCharacter* Character = GetOwnerCharacter();
	AActor* Target = LockOnTarget.Get();
	UBillboardComponent* Indicator = Character
		? Character->GetLockOnIndicator()
		: nullptr;
	if (!Indicator || !IsTargetValid(Target))
	{
		HideIndicator();
		return;
	}

	FVector BoundsOrigin = GetLockOnFocusLocation();
	FVector BoundsExtent = FVector::ZeroVector;
	Target->GetActorBounds(false, BoundsOrigin, BoundsExtent);
	Indicator->SetWorldLocation(
		BoundsOrigin + FVector::UpVector * (BoundsExtent.Z + IndicatorHeightOffset));
	Indicator->SetHiddenInGame(false, true);
	Indicator->SetVisibility(true, true);
}

void URequiemLockOnComponent::HideIndicator()
{
	ARequiemCharacter* Character = GetOwnerCharacter();
	UBillboardComponent* Indicator = Character
		? Character->GetLockOnIndicator()
		: nullptr;
	if (Indicator)
	{
		Indicator->SetVisibility(false, true);
		Indicator->SetHiddenInGame(true, true);
	}
}
