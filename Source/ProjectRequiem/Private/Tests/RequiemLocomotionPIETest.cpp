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
const FName JogState(TEXT("Jog"));
const FName JumpStartState(TEXT("Jump_Start"));
const FName JumpLoopState(TEXT("Jump_Loop"));
const FName JumpLandState(TEXT("Jump_Land"));
const FName CrouchEnterState(TEXT("Crouch_Enter"));
const FName CrouchLoopState(TEXT("Crouch_Loop"));
const FName CrouchExitState(TEXT("Crouch_Exit"));
const FName IdleAnimationName(TEXT("Idle_Loop"));
const FName IdleLookAroundAnimationName(TEXT("Idle_LookAround_Loop"));
const FName CrouchIdleAnimationName(TEXT("Crouch_Idle_Loop"));

struct FRunState
{
	bool bAborted = false;
	bool bHasPlayerStartTransform = false;
	FTransform PlayerStartTransform = FTransform::Identity;
	FRotator PlayerStartControlRotation = FRotator::ZeroRotator;
};

struct FCharacterSnapshot
{
	FVector Location = FVector::ZeroVector;
	FVector Acceleration = FVector::ZeroVector;
	float GroundSpeed = 0.0f;
	float MaximumSpeed = 0.0f;
	float MaximumAcceleration = 0.0f;
	float WalkingBrakingDeceleration = 0.0f;
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
	OutSnapshot.Acceleration = MovementComponent->GetCurrentAcceleration();
	OutSnapshot.GroundSpeed = Character->GetVelocity().Size2D();
	OutSnapshot.MaximumSpeed = MovementComponent->GetMaxSpeed();
	OutSnapshot.MaximumAcceleration = MovementComponent->GetMaxAcceleration();
	OutSnapshot.WalkingBrakingDeceleration = MovementComponent->BrakingDecelerationWalking;
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
		APlayerController* PlayerController = FindPIEPlayerController();
		ARequiemCharacter* Character = PlayerController
			? Cast<ARequiemCharacter>(PlayerController->GetPawn())
			: nullptr;
		FCharacterSnapshot Snapshot;
		if (World && Character && ReadCharacterSnapshot(Snapshot))
		{
			if (!World->GetMapName().Contains(TEXT("L_Dev_Foundation")))
			{
				return AbortWithError(FString::Printf(
					TEXT("PIE opened the wrong map: %s"),
					*World->GetMapName()));
			}

			RunState->PlayerStartTransform = Character->GetActorTransform();
			RunState->PlayerStartControlRotation = PlayerController->GetControlRotation();
			RunState->bHasPlayerStartTransform = true;
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

class FWaitForDiagonalJogCommand final : public FTimedCommand
{
public:
	FWaitForDiagonalJogCommand(FAutomationTestBase* InTest, TSharedRef<FRunState> InRunState)
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
			return AbortIfTimedOut(ElapsedSeconds, TEXT("injecting IA_Move for the jog stage"));
		}

		FCharacterSnapshot Snapshot;
		if (!ReadCharacterSnapshot(Snapshot))
		{
			return AbortIfTimedOut(ElapsedSeconds, TEXT("reading the jog player snapshot"));
		}

		if (!bHasStartLocation)
		{
			StartLocation = Snapshot.Location;
			bHasStartLocation = true;
		}

		if (Snapshot.GroundSpeed >= 20.0f && Snapshot.LocomotionState != JogState)
		{
			return AbortWithError(FString::Printf(
				TEXT("Movement reached %.1f cm/s without transitioning directly to Jog (state=%s)."),
				Snapshot.GroundSpeed,
				*Snapshot.LocomotionState.ToString()));
		}

		bSawJogPlayback |= Snapshot.LocomotionState == JogState
			&& HasExpectedLocomotionPlayback(Snapshot, FName(TEXT("Jog_Fwd_R_Loop")));
		bSawForwardRight |=
			Snapshot.MovementDirection == ERequiemMovementDirection::ForwardRight;
		bSawMeaningfulDisplacement |=
			(Snapshot.Location - StartLocation).Size2D() >= 100.0f;
		bSawAccelerationSample |= Snapshot.MaximumSpeed > 0.0f
			&& Snapshot.GroundSpeed >= 20.0f
			&& Snapshot.GroundSpeed < Snapshot.MaximumSpeed * 0.85f;
		if (FirstMovementSeconds < 0.0 && Snapshot.GroundSpeed >= 20.0f)
		{
			FirstMovementSeconds = ElapsedSeconds;
		}

		const bool bHasConfiguredJogSpeed = FMath::IsNearlyEqual(Snapshot.MaximumSpeed, 500.0f, 1.0f);
		const bool bHasConfiguredAcceleration =
			FMath::IsNearlyEqual(Snapshot.MaximumAcceleration, 2000.0f, 5.0f);
		const bool bHasConfiguredBraking =
			FMath::IsNearlyEqual(Snapshot.WalkingBrakingDeceleration, 2000.0f, 5.0f);
		const bool bAtExpectedSpeed = bHasConfiguredJogSpeed
			&& Snapshot.GroundSpeed >= Snapshot.MaximumSpeed * 0.95f;
		if (ReachedMaximumSpeedSeconds < 0.0 && bAtExpectedSpeed)
		{
			ReachedMaximumSpeedSeconds = ElapsedSeconds;
		}
		const bool bUsedShortNonInstantRamp = FirstMovementSeconds >= 0.0
			&& ReachedMaximumSpeedSeconds > FirstMovementSeconds
			&& ReachedMaximumSpeedSeconds - FirstMovementSeconds <= 1.0;
		const bool bExpectedPlayRate = FMath::IsNearlyEqual(
			Snapshot.LocomotionMontagePlayRate,
			FMath::Clamp(Snapshot.GroundSpeed / 500.0f, 0.35f, 1.2f),
			0.08f);
		if (bSawJogPlayback
			&& bSawForwardRight
			&& bSawMeaningfulDisplacement
			&& bSawAccelerationSample
			&& bHasConfiguredAcceleration
			&& bHasConfiguredBraking
			&& bUsedShortNonInstantRamp
			&& bAtExpectedSpeed
			&& bExpectedPlayRate
			&& Snapshot.LocomotionState == JogState
			&& Snapshot.MovementDirection == ERequiemMovementDirection::ForwardRight
			&& HasValidLocomotionPlayback(Snapshot))
		{
			Test->AddInfo(TEXT("W+D transitioned directly to Jog, accelerated under CharacterMovement, and reached 500 cm/s."));
			return true;
		}

		return AbortIfTimedOut(
			ElapsedSeconds,
			FString::Printf(
				TEXT("W+D jog (state=%s, direction=%d, speed=%.1f, max=%.1f, acceleration=%.1f, braking=%.1f, ramp=%.2fs, sampled=%d, playback=%d)"),
				*Snapshot.LocomotionState.ToString(),
				static_cast<int32>(Snapshot.MovementDirection),
				Snapshot.GroundSpeed,
				Snapshot.MaximumSpeed,
				Snapshot.MaximumAcceleration,
				Snapshot.WalkingBrakingDeceleration,
				FirstMovementSeconds >= 0.0 && ReachedMaximumSpeedSeconds >= 0.0
					? ReachedMaximumSpeedSeconds - FirstMovementSeconds
					: -1.0,
				bSawAccelerationSample,
				bSawJogPlayback));
	}

private:
	FVector StartLocation = FVector::ZeroVector;
	bool bHasStartLocation = false;
	bool bSawJogPlayback = false;
	bool bSawForwardRight = false;
	bool bSawMeaningfulDisplacement = false;
	bool bSawAccelerationSample = false;
	double FirstMovementSeconds = -1.0;
	double ReachedMaximumSpeedSeconds = -1.0;
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
		// small dev floor; the dedicated jog stage covers maximum speed.
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

		if (bCrouched && !Snapshot.bIsCrouched)
		{
			return AbortWithError(FString::Printf(
				TEXT("Ctrl hold was lost while validating crouched %s movement."),
				*DirectionLabel));
		}

		const FName ExpectedState = bCrouched ? CrouchLoopState : JogState;
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

class FWaitForJogStopCommand final : public FTimedCommand
{
public:
	FWaitForJogStopCommand(FAutomationTestBase* InTest, TSharedRef<FRunState> InRunState)
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
		if (!bReleasedMovement
			&& !SetContinuousAction(MoveActionPath, FInputActionValue(FVector2D(0.0, 1.0))))
		{
			return AbortIfTimedOut(ElapsedSeconds, TEXT("accelerating before the jog-stop check"));
		}
		if (bReleasedMovement)
		{
			StopContinuousAction(MoveActionPath);
		}

		FCharacterSnapshot Snapshot;
		if (!ReadCharacterSnapshot(Snapshot))
		{
			return AbortIfTimedOut(ElapsedSeconds, TEXT("reading the jog-stop snapshot"));
		}

		if (!bReleasedMovement)
		{
			if (Snapshot.LocomotionState == JogState
				&& Snapshot.GroundSpeed >= Snapshot.MaximumSpeed * 0.95f)
			{
				InitialSpeed = Snapshot.GroundSpeed;
				bReleasedMovement = true;
				StopContinuousAction(MoveActionPath);
			}

			return AbortIfTimedOut(
				ElapsedSeconds,
				FString::Printf(
					TEXT("reaching full Jog speed before release (state=%s, speed=%.1f, max=%.1f)"),
					*Snapshot.LocomotionState.ToString(),
					Snapshot.GroundSpeed,
					Snapshot.MaximumSpeed));
		}

		bSawIdlePlayback |= Snapshot.LocomotionState == IdleState
			&& HasExpectedLocomotionPlayback(Snapshot, IdleAnimationName);
		bSawJogDuringDeceleration |= Snapshot.LocomotionState == JogState
			&& Snapshot.GroundSpeed >= 20.0f
			&& Snapshot.GroundSpeed <= FMath::Max(20.0f, InitialSpeed - 50.0f);
		bSawDeceleration |= Snapshot.GroundSpeed <= FMath::Max(0.0f, InitialSpeed - 100.0f);
		if (bSawIdlePlayback
			&& bSawDeceleration
			&& Snapshot.LocomotionState == IdleState
			&& Snapshot.GroundSpeed <= 10.0f)
		{
			Test->AddInfo(FString::Printf(
				TEXT("Releasing movement returned to Idle under CharacterMovement braking (intermediate Jog sample=%d)."),
				bSawJogDuringDeceleration));
			return true;
		}

		return AbortIfTimedOut(
			ElapsedSeconds,
			FString::Printf(
				TEXT("jog release (state=%s, speed=%.1f, jogDuringBraking=%d, decelerated=%d)"),
				*Snapshot.LocomotionState.ToString(),
				Snapshot.GroundSpeed,
				bSawJogDuringDeceleration,
				bSawDeceleration));
	}

private:
	float InitialSpeed = 0.0f;
	bool bReleasedMovement = false;
	bool bSawIdlePlayback = false;
	bool bSawJogDuringDeceleration = false;
	bool bSawDeceleration = false;
};

class FObserveStandingJumpCommand final : public FTimedCommand
{
public:
	FObserveStandingJumpCommand(FAutomationTestBase* InTest, TSharedRef<FRunState> InRunState)
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
			Test->AddInfo(TEXT("A stationary jump traversed Jump_Start, Jump_Loop, and Jump_Land."));
			return true;
		}

		return AbortIfTimedOut(
			ElapsedSeconds,
			FString::Printf(
				TEXT("stationary jump sequence (state=%s, start=%d, loop=%d, land=%d)"),
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

class FInterruptJumpLandWithMovementCommand final : public FTimedCommand
{
public:
	FInterruptJumpLandWithMovementCommand(
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
		if (bMovementStarted && !bInterruptedLand
			&& !SetContinuousAction(MoveActionPath, FInputActionValue(FVector2D(0.0, 1.0))))
		{
			return AbortIfTimedOut(ElapsedSeconds, TEXT("injecting movement during Jump_Land"));
		}
		if (bInterruptedLand)
		{
			StopContinuousAction(MoveActionPath);
		}

		FCharacterSnapshot Snapshot;
		if (!ReadCharacterSnapshot(Snapshot))
		{
			return AbortIfTimedOut(ElapsedSeconds, TEXT("reading the Jump_Land interruption snapshot"));
		}

		if (!bMovementStarted)
		{
			if (ElapsedSeconds < 0.12)
			{
				if (!SetContinuousAction(JumpActionPath, FInputActionValue(true)))
				{
					return AbortIfTimedOut(ElapsedSeconds, TEXT("injecting IA_Jump before the interruption check"));
				}
			}
			else
			{
				StopContinuousAction(JumpActionPath);
			}

			if (Snapshot.LocomotionState == JumpLandState
				&& HasExpectedLocomotionPlayback(Snapshot, JumpLandState))
			{
				if (Snapshot.GroundSpeed > 10.0f)
				{
					return AbortWithError(FString::Printf(
						TEXT("Jump_Land interruption setup was not stationary (speed=%.1f)."),
						Snapshot.GroundSpeed));
				}

				if (!SetContinuousAction(MoveActionPath, FInputActionValue(FVector2D(0.0, 1.0))))
				{
					return AbortIfTimedOut(ElapsedSeconds, TEXT("starting movement while Jump_Land is active"));
				}
				bMovementStarted = true;
			}

			return AbortIfTimedOut(
				ElapsedSeconds,
				FString::Printf(
					TEXT("waiting for stationary Jump_Land playback (state=%s, speed=%.1f)"),
					*Snapshot.LocomotionState.ToString(),
					Snapshot.GroundSpeed));
		}

		if (Snapshot.GroundSpeed >= 20.0f && Snapshot.LocomotionState == JumpLandState)
		{
			return AbortWithError(FString::Printf(
				TEXT("Jump_Land kept playing after movement began (speed=%.1f)."),
				Snapshot.GroundSpeed));
		}

		if (!bInterruptedLand
			&& Snapshot.GroundSpeed >= 20.0f
			&& Snapshot.LocomotionState == JogState
			&& Snapshot.MovementDirection == ERequiemMovementDirection::Forward
			&& HasExpectedLocomotionPlayback(Snapshot, FName(TEXT("Jog_Fwd_Loop"))))
		{
			bInterruptedLand = true;
			StopContinuousAction(MoveActionPath);
		}

		if (bInterruptedLand
			&& Snapshot.LocomotionState == IdleState
			&& Snapshot.GroundSpeed <= 10.0f
			&& HasExpectedLocomotionPlayback(Snapshot, IdleAnimationName))
		{
			Test->AddInfo(TEXT("Movement interrupted an active stationary Jump_Land and switched directly to Jog_Fwd_Loop."));
			return true;
		}

		return AbortIfTimedOut(
			ElapsedSeconds,
			FString::Printf(
				TEXT("interrupting Jump_Land (state=%s, speed=%.1f, movementStarted=%d, interrupted=%d)"),
				*Snapshot.LocomotionState.ToString(),
				Snapshot.GroundSpeed,
				bMovementStarted,
				bInterruptedLand));
	}

private:
	bool bMovementStarted = false;
	bool bInterruptedLand = false;
};

class FObserveMovingJumpCommand final : public FTimedCommand
{
public:
	FObserveMovingJumpCommand(FAutomationTestBase* InTest, TSharedRef<FRunState> InRunState)
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
		if (!bReturnedToJog
			&& !SetContinuousAction(MoveActionPath, FInputActionValue(FVector2D(0.0, 1.0))))
		{
			return AbortIfTimedOut(ElapsedSeconds, TEXT("injecting forward movement for the moving jump"));
		}
		if (bReturnedToJog)
		{
			StopContinuousAction(MoveActionPath);
		}

		FCharacterSnapshot Snapshot;
		if (!ReadCharacterSnapshot(Snapshot))
		{
			return AbortIfTimedOut(ElapsedSeconds, TEXT("reading the moving-jump snapshot"));
		}

		if (!bJumpStarted)
		{
			if (Snapshot.LocomotionState == JogState
				&& Snapshot.GroundSpeed >= Snapshot.MaximumSpeed * 0.75f)
			{
				if (!SetContinuousAction(JumpActionPath, FInputActionValue(true)))
				{
					return AbortIfTimedOut(ElapsedSeconds, TEXT("injecting IA_Jump while jogging"));
				}
				bJumpStarted = true;
				JumpStartSeconds = ElapsedSeconds;
			}

			return AbortIfTimedOut(
				ElapsedSeconds,
				FString::Printf(
					TEXT("waiting to begin moving jump (state=%s, speed=%.1f)"),
					*Snapshot.LocomotionState.ToString(),
					Snapshot.GroundSpeed));
		}

		if (ElapsedSeconds - JumpStartSeconds < 0.12)
		{
			if (!SetContinuousAction(JumpActionPath, FInputActionValue(true)))
			{
				return AbortIfTimedOut(ElapsedSeconds, TEXT("holding IA_Jump during moving jump"));
			}
		}
		else
		{
			StopContinuousAction(JumpActionPath);
		}

		if (Snapshot.LocomotionState == JumpLandState)
		{
			return AbortWithError(TEXT("A moving jump incorrectly entered Jump_Land instead of returning directly to Jog."));
		}

		bSawJumpStartPlayback |= Snapshot.LocomotionState == JumpStartState
			&& HasExpectedLocomotionPlayback(Snapshot, JumpStartState);
		bSawJumpLoopPlayback |= Snapshot.LocomotionState == JumpLoopState
			&& HasExpectedLocomotionPlayback(Snapshot, JumpLoopState);
		bSawAirborne |= Snapshot.bIsFalling;

		if (bSawAirborne
			&& !Snapshot.bIsFalling
			&& Snapshot.LocomotionState == JogState
			&& Snapshot.MovementDirection == ERequiemMovementDirection::Forward
			&& HasExpectedLocomotionPlayback(Snapshot, FName(TEXT("Jog_Fwd_Loop"))))
		{
			bReturnedToJog = true;
			StopContinuousAction(MoveActionPath);
		}

		const bool bSettledAfterProof = bReturnedToJog
			&& Snapshot.LocomotionState == IdleState
			&& Snapshot.GroundSpeed <= 10.0f
			&& HasExpectedLocomotionPlayback(Snapshot, IdleAnimationName);
		if (bSawJumpStartPlayback
			&& bSawJumpLoopPlayback
			&& bSettledAfterProof)
		{
			Test->AddInfo(TEXT("A moving jump skipped Jump_Land and returned directly to Jog_Fwd_Loop."));
			return true;
		}

		return AbortIfTimedOut(
			ElapsedSeconds,
			FString::Printf(
				TEXT("moving jump (state=%s, falling=%d, start=%d, loop=%d, returnedJog=%d)"),
				*Snapshot.LocomotionState.ToString(),
				Snapshot.bIsFalling,
				bSawJumpStartPlayback,
				bSawJumpLoopPlayback,
				bReturnedToJog));
	}

private:
	double JumpStartSeconds = -1.0;
	bool bJumpStarted = false;
	bool bSawJumpStartPlayback = false;
	bool bSawJumpLoopPlayback = false;
	bool bSawAirborne = false;
	bool bReturnedToJog = false;
};

class FResetCharacterToStartCommand final : public FTimedCommand
{
public:
	FResetCharacterToStartCommand(FAutomationTestBase* InTest, TSharedRef<FRunState> InRunState)
		: FTimedCommand(InTest, MoveTemp(InRunState), 3.0)
	{
	}

	virtual bool Update() override
	{
		if (ShouldSkip())
		{
			return true;
		}

		const double ElapsedSeconds = GetElapsedSeconds();
		APlayerController* PlayerController = FindPIEPlayerController();
		ARequiemCharacter* Character = PlayerController
			? Cast<ARequiemCharacter>(PlayerController->GetPawn())
			: nullptr;
		UCharacterMovementComponent* MovementComponent = Character
			? Character->GetCharacterMovement()
			: nullptr;
		if (!RunState->bHasPlayerStartTransform || !PlayerController || !Character || !MovementComponent)
		{
			return AbortIfTimedOut(ElapsedSeconds, TEXT("recovering the PIE start transform"));
		}

		ReleaseAllTestInput();
		if (!bTeleported)
		{
			MovementComponent->StopMovementImmediately();
			Character->SetActorLocationAndRotation(
				RunState->PlayerStartTransform.GetLocation(),
				RunState->PlayerStartTransform.Rotator(),
				false,
				nullptr,
				ETeleportType::TeleportPhysics);
			PlayerController->SetControlRotation(RunState->PlayerStartControlRotation);
			bTeleported = true;
			return false;
		}

		FCharacterSnapshot Snapshot;
		if (!ReadCharacterSnapshot(Snapshot))
		{
			return AbortIfTimedOut(ElapsedSeconds, TEXT("reading the recentered player snapshot"));
		}

		const bool bAtStart = Snapshot.Location.Equals(
			RunState->PlayerStartTransform.GetLocation(),
			1.0f);
		if (bAtStart
			&& Snapshot.GroundSpeed <= 10.0f
			&& Snapshot.LocomotionState == IdleState
			&& HasExpectedLocomotionPlayback(Snapshot, IdleAnimationName))
		{
			Test->AddInfo(TEXT("The player was recentered before the crouch traversal block."));
			return true;
		}

		return AbortIfTimedOut(
			ElapsedSeconds,
			FString::Printf(
				TEXT("recentering before crouch (state=%s, speed=%.1f, distance=%.1f)"),
				*Snapshot.LocomotionState.ToString(),
				Snapshot.GroundSpeed,
				FVector::Dist(Snapshot.Location, RunState->PlayerStartTransform.GetLocation())));
	}

private:
	bool bTeleported = false;
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

class FValidateResponsiveCrouchTransitionsCommand final : public FTimedCommand
{
public:
	FValidateResponsiveCrouchTransitionsCommand(
		FAutomationTestBase* InTest,
		TSharedRef<FRunState> InRunState)
		: FTimedCommand(InTest, MoveTemp(InRunState), 5.0)
	{
	}

	virtual bool Update() override
	{
		if (ShouldSkip())
		{
			StopContinuousAction(MoveActionPath);
			StopContinuousAction(CrouchActionPath);
			return true;
		}

		const double ElapsedSeconds = GetElapsedSeconds();
		if (!SetContinuousAction(MoveActionPath, FInputActionValue(FVector2D(0.0, 1.0))))
		{
			return AbortIfTimedOut(ElapsedSeconds, TEXT("injecting movement for responsive crouch transitions"));
		}
		if (!bCrouchReleased
			&& !SetContinuousAction(CrouchActionPath, FInputActionValue(true)))
		{
			return AbortIfTimedOut(ElapsedSeconds, TEXT("holding Ctrl for responsive Crouch_Enter"));
		}
		if (bCrouchReleased)
		{
			StopContinuousAction(CrouchActionPath);
		}

		FCharacterSnapshot Snapshot;
		if (!ReadCharacterSnapshot(Snapshot))
		{
			return AbortIfTimedOut(ElapsedSeconds, TEXT("reading responsive crouch transitions"));
		}

		const bool bHasMovement = !Snapshot.Acceleration.IsNearlyZero(1.0f)
			|| Snapshot.GroundSpeed >= 20.0f;
		if (!bCrouchReleased)
		{
			if (Snapshot.LocomotionState == CrouchEnterState)
			{
				bSawCrouchEnter = true;
				if (bHasMovement)
				{
					++CrouchEnterSamplesWithMovement;
				}
				if (!HasExpectedLocomotionPlayback(Snapshot, CrouchEnterState))
				{
					return AbortWithError(TEXT("Responsive Crouch_Enter did not play on DefaultSlot."));
				}
				if (CrouchEnterSamplesWithMovement > 2)
				{
					return AbortWithError(TEXT("Crouch_Enter kept sliding for more than two updates after movement input."));
				}
			}

			if (bSawCrouchEnter
				&& bHasMovement
				&& Snapshot.bIsCrouched
				&& Snapshot.LocomotionState == CrouchLoopState
				&& Snapshot.MovementDirection == ERequiemMovementDirection::Forward
				&& HasExpectedLocomotionPlayback(Snapshot, FName(TEXT("Crouch_Fwd_Loop"))))
			{
				bCrouchReleased = true;
				StopContinuousAction(CrouchActionPath);
			}
			return AbortIfTimedOut(
				ElapsedSeconds,
				FString::Printf(
					TEXT("responsive Crouch_Enter (state=%s, crouched=%d, speed=%.1f, samples=%d)"),
					*Snapshot.LocomotionState.ToString(),
					Snapshot.bIsCrouched,
					Snapshot.GroundSpeed,
					CrouchEnterSamplesWithMovement));
		}

		if (Snapshot.LocomotionState == CrouchExitState)
		{
			bSawCrouchExit = true;
			if (bHasMovement)
			{
				++CrouchExitSamplesWithMovement;
			}
			if (!HasExpectedLocomotionPlayback(Snapshot, CrouchExitState))
			{
				return AbortWithError(TEXT("Responsive Crouch_Exit did not play on DefaultSlot."));
			}
			if (CrouchExitSamplesWithMovement > 2)
			{
				return AbortWithError(TEXT("Crouch_Exit kept sliding for more than two updates after movement input."));
			}
		}

		if (bSawCrouchExit
			&& bHasMovement
			&& !Snapshot.bIsCrouched
			&& Snapshot.LocomotionState == JogState
			&& Snapshot.MovementDirection == ERequiemMovementDirection::Forward
			&& HasExpectedLocomotionPlayback(Snapshot, FName(TEXT("Jog_Fwd_Loop"))))
		{
			StopContinuousAction(MoveActionPath);
			Test->AddInfo(TEXT("Moving Crouch_Enter handed off to directional crouch and moving Crouch_Exit handed off to Jog within two updates."));
			return true;
		}

		return AbortIfTimedOut(
			ElapsedSeconds,
			FString::Printf(
				TEXT("responsive Crouch_Exit (state=%s, crouched=%d, speed=%.1f, samples=%d)"),
				*Snapshot.LocomotionState.ToString(),
				Snapshot.bIsCrouched,
				Snapshot.GroundSpeed,
				CrouchExitSamplesWithMovement));
	}

private:
	int32 CrouchEnterSamplesWithMovement = 0;
	int32 CrouchExitSamplesWithMovement = 0;
	bool bSawCrouchEnter = false;
	bool bSawCrouchExit = false;
	bool bCrouchReleased = false;
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

	ADD_LATENT_AUTOMATION_COMMAND(FWaitForDiagonalJogCommand(this, RunState));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForDirectionalLoopCommand(
		this, RunState, FVector2D(0.0, 1.0), ERequiemMovementDirection::Forward,
		FName(TEXT("Jog_Fwd_Loop")), TEXT("Forward"), false));
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
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForJogStopCommand(this, RunState));

	ADD_LATENT_AUTOMATION_COMMAND(FObserveStandingJumpCommand(this, RunState));
	ADD_LATENT_AUTOMATION_COMMAND(FInterruptJumpLandWithMovementCommand(this, RunState));
	ADD_LATENT_AUTOMATION_COMMAND(FObserveMovingJumpCommand(this, RunState));

	ADD_LATENT_AUTOMATION_COMMAND(FResetCharacterToStartCommand(this, RunState));
	ADD_LATENT_AUTOMATION_COMMAND(FValidateResponsiveCrouchTransitionsCommand(this, RunState));
	ADD_LATENT_AUTOMATION_COMMAND(FResetCharacterToStartCommand(this, RunState));
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

	// Teardown is deliberately unconditional: every failed phase only marks the
	// shared value state, so these commands still release keys and end PIE.
	ADD_LATENT_AUTOMATION_COMMAND(FReleaseAllInputCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForPIEEndCommand(this));
	return true;
}

#endif // WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
