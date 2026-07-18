// Copyright Project Requiem. All Rights Reserved.

#include "Combat/RequiemCombatDummy.h"

#include "Characters/RequiemCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/World.h"
#include "TimerManager.h"

namespace
{
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

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(RequiemDummyObstruction), false);
	QueryParams.AddIgnoredActor(SourceActor);
	QueryParams.AddIgnoredActor(TargetActor);
	return !World->LineTraceTestByChannel(
		TraceStart,
		TraceEnd,
		ECC_Visibility,
		QueryParams);
}
}

ARequiemCombatDummy::ARequiemCombatDummy()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	SetCanBeDamaged(true);

	HitCollision = CreateDefaultSubobject<UCapsuleComponent>(TEXT("HitCollision"));
	SetRootComponent(HitCollision);
	HitCollision->InitCapsuleSize(55.0f, 112.0f);
	HitCollision->SetCollisionProfileName(TEXT("Pawn"));
	HitCollision->SetGenerateOverlapEvents(false);
	HitCollision->SetCanEverAffectNavigation(false);

	VisualMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisualMesh"));
	VisualMesh->SetupAttachment(HitCollision);
	VisualMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	VisualMesh->SetGenerateOverlapEvents(false);
	VisualMesh->SetCanEverAffectNavigation(false);
}

void ARequiemCombatDummy::BeginPlay()
{
	Super::BeginPlay();

	MaxHealth = FMath::Max(MaxHealth, 1.0f);
	CurrentHealth = MaxHealth;
	DummyState = ERequiemCombatDummyState::Alive;
	RestingVisualRelativeTransform = VisualMesh
		? VisualMesh->GetRelativeTransform()
		: FTransform::Identity;
	bHasCapturedVisualTransform = VisualMesh != nullptr;
}

void ARequiemCombatDummy::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bHitFeedbackActive || DummyState == ERequiemCombatDummyState::Defeated)
	{
		SetActorTickEnabled(false);
		return;
	}

	HitFeedbackElapsedSeconds += FMath::Max(0.0f, DeltaSeconds);
	const float Duration = FMath::Max(HitFeedbackDurationSeconds, 0.05f);
	const float NormalizedTime = FMath::Clamp(HitFeedbackElapsedSeconds / Duration, 0.0f, 1.0f);
	if (VisualMesh && bHasCapturedVisualTransform)
	{
		FRotator FeedbackRotation = RestingVisualRelativeTransform.Rotator();
		const float FeedbackSide = DamageSerial % 2 == 0 ? -1.0f : 1.0f;
		FeedbackRotation.Roll += FMath::Sin(NormalizedTime * PI) * HitFeedbackTiltDegrees * FeedbackSide;
		VisualMesh->SetRelativeRotation(FeedbackRotation);
	}

	if (NormalizedTime >= 1.0f)
	{
		bHitFeedbackActive = false;
		HitFeedbackElapsedSeconds = 0.0f;
		DummyState = ERequiemCombatDummyState::Alive;
		RestoreVisualTransform();
		SetActorTickEnabled(false);
	}
}

float ARequiemCombatDummy::TakeDamage(
	const float DamageAmount,
	const FDamageEvent& DamageEvent,
	AController* EventInstigator,
	AActor* DamageCauser)
{
	FRequiemDamageRequest Request;
	Request.DamageAmount = DamageAmount;
	Request.HitRegion = ERequiemHitRegion::Chest;
	Request.Strength = DamageAmount >= StrongDamageThreshold
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

	return Super::TakeDamage(LastAppliedDamage, DamageEvent, EventInstigator, DamageCauser);
}

ERequiemDamageOutcome ARequiemCombatDummy::ApplyRequiemDamage(
	const FRequiemDamageRequest& Request)
{
	if (!FMath::IsFinite(Request.DamageAmount)
		|| Request.DamageAmount <= UE_KINDA_SMALL_NUMBER)
	{
		return ERequiemDamageOutcome::IgnoredInvalid;
	}
	if (IsDefeated())
	{
		return ERequiemDamageOutcome::IgnoredDead;
	}

	LastDamageStrength = Request.Strength;
	LastImpactDirection = Request.ImpactDirection.GetSafeNormal2D();
	LastAppliedDamage = FMath::Min(CurrentHealth, Request.DamageAmount);
	CurrentHealth = FMath::Clamp(CurrentHealth - LastAppliedDamage, 0.0f, MaxHealth);
	DamageSerial = AdvanceSerial(DamageSerial);

	if (CurrentHealth <= UE_KINDA_SMALL_NUMBER)
	{
		EnterDefeated();
		return ERequiemDamageOutcome::Killed;
	}

	BeginHitFeedback();
	return ERequiemDamageOutcome::Applied;
}

bool ARequiemCombatDummy::IsValidLockOnTarget_Implementation() const
{
	// Reacting remains targetable; only defeat or an explicit damage-disable invalidates it.
	return !IsDefeated() && CanBeDamaged();
}

FVector ARequiemCombatDummy::GetLockOnFocusLocation_Implementation() const
{
	return HitCollision ? HitCollision->GetComponentLocation() : GetActorLocation();
}

bool ARequiemCombatDummy::RequestTestAttack(
	ARequiemCharacter* Target,
	const float WindupOverrideSeconds)
{
#if UE_BUILD_SHIPPING
	return false;
#else
	if (IsDefeated() || bTestAttackPending || !IsValid(Target) || !GetWorld())
	{
		return false;
	}

	const FVector TargetDirection =
		(Target->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
	if (TargetDirection.IsNearlyZero())
	{
		return false;
	}

	SetActorRotation(FRotator(0.0f, TargetDirection.Rotation().Yaw, 0.0f));
	PendingTestAttackTarget = Target;
	bTestAttackPending = true;
	LastTestAttackOutcome = ERequiemDamageOutcome::IgnoredInvalid;
	TestAttackRequestSerial = AdvanceSerial(TestAttackRequestSerial);
	const float WindupSeconds = WindupOverrideSeconds >= 0.0f
		? WindupOverrideSeconds
		: TestAttackWindupSeconds;
	OnTestAttackTelegraphed(Target, WindupSeconds);

	GetWorldTimerManager().SetTimer(
		TestAttackTimerHandle,
		this,
		&ARequiemCombatDummy::ResolveTestAttack,
		FMath::Max(WindupSeconds, 0.01f),
		false);
	return true;
#endif
}

void ARequiemCombatDummy::ResetForTesting()
{
#if !UE_BUILD_SHIPPING
	CancelPendingTestAttack();
	MaxHealth = FMath::Max(MaxHealth, 1.0f);
	CurrentHealth = MaxHealth;
	DummyState = ERequiemCombatDummyState::Alive;
	LastDamageStrength = ERequiemDamageStrength::Light;
	LastTestAttackOutcome = ERequiemDamageOutcome::IgnoredInvalid;
	LastImpactDirection = FVector::ZeroVector;
	LastAppliedDamage = 0.0f;
	HitFeedbackElapsedSeconds = 0.0f;
	bHitFeedbackActive = false;
	ResetSerial = AdvanceSerial(ResetSerial);
	RestoreVisualTransform();
	if (HitCollision)
	{
		HitCollision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}
	SetActorTickEnabled(false);
	OnResetForTesting();
#endif
}

int32 ARequiemCombatDummy::AdvanceSerial(const int32 Serial)
{
	return Serial == MAX_int32 ? 1 : Serial + 1;
}

void ARequiemCombatDummy::BeginHitFeedback()
{
	DummyState = ERequiemCombatDummyState::Reacting;
	bHitFeedbackActive = true;
	HitFeedbackElapsedSeconds = 0.0f;
	HitFeedbackSerial = AdvanceSerial(HitFeedbackSerial);
	SetActorTickEnabled(true);
	OnHitFeedback(LastAppliedDamage, LastDamageStrength);
}

void ARequiemCombatDummy::EnterDefeated()
{
	DummyState = ERequiemCombatDummyState::Defeated;
	bHitFeedbackActive = false;
	HitFeedbackElapsedSeconds = 0.0f;
	DefeatSerial = AdvanceSerial(DefeatSerial);
	CancelPendingTestAttack();
	if (HitCollision)
	{
		HitCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (VisualMesh && bHasCapturedVisualTransform)
	{
		FRotator DefeatedRotation = RestingVisualRelativeTransform.Rotator();
		DefeatedRotation.Roll += DefeatedTiltDegrees;
		VisualMesh->SetRelativeRotation(DefeatedRotation);
	}
	SetActorTickEnabled(false);
	OnDefeated();
}

void ARequiemCombatDummy::ResolveTestAttack()
{
	ARequiemCharacter* Target = PendingTestAttackTarget.Get();
	bTestAttackPending = false;
	PendingTestAttackTarget.Reset();
	TestAttackTimerHandle.Invalidate();
	LastTestAttackOutcome = ERequiemDamageOutcome::IgnoredInvalid;
	TestAttackResolveSerial = AdvanceSerial(TestAttackResolveSerial);

	UWorld* World = GetWorld();
	if (!IsDefeated() && IsValid(Target) && World)
	{
		const FVector ForwardDirection = GetActorForwardVector().GetSafeNormal2D();
		const FVector TraceStart = GetActorLocation() + FVector::UpVector * TestAttackHeight;
		const FVector TraceEnd = TraceStart + ForwardDirection * FMath::Max(0.0f, TestAttackRange);
		FCollisionObjectQueryParams ObjectQueryParams;
		ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);
		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(RequiemDummyTestAttack), false, this);
		TArray<FHitResult> HitResults;
		World->SweepMultiByObjectType(
			HitResults,
			TraceStart,
			TraceEnd,
			FQuat::Identity,
			ObjectQueryParams,
			FCollisionShape::MakeSphere(FMath::Max(TestAttackRadius, 1.0f)),
			QueryParams);

		const FHitResult* TargetHit = HitResults.FindByPredicate([Target](const FHitResult& HitResult)
		{
			return HitResult.GetActor() == Target;
		});
		if (TargetHit)
		{
			if (HasClearDamagePath(
				World,
				TraceStart,
				TargetHit->ImpactPoint,
				this,
				Target))
			{
				FRequiemDamageRequest Request;
				Request.DamageAmount = TestAttackDamage;
				Request.HitRegion = TestAttackHitRegion;
				Request.Strength = TestAttackStrength;
				Request.ImpactDirection =
					(Target->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
				LastTestAttackOutcome = Target->ApplyRequiemDamage(Request);
			}
		}
	}

	OnTestAttackResolved(LastTestAttackOutcome);
}

void ARequiemCombatDummy::CancelPendingTestAttack()
{
	if (GetWorld())
	{
		GetWorldTimerManager().ClearTimer(TestAttackTimerHandle);
	}
	bTestAttackPending = false;
	PendingTestAttackTarget.Reset();
	TestAttackTimerHandle.Invalidate();
}

void ARequiemCombatDummy::RestoreVisualTransform()
{
	if (VisualMesh && bHasCapturedVisualTransform)
	{
		VisualMesh->SetRelativeTransform(RestingVisualRelativeTransform);
	}
}
