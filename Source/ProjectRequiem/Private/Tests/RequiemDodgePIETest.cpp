// Copyright Project Requiem. All Rights Reserved.

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Animation/RequiemPlayerAnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Characters/RequiemCharacter.h"
#include "Components/RequiemCombatComponent.h"
#include "Components/RequiemDodgeComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EnhancedActionKeyMapping.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "Math/RotationMatrix.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "UObject/UnrealType.h"

namespace RequiemDodgePIETest
{
constexpr TCHAR MapPackagePath[] = TEXT("/Game/ProjectRequiem/World/Maps/Dev/L_Dev_Foundation");
constexpr TCHAR MappingContextPath[] =
	TEXT("/Game/ProjectRequiem/Core/Input/IMC_Exploration.IMC_Exploration");
constexpr TCHAR MoveActionPath[] =
	TEXT("/Game/ProjectRequiem/Core/Input/Actions/IA_Move.IA_Move");
constexpr TCHAR RollActionPath[] =
	TEXT("/Game/ProjectRequiem/Core/Input/Actions/IA_Roll.IA_Roll");
constexpr TCHAR PrimaryAttackActionPath[] =
	TEXT("/Game/ProjectRequiem/Core/Input/Actions/IA_PrimaryAttack.IA_PrimaryAttack");
constexpr TCHAR ToggleCombatActionPath[] =
	TEXT("/Game/ProjectRequiem/Core/Input/Actions/IA_ToggleCombat.IA_ToggleCombat");
constexpr TCHAR CrouchActionPath[] =
	TEXT("/Game/ProjectRequiem/Core/Input/Actions/IA_Crouch.IA_Crouch");
constexpr TCHAR JumpActionPath[] =
	TEXT("/Game/ProjectRequiem/Core/Input/Actions/IA_Jump.IA_Jump");
constexpr TCHAR CharacterClassPath[] =
	TEXT("/Game/ProjectRequiem/Characters/Player/Blueprints/BP_CH_Player.BP_CH_Player_C");
constexpr TCHAR AnimInstanceClassPath[] =
	TEXT("/Game/ProjectRequiem/Characters/Player/Animations/Locomotion/"
		 "ABP_PR_PlayerLocomotion.ABP_PR_PlayerLocomotion_C");
constexpr TCHAR PlayerSkeletonPath[] =
	TEXT("/Game/ProjectRequiem/Characters/Player/Meshes/Temporary/"
		 "SKEL_Temp_SuperheroFemale.SKEL_Temp_SuperheroFemale");

constexpr float ExpectedMovementRecoveryNormalized = 0.62f;
constexpr float DirectionDotTolerance = 0.90f;
constexpr float MinimumDodgeDisplacement = 25.0f;
constexpr float MinimumPostDodgeJogSpeed = 100.0f;
constexpr float MinimumPostDodgeDisplacement = 2.0f;
constexpr float NormalizedBoundaryTolerance = 0.025f;
constexpr float PresentationHandoffTolerance = 0.06f;
constexpr double MaximumPostDodgeJogResumeSeconds = 0.15;

const FName DirectionalJogAnimationNames[] = {
	FName(TEXT("Jog_Fwd_L_Loop")),
	FName(TEXT("Jog_Fwd_Loop")),
	FName(TEXT("Jog_Fwd_R_Loop")),
	FName(TEXT("Jog_Bwd_L_Loop")),
	FName(TEXT("Jog_Bwd_Loop")),
	FName(TEXT("Jog_Bwd_R_Loop")),
	FName(TEXT("Jog_Left_Loop")),
	FName(TEXT("Jog_Right_Loop")),
};

struct FRunState
{
	bool bAborted = false;
	bool bHasPlayerStartTransform = false;
	FTransform PlayerStartTransform = FTransform::Identity;
	FRotator PlayerStartControlRotation = FRotator::ZeroRotator;
	FName RollAnimationName = NAME_None;
};

struct FCharacterSnapshot
{
	FVector Location = FVector::ZeroVector;
	FVector Velocity = FVector::ZeroVector;
	FVector Acceleration = FVector::ZeroVector;
	FVector PendingMovementInput = FVector::ZeroVector;
	FVector LastMovementInput = FVector::ZeroVector;
	FVector ActorForward = FVector::ForwardVector;
	float GroundSpeed = 0.0f;
	bool bIsCrouched = false;
	bool bIsFalling = false;

	ERequiemCombatState CombatState = ERequiemCombatState::Normal;
	int32 AttackRequestSerial = 0;
	bool bUnarmedAttackActive = false;
	bool bQueuedUnarmedFollowUp = false;
	bool bUnarmedAttackMovementLocked = false;

	bool bDodgeActive = false;
	bool bDodgeInvulnerable = false;
	bool bDodgeMovementLocked = false;
	bool bDodgeRestrictedActionsLocked = false;
	FVector DodgeDirection = FVector::ZeroVector;
	float DodgeNormalizedTime = 0.0f;
	int32 DodgeRequestSerial = 0;
	float IFrameStartNormalized = 0.0f;
	float IFrameEndNormalized = 0.0f;
	float MovementRecoveryNormalized = 0.0f;

	ERequiemLocomotionState LocomotionState = ERequiemLocomotionState::Idle;
	ERequiemCombatAnimationState CombatAnimationState = ERequiemCombatAnimationState::Inactive;
	int32 ActiveComboAnimationIndex = INDEX_NONE;
	float CombatNormalizedTime = 0.0f;
	FName ActivePresentationAnimationName = NAME_None;
	bool bActivePresentationIsSequence = false;
	bool bPresentationMontageIsPlaying = false;
	FName PresentationMontageSlot = NAME_None;
	float PresentationMontagePlayRate = 0.0f;
	float ActivePresentationPlayRate = 0.0f;
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

ARequiemCharacter* FindPIECharacter()
{
	APlayerController* PlayerController = FindPIEPlayerController();
	return PlayerController ? Cast<ARequiemCharacter>(PlayerController->GetPawn()) : nullptr;
}

URequiemCombatComponent* FindPIECombatComponent()
{
	ARequiemCharacter* Character = FindPIECharacter();
	return Character ? Character->GetCombatComponent() : nullptr;
}

bool ReadCharacterSnapshot(FCharacterSnapshot& OutSnapshot)
{
	ARequiemCharacter* Character = FindPIECharacter();
	UCharacterMovementComponent* MovementComponent = Character
		? Character->GetCharacterMovement()
		: nullptr;
	URequiemCombatComponent* CombatComponent = Character
		? Character->GetCombatComponent()
		: nullptr;
	URequiemDodgeComponent* DodgeComponent = Character
		? Character->GetDodgeComponent()
		: nullptr;
	URequiemPlayerAnimInstance* AnimInstance = Character && Character->GetMesh()
		? Cast<URequiemPlayerAnimInstance>(Character->GetMesh()->GetAnimInstance())
		: nullptr;
	if (!Character || !MovementComponent || !CombatComponent || !DodgeComponent || !AnimInstance)
	{
		return false;
	}

	OutSnapshot.Location = Character->GetActorLocation();
	OutSnapshot.Velocity = Character->GetVelocity();
	OutSnapshot.Acceleration = MovementComponent->GetCurrentAcceleration();
	OutSnapshot.PendingMovementInput = Character->GetPendingMovementInputVector();
	OutSnapshot.LastMovementInput = Character->GetLastMovementInputVector();
	OutSnapshot.ActorForward = Character->GetActorForwardVector().GetSafeNormal2D();
	OutSnapshot.GroundSpeed = Character->GetVelocity().Size2D();
	OutSnapshot.bIsCrouched = Character->IsCrouched();
	OutSnapshot.bIsFalling = MovementComponent->IsFalling();

	OutSnapshot.CombatState = CombatComponent->GetCombatState();
	OutSnapshot.AttackRequestSerial = CombatComponent->GetAttackRequestSerial();
	OutSnapshot.bUnarmedAttackActive = CombatComponent->IsUnarmedAttackActive();
	OutSnapshot.bQueuedUnarmedFollowUp = CombatComponent->HasQueuedUnarmedFollowUp();
	OutSnapshot.bUnarmedAttackMovementLocked =
		CombatComponent->IsUnarmedAttackMovementLocked();

	OutSnapshot.bDodgeActive = DodgeComponent->IsDodgeActive();
	OutSnapshot.bDodgeInvulnerable = DodgeComponent->IsDodgeInvulnerable();
	OutSnapshot.bDodgeMovementLocked = DodgeComponent->IsDodgeMovementLocked();
	OutSnapshot.bDodgeRestrictedActionsLocked =
		DodgeComponent->AreDodgeRestrictedActionsLocked();
	OutSnapshot.DodgeDirection = DodgeComponent->GetDodgeDirection();
	OutSnapshot.DodgeNormalizedTime = DodgeComponent->GetDodgeNormalizedTime();
	OutSnapshot.DodgeRequestSerial = DodgeComponent->GetDodgeRequestSerial();
	OutSnapshot.IFrameStartNormalized = DodgeComponent->GetIFrameStartNormalized();
	OutSnapshot.IFrameEndNormalized = DodgeComponent->GetIFrameEndNormalized();
	OutSnapshot.MovementRecoveryNormalized =
		DodgeComponent->GetMovementControlRecoveryNormalized();

	OutSnapshot.LocomotionState = AnimInstance->GetLocomotionState();
	OutSnapshot.CombatAnimationState = AnimInstance->GetCombatAnimationState();
	OutSnapshot.ActiveComboAnimationIndex = AnimInstance->GetActiveComboAnimationIndex();
	OutSnapshot.CombatNormalizedTime = AnimInstance->GetCombatAnimationNormalizedTime();
	const UAnimSequenceBase* ActiveAnimation = AnimInstance->GetActivePresentationAnimation();
	const UAnimMontage* ActiveMontage = AnimInstance->GetCurrentActiveMontage();
	OutSnapshot.ActivePresentationAnimationName =
		ActiveAnimation ? ActiveAnimation->GetFName() : NAME_None;
	OutSnapshot.bActivePresentationIsSequence = Cast<UAnimSequence>(ActiveAnimation) != nullptr;
	OutSnapshot.bPresentationMontageIsPlaying =
		ActiveMontage && AnimInstance->Montage_IsPlaying(ActiveMontage);
	OutSnapshot.PresentationMontageSlot =
		ActiveMontage && !ActiveMontage->SlotAnimTracks.IsEmpty()
			? ActiveMontage->SlotAnimTracks[0].SlotName
			: NAME_None;
	OutSnapshot.PresentationMontagePlayRate = ActiveMontage
		? AnimInstance->Montage_GetPlayRate(ActiveMontage)
		: 0.0f;
	OutSnapshot.ActivePresentationPlayRate = AnimInstance->GetActivePresentationPlayRate();
	return true;
}

bool HasValidDodgePlayback(const FCharacterSnapshot& Snapshot, const FRunState& RunState)
{
	return Snapshot.LocomotionState == ERequiemLocomotionState::Dodge
		&& Snapshot.ActivePresentationAnimationName == RunState.RollAnimationName
		&& Snapshot.bActivePresentationIsSequence
		&& Snapshot.bPresentationMontageIsPlaying
		&& Snapshot.PresentationMontageSlot == FName(TEXT("DefaultSlot"))
		&& Snapshot.PresentationMontagePlayRate > UE_KINDA_SMALL_NUMBER
		&& Snapshot.ActivePresentationPlayRate > UE_KINDA_SMALL_NUMBER;
}

bool HasValidDirectionalJogPlayback(const FCharacterSnapshot& Snapshot)
{
	bool bUsesDirectionalJogAnimation = false;
	for (const FName AnimationName : DirectionalJogAnimationNames)
	{
		if (Snapshot.ActivePresentationAnimationName == AnimationName)
		{
			bUsesDirectionalJogAnimation = true;
			break;
		}
	}

	return Snapshot.LocomotionState == ERequiemLocomotionState::Jog
		&& bUsesDirectionalJogAnimation
		&& Snapshot.bActivePresentationIsSequence
		&& Snapshot.bPresentationMontageIsPlaying
		&& Snapshot.PresentationMontageSlot == FName(TEXT("DefaultSlot"))
		&& Snapshot.PresentationMontagePlayRate > UE_KINDA_SMALL_NUMBER
		&& Snapshot.ActivePresentationPlayRate > UE_KINDA_SMALL_NUMBER;
}

UEnhancedInputLocalPlayerSubsystem* FindEnhancedInputSubsystem()
{
	APlayerController* PlayerController = FindPIEPlayerController();
	ULocalPlayer* LocalPlayer = PlayerController ? PlayerController->GetLocalPlayer() : nullptr;
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
	if (InputSubsystem && Action && InputSubsystem->HasContinuousInputInjectionForAction(Action))
	{
		InputSubsystem->StopContinuousInputInjectionForAction(Action);
	}
}

void ReleaseAllTestInput()
{
	StopContinuousAction(MoveActionPath);
	StopContinuousAction(RollActionPath);
	StopContinuousAction(PrimaryAttackActionPath);
	StopContinuousAction(ToggleCombatActionPath);
	StopContinuousAction(CrouchActionPath);
	StopContinuousAction(JumpActionPath);
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

FVector GetCameraRelativeDirection(
	const APlayerController* PlayerController,
	const FVector2D MovementInput,
	const FVector& NeutralDirection)
{
	if (MovementInput.IsNearlyZero())
	{
		return NeutralDirection.GetSafeNormal2D();
	}

	const FRotator ControlRotation = PlayerController
		? PlayerController->GetControlRotation()
		: FRotator::ZeroRotator;
	const FRotationMatrix YawMatrix(FRotator(0.0f, ControlRotation.Yaw, 0.0f));
	const FVector ForwardDirection = YawMatrix.GetUnitAxis(EAxis::X);
	const FVector RightDirection = YawMatrix.GetUnitAxis(EAxis::Y);
	return (ForwardDirection * MovementInput.Y + RightDirection * MovementInput.X)
		.GetSafeNormal2D();
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
			TEXT("Dodge PIE stage timed out after %.1fs: %s"),
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
		ARequiemCharacter* Character = FindPIECharacter();
		FCharacterSnapshot Snapshot;
		if (World && PlayerController && Character && ReadCharacterSnapshot(Snapshot))
		{
			if (!World->GetMapName().Contains(TEXT("L_Dev_Foundation")))
			{
				return AbortWithError(FString::Printf(
					TEXT("PIE opened the wrong map: %s"),
					*World->GetMapName()));
			}

			if (StableWorld.Get() != World || StableCharacter.Get() != Character)
			{
				StableWorld = World;
				StableCharacter = Character;
				StableSinceSeconds = ElapsedSeconds;
				return false;
			}
			if (ElapsedSeconds - StableSinceSeconds < 0.5)
			{
				return false;
			}

			const UEnhancedInputComponent* InputComponent =
				Character->FindComponentByClass<UEnhancedInputComponent>();
			const UInputAction* RollAction = LoadObject<UInputAction>(nullptr, RollActionPath);
			const int32 RollBindingCount = CountStartedBindings(InputComponent, RollAction);
			if (InputComponent && RollAction && RollBindingCount != 1)
			{
				return AbortWithError(FString::Printf(
					TEXT("Runtime IA_Roll Started binding count is %d; expected exactly one."),
					RollBindingCount));
			}

			if (!RunState->bHasPlayerStartTransform)
			{
				RunState->PlayerStartTransform = Character->GetActorTransform();
				RunState->PlayerStartControlRotation = PlayerController->GetControlRotation();
				RunState->bHasPlayerStartTransform = true;
			}

			const bool bReady =
				RunState->RollAnimationName != NAME_None
				&& RollBindingCount == 1
				&& Snapshot.CombatState == ERequiemCombatState::Normal
				&& !Snapshot.bDodgeActive
				&& !Snapshot.bDodgeInvulnerable
				&& !Snapshot.bDodgeMovementLocked
				&& !Snapshot.bDodgeRestrictedActionsLocked;
			if (bReady)
			{
				Test->AddInfo(TEXT("PIE started in Normal with one runtime Shift dodge binding."));
				return true;
			}
		}

		return AbortIfTimedOut(ElapsedSeconds, TEXT("waiting for the possessed dodge-ready player"));
	}

private:
	TWeakObjectPtr<UWorld> StableWorld;
	TWeakObjectPtr<ARequiemCharacter> StableCharacter;
	double StableSinceSeconds = -1.0;
};

class FResetCharacterToStartCommand final : public FTimedCommand
{
public:
	FResetCharacterToStartCommand(FAutomationTestBase* InTest, TSharedRef<FRunState> InRunState)
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
		APlayerController* PlayerController = FindPIEPlayerController();
		ARequiemCharacter* Character = FindPIECharacter();
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
			if (Character->IsCrouched())
			{
				Character->UnCrouch();
			}
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
			return AbortIfTimedOut(ElapsedSeconds, TEXT("reading the reset dodge snapshot"));
		}

		const bool bAtStart = Snapshot.Location.Equals(
			RunState->PlayerStartTransform.GetLocation(),
			1.0f);
		if (bAtStart
			&& Snapshot.GroundSpeed <= 1.0f
			&& Snapshot.CombatState == ERequiemCombatState::Normal
			&& Snapshot.CombatAnimationState == ERequiemCombatAnimationState::Inactive
			&& Snapshot.LocomotionState == ERequiemLocomotionState::Idle
			&& !Snapshot.bDodgeActive)
		{
			return true;
		}

		return AbortIfTimedOut(
			ElapsedSeconds,
			FString::Printf(
				TEXT("resetting dodge player (speed=%.1f, combat=%d, locomotion=%d, dodge=%d)"),
				Snapshot.GroundSpeed,
				static_cast<int32>(Snapshot.CombatState),
				static_cast<int32>(Snapshot.LocomotionState),
				Snapshot.bDodgeActive));
	}

private:
	bool bTeleported = false;
};

class FObserveDirectionalDodgeCommand final : public FTimedCommand
{
public:
	FObserveDirectionalDodgeCommand(
		FAutomationTestBase* InTest,
		TSharedRef<FRunState> InRunState,
		const FVector2D InMovementInput,
		FString InDirectionLabel,
		const bool bInRequirePhysicalContinuation)
		: FTimedCommand(InTest, MoveTemp(InRunState), 6.0)
		, MovementInput(InMovementInput.GetSafeNormal())
		, DirectionLabel(MoveTemp(InDirectionLabel))
		, bRequirePhysicalContinuation(bInRequirePhysicalContinuation)
	{
	}

	virtual bool Update() override
	{
		if (ShouldSkip())
		{
			ReleaseAllTestInput();
			return true;
		}

		const double ElapsedSeconds = GetElapsedSeconds();
		if (bRollInputInjected && !bRollInputReleased)
		{
			StopContinuousAction(RollActionPath);
			bRollInputReleased = true;
		}

		if (!MovementInput.IsNearlyZero()
			&& !SetContinuousAction(MoveActionPath, FInputActionValue(MovementInput * 0.55f)))
		{
			return AbortIfTimedOut(ElapsedSeconds, FString::Printf(
				TEXT("injecting %s movement before dodge"), *DirectionLabel));
		}
		if (MovementInput.IsNearlyZero())
		{
			StopContinuousAction(MoveActionPath);
		}

		APlayerController* PlayerController = FindPIEPlayerController();
		FCharacterSnapshot Snapshot;
		if (!PlayerController || !ReadCharacterSnapshot(Snapshot))
		{
			return AbortIfTimedOut(ElapsedSeconds, FString::Printf(
				TEXT("reading %s dodge snapshot"), *DirectionLabel));
		}

		if (!bRollInputInjected)
		{
			const bool bReadyToDodge = MovementInput.IsNearlyZero()
				? Snapshot.GroundSpeed <= 1.0f && Snapshot.Acceleration.IsNearlyZero(1.0f)
				: !Snapshot.Acceleration.IsNearlyZero(1.0f) && Snapshot.GroundSpeed >= 20.0f;
			if (!bReadyToDodge)
			{
				return AbortIfTimedOut(ElapsedSeconds, FString::Printf(
					TEXT("establishing %s dodge input (speed=%.1f, acceleration=%.1f)"),
					*DirectionLabel,
					Snapshot.GroundSpeed,
					Snapshot.Acceleration.Size2D()));
			}

			ExpectedDirection = GetCameraRelativeDirection(
				PlayerController,
				MovementInput,
				Snapshot.ActorForward);
			CombatStateBefore = Snapshot.CombatState;
			AttackRequestSerialBefore = Snapshot.AttackRequestSerial;
			DodgeRequestSerialBefore = Snapshot.DodgeRequestSerial;
			if (!SetContinuousAction(RollActionPath, FInputActionValue(true)))
			{
				return AbortIfTimedOut(ElapsedSeconds, FString::Printf(
					TEXT("injecting Shift for %s dodge"), *DirectionLabel));
			}
			bRollInputInjected = true;
			return false;
		}

		if (Snapshot.bDodgeActive)
		{
			if (!bSawDodge)
			{
				DodgeStartLocation = Snapshot.Location;
				bSawDodge = true;
				if (Snapshot.DodgeRequestSerial == DodgeRequestSerialBefore)
				{
					return AbortWithError(FString::Printf(
						TEXT("%s Shift did not advance the accepted dodge serial."),
						*DirectionLabel));
				}
			}

			const float DirectionDot = FVector::DotProduct(
				Snapshot.DodgeDirection.GetSafeNormal2D(),
				ExpectedDirection);
			if (DirectionDot < 0.98f)
			{
				return AbortWithError(FString::Printf(
					TEXT("%s dodge captured the wrong direction (dot=%.3f)."),
					*DirectionLabel,
					DirectionDot));
			}
			const bool bBeforePresentationHandoff = Snapshot.DodgeNormalizedTime
				< Snapshot.MovementRecoveryNormalized - NormalizedBoundaryTolerance;
			const bool bPastPresentationHandoff = Snapshot.DodgeNormalizedTime
				>= Snapshot.MovementRecoveryNormalized + PresentationHandoffTolerance;
			if (bBeforePresentationHandoff
				&& !HasValidDodgePlayback(Snapshot, *RunState))
			{
				return AbortWithError(FString::Printf(
					TEXT("%s dodge lost Roll before movement recovery."),
					*DirectionLabel));
			}
			if (bPastPresentationHandoff)
			{
				if (MovementInput.IsNearlyZero())
				{
					if (!HasValidDodgePlayback(Snapshot, *RunState))
					{
						return AbortWithError(TEXT("Neutral dodge left Roll without movement intent."));
					}
					bSawNeutralRollAfterRecovery = true;
				}
				else
				{
					if (Snapshot.bDodgeMovementLocked
						|| !HasValidDirectionalJogPlayback(Snapshot))
					{
						return AbortWithError(FString::Printf(
							TEXT("%s dodge did not hand active recovery to directional Jog (t=%.3f, state=%d, animation=%s)."),
							*DirectionLabel,
							Snapshot.DodgeNormalizedTime,
							static_cast<int32>(Snapshot.LocomotionState),
							*Snapshot.ActivePresentationAnimationName.ToString()));
					}
					bSawDirectionalJogWhileDodgeActive = true;
				}
			}
			else if (!bBeforePresentationHandoff
				&& !HasValidDodgePlayback(Snapshot, *RunState)
				&& (MovementInput.IsNearlyZero()
					|| !HasValidDirectionalJogPlayback(Snapshot)))
			{
				return AbortWithError(FString::Printf(
					TEXT("%s dodge had no valid Roll/Jog presentation during handoff."),
					*DirectionLabel));
			}
			if (!Snapshot.bDodgeRestrictedActionsLocked)
			{
				return AbortWithError(FString::Printf(
					TEXT("%s dodge released restricted actions before Roll finished."),
					*DirectionLabel));
			}
			if (Snapshot.DodgeNormalizedTime
				< Snapshot.MovementRecoveryNormalized - NormalizedBoundaryTolerance)
			{
				bSawCommittedMovementLock |= Snapshot.bDodgeMovementLocked;
				if (!Snapshot.bDodgeMovementLocked)
				{
					return AbortWithError(FString::Printf(
						TEXT("%s dodge released movement before its configured recovery cutoff (t=%.3f)."),
						*DirectionLabel,
						Snapshot.DodgeNormalizedTime));
				}
			}
			if (Snapshot.CombatState != CombatStateBefore
				|| Snapshot.AttackRequestSerial != AttackRequestSerialBefore
				|| Snapshot.bUnarmedAttackActive
				|| Snapshot.bQueuedUnarmedFollowUp
				|| Snapshot.ActiveComboAnimationIndex != INDEX_NONE)
			{
				return AbortWithError(FString::Printf(
					TEXT("%s dodge changed combat or combo state."),
					*DirectionLabel));
			}
			return false;
		}

		if (bSawDodge)
		{
			const FVector Displacement = (Snapshot.Location - DodgeStartLocation).GetSafeNormal2D();
			const float DisplacementDistance =
				(Snapshot.Location - DodgeStartLocation).Size2D();
			const float DirectionDot = FVector::DotProduct(Displacement, ExpectedDirection);
			const bool bReturnedUnlocked =
				!Snapshot.bDodgeMovementLocked
				&& !Snapshot.bDodgeRestrictedActionsLocked
				&& !Snapshot.bDodgeInvulnerable;
			const bool bCompletedValidDodge = bSawCommittedMovementLock
				&& DisplacementDistance >= MinimumDodgeDisplacement
				&& DirectionDot >= DirectionDotTolerance
				&& bReturnedUnlocked
				&& (MovementInput.IsNearlyZero()
					? bSawNeutralRollAfterRecovery
					: bSawDirectionalJogWhileDodgeActive)
				&& Snapshot.CombatState == CombatStateBefore
				&& Snapshot.AttackRequestSerial == AttackRequestSerialBefore;

			if (MovementInput.IsNearlyZero() && bCompletedValidDodge)
			{
				StopContinuousAction(MoveActionPath);
				Test->AddInfo(FString::Printf(
					TEXT("%s dodge captured its direction and displaced %.1f uu without touching combat."),
					*DirectionLabel,
					DisplacementDistance));
				return true;
			}

			if (!MovementInput.IsNearlyZero())
			{
				if (PostDodgeObservationStartSeconds < 0.0)
				{
					PostDodgeObservationStartSeconds = ElapsedSeconds;
					PostDodgeStartLocation = Snapshot.Location;
					return false;
				}

				const double PostDodgeElapsedSeconds =
					ElapsedSeconds - PostDodgeObservationStartSeconds;
				const FVector PostDodgeDelta = Snapshot.Location - PostDodgeStartLocation;
				const float PostDodgeForwardDisplacement = FVector::DotProduct(
					PostDodgeDelta.GetSafeNormal2D(),
					ExpectedDirection);
				const FVector AccelerationDirection = Snapshot.Acceleration.GetSafeNormal2D();
				const bool bMovementInputResumed = !AccelerationDirection.IsNearlyZero()
					&& FVector::DotProduct(AccelerationDirection, ExpectedDirection) >= 0.75f;
				const bool bUsefulMovementResumed =
					Snapshot.GroundSpeed >= MinimumPostDodgeJogSpeed
					&& PostDodgeDelta.Size2D() >= MinimumPostDodgeDisplacement
					&& PostDodgeForwardDisplacement >= 0.75f;
				const bool bJogResumed = HasValidDirectionalJogPlayback(Snapshot)
					&& bMovementInputResumed
					&& (!bRequirePhysicalContinuation || bUsefulMovementResumed);

				if (bCompletedValidDodge && bJogResumed)
				{
					StopContinuousAction(MoveActionPath);
					Test->AddInfo(FString::Printf(
						TEXT("%s dodge preserved %s at %.1f uu/s through completion (observed %.3fs later)."),
						*DirectionLabel,
						*Snapshot.ActivePresentationAnimationName.ToString(),
						Snapshot.GroundSpeed,
						PostDodgeElapsedSeconds));
					return true;
				}

				if (PostDodgeElapsedSeconds > MaximumPostDodgeJogResumeSeconds)
				{
					return AbortWithError(FString::Printf(
						TEXT("%s dodge did not resume useful directional Jog within %.2fs (state=%d, animation=%s, speed=%.1f, acceleration=%.1f, postDistance=%.1f)."),
						*DirectionLabel,
						MaximumPostDodgeJogResumeSeconds,
						static_cast<int32>(Snapshot.LocomotionState),
						*Snapshot.ActivePresentationAnimationName.ToString(),
						Snapshot.GroundSpeed,
						Snapshot.Acceleration.Size2D(),
						PostDodgeDelta.Size2D()));
				}
			}
		}

		return AbortIfTimedOut(
			ElapsedSeconds,
			FString::Printf(
				TEXT("%s dodge (active=%d, t=%.3f, distance=%.1f, animation=%s)"),
				*DirectionLabel,
				Snapshot.bDodgeActive,
				Snapshot.DodgeNormalizedTime,
				bSawDodge ? (Snapshot.Location - DodgeStartLocation).Size2D() : 0.0f,
				*Snapshot.ActivePresentationAnimationName.ToString()));
	}

private:
	FVector2D MovementInput = FVector2D::ZeroVector;
	FString DirectionLabel;
	FVector ExpectedDirection = FVector::ForwardVector;
	FVector DodgeStartLocation = FVector::ZeroVector;
	ERequiemCombatState CombatStateBefore = ERequiemCombatState::Normal;
	int32 AttackRequestSerialBefore = 0;
	int32 DodgeRequestSerialBefore = 0;
	double PostDodgeObservationStartSeconds = -1.0;
	FVector PostDodgeStartLocation = FVector::ZeroVector;
	bool bRollInputInjected = false;
	bool bRollInputReleased = false;
	bool bSawDodge = false;
	bool bSawCommittedMovementLock = false;
	bool bSawDirectionalJogWhileDodgeActive = false;
	bool bSawNeutralRollAfterRecovery = false;
	bool bRequirePhysicalContinuation = false;
};

class FObserveSameFrameDirectionalDodgeCommand final : public FTimedCommand
{
public:
	FObserveSameFrameDirectionalDodgeCommand(
		FAutomationTestBase* InTest,
		TSharedRef<FRunState> InRunState)
		: FTimedCommand(InTest, MoveTemp(InRunState), 6.0)
	{
	}

	virtual bool Update() override
	{
		if (ShouldSkip())
		{
			ReleaseAllTestInput();
			return true;
		}

		const double ElapsedSeconds = GetElapsedSeconds();
		APlayerController* PlayerController = FindPIEPlayerController();
		FCharacterSnapshot Snapshot;
		if (!PlayerController || !ReadCharacterSnapshot(Snapshot))
		{
			return AbortIfTimedOut(
				ElapsedSeconds,
				TEXT("reading same-frame direction and Shift snapshot"));
		}

		const FVector2D ImmediateInput(-1.0f, 1.0f);
		if (!bInputInjected)
		{
			if (Snapshot.GroundSpeed > 1.0f || !Snapshot.Acceleration.IsNearlyZero(1.0f))
			{
				return AbortIfTimedOut(
					ElapsedSeconds,
					TEXT("waiting for a stationary same-frame dodge start"));
			}

			ExpectedDirection = GetCameraRelativeDirection(
				PlayerController,
				ImmediateInput,
				Snapshot.ActorForward);
			DodgeRequestSerialBefore = Snapshot.DodgeRequestSerial;
			const bool bMoveInjected = SetContinuousAction(
				MoveActionPath,
				FInputActionValue(ImmediateInput.GetSafeNormal() * 0.55f));
			const bool bRollInjected = SetContinuousAction(
				RollActionPath,
				FInputActionValue(true));
			if (!bMoveInjected || !bRollInjected)
			{
				return AbortIfTimedOut(
					ElapsedSeconds,
					TEXT("injecting direction and Shift together"));
			}
			bInputInjected = true;
			return false;
		}

		if (Snapshot.bDodgeActive)
		{
			if (!bSawDodge)
			{
				StopContinuousAction(MoveActionPath);
				StopContinuousAction(RollActionPath);
				bSawDodge = true;
			}

			const float DirectionDot = FVector::DotProduct(
				Snapshot.DodgeDirection.GetSafeNormal2D(),
				ExpectedDirection);
			if (Snapshot.DodgeRequestSerial == DodgeRequestSerialBefore
				|| DirectionDot < 0.98f
				|| !HasValidDodgePlayback(Snapshot, *RunState))
			{
				return AbortWithError(FString::Printf(
					TEXT("Direction+Shift in one frame did not capture the evaluated axis (dot=%.3f)."),
					DirectionDot));
			}
			return false;
		}

		if (bSawDodge)
		{
			Test->AddInfo(TEXT("Direction and Shift in the same frame captured the current movement axis."));
			return true;
		}

		return AbortIfTimedOut(
			ElapsedSeconds,
			TEXT("starting direction and Shift in the same frame"));
	}

private:
	FVector ExpectedDirection = FVector::ForwardVector;
	int32 DodgeRequestSerialBefore = 0;
	bool bInputInjected = false;
	bool bSawDodge = false;
};

class FValidateDodgeTimelineCommand final : public FTimedCommand
{
public:
	FValidateDodgeTimelineCommand(FAutomationTestBase* InTest, TSharedRef<FRunState> InRunState)
		: FTimedCommand(InTest, MoveTemp(InRunState), 7.0)
	{
	}

	virtual bool Update() override
	{
		if (ShouldSkip())
		{
			ReleaseAllTestInput();
			return true;
		}

		const double ElapsedSeconds = GetElapsedSeconds();
		ReleasePulsedActions();

		const FVector2D InitialInput(1.0f, 1.0f);
		const FVector2D RedirectInput(-1.0f, -1.0f);
		if (!bRollInputInjected)
		{
			if (!SetContinuousAction(
				MoveActionPath,
				FInputActionValue(InitialInput.GetSafeNormal() * 0.55f)))
			{
				return AbortIfTimedOut(ElapsedSeconds, TEXT("injecting initial timeline movement"));
			}
		}
		else if (!SetContinuousAction(
			MoveActionPath,
			FInputActionValue(RedirectInput.GetSafeNormal() * 0.55f)))
		{
			return AbortIfTimedOut(ElapsedSeconds, TEXT("injecting opposite movement during dodge"));
		}

		APlayerController* PlayerController = FindPIEPlayerController();
		FCharacterSnapshot Snapshot;
		if (!PlayerController || !ReadCharacterSnapshot(Snapshot))
		{
			return AbortIfTimedOut(ElapsedSeconds, TEXT("reading the dodge timeline"));
		}

		if (!bRollInputInjected)
		{
			if (Snapshot.Acceleration.IsNearlyZero(1.0f) || Snapshot.GroundSpeed < 20.0f)
			{
				return AbortIfTimedOut(ElapsedSeconds, TEXT("establishing initial timeline movement"));
			}

			ExpectedDodgeDirection = GetCameraRelativeDirection(
				PlayerController,
				InitialInput,
				Snapshot.ActorForward);
			ExpectedRedirectDirection = GetCameraRelativeDirection(
				PlayerController,
				RedirectInput,
				Snapshot.ActorForward);
			AttackRequestSerialBefore = Snapshot.AttackRequestSerial;
			DodgeRequestSerialBefore = Snapshot.DodgeRequestSerial;
			if (!SetContinuousAction(RollActionPath, FInputActionValue(true)))
			{
				return AbortIfTimedOut(ElapsedSeconds, TEXT("injecting initial timeline Shift"));
			}
			bRollInputInjected = true;
			return false;
		}

		if (Snapshot.bDodgeActive)
		{
			if (!bSawDodge)
			{
				bSawDodge = true;
				if (Snapshot.DodgeRequestSerial == DodgeRequestSerialBefore)
				{
					return AbortWithError(TEXT("Accepted timeline dodge did not advance its request serial."));
				}
				AcceptedDodgeRequestSerial = Snapshot.DodgeRequestSerial;
			}
			if (Snapshot.DodgeRequestSerial != AcceptedDodgeRequestSerial)
			{
				return AbortWithError(TEXT("A repeated Shift advanced the accepted-dodge serial."));
			}

			const float NormalizedTime = Snapshot.DodgeNormalizedTime;
			const bool bBeforePresentationHandoff = NormalizedTime
				< Snapshot.MovementRecoveryNormalized - NormalizedBoundaryTolerance;
			const bool bPastPresentationHandoff = NormalizedTime
				>= Snapshot.MovementRecoveryNormalized + PresentationHandoffTolerance;
			if (bBeforePresentationHandoff
				&& !HasValidDodgePlayback(Snapshot, *RunState))
			{
				return AbortWithError(TEXT("Timeline dodge lost Roll before recovery."));
			}
			if (bPastPresentationHandoff)
			{
				if (!HasValidDirectionalJogPlayback(Snapshot))
				{
					return AbortWithError(FString::Printf(
						TEXT("Timeline dodge did not use directional Jog during active recovery (t=%.3f, animation=%s)."),
						NormalizedTime,
						*Snapshot.ActivePresentationAnimationName.ToString()));
				}
				bSawActiveJogHandoff = true;
				bSawRestrictedLockDuringJog |= Snapshot.bDodgeRestrictedActionsLocked;
			}
			else if (!bBeforePresentationHandoff
				&& !HasValidDodgePlayback(Snapshot, *RunState)
				&& !HasValidDirectionalJogPlayback(Snapshot))
			{
				return AbortWithError(TEXT("Timeline dodge had no valid Roll/Jog presentation during handoff."));
			}
			if (Snapshot.CombatState != ERequiemCombatState::Normal
				|| Snapshot.AttackRequestSerial != AttackRequestSerialBefore
				|| Snapshot.bUnarmedAttackActive
				|| Snapshot.bQueuedUnarmedFollowUp
				|| Snapshot.bIsCrouched
				|| Snapshot.bIsFalling)
			{
				return AbortWithError(TEXT("A restricted attack, stance, crouch, or jump action escaped the active dodge lock."));
			}
			if (!Snapshot.bDodgeRestrictedActionsLocked)
			{
				return AbortWithError(TEXT("Restricted dodge actions unlocked before Roll finished."));
			}

			const float DirectionDot = FVector::DotProduct(
				Snapshot.DodgeDirection.GetSafeNormal2D(),
				ExpectedDodgeDirection);
			if (DirectionDot < 0.98f)
			{
				return AbortWithError(FString::Printf(
					TEXT("Opposite input redirected the committed dodge direction (dot=%.3f)."),
					DirectionDot));
			}

			if (NormalizedTime + 0.04f < HighestObservedNormalizedTime)
			{
				return AbortWithError(FString::Printf(
					TEXT("A second Shift restarted Roll (previous=%.3f, current=%.3f)."),
					HighestObservedNormalizedTime,
					NormalizedTime));
			}
			HighestObservedNormalizedTime = FMath::Max(
				HighestObservedNormalizedTime,
				NormalizedTime);

			if (NormalizedTime
				< Snapshot.MovementRecoveryNormalized - NormalizedBoundaryTolerance)
			{
				bSawPreRecoveryMovementLock |= Snapshot.bDodgeMovementLocked;
				if (!Snapshot.bDodgeMovementLocked)
				{
					return AbortWithError(FString::Printf(
						TEXT("Dodge movement unlocked before recovery (t=%.3f)."),
						NormalizedTime));
				}
			}
			else if (!Snapshot.bDodgeMovementLocked)
			{
				bSawMovementRecovery = true;
				const FVector AccelerationDirection = Snapshot.Acceleration.GetSafeNormal2D();
				if (!AccelerationDirection.IsNearlyZero()
					&& FVector::DotProduct(
						AccelerationDirection,
						ExpectedRedirectDirection) >= 0.75f)
				{
					bSawRecoveredRedirection = true;
				}

				if (!bHasRecoveryLocation)
				{
					RecoveryLocation = Snapshot.Location;
					bHasRecoveryLocation = true;
				}
				else
				{
					const FVector RecoveryStep =
						(Snapshot.Location - RecoveryLocation).GetSafeNormal2D();
					const float RecoveryStepDistance =
						(Snapshot.Location - RecoveryLocation).Size2D();
					if (RecoveryStepDistance >= 0.25f
						&& FVector::DotProduct(
							RecoveryStep,
							ExpectedRedirectDirection) >= 0.50f)
					{
						bSawRecoveredDisplacement = true;
					}
					RecoveryLocation = Snapshot.Location;
				}
			}
			if (NormalizedTime
				> Snapshot.MovementRecoveryNormalized + 0.06f
				&& Snapshot.bDodgeMovementLocked)
			{
				return AbortWithError(FString::Printf(
					TEXT("Dodge movement remained locked past its configured recovery cutoff (t=%.3f)."),
					NormalizedTime));
			}

			ValidateIFramePhase(Snapshot, NormalizedTime);
			if (ShouldSkip())
			{
				return true;
			}

			if (!bIFrameDamageProbeCompleted
				&& NormalizedTime >= Snapshot.IFrameStartNormalized + 0.04f
				&& NormalizedTime <= Snapshot.IFrameEndNormalized - 0.04f)
			{
				ARequiemCharacter* Character = FindPIECharacter();
				if (!Character)
				{
					return AbortIfTimedOut(ElapsedSeconds, TEXT("probing damage during dodge i-frames"));
				}

				const FDamageEvent DamageEvent;
				const float AppliedDamage = Character->TakeDamage(
					25.0f,
					DamageEvent,
					nullptr,
					nullptr);
				if (!FMath::IsNearlyZero(AppliedDamage))
				{
					return AbortWithError(FString::Printf(
						TEXT("Generic damage was not ignored during i-frames (applied=%.2f)."),
						AppliedDamage));
				}
				bIFrameDamageProbeCompleted = true;
			}

			if (!bAttackInputInjected
				&& NormalizedTime >= Snapshot.IFrameStartNormalized + 0.04f)
			{
				if (!SetContinuousAction(PrimaryAttackActionPath, FInputActionValue(true)))
				{
					return AbortIfTimedOut(ElapsedSeconds, TEXT("injecting blocked LMB during dodge"));
				}
				bAttackInputInjected = true;
				return false;
			}

			if (!bRestrictedInputsInjected
				&& NormalizedTime >= Snapshot.IFrameStartNormalized + 0.08f)
			{
				const bool bInjectedToggle = SetContinuousAction(
					ToggleCombatActionPath,
					FInputActionValue(true));
				const bool bInjectedCrouch = SetContinuousAction(
					CrouchActionPath,
					FInputActionValue(true));
				const bool bInjectedJump = SetContinuousAction(
					JumpActionPath,
					FInputActionValue(true));
				if (!bInjectedToggle || !bInjectedCrouch || !bInjectedJump)
				{
					return AbortIfTimedOut(
						ElapsedSeconds,
						TEXT("injecting blocked Z, Ctrl, and Space during dodge"));
				}
				bRestrictedInputsInjected = true;
				return false;
			}

			if (!bSecondRollInputInjected
				&& NormalizedTime >= FMath::Max(0.35f, Snapshot.IFrameStartNormalized + 0.10f))
			{
				if (!SetContinuousAction(RollActionPath, FInputActionValue(true)))
				{
					return AbortIfTimedOut(ElapsedSeconds, TEXT("injecting a rejected second Shift"));
				}
				bSecondRollInputInjected = true;
				return false;
			}

			return false;
		}

		if (bSawDodge)
		{
			ReleaseAllTestInput();
			const bool bPresentationReleased =
				Snapshot.LocomotionState != ERequiemLocomotionState::Dodge
				&& Snapshot.ActivePresentationAnimationName != RunState->RollAnimationName;
			const bool bRuntimeReleased =
				!Snapshot.bDodgeMovementLocked
				&& !Snapshot.bDodgeRestrictedActionsLocked
				&& !Snapshot.bDodgeInvulnerable;
			if (bPresentationReleased
				&& bRuntimeReleased
				&& bSawPreRecoveryMovementLock
				&& bSawMovementRecovery
				&& bSawRecoveredRedirection
				&& bSawRecoveredDisplacement
				&& bSawActiveJogHandoff
				&& bSawRestrictedLockDuringJog
				&& bSawPreIFrame
				&& bSawInsideIFrame
				&& bSawPostIFrame
				&& bIFrameDamageProbeCompleted
				&& bAttackInputInjected
				&& bRestrictedInputsInjected
				&& bSecondRollInputInjected
				&& Snapshot.AttackRequestSerial == AttackRequestSerialBefore
				&& Snapshot.CombatState == ERequiemCombatState::Normal)
			{
				Test->AddInfo(TEXT("Dodge kept a fixed commitment, rejected queued actions, exposed central i-frames, and returned movement at its configured recovery cutoff."));
				return true;
			}
		}

		return AbortIfTimedOut(
			ElapsedSeconds,
			FString::Printf(
				TEXT("dodge timeline (active=%d, t=%.3f, pre=%d, iframe=%d, post=%d, recovery=%d, redirected=%d, moved=%d)"),
				Snapshot.bDodgeActive,
				Snapshot.DodgeNormalizedTime,
				bSawPreIFrame,
				bSawInsideIFrame,
				bSawPostIFrame,
				bSawMovementRecovery,
				bSawRecoveredRedirection,
				bSawRecoveredDisplacement));
	}

private:
	void ReleasePulsedActions()
	{
		if (bRollInputInjected && !bRollInputReleased)
		{
			StopContinuousAction(RollActionPath);
			bRollInputReleased = true;
		}
		if (bAttackInputInjected && !bAttackInputReleased)
		{
			StopContinuousAction(PrimaryAttackActionPath);
			bAttackInputReleased = true;
		}
		if (bRestrictedInputsInjected && !bRestrictedInputsReleased)
		{
			StopContinuousAction(ToggleCombatActionPath);
			StopContinuousAction(CrouchActionPath);
			StopContinuousAction(JumpActionPath);
			bRestrictedInputsReleased = true;
		}
		if (bSecondRollInputInjected && !bSecondRollInputReleased)
		{
			StopContinuousAction(RollActionPath);
			bSecondRollInputReleased = true;
		}
	}

	void ValidateIFramePhase(
		const FCharacterSnapshot& Snapshot,
		const float NormalizedTime)
	{
		if (NormalizedTime
			< Snapshot.IFrameStartNormalized - NormalizedBoundaryTolerance)
		{
			bSawPreIFrame = true;
			if (Snapshot.bDodgeInvulnerable)
			{
				AbortWithError(FString::Printf(
					TEXT("Dodge became invulnerable before its configured window (t=%.3f)."),
					NormalizedTime));
			}
		}
		else if (NormalizedTime
			>= Snapshot.IFrameStartNormalized + NormalizedBoundaryTolerance
			&& NormalizedTime
			<= Snapshot.IFrameEndNormalized - NormalizedBoundaryTolerance)
		{
			bSawInsideIFrame = true;
			if (!Snapshot.bDodgeInvulnerable)
			{
				AbortWithError(FString::Printf(
					TEXT("Dodge was vulnerable inside its configured i-frame window (t=%.3f)."),
					NormalizedTime));
			}
		}
		else if (NormalizedTime
			> Snapshot.IFrameEndNormalized + NormalizedBoundaryTolerance
			&& NormalizedTime < 1.0f - NormalizedBoundaryTolerance)
		{
			bSawPostIFrame = true;
			if (Snapshot.bDodgeInvulnerable)
			{
				AbortWithError(FString::Printf(
					TEXT("Dodge remained invulnerable after its configured window (t=%.3f)."),
					NormalizedTime));
			}
		}
	}

	FVector ExpectedDodgeDirection = FVector::ForwardVector;
	FVector ExpectedRedirectDirection = -FVector::ForwardVector;
	int32 AttackRequestSerialBefore = 0;
	int32 DodgeRequestSerialBefore = 0;
	int32 AcceptedDodgeRequestSerial = 0;
	float HighestObservedNormalizedTime = 0.0f;
	bool bRollInputInjected = false;
	bool bRollInputReleased = false;
	bool bAttackInputInjected = false;
	bool bAttackInputReleased = false;
	bool bRestrictedInputsInjected = false;
	bool bRestrictedInputsReleased = false;
	bool bSecondRollInputInjected = false;
	bool bSecondRollInputReleased = false;
	bool bSawDodge = false;
	bool bSawPreRecoveryMovementLock = false;
	bool bSawMovementRecovery = false;
	bool bSawRecoveredRedirection = false;
	bool bHasRecoveryLocation = false;
	bool bSawRecoveredDisplacement = false;
	bool bSawActiveJogHandoff = false;
	bool bSawRestrictedLockDuringJog = false;
	FVector RecoveryLocation = FVector::ZeroVector;
	bool bSawPreIFrame = false;
	bool bSawInsideIFrame = false;
	bool bSawPostIFrame = false;
	bool bIFrameDamageProbeCompleted = false;
};

class FValidateCombatModeDodgeCommand final : public FTimedCommand
{
public:
	FValidateCombatModeDodgeCommand(FAutomationTestBase* InTest, TSharedRef<FRunState> InRunState)
		: FTimedCommand(InTest, MoveTemp(InRunState), 8.0)
	{
	}

	virtual bool Update() override
	{
		if (ShouldSkip())
		{
			ReleaseAllTestInput();
			return true;
		}

		const double ElapsedSeconds = GetElapsedSeconds();
		if (bRollInputInjected && !bRollInputReleased)
		{
			StopContinuousAction(RollActionPath);
			bRollInputReleased = true;
		}

		URequiemCombatComponent* CombatComponent = FindPIECombatComponent();
		FCharacterSnapshot Snapshot;
		if (!CombatComponent || !ReadCharacterSnapshot(Snapshot))
		{
			return AbortIfTimedOut(ElapsedSeconds, TEXT("reading CombatUnarmed dodge state"));
		}

		if (!bRequestedCombatEntry)
		{
			CombatComponent->EnterUnarmedCombat(ERequiemCombatEntryReason::Manual);
			bRequestedCombatEntry = true;
			return false;
		}

		if (!bRollInputInjected)
		{
			if (Snapshot.CombatState != ERequiemCombatState::CombatUnarmed
				|| Snapshot.CombatAnimationState == ERequiemCombatAnimationState::Attack
				|| Snapshot.CombatAnimationState == ERequiemCombatAnimationState::Recovery)
			{
				return AbortIfTimedOut(
					ElapsedSeconds,
					FString::Printf(
						TEXT("waiting for safe CombatUnarmed state before dodge (combat=%d, presentation=%d)"),
						static_cast<int32>(Snapshot.CombatState),
						static_cast<int32>(Snapshot.CombatAnimationState)));
			}
			if (!SetContinuousAction(
				MoveActionPath,
				FInputActionValue(FVector2D(-0.55f, 0.0f))))
			{
				return AbortIfTimedOut(ElapsedSeconds, TEXT("injecting left movement before combat dodge"));
			}
			if (Snapshot.Acceleration.IsNearlyZero(1.0f) || Snapshot.GroundSpeed < 20.0f)
			{
				return false;
			}

			AttackRequestSerialBefore = Snapshot.AttackRequestSerial;
			DodgeRequestSerialBefore = Snapshot.DodgeRequestSerial;
			if (!SetContinuousAction(RollActionPath, FInputActionValue(true)))
			{
				return AbortIfTimedOut(ElapsedSeconds, TEXT("injecting Shift in CombatUnarmed"));
			}
			bRollInputInjected = true;
			return false;
		}

		if (Snapshot.bDodgeActive)
		{
			bSawDodge = true;
			if (Snapshot.DodgeRequestSerial == DodgeRequestSerialBefore)
			{
				return AbortWithError(TEXT("CombatUnarmed Shift did not start a dodge."));
			}

			const bool bBeforePresentationHandoff = Snapshot.DodgeNormalizedTime
				< Snapshot.MovementRecoveryNormalized - NormalizedBoundaryTolerance;
			const bool bPastPresentationHandoff = Snapshot.DodgeNormalizedTime
				>= Snapshot.MovementRecoveryNormalized + PresentationHandoffTolerance;
			if (bBeforePresentationHandoff
				&& !HasValidDodgePlayback(Snapshot, *RunState))
			{
				return AbortWithError(TEXT("CombatUnarmed dodge lost Roll before recovery."));
			}
			if (bPastPresentationHandoff)
			{
				if (Snapshot.bDodgeMovementLocked
					|| !Snapshot.bDodgeRestrictedActionsLocked
					|| !HasValidDirectionalJogPlayback(Snapshot))
				{
					return AbortWithError(TEXT("CombatUnarmed dodge did not preserve locks while handing recovery to Jog."));
				}
				bSawCombatJogHandoff = true;
			}
			else if (!bBeforePresentationHandoff
				&& !HasValidDodgePlayback(Snapshot, *RunState)
				&& !HasValidDirectionalJogPlayback(Snapshot))
			{
				return AbortWithError(TEXT("CombatUnarmed dodge had no valid Roll/Jog presentation during handoff."));
			}
			if (Snapshot.CombatState != ERequiemCombatState::CombatUnarmed
				|| Snapshot.AttackRequestSerial != AttackRequestSerialBefore
				|| Snapshot.bUnarmedAttackActive
				|| Snapshot.bQueuedUnarmedFollowUp
				|| Snapshot.ActiveComboAnimationIndex != INDEX_NONE)
			{
				return AbortWithError(TEXT("CombatUnarmed dodge mutated the combat mode or combo runtime."));
			}
			return false;
		}

		if (bSawDodge && !bMoveInputReleasedAfterDodge)
		{
			StopContinuousAction(MoveActionPath);
			bMoveInputReleasedAfterDodge = true;
		}

		if (bSawDodge
			&& bSawCombatJogHandoff
			&& Snapshot.CombatState == ERequiemCombatState::CombatUnarmed
			&& Snapshot.CombatAnimationState == ERequiemCombatAnimationState::Idle
			&& Snapshot.AttackRequestSerial == AttackRequestSerialBefore
			&& !Snapshot.bUnarmedAttackActive
			&& !Snapshot.bQueuedUnarmedFollowUp
			&& Snapshot.ActiveComboAnimationIndex == INDEX_NONE
			&& !Snapshot.bDodgeMovementLocked
			&& !Snapshot.bDodgeRestrictedActionsLocked)
		{
			Test->AddInfo(TEXT("CombatUnarmed dodge returned to combat idle without entering or advancing the combo."));
			return true;
		}

		return AbortIfTimedOut(
			ElapsedSeconds,
			FString::Printf(
				TEXT("CombatUnarmed dodge (combat=%d, presentation=%d, dodge=%d, combo=%d)"),
				static_cast<int32>(Snapshot.CombatState),
				static_cast<int32>(Snapshot.CombatAnimationState),
				Snapshot.bDodgeActive,
				Snapshot.ActiveComboAnimationIndex));
	}

private:
	int32 AttackRequestSerialBefore = 0;
	int32 DodgeRequestSerialBefore = 0;
	bool bRequestedCombatEntry = false;
	bool bRollInputInjected = false;
	bool bRollInputReleased = false;
	bool bSawDodge = false;
	bool bSawCombatJogHandoff = false;
	bool bMoveInputReleasedAfterDodge = false;
};

class FValidateDodgeRejectedDuringComboCommand final : public FTimedCommand
{
public:
	FValidateDodgeRejectedDuringComboCommand(
		FAutomationTestBase* InTest,
		TSharedRef<FRunState> InRunState)
		: FTimedCommand(InTest, MoveTemp(InRunState), 7.0)
	{
	}

	virtual bool Update() override
	{
		if (ShouldSkip())
		{
			ReleaseAllTestInput();
			return true;
		}

		const double ElapsedSeconds = GetElapsedSeconds();
		if (bRollInputInjected && !bRollInputReleased)
		{
			StopContinuousAction(RollActionPath);
			bRollInputReleased = true;
		}

		URequiemCombatComponent* CombatComponent = FindPIECombatComponent();
		FCharacterSnapshot Snapshot;
		if (!CombatComponent || !ReadCharacterSnapshot(Snapshot))
		{
			return AbortIfTimedOut(ElapsedSeconds, TEXT("reading combo dodge rejection"));
		}

		if (!bAttackRequested)
		{
			if (Snapshot.CombatState != ERequiemCombatState::CombatUnarmed
				|| Snapshot.CombatAnimationState != ERequiemCombatAnimationState::Idle)
			{
				return AbortIfTimedOut(ElapsedSeconds, TEXT("waiting for combat idle before rejection check"));
			}

			const ERequiemUnarmedAttackRequestResult Result =
				CombatComponent->RequestUnarmedAttack();
			if (Result != ERequiemUnarmedAttackRequestResult::InitialAccepted)
			{
				return AbortWithError(TEXT("Could not start the isolated Cross for dodge rejection."));
			}
			bAttackRequested = true;
			return false;
		}

		if (!bRollInputInjected)
		{
			if (Snapshot.CombatAnimationState != ERequiemCombatAnimationState::Attack
				|| !Snapshot.bUnarmedAttackActive
				|| Snapshot.ActiveComboAnimationIndex == INDEX_NONE
				|| Snapshot.CombatNormalizedTime < 0.65f
				|| Snapshot.bUnarmedAttackMovementLocked)
			{
				return AbortIfTimedOut(
					ElapsedSeconds,
					TEXT("waiting for committed Cross movement recovery before Shift"));
			}

			AttackRequestSerialAfterInitial = Snapshot.AttackRequestSerial;
			DodgeRequestSerialBeforeLateShift = Snapshot.DodgeRequestSerial;
			CommittedComboIndex = Snapshot.ActiveComboAnimationIndex;
			CommittedAnimationName = Snapshot.ActivePresentationAnimationName;
			if (!SetContinuousAction(RollActionPath, FInputActionValue(true)))
			{
				return AbortIfTimedOut(ElapsedSeconds, TEXT("injecting rejected Shift during Cross"));
			}
			bRollInputInjected = true;
			ValidationStartedSeconds = ElapsedSeconds;
			return false;
		}

		if (!bValidatedCommittedAttack)
		{
			if (Snapshot.bDodgeActive
				|| Snapshot.LocomotionState == ERequiemLocomotionState::Dodge
				|| Snapshot.ActivePresentationAnimationName == RunState->RollAnimationName
				|| Snapshot.DodgeRequestSerial != DodgeRequestSerialBeforeLateShift)
			{
				return AbortWithError(TEXT("Shift was accepted after the committed attack released movement."));
			}
			if (Snapshot.AttackRequestSerial != AttackRequestSerialAfterInitial
				|| Snapshot.bQueuedUnarmedFollowUp)
			{
				return AbortWithError(TEXT("Rejected Shift changed the attack serial or queued a follow-up."));
			}

			if (ElapsedSeconds - ValidationStartedSeconds < 0.10)
			{
				if (Snapshot.CombatAnimationState != ERequiemCombatAnimationState::Attack
					|| Snapshot.ActiveComboAnimationIndex != CommittedComboIndex
					|| Snapshot.ActivePresentationAnimationName != CommittedAnimationName)
				{
					return AbortWithError(TEXT("Rejected Shift changed the committed combo presentation."));
				}
				return false;
			}
			bValidatedCommittedAttack = true;
		}

		if (Snapshot.CombatState == ERequiemCombatState::CombatUnarmed
			&& Snapshot.CombatAnimationState == ERequiemCombatAnimationState::Idle
			&& !Snapshot.bUnarmedAttackActive
			&& !Snapshot.bQueuedUnarmedFollowUp
			&& Snapshot.ActiveComboAnimationIndex == INDEX_NONE
			&& !Snapshot.bDodgeActive)
		{
			Test->AddInfo(TEXT("Shift was rejected after Cross released movement, while the committed combo returned normally to combat idle."));
			return true;
		}

		return AbortIfTimedOut(
			ElapsedSeconds,
			FString::Printf(
				TEXT("combo dodge rejection (presentation=%d, combo=%d, dodge=%d, validated=%d)"),
				static_cast<int32>(Snapshot.CombatAnimationState),
				Snapshot.ActiveComboAnimationIndex,
				Snapshot.bDodgeActive,
				bValidatedCommittedAttack));
	}

private:
	FName CommittedAnimationName = NAME_None;
	int32 AttackRequestSerialAfterInitial = 0;
	int32 DodgeRequestSerialBeforeLateShift = 0;
	int32 CommittedComboIndex = INDEX_NONE;
	double ValidationStartedSeconds = -1.0;
	bool bAttackRequested = false;
	bool bRollInputInjected = false;
	bool bRollInputReleased = false;
	bool bValidatedCommittedAttack = false;
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
			Test->AddError(TEXT("PIE did not finish within 5 seconds of teardown."));
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
	FRequiemDodgePIETest,
	"ProjectRequiem.Movement.DodgePIE",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::NonNullRHI
		| EAutomationTestFlags::ProductFilter)

bool FRequiemDodgePIETest::RunTest(const FString& Parameters)
{
	using namespace RequiemDodgePIETest;

	const UInputMappingContext* MappingContext = LoadObject<UInputMappingContext>(
		nullptr,
		MappingContextPath);
	const UInputAction* MoveAction = LoadObject<UInputAction>(nullptr, MoveActionPath);
	const UInputAction* RollAction = LoadObject<UInputAction>(nullptr, RollActionPath);
	const UInputAction* PrimaryAttackAction = LoadObject<UInputAction>(
		nullptr,
		PrimaryAttackActionPath);
	TestNotNull(TEXT("IMC_Exploration exists"), MappingContext);
	TestNotNull(TEXT("IA_Move exists"), MoveAction);
	TestNotNull(TEXT("IA_Roll exists"), RollAction);
	TestNotNull(TEXT("IA_PrimaryAttack exists"), PrimaryAttackAction);
	if (!MappingContext || !MoveAction || !RollAction || !PrimaryAttackAction)
	{
		return false;
	}

	TestEqual(
		TEXT("IA_Roll is Boolean"),
		RollAction->ValueType,
		EInputActionValueType::Boolean);
	TestTrue(
		TEXT("IMC_Exploration maps Left Shift to IA_Roll"),
		HasExactMapping(MappingContext, RollAction, EKeys::LeftShift));

	UClass* CharacterClass = LoadClass<ARequiemCharacter>(nullptr, CharacterClassPath);
	ARequiemCharacter* CharacterCDO = CharacterClass
		? Cast<ARequiemCharacter>(CharacterClass->GetDefaultObject())
		: nullptr;
	TestNotNull(TEXT("BP_CH_Player generated class exists"), CharacterClass);
	TestNotNull(TEXT("BP_CH_Player CDO exists"), CharacterCDO);
	if (CharacterCDO && CharacterClass)
	{
		TestEqual(
			TEXT("BP_CH_Player assigns IA_Roll"),
			GetObjectPropertyValue(CharacterCDO, CharacterClass, TEXT("RollAction")),
			static_cast<UObject*>(const_cast<UInputAction*>(RollAction)));

		URequiemDodgeComponent* DodgeCDO = CharacterCDO->GetDodgeComponent();
		TestNotNull(TEXT("BP_CH_Player owns RequiemDodgeComponent"), DodgeCDO);
		if (DodgeCDO)
		{
			const float IFrameStart = DodgeCDO->GetIFrameStartNormalized();
			const float IFrameEnd = DodgeCDO->GetIFrameEndNormalized();
			const float MovementRecovery =
				DodgeCDO->GetMovementControlRecoveryNormalized();
			TestTrue(
				TEXT("Dodge movement control recovers at 0.62"),
				FMath::IsNearlyEqual(
					MovementRecovery,
					ExpectedMovementRecoveryNormalized,
					0.001f));
			TestTrue(
				TEXT("Dodge i-frames form a central window"),
				IFrameStart > 0.0f
					&& IFrameEnd > IFrameStart + 0.10f
					&& IFrameEnd < 0.90f);
		}
	}

	UClass* AnimInstanceClass = LoadClass<URequiemPlayerAnimInstance>(
		nullptr,
		AnimInstanceClassPath);
	URequiemPlayerAnimInstance* AnimInstanceCDO = AnimInstanceClass
		? Cast<URequiemPlayerAnimInstance>(AnimInstanceClass->GetDefaultObject())
		: nullptr;
	TestNotNull(TEXT("ABP_PR_PlayerLocomotion generated class exists"), AnimInstanceClass);
	TestNotNull(TEXT("ABP_PR_PlayerLocomotion CDO exists"), AnimInstanceCDO);
	UAnimSequence* RollAnimation = AnimInstanceCDO && AnimInstanceClass
		? Cast<UAnimSequence>(GetObjectPropertyValue(
			AnimInstanceCDO,
			AnimInstanceClass,
			TEXT("RollAnimation")))
		: nullptr;
	TestNotNull(TEXT("ABP_PR_PlayerLocomotion assigns RollAnimation"), RollAnimation);
	USkeleton* PlayerSkeleton = LoadObject<USkeleton>(nullptr, PlayerSkeletonPath);
	TestNotNull(TEXT("Player skeleton exists"), PlayerSkeleton);
	if (RollAnimation && PlayerSkeleton)
	{
		TestEqual(
			TEXT("RollAnimation uses the player skeleton"),
			RollAnimation->GetSkeleton(),
			PlayerSkeleton);
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
	RunState->RollAnimationName = RollAnimation->GetFName();
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForPIEReadyCommand(this, RunState));
	ADD_LATENT_AUTOMATION_COMMAND(FResetCharacterToStartCommand(this, RunState));
	ADD_LATENT_AUTOMATION_COMMAND(FObserveSameFrameDirectionalDodgeCommand(this, RunState));

	struct FDirectionCase
	{
		FVector2D Input;
		const TCHAR* Label;
		bool bRequirePhysicalContinuation;
	};
	const FDirectionCase DirectionCases[] = {
		{FVector2D::ZeroVector, TEXT("Neutral"), false},
		{FVector2D(0.0f, 1.0f), TEXT("Forward"), false},
		{FVector2D(1.0f, 1.0f), TEXT("ForwardRight"), false},
		{FVector2D(1.0f, 0.0f), TEXT("Right"), true},
		{FVector2D(1.0f, -1.0f), TEXT("BackwardRight"), false},
		{FVector2D(0.0f, -1.0f), TEXT("Backward"), false},
		{FVector2D(-1.0f, -1.0f), TEXT("BackwardLeft"), false},
		{FVector2D(-1.0f, 0.0f), TEXT("Left"), false},
		{FVector2D(-1.0f, 1.0f), TEXT("ForwardLeft"), false},
	};
	for (const FDirectionCase& DirectionCase : DirectionCases)
	{
		ADD_LATENT_AUTOMATION_COMMAND(FResetCharacterToStartCommand(this, RunState));
		ADD_LATENT_AUTOMATION_COMMAND(FObserveDirectionalDodgeCommand(
			this,
			RunState,
			DirectionCase.Input,
			DirectionCase.Label,
			DirectionCase.bRequirePhysicalContinuation));
	}

	ADD_LATENT_AUTOMATION_COMMAND(FResetCharacterToStartCommand(this, RunState));
	ADD_LATENT_AUTOMATION_COMMAND(FValidateDodgeTimelineCommand(this, RunState));
	ADD_LATENT_AUTOMATION_COMMAND(FResetCharacterToStartCommand(this, RunState));
	ADD_LATENT_AUTOMATION_COMMAND(FValidateCombatModeDodgeCommand(this, RunState));
	ADD_LATENT_AUTOMATION_COMMAND(FValidateDodgeRejectedDuringComboCommand(this, RunState));

	// Teardown remains unconditional even after a failed phase.
	ADD_LATENT_AUTOMATION_COMMAND(FReleaseAllInputCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForPIEEndCommand(this));
	return true;
}

#endif // WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
