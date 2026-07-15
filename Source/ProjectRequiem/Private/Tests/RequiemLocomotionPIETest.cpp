// Copyright Project Requiem. All Rights Reserved.

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Animation/RequiemPlayerAnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Characters/RequiemCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EnhancedActionKeyMapping.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"

namespace RequiemLocomotionPIETest
{
constexpr TCHAR MapPackagePath[] = TEXT("/Game/ProjectRequiem/World/Maps/Dev/L_Dev_Foundation");
constexpr TCHAR MappingContextPath[] =
	TEXT("/Game/ProjectRequiem/Core/Input/IMC_Exploration.IMC_Exploration");
constexpr TCHAR MoveActionPath[] =
	TEXT("/Game/ProjectRequiem/Core/Input/Actions/IA_Move.IA_Move");
constexpr TCHAR JumpActionPath[] =
	TEXT("/Game/ProjectRequiem/Core/Input/Actions/IA_Jump.IA_Jump");
constexpr TCHAR CrouchActionPath[] =
	TEXT("/Game/ProjectRequiem/Core/Input/Actions/IA_Crouch.IA_Crouch");
constexpr TCHAR RollActionPath[] =
	TEXT("/Game/ProjectRequiem/Core/Input/Actions/IA_Roll.IA_Roll");

const FName IdleState(TEXT("Idle"));
const FName IdleLookAroundState(TEXT("Idle_LookAround"));
const FName SprintEnterState(TEXT("Sprint_Enter"));
const FName SprintLoopState(TEXT("Sprint_Loop"));
const FName SprintExitState(TEXT("Sprint_Exit"));
const FName JumpStartState(TEXT("Jump_Start"));
const FName JumpLoopState(TEXT("Jump_Loop"));
const FName JumpLandState(TEXT("Jump_Land"));
const FName CrouchEnterState(TEXT("Crouch_Enter"));
const FName CrouchLoopState(TEXT("Crouch_Loop"));
const FName CrouchExitState(TEXT("Crouch_Exit"));
const FName ReservedRollState(TEXT("Roll"));
const FName IdleAnimationName(TEXT("Idle_Loop"));
const FName IdleLookAroundAnimationName(TEXT("Idle_LookAround_Loop"));
const FName CrouchIdleAnimationName(TEXT("Crouch_Idle_Loop"));

struct FRunState
{
	bool bAborted = false;
};

struct FCharacterSnapshot
{
	FVector Location = FVector::ZeroVector;
	float GroundSpeed = 0.0f;
	float MaximumSpeed = 0.0f;
	bool bIsCrouched = false;
	bool bIsFalling = false;
	FName LocomotionState = NAME_None;
	ERequiemMovementDirection MovementDirection = ERequiemMovementDirection::None;
	bool bHasActiveAnimation = false;
	bool bActiveAnimationIsSequence = false;
	bool bActiveAnimationUsesRootMotion = false;
	FName ActiveAnimationName = NAME_None;
	bool bLocomotionMontageIsPlaying = false;
	FName LocomotionMontageSlot = NAME_None;
	float LocomotionMontagePlayRate = 0.0f;
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

APlayerController* FindPIEPlayerController()
{
	UWorld* World = FindPIEWorld();
	return World ? World->GetFirstPlayerController() : nullptr;
}

bool ReadCharacterSnapshot(FCharacterSnapshot& OutSnapshot)
{
	APlayerController* PlayerController = FindPIEPlayerController();
	ARequiemCharacter* Character = PlayerController
		? Cast<ARequiemCharacter>(PlayerController->GetPawn())
		: nullptr;
	UCharacterMovementComponent* MovementComponent = Character
		? Character->GetCharacterMovement()
		: nullptr;
	URequiemPlayerAnimInstance* AnimInstance = Character && Character->GetMesh()
		? Cast<URequiemPlayerAnimInstance>(Character->GetMesh()->GetAnimInstance())
		: nullptr;

	if (!Character || !MovementComponent || !AnimInstance)
	{
		return false;
	}

	OutSnapshot.Location = Character->GetActorLocation();
	OutSnapshot.GroundSpeed = Character->GetVelocity().Size2D();
	OutSnapshot.MaximumSpeed = MovementComponent->GetMaxSpeed();
	OutSnapshot.bIsCrouched = Character->IsCrouched();
	OutSnapshot.bIsFalling = MovementComponent->IsFalling();
	OutSnapshot.LocomotionState = AnimInstance->GetLocomotionStateName();
	OutSnapshot.MovementDirection = AnimInstance->GetMovementDirection();
	const UAnimSequenceBase* ActiveAnimation = AnimInstance->GetActiveLocomotionAnimation();
	const UAnimSequence* ActiveSequence = Cast<UAnimSequence>(ActiveAnimation);
	const UAnimMontage* ActiveMontage = AnimInstance->GetCurrentActiveMontage();
	OutSnapshot.bHasActiveAnimation = ActiveAnimation != nullptr;
	OutSnapshot.bActiveAnimationIsSequence = ActiveSequence != nullptr;
	OutSnapshot.bActiveAnimationUsesRootMotion = ActiveSequence && ActiveSequence->bEnableRootMotion;
	OutSnapshot.ActiveAnimationName = ActiveAnimation ? ActiveAnimation->GetFName() : NAME_None;
	OutSnapshot.bLocomotionMontageIsPlaying =
		ActiveMontage && AnimInstance->Montage_IsPlaying(ActiveMontage);
	OutSnapshot.LocomotionMontageSlot =
		ActiveMontage && !ActiveMontage->SlotAnimTracks.IsEmpty()
			? ActiveMontage->SlotAnimTracks[0].SlotName
			: NAME_None;
	OutSnapshot.LocomotionMontagePlayRate =
		ActiveMontage ? AnimInstance->Montage_GetPlayRate(ActiveMontage) : 0.0f;
	return true;
}

bool HasValidLocomotionPlayback(const FCharacterSnapshot& Snapshot)
{
	return Snapshot.bHasActiveAnimation
		&& Snapshot.bActiveAnimationIsSequence
		&& !Snapshot.bActiveAnimationUsesRootMotion
		&& Snapshot.bLocomotionMontageIsPlaying
		&& Snapshot.LocomotionMontageSlot == FName(TEXT("DefaultSlot"));
}

bool HasExpectedLocomotionPlayback(
	const FCharacterSnapshot& Snapshot, const FName ExpectedAnimationName)
{
	return HasValidLocomotionPlayback(Snapshot)
		&& Snapshot.ActiveAnimationName == ExpectedAnimationName;
}

UEnhancedInputLocalPlayerSubsystem* FindEnhancedInputSubsystem()
{
	APlayerController* PlayerController = FindPIEPlayerController();
	ULocalPlayer* LocalPlayer = PlayerController ? PlayerController->GetLocalPlayer() : nullptr;
	return LocalPlayer ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
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
	if (InputSubsystem && Action && InputSubsystem->HasContinuousInputInjectionForAction(Action))
	{
		InputSubsystem->StopContinuousInputInjectionForAction(Action);
	}
}

void ReleaseAllTestInput()
{
	StopContinuousAction(MoveActionPath);
	StopContinuousAction(JumpActionPath);
	StopContinuousAction(CrouchActionPath);
	StopContinuousAction(RollActionPath);
}

bool HasExactMapping(
	const UInputMappingContext* MappingContext,
	const UInputAction* Action,
	const FKey& Key)
{
	if (!MappingContext || !Action)
	{
		return false;
	}

	for (const FEnhancedActionKeyMapping& Mapping : MappingContext->GetMappings())
	{
		if (Mapping.Action == Action && Mapping.Key == Key)
		{
			return true;
		}
	}

	return false;
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
	bool ShouldSkip() const
	{
		return RunState->bAborted;
	}

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

	bool AbortIfTimedOut(const double ElapsedSeconds, const FString& StageDetails)
	{
		if (ElapsedSeconds < TimeoutSeconds)
		{
			return false;
		}

		return AbortWithError(FString::Printf(
			TEXT("Locomotion PIE stage timed out after %.1fs: %s"),
			TimeoutSeconds,
			*StageDetails));
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
	FWaitForPIEReadyCommand(FAutomationTestBase* InTest, TSharedRef<FRunState> InRunState)
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
		UWorld* World = FindPIEWorld();
		FCharacterSnapshot Snapshot;
		if (World && ReadCharacterSnapshot(Snapshot))
		{
			if (!World->GetMapName().Contains(TEXT("L_Dev_Foundation")))
			{
				return AbortWithError(FString::Printf(
					TEXT("PIE opened the wrong map: %s"),
					*World->GetMapName()));
			}

			Test->AddInfo(TEXT("PIE opened L_Dev_Foundation with the Requiem player AnimInstance."));
			return true;
		}

		return AbortIfTimedOut(
			ElapsedSeconds,
			TEXT("waiting for L_Dev_Foundation, the possessed player, and its locomotion AnimInstance"));
	}
};

class FWaitForIdleLookAroundCommand final : public FTimedCommand
{
public:
	FWaitForIdleLookAroundCommand(FAutomationTestBase* InTest, TSharedRef<FRunState> InRunState)
		: FTimedCommand(InTest, MoveTemp(InRunState), 17.0)
	{
	}

	virtual bool Update() override
	{
		if (ShouldSkip())
		{
			return true;
		}

		const double ElapsedSeconds = GetElapsedSeconds();
		FCharacterSnapshot Snapshot;
		if (!ReadCharacterSnapshot(Snapshot))
		{
			return AbortIfTimedOut(ElapsedSeconds, TEXT("reading idle playback before look-around"));
		}

		bSawIdlePlayback |= Snapshot.LocomotionState == IdleState
			&& HasExpectedLocomotionPlayback(Snapshot, IdleAnimationName);
		bSawLookAroundPlayback |= Snapshot.LocomotionState == IdleLookAroundState
			&& HasExpectedLocomotionPlayback(Snapshot, IdleLookAroundAnimationName);
		if (bSawIdlePlayback && bSawLookAroundPlayback)
		{
			Test->AddInfo(TEXT("Idle_Loop played on DefaultSlot and transitioned to the occasional Idle_LookAround_Loop."));
			return true;
		}

		return AbortIfTimedOut(
			ElapsedSeconds,
			FString::Printf(
				TEXT("waiting for occasional idle look-around (state=%s, idlePlayback=%d, lookPlayback=%d)"),
				*Snapshot.LocomotionState.ToString(),
				bSawIdlePlayback,
				bSawLookAroundPlayback));
	}

private:
	bool bSawIdlePlayback = false;
	bool bSawLookAroundPlayback = false;
};

class FWaitForPartialSprintCommand final : public FTimedCommand
{
public:
	FWaitForPartialSprintCommand(FAutomationTestBase* InTest, TSharedRef<FRunState> InRunState)
		: FTimedCommand(InTest, MoveTemp(InRunState), 5.0)
	{
	}

	virtual bool Update() override
	{
		if (ShouldSkip())
		{
			return true;
		}

		const double ElapsedSeconds = GetElapsedSeconds();
		if (!SetContinuousAction(MoveActionPath, FInputActionValue(FVector2D(0.0, 0.3))))
		{
			return AbortIfTimedOut(ElapsedSeconds, TEXT("injecting partial IA_Move input"));
		}

		FCharacterSnapshot Snapshot;
		if (!ReadCharacterSnapshot(Snapshot))
		{
			return AbortIfTimedOut(ElapsedSeconds, TEXT("reading the partial-sprint snapshot"));
		}

		bSawSprintEnterPlayback |= Snapshot.LocomotionState == SprintEnterState
			&& HasExpectedLocomotionPlayback(Snapshot, SprintEnterState);
		bSawSprintLoopPlayback |= Snapshot.LocomotionState == SprintLoopState
			&& HasExpectedLocomotionPlayback(Snapshot, SprintLoopState);
		const bool bAtPartialSpeed = Snapshot.MaximumSpeed > 0.0f
			&& Snapshot.GroundSpeed >= 50.0f
			&& Snapshot.GroundSpeed < Snapshot.MaximumSpeed * 0.75f;
		if (bSawSprintEnterPlayback
			&& bSawSprintLoopPlayback
			&& bAtPartialSpeed
			&& Snapshot.LocomotionState == SprintLoopState
			&& Snapshot.MovementDirection == ERequiemMovementDirection::Forward)
		{
			Test->AddInfo(TEXT("Partial movement reached Sprint_Loop without reaching full CharacterMovement speed."));
			return true;
		}

		return AbortIfTimedOut(
			ElapsedSeconds,
			FString::Printf(
				TEXT("partial sprint (state=%s, direction=%d, speed=%.1f, max=%.1f)"),
				*Snapshot.LocomotionState.ToString(),
				static_cast<int32>(Snapshot.MovementDirection),
				Snapshot.GroundSpeed,
				Snapshot.MaximumSpeed));
	}

private:
	bool bSawSprintEnterPlayback = false;
	bool bSawSprintLoopPlayback = false;
};

class FWaitForPartialSprintStopCommand final : public FTimedCommand
{
public:
	FWaitForPartialSprintStopCommand(FAutomationTestBase* InTest, TSharedRef<FRunState> InRunState)
		: FTimedCommand(InTest, MoveTemp(InRunState), 5.0)
	{
	}

	virtual bool Update() override
	{
		if (ShouldSkip())
		{
			return true;
		}

		const double ElapsedSeconds = GetElapsedSeconds();
		StopContinuousAction(MoveActionPath);
		FCharacterSnapshot Snapshot;
		if (!ReadCharacterSnapshot(Snapshot))
		{
			return AbortIfTimedOut(ElapsedSeconds, TEXT("reading the partial-sprint release snapshot"));
		}

		if (!bHasInitialSpeed)
		{
			InitialSpeed = Snapshot.GroundSpeed;
			bHasInitialSpeed = true;
		}
		if (Snapshot.LocomotionState == SprintExitState)
		{
			return AbortWithError(TEXT("Stopping before full speed incorrectly traversed Sprint_Exit."));
		}

		bSawIdlePlayback |= Snapshot.LocomotionState == IdleState
			&& HasExpectedLocomotionPlayback(Snapshot, IdleAnimationName);
		bSawDeceleration |= Snapshot.GroundSpeed <= FMath::Max(0.0f, InitialSpeed - 40.0f);
		if (bSawIdlePlayback && bSawDeceleration && Snapshot.GroundSpeed <= 10.0f)
		{
			Test->AddInfo(TEXT("Stopping before full speed returned directly to Idle while CharacterMovement decelerated."));
			return true;
		}

		return AbortIfTimedOut(
			ElapsedSeconds,
			FString::Printf(
				TEXT("partial sprint release (state=%s, speed=%.1f, idle=%d, decelerated=%d)"),
				*Snapshot.LocomotionState.ToString(),
				Snapshot.GroundSpeed,
				bSawIdlePlayback,
				bSawDeceleration));
	}

private:
	float InitialSpeed = 0.0f;
	bool bHasInitialSpeed = false;
	bool bSawIdlePlayback = false;
	bool bSawDeceleration = false;
};

class FWaitForDiagonalSprintCommand final : public FTimedCommand
{
public:
	FWaitForDiagonalSprintCommand(FAutomationTestBase* InTest, TSharedRef<FRunState> InRunState)
		: FTimedCommand(InTest, MoveTemp(InRunState), 5.0)
	{
	}

	virtual bool Update() override
	{
		if (ShouldSkip())
		{
			return true;
		}

		const double ElapsedSeconds = GetElapsedSeconds();
		if (!SetContinuousAction(MoveActionPath, FInputActionValue(FVector2D(1.0, 1.0))))
		{
			return AbortIfTimedOut(ElapsedSeconds, TEXT("injecting IA_Move for the sprint stage"));
		}

		FCharacterSnapshot Snapshot;
		if (!ReadCharacterSnapshot(Snapshot))
		{
			return AbortIfTimedOut(ElapsedSeconds, TEXT("reading the sprint player snapshot"));
		}

		if (!bHasStartLocation)
		{
			StartLocation = Snapshot.Location;
			bHasStartLocation = true;
		}

		bSawSprintEnter |= Snapshot.LocomotionState == SprintEnterState;
		bSawSprintLoop |= Snapshot.LocomotionState == SprintLoopState;
		bSawSprintEnterPlayback |= Snapshot.LocomotionState == SprintEnterState
			&& HasExpectedLocomotionPlayback(Snapshot, SprintEnterState);
		bSawSprintLoopPlayback |= Snapshot.LocomotionState == SprintLoopState
			&& HasExpectedLocomotionPlayback(Snapshot, FName(TEXT("Jog_Fwd_R_Loop")));
		bSawForwardRight |=
			Snapshot.MovementDirection == ERequiemMovementDirection::ForwardRight;
		bSawMeaningfulDisplacement |=
			(Snapshot.Location - StartLocation).Size2D() >= 100.0f;

		const bool bAtExpectedSpeed = Snapshot.MaximumSpeed > 0.0f
			&& Snapshot.GroundSpeed >= Snapshot.MaximumSpeed * 0.8f;
		if (bSawSprintEnter
			&& bSawSprintLoop
			&& bSawSprintEnterPlayback
			&& bSawSprintLoopPlayback
			&& bSawForwardRight
			&& bSawMeaningfulDisplacement
			&& bAtExpectedSpeed
			&& Snapshot.LocomotionState == SprintLoopState
			&& Snapshot.MovementDirection == ERequiemMovementDirection::ForwardRight)
		{
			Test->AddInfo(TEXT("W+D reached Sprint_Enter, Sprint_Loop, and ForwardRight movement."));
			return true;
		}

		return AbortIfTimedOut(
			ElapsedSeconds,
			FString::Printf(
				TEXT("W+D sprint (state=%s, direction=%d, speed=%.1f, enter=%d, loop=%d)"),
				*Snapshot.LocomotionState.ToString(),
				static_cast<int32>(Snapshot.MovementDirection),
				Snapshot.GroundSpeed,
				bSawSprintEnter,
				bSawSprintLoop));
	}

private:
	FVector StartLocation = FVector::ZeroVector;
	bool bHasStartLocation = false;
	bool bSawSprintEnter = false;
	bool bSawSprintLoop = false;
	bool bSawSprintEnterPlayback = false;
	bool bSawSprintLoopPlayback = false;
	bool bSawForwardRight = false;
	bool bSawMeaningfulDisplacement = false;
};

class FWaitForDirectionalLoopCommand final : public FTimedCommand
{
public:
	FWaitForDirectionalLoopCommand(
		FAutomationTestBase* InTest,
		TSharedRef<FRunState> InRunState,
		const FVector2D InMovementInput,
		const ERequiemMovementDirection InExpectedDirection,
		const FName InExpectedAnimationName,
		FString InDirectionLabel,
		const bool bInCrouched)
		: FTimedCommand(InTest, MoveTemp(InRunState), 4.0)
		// Direction selection does not need another full-speed traversal of the
		// small dev floor; the dedicated sprint stages cover maximum speed.
		, MovementInput(InMovementInput.GetSafeNormal() * 0.35f)
		, ExpectedDirection(InExpectedDirection)
		, ExpectedAnimationName(InExpectedAnimationName)
		, DirectionLabel(MoveTemp(InDirectionLabel))
		, bCrouched(bInCrouched)
	{
	}

	virtual bool Update() override
	{
		if (ShouldSkip())
		{
			return true;
		}

		const double ElapsedSeconds = GetElapsedSeconds();
		if ((bCrouched && !SetContinuousAction(CrouchActionPath, FInputActionValue(true)))
			|| !SetContinuousAction(MoveActionPath, FInputActionValue(MovementInput)))
		{
			return AbortIfTimedOut(ElapsedSeconds, FString::Printf(
				TEXT("injecting %s directional input"), *DirectionLabel));
		}

		FCharacterSnapshot Snapshot;
		if (!ReadCharacterSnapshot(Snapshot))
		{
			return AbortIfTimedOut(ElapsedSeconds, FString::Printf(
				TEXT("reading %s directional snapshot"), *DirectionLabel));
		}

		if (!bCrouched && Snapshot.LocomotionState == SprintExitState)
		{
			return AbortWithError(FString::Printf(
				TEXT("Changing to %s incorrectly traversed Sprint_Exit without a 180-degree reversal."),
				*DirectionLabel));
		}
		if (bCrouched && !Snapshot.bIsCrouched)
		{
			return AbortWithError(FString::Printf(
				TEXT("Ctrl hold was lost while validating crouched %s movement."),
				*DirectionLabel));
		}

		const FName ExpectedState = bCrouched ? CrouchLoopState : SprintLoopState;
		const float ExpectedPlayRate = Snapshot.MaximumSpeed > UE_KINDA_SMALL_NUMBER
			? FMath::Clamp(Snapshot.GroundSpeed / Snapshot.MaximumSpeed, 0.35f, 1.2f)
			: 1.0f;
		const bool bPlayRateMatchesCharacterMovement =
			FMath::IsNearlyEqual(Snapshot.LocomotionMontagePlayRate, ExpectedPlayRate, 0.08f);
		if (Snapshot.LocomotionState == ExpectedState
			&& Snapshot.MovementDirection == ExpectedDirection
			&& Snapshot.GroundSpeed >= 20.0f
			&& bPlayRateMatchesCharacterMovement
			&& HasExpectedLocomotionPlayback(Snapshot, ExpectedAnimationName))
		{
			Test->AddInfo(FString::Printf(
				TEXT("%s %s movement selected its NRM animation on DefaultSlot."),
				bCrouched ? TEXT("Crouched") : TEXT("Standing"),
				*DirectionLabel));
			return true;
		}

		return AbortIfTimedOut(
			ElapsedSeconds,
			FString::Printf(
				TEXT("%s direction (state=%s, direction=%d, speed=%.1f, animation=%s, expected=%s, montage=%d, slot=%s, rate=%.2f, expectedRate=%.2f)"),
				*DirectionLabel,
				*Snapshot.LocomotionState.ToString(),
				static_cast<int32>(Snapshot.MovementDirection),
				Snapshot.GroundSpeed,
				*Snapshot.ActiveAnimationName.ToString(),
				*ExpectedAnimationName.ToString(),
				Snapshot.bLocomotionMontageIsPlaying,
				*Snapshot.LocomotionMontageSlot.ToString(),
				Snapshot.LocomotionMontagePlayRate,
				ExpectedPlayRate));
	}

private:
	FVector2D MovementInput = FVector2D::ZeroVector;
	ERequiemMovementDirection ExpectedDirection = ERequiemMovementDirection::None;
	FName ExpectedAnimationName = NAME_None;
	FString DirectionLabel;
	bool bCrouched = false;
};

class FWaitForSprintReversalCommand final : public FTimedCommand
{
public:
	FWaitForSprintReversalCommand(FAutomationTestBase* InTest, TSharedRef<FRunState> InRunState)
		: FTimedCommand(InTest, MoveTemp(InRunState), 5.0)
	{
	}

	virtual bool Update() override
	{
		if (ShouldSkip())
		{
			return true;
		}

		const double ElapsedSeconds = GetElapsedSeconds();
		if (!SetContinuousAction(MoveActionPath, FInputActionValue(FVector2D(-1.0, -1.0))))
		{
			return AbortIfTimedOut(ElapsedSeconds, TEXT("injecting the 180-degree movement reversal"));
		}

		FCharacterSnapshot Snapshot;
		if (!ReadCharacterSnapshot(Snapshot))
		{
			return AbortIfTimedOut(ElapsedSeconds, TEXT("reading the reversal player snapshot"));
		}

		bSawSprintExit |= Snapshot.LocomotionState == SprintExitState;
		bSawSprintExitPlayback |= Snapshot.LocomotionState == SprintExitState
			&& HasExpectedLocomotionPlayback(Snapshot, SprintExitState);
		if (bSawSprintExit)
		{
			bSawSprintEnterAfterExit |= Snapshot.LocomotionState == SprintEnterState;
			bSawSprintLoopAfterExit |= Snapshot.LocomotionState == SprintLoopState;
			bSawSprintEnterPlaybackAfterExit |= Snapshot.LocomotionState == SprintEnterState
				&& HasExpectedLocomotionPlayback(Snapshot, SprintEnterState);
			bSawSprintLoopPlaybackAfterExit |= Snapshot.LocomotionState == SprintLoopState
				&& HasExpectedLocomotionPlayback(Snapshot, FName(TEXT("Jog_Bwd_L_Loop")));
		}
		bSawBackwardLeft |=
			Snapshot.MovementDirection == ERequiemMovementDirection::BackwardLeft;

		const bool bAtExpectedSpeed = Snapshot.MaximumSpeed > 0.0f
			&& Snapshot.GroundSpeed >= Snapshot.MaximumSpeed * 0.8f;
		if (bSawSprintExit
			&& bSawSprintExitPlayback
			&& bSawSprintEnterAfterExit
			&& bSawSprintEnterPlaybackAfterExit
			&& bSawSprintLoopAfterExit
			&& bSawSprintLoopPlaybackAfterExit
			&& bSawBackwardLeft
			&& bAtExpectedSpeed
			&& Snapshot.LocomotionState == SprintLoopState
			&& Snapshot.MovementDirection == ERequiemMovementDirection::BackwardLeft)
		{
			Test->AddInfo(TEXT("A 180-degree reversal traversed Sprint_Exit, Sprint_Enter, and BackwardLeft Sprint_Loop."));
			return true;
		}

		return AbortIfTimedOut(
			ElapsedSeconds,
			FString::Printf(
				TEXT("180 reversal (state=%s, direction=%d, speed=%.1f, exit=%d, enter=%d, loop=%d)"),
				*Snapshot.LocomotionState.ToString(),
				static_cast<int32>(Snapshot.MovementDirection),
				Snapshot.GroundSpeed,
				bSawSprintExit,
				bSawSprintEnterAfterExit,
				bSawSprintLoopAfterExit));
	}

private:
	bool bSawSprintExit = false;
	bool bSawSprintExitPlayback = false;
	bool bSawSprintEnterAfterExit = false;
	bool bSawSprintEnterPlaybackAfterExit = false;
	bool bSawSprintLoopAfterExit = false;
	bool bSawSprintLoopPlaybackAfterExit = false;
	bool bSawBackwardLeft = false;
};

class FWaitForSprintStopCommand final : public FTimedCommand
{
public:
	FWaitForSprintStopCommand(FAutomationTestBase* InTest, TSharedRef<FRunState> InRunState)
		: FTimedCommand(InTest, MoveTemp(InRunState), 5.0)
	{
	}

	virtual bool Update() override
	{
		if (ShouldSkip())
		{
			return true;
		}

		const double ElapsedSeconds = GetElapsedSeconds();
		StopContinuousAction(MoveActionPath);
		FCharacterSnapshot Snapshot;
		if (!ReadCharacterSnapshot(Snapshot))
		{
			return AbortIfTimedOut(ElapsedSeconds, TEXT("reading the sprint-stop snapshot"));
		}

		if (!bHasInitialSpeed)
		{
			InitialSpeed = Snapshot.GroundSpeed;
			bHasInitialSpeed = true;
		}

		bSawSprintExit |= Snapshot.LocomotionState == SprintExitState;
		bSawSprintExitPlayback |= Snapshot.LocomotionState == SprintExitState
			&& HasExpectedLocomotionPlayback(Snapshot, SprintExitState);
		bSawIdlePlayback |= Snapshot.LocomotionState == IdleState
			&& HasExpectedLocomotionPlayback(Snapshot, IdleAnimationName);
		bSawDeceleration |= Snapshot.GroundSpeed <= FMath::Max(0.0f, InitialSpeed - 100.0f);
		if (bSawSprintExit
			&& bSawSprintExitPlayback
			&& bSawIdlePlayback
			&& bSawDeceleration
			&& Snapshot.LocomotionState == IdleState
			&& Snapshot.GroundSpeed <= 10.0f)
		{
			Test->AddInfo(TEXT("Releasing W+D traversed Sprint_Exit, decelerated, and returned to Idle."));
			return true;
		}

		return AbortIfTimedOut(
			ElapsedSeconds,
			FString::Printf(
				TEXT("sprint release (state=%s, speed=%.1f, exit=%d, decelerated=%d)"),
				*Snapshot.LocomotionState.ToString(),
				Snapshot.GroundSpeed,
				bSawSprintExit,
				bSawDeceleration));
	}

private:
	float InitialSpeed = 0.0f;
	bool bHasInitialSpeed = false;
	bool bSawSprintExit = false;
	bool bSawSprintExitPlayback = false;
	bool bSawIdlePlayback = false;
	bool bSawDeceleration = false;
};

class FObserveJumpCommand final : public FTimedCommand
{
public:
	FObserveJumpCommand(FAutomationTestBase* InTest, TSharedRef<FRunState> InRunState)
		: FTimedCommand(InTest, MoveTemp(InRunState), 6.0)
	{
	}

	virtual bool Update() override
	{
		if (ShouldSkip())
		{
			return true;
		}

		const double ElapsedSeconds = GetElapsedSeconds();
		FCharacterSnapshot Snapshot;
		if (!ReadCharacterSnapshot(Snapshot))
		{
			return AbortIfTimedOut(ElapsedSeconds, TEXT("reading the jump player snapshot"));
		}

		if (ElapsedSeconds < 0.12)
		{
			if (!SetContinuousAction(JumpActionPath, FInputActionValue(true)))
			{
				return AbortIfTimedOut(ElapsedSeconds, TEXT("injecting IA_Jump"));
			}
		}
		else
		{
			StopContinuousAction(JumpActionPath);
			bReleasedJump = true;
		}

		bSawJumpStart |= Snapshot.LocomotionState == JumpStartState;
		bSawJumpLoop |= Snapshot.LocomotionState == JumpLoopState;
		bSawJumpLand |= Snapshot.LocomotionState == JumpLandState;
		bSawJumpStartPlayback |= Snapshot.LocomotionState == JumpStartState
			&& HasExpectedLocomotionPlayback(Snapshot, JumpStartState);
		bSawJumpLoopPlayback |= Snapshot.LocomotionState == JumpLoopState
			&& HasExpectedLocomotionPlayback(Snapshot, JumpLoopState);
		bSawJumpLandPlayback |= Snapshot.LocomotionState == JumpLandState
			&& HasExpectedLocomotionPlayback(Snapshot, JumpLandState);
		bSawIdlePlaybackAfterLand |= Snapshot.LocomotionState == IdleState
			&& HasExpectedLocomotionPlayback(Snapshot, IdleAnimationName);

		if (bReleasedJump
			&& bSawJumpStart
			&& bSawJumpStartPlayback
			&& bSawJumpLoop
			&& bSawJumpLoopPlayback
			&& bSawJumpLand
			&& bSawJumpLandPlayback
			&& bSawIdlePlaybackAfterLand
			&& !Snapshot.bIsFalling
			&& Snapshot.LocomotionState == IdleState)
		{
			Test->AddInfo(TEXT("Space traversed Jump_Start, Jump_Loop, and Jump_Land."));
			return true;
		}

		return AbortIfTimedOut(
			ElapsedSeconds,
			FString::Printf(
				TEXT("jump sequence (state=%s, start=%d, loop=%d, land=%d)"),
				*Snapshot.LocomotionState.ToString(),
				bSawJumpStart,
				bSawJumpLoop,
				bSawJumpLand));
	}

private:
	bool bReleasedJump = false;
	bool bSawJumpStart = false;
	bool bSawJumpStartPlayback = false;
	bool bSawJumpLoop = false;
	bool bSawJumpLoopPlayback = false;
	bool bSawJumpLand = false;
	bool bSawJumpLandPlayback = false;
	bool bSawIdlePlaybackAfterLand = false;
};

class FWaitForCrouchHoldCommand final : public FTimedCommand
{
public:
	FWaitForCrouchHoldCommand(FAutomationTestBase* InTest, TSharedRef<FRunState> InRunState)
		: FTimedCommand(InTest, MoveTemp(InRunState), 4.0)
	{
	}

	virtual bool Update() override
	{
		if (ShouldSkip())
		{
			return true;
		}

		const double ElapsedSeconds = GetElapsedSeconds();
		if (!SetContinuousAction(CrouchActionPath, FInputActionValue(true)))
		{
			return AbortIfTimedOut(ElapsedSeconds, TEXT("injecting held IA_Crouch"));
		}

		FCharacterSnapshot Snapshot;
		if (!ReadCharacterSnapshot(Snapshot))
		{
			return AbortIfTimedOut(ElapsedSeconds, TEXT("reading the crouch-hold snapshot"));
		}

		bSawCrouchEnter |= Snapshot.LocomotionState == CrouchEnterState;
		bSawCrouchEnterPlayback |= Snapshot.LocomotionState == CrouchEnterState
			&& HasExpectedLocomotionPlayback(Snapshot, CrouchEnterState);
		bSawCrouchIdlePlayback |= Snapshot.LocomotionState == CrouchLoopState
			&& Snapshot.MovementDirection == ERequiemMovementDirection::None
			&& HasExpectedLocomotionPlayback(Snapshot, CrouchIdleAnimationName);
		if (Snapshot.bIsCrouched)
		{
			bSawCrouched = true;
		}
		else if (bSawCrouched)
		{
			return AbortWithError(TEXT("Ctrl hold stopped crouching before Ctrl was released."));
		}

		if (bSawCrouchEnter && Snapshot.LocomotionState == CrouchLoopState)
		{
			if (CrouchLoopStartSeconds < 0.0)
			{
				CrouchLoopStartSeconds = ElapsedSeconds;
			}
		}

		if (bSawCrouchEnter
			&& bSawCrouchEnterPlayback
			&& bSawCrouchIdlePlayback
			&& Snapshot.bIsCrouched
			&& Snapshot.LocomotionState == CrouchLoopState
			&& CrouchLoopStartSeconds >= 0.0
			&& ElapsedSeconds - CrouchLoopStartSeconds >= 0.4)
		{
			Test->AddInfo(TEXT("Ctrl press entered crouch and remained crouched while held."));
			return true;
		}

		return AbortIfTimedOut(
			ElapsedSeconds,
			FString::Printf(
				TEXT("Ctrl hold (state=%s, crouched=%d, enter=%d)"),
				*Snapshot.LocomotionState.ToString(),
				Snapshot.bIsCrouched,
				bSawCrouchEnter));
	}

private:
	double CrouchLoopStartSeconds = -1.0;
	bool bSawCrouchEnter = false;
	bool bSawCrouchEnterPlayback = false;
	bool bSawCrouchIdlePlayback = false;
	bool bSawCrouched = false;
};

class FWaitForCrouchDiagonalCommand final : public FTimedCommand
{
public:
	FWaitForCrouchDiagonalCommand(FAutomationTestBase* InTest, TSharedRef<FRunState> InRunState)
		: FTimedCommand(InTest, MoveTemp(InRunState), 4.0)
	{
	}

	virtual bool Update() override
	{
		if (ShouldSkip())
		{
			return true;
		}

		const double ElapsedSeconds = GetElapsedSeconds();
		if (!SetContinuousAction(CrouchActionPath, FInputActionValue(true))
			|| !SetContinuousAction(MoveActionPath, FInputActionValue(FVector2D(1.0, 1.0))))
		{
			return AbortIfTimedOut(ElapsedSeconds, TEXT("injecting crouched diagonal movement"));
		}

		FCharacterSnapshot Snapshot;
		if (!ReadCharacterSnapshot(Snapshot))
		{
			return AbortIfTimedOut(ElapsedSeconds, TEXT("reading the directional crouch snapshot"));
		}

		if (!Snapshot.bIsCrouched)
		{
			return AbortWithError(TEXT("Character uncrouched while Ctrl remained held during W+D movement."));
		}

		if (!bHasStartLocation)
		{
			StartLocation = Snapshot.Location;
			bHasStartLocation = true;
		}

		const bool bMoved = (Snapshot.Location - StartLocation).Size2D() >= 30.0f;
		if (bMoved
			&& Snapshot.GroundSpeed >= 20.0f
			&& Snapshot.LocomotionState == CrouchLoopState
			&& Snapshot.MovementDirection == ERequiemMovementDirection::ForwardRight
			&& HasExpectedLocomotionPlayback(Snapshot, FName(TEXT("Crouch_Fwd_R_Loop"))))
		{
			Test->AddInfo(TEXT("Held Ctrl + W+D kept Crouch_Loop and selected ForwardRight."));
			return true;
		}

		return AbortIfTimedOut(
			ElapsedSeconds,
			FString::Printf(
				TEXT("directional crouch (state=%s, direction=%d, speed=%.1f, moved=%d)"),
				*Snapshot.LocomotionState.ToString(),
				static_cast<int32>(Snapshot.MovementDirection),
				Snapshot.GroundSpeed,
				bMoved));
	}

private:
	FVector StartLocation = FVector::ZeroVector;
	bool bHasStartLocation = false;
};

class FWaitForCrouchExitCommand final : public FTimedCommand
{
public:
	FWaitForCrouchExitCommand(FAutomationTestBase* InTest, TSharedRef<FRunState> InRunState)
		: FTimedCommand(InTest, MoveTemp(InRunState), 5.0)
	{
	}

	virtual bool Update() override
	{
		if (ShouldSkip())
		{
			return true;
		}

		const double ElapsedSeconds = GetElapsedSeconds();
		StopContinuousAction(MoveActionPath);
		StopContinuousAction(CrouchActionPath);
		FCharacterSnapshot Snapshot;
		if (!ReadCharacterSnapshot(Snapshot))
		{
			return AbortIfTimedOut(ElapsedSeconds, TEXT("reading the crouch-exit snapshot"));
		}

		bSawCrouchExit |= Snapshot.LocomotionState == CrouchExitState;
		bSawCrouchExitPlayback |= Snapshot.LocomotionState == CrouchExitState
			&& HasExpectedLocomotionPlayback(Snapshot, CrouchExitState);
		bSawIdlePlayback |= Snapshot.LocomotionState == IdleState
			&& HasExpectedLocomotionPlayback(Snapshot, IdleAnimationName);
		if (bSawCrouchExit
			&& bSawCrouchExitPlayback
			&& bSawIdlePlayback
			&& !Snapshot.bIsCrouched
			&& Snapshot.LocomotionState == IdleState
			&& Snapshot.GroundSpeed <= 10.0f)
		{
			Test->AddInfo(TEXT("Releasing Ctrl traversed Crouch_Exit and uncrouched back to Idle."));
			return true;
		}

		return AbortIfTimedOut(
			ElapsedSeconds,
			FString::Printf(
				TEXT("Ctrl release (state=%s, crouched=%d, speed=%.1f, exit=%d)"),
				*Snapshot.LocomotionState.ToString(),
				Snapshot.bIsCrouched,
				Snapshot.GroundSpeed,
				bSawCrouchExit));
	}

private:
	bool bSawCrouchExit = false;
	bool bSawCrouchExitPlayback = false;
	bool bSawIdlePlayback = false;
};

class FValidateReservedRollCommand final : public FTimedCommand
{
public:
	FValidateReservedRollCommand(FAutomationTestBase* InTest, TSharedRef<FRunState> InRunState)
		: FTimedCommand(InTest, MoveTemp(InRunState), 2.0)
	{
	}

	virtual bool Update() override
	{
		if (ShouldSkip())
		{
			return true;
		}

		const double ElapsedSeconds = GetElapsedSeconds();
		FCharacterSnapshot Snapshot;
		if (!ReadCharacterSnapshot(Snapshot))
		{
			return AbortIfTimedOut(ElapsedSeconds, TEXT("reading the reserved-roll snapshot"));
		}

		if (ElapsedSeconds < 0.55)
		{
			if (!bPressedShift)
			{
				StartLocation = Snapshot.Location;
				bPressedShift = true;
			}
			if (!SetContinuousAction(RollActionPath, FInputActionValue(true)))
			{
				return AbortIfTimedOut(ElapsedSeconds, TEXT("injecting reserved IA_Roll"));
			}
		}
		else
		{
			StopContinuousAction(RollActionPath);
			bReleasedShift = true;
		}

		if (Snapshot.LocomotionState == ReservedRollState)
		{
			StopContinuousAction(RollActionPath);
			return AbortWithError(TEXT("Left Shift entered Roll even though roll is reserved for a future pass."));
		}

		if (bReleasedShift && ElapsedSeconds >= 0.7)
		{
			const float Displacement = (Snapshot.Location - StartLocation).Size2D();
			if (Displacement > 5.0f || Snapshot.GroundSpeed > 5.0f)
			{
				return AbortWithError(FString::Printf(
					TEXT("Left Shift alone moved the character (distance=%.2f, speed=%.2f)."),
					Displacement,
					Snapshot.GroundSpeed));
			}

			Test->AddInfo(TEXT("Left Shift remained reserved: no displacement and no Roll state."));
			return true;
		}

		return AbortIfTimedOut(ElapsedSeconds, TEXT("validating reserved Left Shift roll input"));
	}

private:
	FVector StartLocation = FVector::ZeroVector;
	bool bPressedShift = false;
	bool bReleasedShift = false;
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
			Test->AddError(TEXT("PIE did not finish within 5 seconds of the teardown request."));
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
	FRequiemLocomotionPIETest,
	"ProjectRequiem.Movement.LocomotionPIE",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::NonNullRHI
		| EAutomationTestFlags::ProductFilter)

bool FRequiemLocomotionPIETest::RunTest(const FString& Parameters)
{
	using namespace RequiemLocomotionPIETest;

	const UInputMappingContext* MappingContext = LoadObject<UInputMappingContext>(
		nullptr,
		MappingContextPath);
	const UInputAction* CrouchAction = LoadObject<UInputAction>(nullptr, CrouchActionPath);
	const UInputAction* RollAction = LoadObject<UInputAction>(nullptr, RollActionPath);

	TestNotNull(TEXT("IMC_Exploration exists"), MappingContext);
	TestNotNull(TEXT("IA_Crouch exists"), CrouchAction);
	TestNotNull(TEXT("IA_Roll exists"), RollAction);
	if (!MappingContext || !CrouchAction || !RollAction)
	{
		return false;
	}

	TestTrue(
		TEXT("IMC_Exploration maps Left Ctrl to IA_Crouch"),
		HasExactMapping(MappingContext, CrouchAction, EKeys::LeftControl));
	TestTrue(
		TEXT("IMC_Exploration maps Left Shift to IA_Roll"),
		HasExactMapping(MappingContext, RollAction, EKeys::LeftShift));

	if (!AutomationOpenMap(MapPackagePath))
	{
		AddError(FString::Printf(TEXT("Could not open automation map %s."), MapPackagePath));
		return false;
	}

	TSharedRef<FRunState> RunState = MakeShared<FRunState>();
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForPIEReadyCommand(this, RunState));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForIdleLookAroundCommand(this, RunState));

	ADD_LATENT_AUTOMATION_COMMAND(FWaitForPartialSprintCommand(this, RunState));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForPartialSprintStopCommand(this, RunState));

	ADD_LATENT_AUTOMATION_COMMAND(FWaitForDiagonalSprintCommand(this, RunState));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForDirectionalLoopCommand(
		this, RunState, FVector2D(0.0, 1.0), ERequiemMovementDirection::Forward,
		FName(TEXT("Sprint_Loop")), TEXT("Forward"), false));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForDirectionalLoopCommand(
		this, RunState, FVector2D(-1.0, 1.0), ERequiemMovementDirection::ForwardLeft,
		FName(TEXT("Jog_Fwd_L_Loop")), TEXT("ForwardLeft"), false));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForDirectionalLoopCommand(
		this, RunState, FVector2D(-1.0, 0.0), ERequiemMovementDirection::Left,
		FName(TEXT("Jog_Left_Loop")), TEXT("Left"), false));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForDirectionalLoopCommand(
		this, RunState, FVector2D(-1.0, -1.0), ERequiemMovementDirection::BackwardLeft,
		FName(TEXT("Jog_Bwd_L_Loop")), TEXT("BackwardLeft"), false));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForDirectionalLoopCommand(
		this, RunState, FVector2D(0.0, -1.0), ERequiemMovementDirection::Backward,
		FName(TEXT("Jog_Bwd_Loop")), TEXT("Backward"), false));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForDirectionalLoopCommand(
		this, RunState, FVector2D(1.0, -1.0), ERequiemMovementDirection::BackwardRight,
		FName(TEXT("Jog_Bwd_R_Loop")), TEXT("BackwardRight"), false));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForDirectionalLoopCommand(
		this, RunState, FVector2D(1.0, 0.0), ERequiemMovementDirection::Right,
		FName(TEXT("Jog_Right_Loop")), TEXT("Right"), false));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForDirectionalLoopCommand(
		this, RunState, FVector2D(1.0, 1.0), ERequiemMovementDirection::ForwardRight,
		FName(TEXT("Jog_Fwd_R_Loop")), TEXT("ForwardRight"), false));
	// A 135-degree turn is not the special 180-degree reversal from the plan.
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForDirectionalLoopCommand(
		this, RunState, FVector2D(0.0, -1.0), ERequiemMovementDirection::Backward,
		FName(TEXT("Jog_Bwd_Loop")), TEXT("Backward 135-degree guard"), false));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForDirectionalLoopCommand(
		this, RunState, FVector2D(1.0, 1.0), ERequiemMovementDirection::ForwardRight,
		FName(TEXT("Jog_Fwd_R_Loop")), TEXT("ForwardRight 135-degree guard"), false));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForSprintReversalCommand(this, RunState));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForSprintStopCommand(this, RunState));

	ADD_LATENT_AUTOMATION_COMMAND(FObserveJumpCommand(this, RunState));

	ADD_LATENT_AUTOMATION_COMMAND(FWaitForCrouchHoldCommand(this, RunState));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForCrouchDiagonalCommand(this, RunState));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForDirectionalLoopCommand(
		this, RunState, FVector2D(0.0, 1.0), ERequiemMovementDirection::Forward,
		FName(TEXT("Crouch_Fwd_Loop")), TEXT("Forward"), true));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForDirectionalLoopCommand(
		this, RunState, FVector2D(-1.0, 1.0), ERequiemMovementDirection::ForwardLeft,
		FName(TEXT("Crouch_Fwd_L_Loop")), TEXT("ForwardLeft"), true));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForDirectionalLoopCommand(
		this, RunState, FVector2D(-1.0, 0.0), ERequiemMovementDirection::Left,
		FName(TEXT("Crouch_Left_Loop")), TEXT("Left"), true));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForDirectionalLoopCommand(
		this, RunState, FVector2D(-1.0, -1.0), ERequiemMovementDirection::BackwardLeft,
		FName(TEXT("Crouch_Bwd_L_Loop")), TEXT("BackwardLeft"), true));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForDirectionalLoopCommand(
		this, RunState, FVector2D(0.0, -1.0), ERequiemMovementDirection::Backward,
		FName(TEXT("Crouch_Bwd_Loop")), TEXT("Backward"), true));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForDirectionalLoopCommand(
		this, RunState, FVector2D(1.0, -1.0), ERequiemMovementDirection::BackwardRight,
		FName(TEXT("Crouch_Bwd_R_Loop")), TEXT("BackwardRight"), true));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForDirectionalLoopCommand(
		this, RunState, FVector2D(1.0, 0.0), ERequiemMovementDirection::Right,
		FName(TEXT("Crouch_Right_Loop")), TEXT("Right"), true));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForDirectionalLoopCommand(
		this, RunState, FVector2D(1.0, 1.0), ERequiemMovementDirection::ForwardRight,
		FName(TEXT("Crouch_Fwd_R_Loop")), TEXT("ForwardRight"), true));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForCrouchExitCommand(this, RunState));

	ADD_LATENT_AUTOMATION_COMMAND(FValidateReservedRollCommand(this, RunState));

	// Teardown is deliberately unconditional: every failed phase only marks the
	// shared value state, so these commands still release keys and end PIE.
	ADD_LATENT_AUTOMATION_COMMAND(FReleaseAllInputCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForPIEEndCommand(this));
	return true;
}

#endif // WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
