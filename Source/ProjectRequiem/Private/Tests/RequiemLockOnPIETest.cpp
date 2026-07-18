// Copyright Project Requiem. All Rights Reserved.

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Characters/RequiemCharacter.h"
#include "Combat/RequiemCombatDummy.h"
#include "Combat/RequiemLockOnTargetInterface.h"
#include "Components/RequiemCombatComponent.h"
#include "Components/RequiemDodgeComponent.h"
#include "Components/RequiemHealthComponent.h"
#include "Components/RequiemLockOnComponent.h"
#include "Components/DecalComponent.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "EnhancedActionKeyMapping.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "Kismet/GameplayStatics.h"
#include "Math/RotationMatrix.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionDecalColor.h"
#include "Materials/MaterialExpressionSphereMask.h"
#include "Materials/MaterialExpressionSubtract.h"
#include "Materials/MaterialInterface.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "UObject/UnrealType.h"

namespace RequiemLockOnPIETest
{
constexpr TCHAR MapPackagePath[] =
	TEXT("/Game/ProjectRequiem/World/Maps/Dev/L_Dev_Foundation");
constexpr TCHAR MappingContextPath[] =
	TEXT("/Game/ProjectRequiem/Core/Input/IMC_Exploration.IMC_Exploration");
constexpr TCHAR LockOnActionPath[] =
	TEXT("/Game/ProjectRequiem/Core/Input/Actions/IA_LockOn.IA_LockOn");
constexpr TCHAR MoveActionPath[] =
	TEXT("/Game/ProjectRequiem/Core/Input/Actions/IA_Move.IA_Move");
constexpr TCHAR RollActionPath[] =
	TEXT("/Game/ProjectRequiem/Core/Input/Actions/IA_Roll.IA_Roll");
constexpr TCHAR PrimaryAttackActionPath[] =
	TEXT("/Game/ProjectRequiem/Core/Input/Actions/IA_PrimaryAttack.IA_PrimaryAttack");
constexpr TCHAR CharacterClassPath[] =
	TEXT("/Game/ProjectRequiem/Characters/Player/Blueprints/BP_CH_Player.BP_CH_Player_C");
constexpr TCHAR DummyClassPath[] =
	TEXT("/Game/ProjectRequiem/Combat/Blueprints/Targets/"
		 "BP_PR_CombatDummy.BP_PR_CombatDummy_C");
constexpr TCHAR GroundRingMaterialPath[] =
	TEXT("/Game/ProjectRequiem/UI/HUD/Temporary/"
		 "M_LockOnGroundRing.M_LockOnGroundRing");

constexpr float FacingDotThreshold = 0.95f;
constexpr float CameraTargetDotThreshold = 0.90f;
constexpr float CameraCharacterDotThreshold = 0.70f;
constexpr float MinimumFollowYawDegrees = 8.0f;
constexpr float ReleasedCameraYawToleranceDegrees = 3.0f;
constexpr float ExpectedLockedCameraPitch = -18.0f;
constexpr float LockedCameraPitchToleranceDegrees = 1.0f;
constexpr float IndicatorPlanarTolerance = 2.0f;
constexpr float IndicatorGroundOffsetTolerance = 2.0f;
constexpr float MaximumReasonableIndicatorGroundOffset = 10.0f;
constexpr float IndicatorProjectionDotThreshold = 0.99f;
constexpr double StableObservationSeconds = 0.15;
constexpr double InputPulseSeconds = 0.05;
const FName TemporaryCandidateTag(TEXT("ProjectRequiem.LockOnPIETemporaryCandidate"));

struct FRunState
{
	bool bAborted = false;
};

struct FRuntimeRefs
{
	UWorld* World = nullptr;
	APlayerController* PlayerController = nullptr;
	ARequiemCharacter* Character = nullptr;
	ARequiemCombatDummy* Dummy = nullptr;
	UCharacterMovementComponent* Movement = nullptr;
	URequiemCombatComponent* Combat = nullptr;
	URequiemDodgeComponent* Dodge = nullptr;
	URequiemHealthComponent* Health = nullptr;
	URequiemLockOnComponent* LockOn = nullptr;
	int32 DummyCount = 0;

	bool IsValid() const
	{
		return World
			&& PlayerController
			&& Character
			&& Dummy
			&& Movement
			&& Combat
			&& Dodge
			&& Health
			&& LockOn
			&& DummyCount == 1;
	}
};

UWorld* FindPIEWorld()
{
	if (!GEngine)
	{
		return nullptr;
	}

	for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
	{
		if (WorldContext.WorldType == EWorldType::PIE && WorldContext.World())
		{
			return WorldContext.World();
		}
	}
	return nullptr;
}

FRuntimeRefs FindRuntimeRefs()
{
	FRuntimeRefs Refs;
	Refs.World = FindPIEWorld();
	Refs.PlayerController = Refs.World ? Refs.World->GetFirstPlayerController() : nullptr;
	Refs.Character = Refs.PlayerController
		? Cast<ARequiemCharacter>(Refs.PlayerController->GetPawn())
		: nullptr;
	Refs.Movement = Refs.Character ? Refs.Character->GetCharacterMovement() : nullptr;
	Refs.Combat = Refs.Character ? Refs.Character->GetCombatComponent() : nullptr;
	Refs.Dodge = Refs.Character ? Refs.Character->GetDodgeComponent() : nullptr;
	Refs.Health = Refs.Character ? Refs.Character->GetHealthComponent() : nullptr;
	Refs.LockOn = Refs.Character ? Refs.Character->GetLockOnComponent() : nullptr;
	if (Refs.World)
	{
		for (TActorIterator<ARequiemCombatDummy> Iterator(Refs.World); Iterator; ++Iterator)
		{
			if (Iterator->ActorHasTag(TemporaryCandidateTag))
			{
				continue;
			}
			++Refs.DummyCount;
			if (!Refs.Dummy)
			{
				Refs.Dummy = *Iterator;
			}
		}
	}
	return Refs;
}

UEnhancedInputLocalPlayerSubsystem* FindEnhancedInputSubsystem()
{
	const FRuntimeRefs Refs = FindRuntimeRefs();
	ULocalPlayer* LocalPlayer = Refs.PlayerController
		? Refs.PlayerController->GetLocalPlayer()
		: nullptr;
	return LocalPlayer
		? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>()
		: nullptr;
}

bool SetContinuousAction(const TCHAR* ActionPath, const FInputActionValue& Value)
{
	UEnhancedInputLocalPlayerSubsystem* InputSubsystem = FindEnhancedInputSubsystem();
	const UInputAction* Action = LoadObject<UInputAction>(nullptr, ActionPath);
	if (!InputSubsystem || !Action)
	{
		return false;
	}

	if (InputSubsystem->HasContinuousInputInjectionForAction(Action))
	{
		InputSubsystem->UpdateValueOfContinuousInputInjectionForAction(Action, Value);
	}
	else
	{
		InputSubsystem->StartContinuousInputInjectionForAction(Action, Value, {}, {});
	}
	return true;
}

void StopContinuousAction(const TCHAR* ActionPath)
{
	UEnhancedInputLocalPlayerSubsystem* InputSubsystem = FindEnhancedInputSubsystem();
	const UInputAction* Action = LoadObject<UInputAction>(nullptr, ActionPath);
	if (InputSubsystem
		&& Action
		&& InputSubsystem->HasContinuousInputInjectionForAction(Action))
	{
		InputSubsystem->StopContinuousInputInjectionForAction(Action);
	}
}

void ReleaseAllTestInput()
{
	StopContinuousAction(LockOnActionPath);
	StopContinuousAction(MoveActionPath);
	StopContinuousAction(RollActionPath);
	StopContinuousAction(PrimaryAttackActionPath);
}

struct FExactMappingAudit
{
	int32 Count = 0;
	bool bAllMappingsAreClean = true;
};

FExactMappingAudit AuditExactMappings(
	const UInputMappingContext* MappingContext,
	const UInputAction* Action,
	const FKey& Key)
{
	FExactMappingAudit Result;
	if (!MappingContext || !Action)
	{
		return Result;
	}

	for (const FEnhancedActionKeyMapping& Mapping : MappingContext->GetMappings())
	{
		if (Mapping.Action == Action && Mapping.Key == Key)
		{
			++Result.Count;
			Result.bAllMappingsAreClean &=
				Mapping.Modifiers.IsEmpty() && Mapping.Triggers.IsEmpty();
		}
	}
	return Result;
}

int32 CountStartedBindings(
	const UEnhancedInputComponent* InputComponent,
	const UInputAction* Action)
{
	if (!InputComponent || !Action)
	{
		return 0;
	}

	int32 BindingCount = 0;
	for (const TUniquePtr<FEnhancedInputActionEventBinding>& Binding
		: InputComponent->GetActionEventBindings())
	{
		if (Binding
			&& Binding->GetAction() == Action
			&& Binding->GetTriggerEvent() == ETriggerEvent::Started)
		{
			++BindingCount;
		}
	}
	return BindingCount;
}

UObject* GetObjectPropertyValue(
	const UObject* Container,
	const UClass* OwnerClass,
	const TCHAR* PropertyName)
{
	const FObjectPropertyBase* Property =
		FindFProperty<FObjectPropertyBase>(OwnerClass, PropertyName);
	return Container && Property
		? Property->GetObjectPropertyValue_InContainer(Container)
		: nullptr;
}

FVector GetLockOnFocusLocation(AActor* Target)
{
	return Target
		&& Target->GetClass()->ImplementsInterface(
			URequiemLockOnTargetInterface::StaticClass())
		? IRequiemLockOnTargetInterface::Execute_GetLockOnFocusLocation(Target)
		: FVector::ZeroVector;
}

bool IsIndicatorVisible(const FRuntimeRefs& Refs)
{
	const UDecalComponent* Indicator = Refs.Character
		? Refs.Character->GetLockOnGroundIndicator()
		: nullptr;
	return Indicator && Indicator->IsVisible() && !Indicator->bHiddenInGame;
}

bool ReadIndicatorPlacement(
	const FRuntimeRefs& Refs,
	AActor* Target,
	FVector& OutIndicatorLocation,
	float& OutGroundOffsetAboveBase)
{
	const UDecalComponent* Indicator = Refs.Character
		? Refs.Character->GetLockOnGroundIndicator()
		: nullptr;
	if (!Indicator || !Target || !IsIndicatorVisible(Refs))
	{
		return false;
	}

	FVector BoundsOrigin = FVector::ZeroVector;
	FVector BoundsExtent = FVector::ZeroVector;
	Target->GetActorBounds(false, BoundsOrigin, BoundsExtent);
	OutIndicatorLocation = Indicator->GetComponentLocation();
	OutGroundOffsetAboveBase =
		OutIndicatorLocation.Z - (BoundsOrigin.Z - BoundsExtent.Z);
	const FVector ProjectionDirection = Indicator->GetForwardVector().GetSafeNormal();
	return !OutIndicatorLocation.ContainsNaN()
		&& FVector::DistSquared2D(OutIndicatorLocation, BoundsOrigin)
			<= FMath::Square(IndicatorPlanarTolerance)
		&& OutGroundOffsetAboveBase >= -IndicatorGroundOffsetTolerance
		&& OutGroundOffsetAboveBase <= MaximumReasonableIndicatorGroundOffset
		&& FVector::DotProduct(ProjectionDirection, -FVector::UpVector)
			>= IndicatorProjectionDotThreshold
		&& Indicator->DecalSize.X > UE_KINDA_SMALL_NUMBER
		&& Indicator->DecalSize.X < Indicator->DecalSize.Y
		&& FMath::IsNearlyEqual(
			Indicator->DecalSize.Y,
			Indicator->DecalSize.Z,
			UE_KINDA_SMALL_NUMBER);
}

bool IsIndicatorStillPlacedAtTarget(
	const FRuntimeRefs& Refs,
	AActor* Target,
	const float ExpectedGroundOffsetAboveBase,
	FVector& OutIndicatorLocation)
{
	float GroundOffsetAboveBase = 0.0f;
	return ReadIndicatorPlacement(
			Refs,
			Target,
			OutIndicatorLocation,
			GroundOffsetAboveBase)
		&& FMath::IsNearlyEqual(
			GroundOffsetAboveBase,
			ExpectedGroundOffsetAboveBase,
			IndicatorGroundOffsetTolerance);
}

float GetPlanarFacingDot(const FRuntimeRefs& Refs)
{
	const FVector ToTarget = (
		GetLockOnFocusLocation(Refs.Dummy) - Refs.Character->GetActorLocation())
		.GetSafeNormal2D();
	return FVector::DotProduct(
		Refs.Character->GetActorForwardVector().GetSafeNormal2D(),
		ToTarget);
}

bool ReadCameraGeometry(
	const FRuntimeRefs& Refs,
	float& OutTargetDot,
	float& OutCharacterDot,
	float& OutViewYaw,
	float& OutViewPitch)
{
	if (!Refs.PlayerController || !Refs.Character || !Refs.Dummy)
	{
		return false;
	}

	FVector ViewLocation = FVector::ZeroVector;
	FRotator ViewRotation = FRotator::ZeroRotator;
	Refs.PlayerController->GetPlayerViewPoint(ViewLocation, ViewRotation);
	const FVector ViewDirection = ViewRotation.Vector().GetSafeNormal();
	const FVector PlanarViewDirection = ViewDirection.GetSafeNormal2D();
	const FVector ToTarget = (
		GetLockOnFocusLocation(Refs.Dummy) - ViewLocation).GetSafeNormal2D();
	const FVector CharacterFocus =
		Refs.Character->GetActorLocation() + FVector(0.0f, 0.0f, 80.0f);
	const FVector ToCharacter = (CharacterFocus - ViewLocation).GetSafeNormal();
	if (PlanarViewDirection.IsNearlyZero()
		|| ToTarget.IsNearlyZero()
		|| ToCharacter.IsNearlyZero())
	{
		return false;
	}

	OutTargetDot = FVector::DotProduct(PlanarViewDirection, ToTarget);
	OutCharacterDot = FVector::DotProduct(ViewDirection, ToCharacter);
	OutViewYaw = ViewRotation.Yaw;
	OutViewPitch = FRotator::NormalizeAxis(ViewRotation.Pitch);
	return true;
}

float GetAcquisitionFixtureDistance(const URequiemLockOnComponent* LockOn)
{
	const float Range = LockOn ? LockOn->GetAcquisitionRange() : 0.0f;
	return FMath::Min(300.0f, Range * 0.4f);
}

void PlaceCharacterAndDummy(
	const FRuntimeRefs& Refs,
	const FVector2D& DummyPlanarOffset)
{
	Refs.Movement->SetMovementMode(MOVE_Walking);
	Refs.Movement->StopMovementImmediately();
	Refs.Character->ConsumeMovementInputVector();
	Refs.Character->SetActorLocationAndRotation(
		FVector(0.0f, 0.0f, Refs.Character->GetActorLocation().Z),
		FRotator::ZeroRotator,
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
	Refs.PlayerController->SetControlRotation(
		FRotator(ExpectedLockedCameraPitch, 0.0f, 0.0f));
	const FVector DummyLocation(
		DummyPlanarOffset.X,
		DummyPlanarOffset.Y,
		112.0f);
	const FRotator DummyRotation(
		0.0f,
		(Refs.Character->GetActorLocation() - DummyLocation).Rotation().Yaw,
		0.0f);
	Refs.Dummy->SetActorLocationAndRotation(
		DummyLocation,
		DummyRotation,
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
}

FVector2D MakePlanarOffset(const float Distance, const float YawDegrees)
{
	const FVector Direction =
		FRotationMatrix(FRotator(0.0f, YawDegrees, 0.0f)).GetUnitAxis(EAxis::X);
	return FVector2D(Direction.X, Direction.Y) * Distance;
}

ARequiemCombatDummy* SpawnTemporaryCandidate(
	const FRuntimeRefs& Refs,
	const FVector2D& PlanarOffset,
	const bool bValidTarget)
{
	if (!Refs.World || !Refs.Character)
	{
		return nullptr;
	}

	const FVector CharacterLocation = Refs.Character->GetActorLocation();
	const FVector CandidateLocation(
		CharacterLocation.X + PlanarOffset.X,
		CharacterLocation.Y + PlanarOffset.Y,
		112.0f);
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ARequiemCombatDummy* Candidate = Refs.World->SpawnActor<ARequiemCombatDummy>(
		ARequiemCombatDummy::StaticClass(),
		CandidateLocation,
		(Refs.Character->GetActorLocation() - CandidateLocation).Rotation(),
		SpawnParameters);
	if (!Candidate)
	{
		return nullptr;
	}

	Candidate->Tags.AddUnique(TemporaryCandidateTag);
	Candidate->ResetForTesting();
	Candidate->SetCanBeDamaged(bValidTarget);
	Candidate->SetActorEnableCollision(bValidTarget);
	return Candidate;
}

void DestroyTemporaryCandidates(
	UWorld* World,
	URequiemLockOnComponent* LockOn = nullptr)
{
	if (!World)
	{
		return;
	}

	AActor* CurrentTarget = LockOn ? LockOn->GetLockOnTarget() : nullptr;
	if (CurrentTarget && CurrentTarget->ActorHasTag(TemporaryCandidateTag))
	{
		LockOn->ClearLockOn();
	}

	TArray<TWeakObjectPtr<ARequiemCombatDummy>> Candidates;
	for (TActorIterator<ARequiemCombatDummy> Iterator(World); Iterator; ++Iterator)
	{
		if (Iterator->ActorHasTag(TemporaryCandidateTag))
		{
			Candidates.Add(*Iterator);
		}
	}
	for (const TWeakObjectPtr<ARequiemCombatDummy>& Candidate : Candidates)
	{
		if (Candidate.IsValid())
		{
			Candidate->Destroy();
		}
	}
}

class FTimedCommand : public IAutomationLatentCommand
{
public:
	FTimedCommand(
		FAutomationTestBase* InTest,
		TSharedRef<FRunState> InRunState,
		const double InTimeoutSeconds)
		: Test(InTest)
		, RunState(MoveTemp(InRunState))
		, TimeoutSeconds(InTimeoutSeconds)
	{
	}

protected:
	bool ShouldSkip() const { return RunState->bAborted; }

	double GetElapsedSeconds()
	{
		const double Now = FPlatformTime::Seconds();
		if (StartTimeSeconds < 0.0)
		{
			StartTimeSeconds = Now;
		}
		return Now - StartTimeSeconds;
	}

	bool AbortWithError(const FString& Message)
	{
		Test->AddError(Message);
		RunState->bAborted = true;
		ReleaseAllTestInput();
		return true;
	}

	bool AbortIfTimedOut(const double ElapsedSeconds, const FString& Details)
	{
		if (ElapsedSeconds < TimeoutSeconds)
		{
			return false;
		}
		return AbortWithError(FString::Printf(
			TEXT("Lock-on PIE stage timed out after %.1fs: %s"),
			TimeoutSeconds,
			*Details));
	}

	FAutomationTestBase* Test;
	TSharedRef<FRunState> RunState;

private:
	double TimeoutSeconds;
	double StartTimeSeconds = -1.0;
};

class FWaitForPIEReadyCommand final : public FTimedCommand
{
public:
	FWaitForPIEReadyCommand(
		FAutomationTestBase* InTest,
		TSharedRef<FRunState> InRunState)
		: FTimedCommand(InTest, MoveTemp(InRunState), 8.0)
	{
	}

	virtual bool Update() override
	{
		if (ShouldSkip())
		{
			return true;
		}

		const double ElapsedSeconds = GetElapsedSeconds();
		const FRuntimeRefs Refs = FindRuntimeRefs();
		if (Refs.IsValid())
		{
			if (!Refs.World->GetMapName().Contains(TEXT("L_Dev_Foundation")))
			{
				return AbortWithError(FString::Printf(
					TEXT("PIE opened the wrong map: %s"),
					*Refs.World->GetMapName()));
			}

			if (StableWorld.Get() != Refs.World
				|| StableCharacter.Get() != Refs.Character
				|| StableDummy.Get() != Refs.Dummy)
			{
				StableWorld = Refs.World;
				StableCharacter = Refs.Character;
				StableDummy = Refs.Dummy;
				StableSinceSeconds = ElapsedSeconds;
				return false;
			}
			if (ElapsedSeconds - StableSinceSeconds < 0.5)
			{
				return false;
			}

			const UEnhancedInputComponent* InputComponent =
				Refs.Character->FindComponentByClass<UEnhancedInputComponent>();
			const UInputAction* LockOnAction =
				LoadObject<UInputAction>(nullptr, LockOnActionPath);
			const int32 BindingCount =
				CountStartedBindings(InputComponent, LockOnAction);
			if (InputComponent && LockOnAction && BindingCount != 1)
			{
				return AbortWithError(FString::Printf(
					TEXT("Runtime IA_LockOn Started binding count is %d; expected exactly one."),
					BindingCount));
			}

			const bool bReady =
				InputComponent
				&& LockOnAction
				&& BindingCount == 1
				&& !Refs.LockOn->IsLockOnActive()
				&& !Refs.LockOn->GetLockOnTarget()
				&& !IsIndicatorVisible(Refs)
				&& !Refs.Dummy->IsDefeated();
			if (bReady)
			{
				Test->AddInfo(TEXT(
					"PIE started with one valid dummy and one runtime middle-mouse lock-on binding."));
				return true;
			}
		}

		return AbortIfTimedOut(
			ElapsedSeconds,
			FString::Printf(
				TEXT("waiting for the lock-on-ready player and exactly one dummy (count=%d)"),
				Refs.DummyCount));
	}

private:
	TWeakObjectPtr<UWorld> StableWorld;
	TWeakObjectPtr<ARequiemCharacter> StableCharacter;
	TWeakObjectPtr<ARequiemCombatDummy> StableDummy;
	double StableSinceSeconds = -1.0;
};

enum class EFlowStage : uint8
{
	PrepareNearestSelection,
	PulseNearestSelection,
	VerifyNearestSelection,
	CleanupNearestSelection,
	PrepareBehindCone,
	PulseBehindCone,
	VerifyBehindConeRejected,
	PlaceInsideCone,
	PulseAcquire,
	VerifyAcquire,
	VerifyInitialOrientationAndCamera,
	VerifyCameraFollow,
	PulseManualUnlock,
	VerifyManualUnlock,
	VerifyReleasedCamera,
	PrepareAttackReacquire,
	PulseAttackReacquire,
	VerifyAttackReacquire,
	PulsePrimaryAttack,
	VerifyPrimaryAttack,
	PrepareDodge,
	EstablishDodgeInput,
	PulseDodge,
	VerifyDodge,
	DefeatTarget,
	VerifyDefeatUnlock,
	PrepareRangeReacquire,
	PulseRangeReacquire,
	VerifyRangeReacquire,
	VerifyRangeUnlock,
	Complete
};

class FValidateLockOnFlowCommand final : public FTimedCommand
{
public:
	FValidateLockOnFlowCommand(
		FAutomationTestBase* InTest,
		TSharedRef<FRunState> InRunState)
		: FTimedCommand(InTest, MoveTemp(InRunState), 45.0)
	{
	}

	virtual bool Update() override
	{
		if (ShouldSkip())
		{
			ReleaseAllTestInput();
			return true;
		}

		const double TotalElapsedSeconds = GetElapsedSeconds();
		const FRuntimeRefs Refs = FindRuntimeRefs();
		if (!Refs.IsValid())
		{
			return AbortIfTimedOut(TotalElapsedSeconds, TEXT("recovering runtime references"));
		}

		if (PulsedActionPath)
		{
			if (FPlatformTime::Seconds() - PulseStartTimeSeconds < InputPulseSeconds)
			{
				return false;
			}
			StopContinuousAction(PulsedActionPath);
			PulsedActionPath = nullptr;
			PulseStartTimeSeconds = -1.0;
			return false;
		}

		const double StageElapsedSeconds = GetStageElapsedSeconds();
		switch (Stage)
		{
		case EFlowStage::PrepareNearestSelection:
		{
			ReleaseAllTestInput();
			Refs.LockOn->ClearLockOn();
			DestroyTemporaryCandidates(Refs.World, Refs.LockOn);
			Refs.Health->ResetForTesting();
			Refs.Dummy->ResetForTesting();
			AcquisitionDistance = GetAcquisitionFixtureDistance(Refs.LockOn);
			PlaceCharacterAndDummy(
				Refs,
				MakePlanarOffset(AcquisitionDistance, -25.0f));

			NearestCandidate = SpawnTemporaryCandidate(
				Refs,
				MakePlanarOffset(AcquisitionDistance * 0.6f, 25.0f),
				true);
			InvalidCandidate = SpawnTemporaryCandidate(
				Refs,
				MakePlanarOffset(AcquisitionDistance * 0.33f, 0.0f),
				false);
			if (!NearestCandidate.IsValid() || !InvalidCandidate.IsValid())
			{
				return AbortWithError(TEXT(
					"Could not spawn the temporary lock-on selection candidates."));
			}
			if (!IRequiemLockOnTargetInterface::Execute_IsValidLockOnTarget(
					NearestCandidate.Get())
				|| IRequiemLockOnTargetInterface::Execute_IsValidLockOnTarget(
					InvalidCandidate.Get()))
			{
				return AbortWithError(TEXT(
					"Temporary lock-on candidate validity fixture was configured incorrectly."));
			}
			AdvanceTo(EFlowStage::PulseNearestSelection);
			return false;
		}

		case EFlowStage::PulseNearestSelection:
			return PulseAndAdvance(
				LockOnActionPath,
				EFlowStage::VerifyNearestSelection);

		case EFlowStage::VerifyNearestSelection:
		{
			AActor* SelectedTarget = Refs.LockOn->GetLockOnTarget();
			if (SelectedTarget && SelectedTarget != NearestCandidate.Get())
			{
				return AbortWithError(FString::Printf(
					TEXT("Lock-on selected %s instead of the nearest valid in-cone candidate."),
					*SelectedTarget->GetName()));
			}
			if (SelectedTarget == NearestCandidate.Get()
				&& Refs.LockOn->IsLockOnActive()
				&& IsIndicatorVisible(Refs)
				&& Refs.Combat->GetCombatState() == ERequiemCombatState::CombatUnarmed)
			{
				Test->AddInfo(TEXT(
					"Lock-on ignored a closer invalid candidate and selected the nearest valid target in the forward cone."));
				AdvanceTo(EFlowStage::CleanupNearestSelection);
				return false;
			}
			break;
		}

		case EFlowStage::CleanupNearestSelection:
			Refs.LockOn->ClearLockOn();
			DestroyTemporaryCandidates(Refs.World, Refs.LockOn);
			NearestCandidate.Reset();
			InvalidCandidate.Reset();
			AdvanceTo(EFlowStage::PrepareBehindCone);
			return false;

		case EFlowStage::PrepareBehindCone:
			ReleaseAllTestInput();
			Refs.LockOn->ClearLockOn();
			Refs.Health->ResetForTesting();
			Refs.Dummy->ResetForTesting();
			AcquisitionDistance = GetAcquisitionFixtureDistance(Refs.LockOn);
			PlaceCharacterAndDummy(Refs, FVector2D(-AcquisitionDistance, 0.0f));
			AdvanceTo(EFlowStage::PulseBehindCone);
			return false;

		case EFlowStage::PulseBehindCone:
			return PulseAndAdvance(LockOnActionPath, EFlowStage::VerifyBehindConeRejected);

		case EFlowStage::VerifyBehindConeRejected:
			if (Refs.LockOn->IsLockOnActive()
				|| Refs.LockOn->GetLockOnTarget()
				|| IsIndicatorVisible(Refs))
			{
				return AbortWithError(TEXT("Middle mouse acquired the dummy behind the player cone."));
			}
			if (StageElapsedSeconds >= StableObservationSeconds)
			{
				AdvanceTo(EFlowStage::PlaceInsideCone);
				return false;
			}
			break;

		case EFlowStage::PlaceInsideCone:
		{
			const float InsideAngleDegrees = FMath::Min(
				20.0f,
				Refs.LockOn->GetAcquisitionConeHalfAngleDegrees() * 0.5f);
			const FVector InsideDirection =
				FRotationMatrix(FRotator(0.0f, InsideAngleDegrees, 0.0f))
				.GetUnitAxis(EAxis::X);
			PlaceCharacterAndDummy(
				Refs,
				FVector2D(InsideDirection.X, InsideDirection.Y) * AcquisitionDistance);
			AdvanceTo(EFlowStage::PulseAcquire);
			return false;
		}

		case EFlowStage::PulseAcquire:
			return PulseAndAdvance(LockOnActionPath, EFlowStage::VerifyAcquire);

		case EFlowStage::VerifyAcquire:
			if (HasExpectedActiveLock(Refs)
				&& Refs.Combat->GetCombatState() == ERequiemCombatState::CombatUnarmed)
			{
				AdvanceTo(EFlowStage::VerifyInitialOrientationAndCamera);
				return false;
			}
			break;

		case EFlowStage::VerifyInitialOrientationAndCamera:
		{
			float TargetDot = -1.0f;
			float CharacterDot = -1.0f;
			float ViewYaw = 0.0f;
			float ViewPitch = 0.0f;
			FVector IndicatorLocation = FVector::ZeroVector;
			float MeasuredIndicatorGroundOffset = 0.0f;
			if (HasExpectedActiveLock(Refs)
				&& GetPlanarFacingDot(Refs) >= FacingDotThreshold
				&& ReadCameraGeometry(
					Refs,
					TargetDot,
					CharacterDot,
					ViewYaw,
					ViewPitch)
				&& TargetDot >= CameraTargetDotThreshold
				&& CharacterDot >= CameraCharacterDotThreshold
				&& FMath::Abs(FMath::FindDeltaAngleDegrees(
					ExpectedLockedCameraPitch,
					ViewPitch)) <= LockedCameraPitchToleranceDegrees
				&& ReadIndicatorPlacement(
					Refs,
					Refs.Dummy,
					IndicatorLocation,
					MeasuredIndicatorGroundOffset))
			{
				InitialTrackingViewYaw = ViewYaw;
				InitialTrackingViewPitch = ViewPitch;
				InitialIndicatorLocation = IndicatorLocation;
				IndicatorGroundOffsetAboveBase = MeasuredIndicatorGroundOffset;
				const float FollowDistance = FMath::Min(
					AcquisitionDistance,
					Refs.LockOn->GetBreakRange() * 0.4f);
				const FVector FollowDirection =
					FRotationMatrix(FRotator(0.0f, -35.0f, 0.0f))
					.GetUnitAxis(EAxis::X);
				const FVector CharacterLocation = Refs.Character->GetActorLocation();
				Refs.Dummy->SetActorLocation(
					FVector(
						CharacterLocation.X + FollowDirection.X * FollowDistance,
						CharacterLocation.Y + FollowDirection.Y * FollowDistance,
						260.0f),
					false,
					nullptr,
					ETeleportType::TeleportPhysics);
				AdvanceTo(EFlowStage::VerifyCameraFollow);
				return false;
			}
			break;
		}

		case EFlowStage::VerifyCameraFollow:
		{
			float TargetDot = -1.0f;
			float CharacterDot = -1.0f;
			float ViewYaw = 0.0f;
			float ViewPitch = 0.0f;
			FVector IndicatorLocation = FVector::ZeroVector;
			const bool bFollowing =
				HasExpectedActiveLock(Refs)
				&& GetPlanarFacingDot(Refs) >= FacingDotThreshold
				&& ReadCameraGeometry(
					Refs,
					TargetDot,
					CharacterDot,
					ViewYaw,
					ViewPitch)
				&& TargetDot >= CameraTargetDotThreshold
				&& CharacterDot >= CameraCharacterDotThreshold
				&& FMath::Abs(FMath::FindDeltaAngleDegrees(
					InitialTrackingViewPitch,
					ViewPitch)) <= LockedCameraPitchToleranceDegrees
				&& FMath::Abs(FMath::FindDeltaAngleDegrees(
					InitialTrackingViewYaw,
					ViewYaw)) >= MinimumFollowYawDegrees
				&& IsIndicatorStillPlacedAtTarget(
					Refs,
					Refs.Dummy,
					IndicatorGroundOffsetAboveBase,
					IndicatorLocation)
				&& FVector::Dist2D(InitialIndicatorLocation, IndicatorLocation)
					>= AcquisitionDistance * 0.25f;
			if (WaitForStableCondition(bFollowing))
			{
				Test->AddInfo(TEXT(
					"Lock-on selected the in-cone dummy, entered combat, kept a natural camera pitch, and tracked a moved target in yaw."));
				AdvanceTo(EFlowStage::PulseManualUnlock);
				return false;
			}
			break;
		}

		case EFlowStage::PulseManualUnlock:
			return PulseAndAdvance(LockOnActionPath, EFlowStage::VerifyManualUnlock);

		case EFlowStage::VerifyManualUnlock:
			if (!Refs.LockOn->IsLockOnActive()
				&& !Refs.LockOn->GetLockOnTarget()
				&& !IsIndicatorVisible(Refs))
			{
				float TargetDot = 0.0f;
				float CharacterDot = 0.0f;
				float ViewPitch = 0.0f;
				if (ReadCameraGeometry(
					Refs,
					TargetDot,
					CharacterDot,
					ReleasedCameraViewYaw,
					ViewPitch))
				{
					const FVector CharacterLocation = Refs.Character->GetActorLocation();
					Refs.Dummy->SetActorLocation(
						FVector(
							CharacterLocation.X,
							CharacterLocation.Y + AcquisitionDistance,
							112.0f),
						false,
						nullptr,
						ETeleportType::TeleportPhysics);
					AdvanceTo(EFlowStage::VerifyReleasedCamera);
					return false;
				}
			}
			break;

		case EFlowStage::VerifyReleasedCamera:
		{
			float TargetDot = 0.0f;
			float CharacterDot = 0.0f;
			float ViewYaw = 0.0f;
			float ViewPitch = 0.0f;
			if (StageElapsedSeconds >= 0.25
				&& ReadCameraGeometry(
					Refs,
					TargetDot,
					CharacterDot,
					ViewYaw,
					ViewPitch))
			{
				if (FMath::Abs(FMath::FindDeltaAngleDegrees(
					ReleasedCameraViewYaw,
					ViewYaw)) > ReleasedCameraYawToleranceDegrees)
				{
					return AbortWithError(TEXT(
						"Camera continued following the dummy after middle-mouse unlock."));
				}
				Test->AddInfo(TEXT(
					"A second middle-mouse press cleared the target and stopped camera tracking."));
				AdvanceTo(EFlowStage::PrepareAttackReacquire);
				return false;
			}
			break;
		}

		case EFlowStage::PrepareAttackReacquire:
			Refs.LockOn->ClearLockOn();
			Refs.Health->ResetForTesting();
			Refs.Dummy->ResetForTesting();
			PlaceCharacterAndDummy(Refs, FVector2D(145.0f, 0.0f));
			AdvanceTo(EFlowStage::PulseAttackReacquire);
			return false;

		case EFlowStage::PulseAttackReacquire:
			return PulseAndAdvance(LockOnActionPath, EFlowStage::VerifyAttackReacquire);

		case EFlowStage::VerifyAttackReacquire:
			if (HasExpectedActiveLock(Refs)
				&& GetPlanarFacingDot(Refs) >= FacingDotThreshold)
			{
				DummyHealthBeforeAttack = Refs.Dummy->GetCurrentHealth();
				ExpectedAttackDamage = Refs.Combat->GetUnarmedHitDamage();
				DummyDamageSerialBeforeAttack = Refs.Dummy->GetDamageSerial();
				HitConfirmSerialBeforeAttack =
					Refs.Combat->GetUnarmedHitConfirmSerial();
				AttackRequestSerialBefore = Refs.Combat->GetAttackRequestSerial();
				AdvanceTo(EFlowStage::PulsePrimaryAttack);
				return false;
			}
			break;

		case EFlowStage::PulsePrimaryAttack:
			return PulseAndAdvance(
				PrimaryAttackActionPath,
				EFlowStage::VerifyPrimaryAttack);

		case EFlowStage::VerifyPrimaryAttack:
		{
			if (!HasExpectedActiveLock(Refs))
			{
				return AbortWithError(TEXT("Primary attack cleared the active lock-on."));
			}
			if (Refs.Dummy->GetDamageSerial() > DummyDamageSerialBeforeAttack + 1
				|| Refs.Combat->GetUnarmedHitConfirmSerial()
					> HitConfirmSerialBeforeAttack + 1)
			{
				return AbortWithError(TEXT("One locked-on strike damaged the dummy more than once."));
			}

			const bool bAppliedOneHit =
				Refs.Combat->GetAttackRequestSerial() == AttackRequestSerialBefore + 1
				&& Refs.Dummy->GetDamageSerial() == DummyDamageSerialBeforeAttack + 1
				&& Refs.Combat->GetUnarmedHitConfirmSerial()
					== HitConfirmSerialBeforeAttack + 1
				&& FMath::IsNearlyEqual(
					Refs.Dummy->GetCurrentHealth(),
					DummyHealthBeforeAttack - ExpectedAttackDamage,
					0.01f);
			if (bAppliedOneHit
				&& !Refs.Combat->IsUnarmedAttackActive()
				&& !Refs.Combat->HasPendingInitialUnarmedAttackRequest())
			{
				Test->AddInfo(TEXT(
					"A real primary attack hit exactly once without clearing lock-on."));
				AdvanceTo(EFlowStage::PrepareDodge);
				return false;
			}
			break;
		}

		case EFlowStage::PrepareDodge:
		{
			Refs.Movement->StopMovementImmediately();
			const float SafeDistance = FMath::Min(
				250.0f,
				Refs.LockOn->GetBreakRange() * 0.2f);
			PlaceCharacterAndDummy(Refs, FVector2D(SafeDistance, 0.0f));
			AdvanceTo(EFlowStage::EstablishDodgeInput);
			return false;
		}

		case EFlowStage::EstablishDodgeInput:
			if (!SetContinuousAction(
				MoveActionPath,
				FInputActionValue(FVector2D(0.55f, 0.0f))))
			{
				break;
			}
			if (Refs.Character->HasCurrentMovementInput()
				&& !Refs.Movement->GetCurrentAcceleration().IsNearlyZero(1.0f))
			{
				DodgeRequestSerialBefore = Refs.Dodge->GetDodgeRequestSerial();
				AdvanceTo(EFlowStage::PulseDodge);
				return false;
			}
			break;

		case EFlowStage::PulseDodge:
			return PulseAndAdvance(RollActionPath, EFlowStage::VerifyDodge);

		case EFlowStage::VerifyDodge:
			if (!HasExpectedActiveLock(Refs))
			{
				return AbortWithError(TEXT("Dodge cleared the active lock-on."));
			}
			if (Refs.Dodge->IsDodgeActive())
			{
				if (!bSawDodge)
				{
					bSawDodge = true;
					StopContinuousAction(MoveActionPath);
					if (Refs.Dodge->GetDodgeRequestSerial()
						!= DodgeRequestSerialBefore + 1)
					{
						return AbortWithError(TEXT(
							"Locked-on Shift dodge did not advance its accepted serial exactly once."));
					}
				}

				const FVector CapturedDirection =
					Refs.Dodge->GetDodgeDirection().GetSafeNormal2D();
				if (FVector::DotProduct(
					Refs.Character->GetActorForwardVector().GetSafeNormal2D(),
					CapturedDirection) >= FacingDotThreshold)
				{
					bSawCommittedDodgeOrientation = true;
				}
				return false;
			}

			if (bSawDodge
				&& bSawCommittedDodgeOrientation
				&& Refs.Dodge->GetDodgeRequestSerial() == DodgeRequestSerialBefore + 1
				&& GetPlanarFacingDot(Refs) >= FacingDotThreshold)
			{
				float TargetDot = -1.0f;
				float CharacterDot = -1.0f;
				float ViewYaw = 0.0f;
				float ViewPitch = 0.0f;
				if (ReadCameraGeometry(
						Refs,
						TargetDot,
						CharacterDot,
						ViewYaw,
						ViewPitch)
					&& TargetDot >= CameraTargetDotThreshold)
				{
					Test->AddInfo(TEXT(
						"A real directional dodge kept lock-on, preserved its captured orientation, and reacquired target facing after recovery."));
					AdvanceTo(EFlowStage::DefeatTarget);
					return false;
				}
			}
			break;

		case EFlowStage::DefeatTarget:
		{
			const float AppliedDamage = UGameplayStatics::ApplyPointDamage(
				Refs.Dummy,
				Refs.Dummy->GetCurrentHealth(),
				Refs.Character->GetActorForwardVector(),
				FHitResult(),
				Refs.PlayerController,
				Refs.Character,
				nullptr);
			if (AppliedDamage <= UE_KINDA_SMALL_NUMBER || !Refs.Dummy->IsDefeated())
			{
				return AbortWithError(TEXT("Could not defeat the locked combat dummy."));
			}
			AdvanceTo(EFlowStage::VerifyDefeatUnlock);
			return false;
		}

		case EFlowStage::VerifyDefeatUnlock:
		{
			const bool bCleared =
				Refs.Dummy->IsDefeated()
				&& !Refs.LockOn->IsLockOnActive()
				&& !Refs.LockOn->GetLockOnTarget()
				&& !IsIndicatorVisible(Refs);
			if (WaitForStableCondition(bCleared))
			{
				Test->AddInfo(TEXT(
					"Dummy defeat invalidated the target and cleared lock-on without reacquiring it."));
				AdvanceTo(EFlowStage::PrepareRangeReacquire);
				return false;
			}
			break;
		}

		case EFlowStage::PrepareRangeReacquire:
			Refs.Dummy->ResetForTesting();
			Refs.LockOn->ClearLockOn();
			PlaceCharacterAndDummy(Refs, FVector2D(AcquisitionDistance, 0.0f));
			AdvanceTo(EFlowStage::PulseRangeReacquire);
			return false;

		case EFlowStage::PulseRangeReacquire:
			return PulseAndAdvance(LockOnActionPath, EFlowStage::VerifyRangeReacquire);

		case EFlowStage::VerifyRangeReacquire:
			if (HasExpectedActiveLock(Refs)
				&& GetPlanarFacingDot(Refs) >= FacingDotThreshold)
			{
				const float BreakRange = Refs.LockOn->GetBreakRange();
				const float FarDistance =
					BreakRange + FMath::Max(100.0f, BreakRange * 0.1f);
				const FVector CharacterLocation = Refs.Character->GetActorLocation();
				Refs.Dummy->SetActorLocation(
					FVector(
						CharacterLocation.X + FarDistance,
						CharacterLocation.Y,
						112.0f),
					false,
					nullptr,
					ETeleportType::TeleportPhysics);
				AdvanceTo(EFlowStage::VerifyRangeUnlock);
				return false;
			}
			break;

		case EFlowStage::VerifyRangeUnlock:
		{
			const bool bCleared =
				!Refs.LockOn->IsLockOnActive()
				&& !Refs.LockOn->GetLockOnTarget()
				&& !IsIndicatorVisible(Refs);
			if (WaitForStableCondition(bCleared))
			{
				Test->AddInfo(TEXT(
					"Moving the target beyond the configured break range cleared lock-on."));
				AdvanceTo(EFlowStage::Complete);
				return false;
			}
			break;
		}

		case EFlowStage::Complete:
			ReleaseAllTestInput();
			return true;
		}

		return AbortIfTimedOut(
			TotalElapsedSeconds,
			FString::Printf(
				TEXT("flow stage %d (stage elapsed %.2fs, active=%d, target=%s, distance=%.1f)"),
				static_cast<int32>(Stage),
				StageElapsedSeconds,
				Refs.LockOn->IsLockOnActive(),
				Refs.LockOn->GetLockOnTarget()
					? *Refs.LockOn->GetLockOnTarget()->GetName()
					: TEXT("None"),
				FVector::Dist2D(
					Refs.Character->GetActorLocation(),
					Refs.Dummy->GetActorLocation())));
	}

private:
	void AdvanceTo(const EFlowStage NextStage)
	{
		Stage = NextStage;
		StageStartTimeSeconds = FPlatformTime::Seconds();
		ConditionStableSinceSeconds = -1.0;
	}

	double GetStageElapsedSeconds()
	{
		const double Now = FPlatformTime::Seconds();
		if (StageStartTimeSeconds < 0.0)
		{
			StageStartTimeSeconds = Now;
		}
		return Now - StageStartTimeSeconds;
	}

	bool PulseAndAdvance(const TCHAR* ActionPath, const EFlowStage NextStage)
	{
		if (!SetContinuousAction(ActionPath, FInputActionValue(true)))
		{
			return false;
		}
		PulsedActionPath = ActionPath;
		PulseStartTimeSeconds = FPlatformTime::Seconds();
		AdvanceTo(NextStage);
		return false;
	}

	bool HasExpectedActiveLock(const FRuntimeRefs& Refs) const
	{
		return Refs.LockOn->IsLockOnActive()
			&& Refs.LockOn->GetLockOnTarget() == Refs.Dummy
			&& IsIndicatorVisible(Refs)
			&& IRequiemLockOnTargetInterface::Execute_IsValidLockOnTarget(Refs.Dummy);
	}

	bool WaitForStableCondition(const bool bCondition)
	{
		const double Now = FPlatformTime::Seconds();
		if (!bCondition)
		{
			ConditionStableSinceSeconds = -1.0;
			return false;
		}
		if (ConditionStableSinceSeconds < 0.0)
		{
			ConditionStableSinceSeconds = Now;
			return false;
		}
		return Now - ConditionStableSinceSeconds >= StableObservationSeconds;
	}

	EFlowStage Stage = EFlowStage::PrepareNearestSelection;
	const TCHAR* PulsedActionPath = nullptr;
	double PulseStartTimeSeconds = -1.0;
	double StageStartTimeSeconds = -1.0;
	double ConditionStableSinceSeconds = -1.0;
	TWeakObjectPtr<ARequiemCombatDummy> NearestCandidate;
	TWeakObjectPtr<ARequiemCombatDummy> InvalidCandidate;
	float AcquisitionDistance = 0.0f;
	float InitialTrackingViewYaw = 0.0f;
	float InitialTrackingViewPitch = 0.0f;
	float ReleasedCameraViewYaw = 0.0f;
	FVector InitialIndicatorLocation = FVector::ZeroVector;
	float IndicatorGroundOffsetAboveBase = 0.0f;
	float DummyHealthBeforeAttack = 0.0f;
	float ExpectedAttackDamage = 0.0f;
	int32 DummyDamageSerialBeforeAttack = 0;
	int32 HitConfirmSerialBeforeAttack = 0;
	int32 AttackRequestSerialBefore = 0;
	int32 DodgeRequestSerialBefore = 0;
	bool bSawDodge = false;
	bool bSawCommittedDodgeOrientation = false;
};

class FReleaseAllInputCommand final : public IAutomationLatentCommand
{
public:
	virtual bool Update() override
	{
		ReleaseAllTestInput();
		return true;
	}
};

class FCleanupTemporaryCandidatesCommand final : public IAutomationLatentCommand
{
public:
	virtual bool Update() override
	{
		const FRuntimeRefs Refs = FindRuntimeRefs();
		DestroyTemporaryCandidates(Refs.World, Refs.LockOn);
		return true;
	}
};

class FWaitForPIEEndCommand final : public IAutomationLatentCommand
{
public:
	explicit FWaitForPIEEndCommand(FAutomationTestBase* InTest)
		: Test(InTest)
	{
	}

	virtual bool Update() override
	{
		if (!FindPIEWorld())
		{
			return true;
		}

		const double Now = FPlatformTime::Seconds();
		if (StartTimeSeconds < 0.0)
		{
			StartTimeSeconds = Now;
		}
		if (Now - StartTimeSeconds >= 5.0)
		{
			Test->AddError(TEXT("PIE did not finish within 5 seconds of lock-on teardown."));
			return true;
		}
		return false;
	}

private:
	FAutomationTestBase* Test;
	double StartTimeSeconds = -1.0;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRequiemLockOnPIETest,
	"ProjectRequiem.Combat.LockOnPIE",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::NonNullRHI
		| EAutomationTestFlags::ProductFilter)

bool FRequiemLockOnPIETest::RunTest(const FString& Parameters)
{
	using namespace RequiemLockOnPIETest;

	const UInputMappingContext* MappingContext = LoadObject<UInputMappingContext>(
		nullptr,
		MappingContextPath);
	const UInputAction* LockOnAction = LoadObject<UInputAction>(nullptr, LockOnActionPath);
	TestNotNull(TEXT("IMC_Exploration exists"), MappingContext);
	TestNotNull(TEXT("IA_LockOn exists"), LockOnAction);
	if (!MappingContext || !LockOnAction)
	{
		return false;
	}

	TestEqual(
		TEXT("IA_LockOn is Boolean"),
		LockOnAction->ValueType,
		EInputActionValueType::Boolean);
	const FExactMappingAudit MiddleMouseMappingAudit = AuditExactMappings(
		MappingContext,
		LockOnAction,
		EKeys::MiddleMouseButton);
	TestEqual(
		TEXT("IMC_Exploration has exactly one middle-mouse IA_LockOn mapping"),
		MiddleMouseMappingAudit.Count,
		1);
	TestTrue(
		TEXT("Middle-mouse IA_LockOn mapping has no modifiers or triggers"),
		MiddleMouseMappingAudit.bAllMappingsAreClean);

	UClass* CharacterClass = LoadClass<ARequiemCharacter>(nullptr, CharacterClassPath);
	ARequiemCharacter* CharacterCDO = CharacterClass
		? Cast<ARequiemCharacter>(CharacterClass->GetDefaultObject())
		: nullptr;
	TestNotNull(TEXT("BP_CH_Player generated class exists"), CharacterClass);
	TestNotNull(TEXT("BP_CH_Player CDO exists"), CharacterCDO);
	if (CharacterCDO && CharacterClass)
	{
		TestEqual(
			TEXT("BP_CH_Player assigns IA_LockOn"),
			GetObjectPropertyValue(CharacterCDO, CharacterClass, TEXT("LockOnAction")),
			static_cast<UObject*>(const_cast<UInputAction*>(LockOnAction)));
		URequiemLockOnComponent* LockOnCDO = CharacterCDO->GetLockOnComponent();
		TestNotNull(TEXT("BP_CH_Player owns RequiemLockOnComponent"), LockOnCDO);
		UDecalComponent* IndicatorCDO = CharacterCDO->GetLockOnGroundIndicator();
		TestNotNull(TEXT("BP_CH_Player owns its lock-on ground indicator"), IndicatorCDO);
		if (IndicatorCDO)
		{
			UMaterialInterface* IndicatorMaterial = IndicatorCDO->GetDecalMaterial();
			TestNotNull(TEXT("BP_CH_Player assigns the ground-ring decal material"), IndicatorMaterial);
			if (IndicatorMaterial)
			{
				TestEqual(
					TEXT("Ground indicator uses the ProjectRequiem ring material"),
					IndicatorMaterial->GetPathName(),
					FString(GroundRingMaterialPath));
				UMaterial* BaseMaterial = IndicatorMaterial->GetMaterial();
				TestNotNull(TEXT("Ground-ring decal resolves to a base material"), BaseMaterial);
				if (BaseMaterial)
				{
					TestEqual(
						TEXT("Ground-ring material uses the deferred decal domain"),
						static_cast<EMaterialDomain>(BaseMaterial->MaterialDomain),
						MD_DeferredDecal);

					const FExpressionInput* BaseColorInput =
						BaseMaterial->GetExpressionInputForProperty(MP_BaseColor);
					TestNotNull(
						TEXT("Ground-ring visible color comes from the decal component"),
						Cast<UMaterialExpressionDecalColor>(
							BaseColorInput ? BaseColorInput->Expression : nullptr));

					const FExpressionInput* EmissiveInput =
						BaseMaterial->GetExpressionInputForProperty(MP_EmissiveColor);
					TestTrue(
						TEXT("Ground-ring color is not doubled through emissive"),
						!EmissiveInput || !EmissiveInput->Expression);

					const FExpressionInput* OpacityInput =
						BaseMaterial->GetExpressionInputForProperty(MP_Opacity);
					const UMaterialExpressionSubtract* RingMask =
						Cast<UMaterialExpressionSubtract>(
							OpacityInput ? OpacityInput->Expression : nullptr);
					TestNotNull(
						TEXT("Ground-ring opacity subtracts its inner mask"),
						RingMask);
					if (RingMask)
					{
						const UMaterialExpressionSphereMask* OuterMask =
							Cast<UMaterialExpressionSphereMask>(RingMask->A.Expression);
						const UMaterialExpressionSphereMask* InnerMask =
							Cast<UMaterialExpressionSphereMask>(RingMask->B.Expression);
						TestNotNull(TEXT("Ground ring has an outer circle mask"), OuterMask);
						TestNotNull(TEXT("Ground ring has an inner cutout mask"), InnerMask);
						if (OuterMask && InnerMask)
						{
							const float RingThickness =
								OuterMask->AttenuationRadius - InnerMask->AttenuationRadius;
							TestTrue(
								TEXT("Ground ring keeps a thin hollow border"),
								InnerMask->AttenuationRadius >= 0.4f
									&& OuterMask->AttenuationRadius <= 0.5f
									&& RingThickness >= 0.015f
									&& RingThickness <= 0.04f
									&& InnerMask->HardnessPercent >= 95.0f
									&& OuterMask->HardnessPercent >= 95.0f);
						}
					}
				}
				TestEqual(
					TEXT("Ground-ring material uses translucent blending"),
					IndicatorMaterial->GetBlendMode(),
					BLEND_Translucent);
			}
			TestTrue(
				TEXT("Ground indicator is a shallow circular decal"),
				IndicatorCDO->DecalSize.X > UE_KINDA_SMALL_NUMBER
					&& IndicatorCDO->DecalSize.X < IndicatorCDO->DecalSize.Y
					&& FMath::IsNearlyEqual(
						IndicatorCDO->DecalSize.Y,
						IndicatorCDO->DecalSize.Z,
						UE_KINDA_SMALL_NUMBER));
			TestTrue(
				TEXT("Ground indicator is yellow"),
				IndicatorCDO->DecalColor.R >= 0.95f
					&& IndicatorCDO->DecalColor.G >= 0.65f
					&& IndicatorCDO->DecalColor.B <= 0.1f);
			TestTrue(
				TEXT("Ground indicator projects downward"),
				FVector::DotProduct(
					IndicatorCDO->GetRelativeRotation().Vector().GetSafeNormal(),
					-FVector::UpVector) >= IndicatorProjectionDotThreshold);
			TestFalse(
				TEXT("Lock-on ground indicator CDO starts hidden"),
				IndicatorCDO->IsVisible() && !IndicatorCDO->bHiddenInGame);
		}
		if (LockOnCDO)
		{
			TestTrue(
				TEXT("Lock-on acquisition range is positive"),
				LockOnCDO->GetAcquisitionRange() > UE_KINDA_SMALL_NUMBER);
			TestTrue(
				TEXT("Lock-on break range is not shorter than acquisition range"),
				LockOnCDO->GetBreakRange() >= LockOnCDO->GetAcquisitionRange());
			TestTrue(
				TEXT("Lock-on acquisition cone is a forward half-angle"),
				LockOnCDO->GetAcquisitionConeHalfAngleDegrees() > 0.0f
					&& LockOnCDO->GetAcquisitionConeHalfAngleDegrees() < 180.0f);
			TestFalse(TEXT("Lock-on CDO starts inactive"), LockOnCDO->IsLockOnActive());
			TestNull(TEXT("Lock-on CDO starts without a target"), LockOnCDO->GetLockOnTarget());
		}
	}

	UClass* DummyClass = LoadClass<ARequiemCombatDummy>(nullptr, DummyClassPath);
	ARequiemCombatDummy* DummyCDO = DummyClass
		? Cast<ARequiemCombatDummy>(DummyClass->GetDefaultObject())
		: nullptr;
	TestNotNull(TEXT("BP_PR_CombatDummy generated class exists"), DummyClass);
	TestNotNull(TEXT("BP_PR_CombatDummy CDO exists"), DummyCDO);
	if (DummyClass && DummyCDO)
	{
		TestTrue(
			TEXT("Combat dummy implements the ProjectRequiem lock-on target contract"),
			DummyClass->ImplementsInterface(URequiemLockOnTargetInterface::StaticClass()));
		if (DummyClass->ImplementsInterface(URequiemLockOnTargetInterface::StaticClass()))
		{
			TestTrue(
				TEXT("Fresh combat dummy is a valid lock-on target"),
				IRequiemLockOnTargetInterface::Execute_IsValidLockOnTarget(DummyCDO));
		}
	}

	if (HasAnyErrors())
	{
		return false;
	}
	if (!AutomationOpenMap(MapPackagePath))
	{
		AddError(FString::Printf(TEXT("Could not open automation map %s."), MapPackagePath));
		return false;
	}

	TSharedRef<FRunState> RunState = MakeShared<FRunState>();
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForPIEReadyCommand(this, RunState));
	ADD_LATENT_AUTOMATION_COMMAND(FValidateLockOnFlowCommand(this, RunState));
	// Every failure only marks RunState, so fixture cleanup, input release and PIE teardown remain unconditional.
	ADD_LATENT_AUTOMATION_COMMAND(FCleanupTemporaryCandidatesCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FReleaseAllInputCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForPIEEndCommand(this));
	return true;
}

#endif // WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
