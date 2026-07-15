// Copyright Project Requiem. All Rights Reserved.

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Animation/RequiemPlayerAnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Characters/RequiemCharacter.h"
#include "Components/RequiemCombatComponent.h"
#include "Components/SkeletalMeshComponent.h"
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
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "UObject/UnrealType.h"

namespace RequiemUnarmedCombatPIETest
{
constexpr TCHAR MapPackagePath[] = TEXT("/Game/ProjectRequiem/World/Maps/Dev/L_Dev_Foundation");
constexpr TCHAR MappingContextPath[] =
	TEXT("/Game/ProjectRequiem/Core/Input/IMC_Exploration.IMC_Exploration");
constexpr TCHAR MoveActionPath[] =
	TEXT("/Game/ProjectRequiem/Core/Input/Actions/IA_Move.IA_Move");
constexpr TCHAR ToggleCombatActionPath[] =
	TEXT("/Game/ProjectRequiem/Core/Input/Actions/IA_ToggleCombat.IA_ToggleCombat");
constexpr TCHAR PrimaryAttackActionPath[] =
	TEXT("/Game/ProjectRequiem/Core/Input/Actions/IA_PrimaryAttack.IA_PrimaryAttack");
constexpr TCHAR CharacterClassPath[] =
	TEXT("/Game/ProjectRequiem/Characters/Player/Blueprints/BP_CH_Player.BP_CH_Player_C");
constexpr TCHAR AnimInstanceClassPath[] =
	TEXT("/Game/ProjectRequiem/Characters/Player/Animations/Locomotion/"
		 "ABP_PR_PlayerLocomotion.ABP_PR_PlayerLocomotion_C");
constexpr TCHAR PlayerSkeletonPath[] =
	TEXT("/Game/ProjectRequiem/Characters/Player/Meshes/Temporary/"
		 "SKEL_Temp_SuperheroFemale.SKEL_Temp_SuperheroFemale");

const FName NormalStateName(TEXT("Normal"));
const FName CombatUnarmedStateName(TEXT("CombatUnarmed"));
const FName InactiveAnimationStateName(TEXT("Inactive"));
const FName EnterAnimationStateName(TEXT("Enter"));
const FName IdleAnimationStateName(TEXT("Idle"));
const FName AttackAnimationStateName(TEXT("Attack"));
const FName RecoveryAnimationStateName(TEXT("Recovery"));
const FName ExitAnimationStateName(TEXT("Exit"));

const FName JogForwardAnimationName(TEXT("Jog_Fwd_Loop"));
const FName CombatEnterAnimationName(TEXT("PunchKick_Enter"));
const FName CombatIdleAnimationName(TEXT("CombatUnarmed_Idle_Loop"));
const FName CombatExitAnimationName(TEXT("PunchKick_Exit"));

struct FClipExpectation
{
	const TCHAR* AssetPath;
	const TCHAR* PropertyName;
	FName AnimationName;
	FName PresentationStateName;
};

const FClipExpectation CombatClips[] = {
	{
		TEXT("/Game/ProjectRequiem/Characters/Player/Animations/Combat/Unarmed/UAL1/"
			 "PunchKick_Enter.PunchKick_Enter"),
		TEXT("CombatEnterAnimation"),
		CombatEnterAnimationName,
		EnterAnimationStateName,
	},
	{
		TEXT("/Game/ProjectRequiem/Characters/Player/Animations/Combat/Unarmed/"
			 "CombatUnarmed_Idle_Loop.CombatUnarmed_Idle_Loop"),
		TEXT("CombatIdleAnimation"),
		CombatIdleAnimationName,
		IdleAnimationStateName,
	},
	{
		TEXT("/Game/ProjectRequiem/Characters/Player/Animations/Combat/Unarmed/UAL1/"
			 "Punch_Cross.Punch_Cross"),
		TEXT("PunchCrossAnimation"),
		FName(TEXT("Punch_Cross")),
		AttackAnimationStateName,
	},
	{
		TEXT("/Game/ProjectRequiem/Characters/Player/Animations/Combat/Unarmed/UAL1/"
			 "Punch_Jab.Punch_Jab"),
		TEXT("PunchJabAnimation"),
		FName(TEXT("Punch_Jab")),
		AttackAnimationStateName,
	},
	{
		TEXT("/Game/ProjectRequiem/Characters/Player/Animations/Combat/Unarmed/UAL2/"
			 "Melee_Knee.Melee_Knee"),
		TEXT("MeleeKneeAnimation"),
		FName(TEXT("Melee_Knee")),
		AttackAnimationStateName,
	},
	{
		TEXT("/Game/ProjectRequiem/Characters/Player/Animations/Combat/Unarmed/UAL2/"
			 "Melee_Knee_Rec.Melee_Knee_Rec"),
		TEXT("MeleeKneeRecoveryAnimation"),
		FName(TEXT("Melee_Knee_Rec")),
		RecoveryAnimationStateName,
	},
	{
		TEXT("/Game/ProjectRequiem/Characters/Player/Animations/Combat/Unarmed/UAL2/"
			 "Melee_Hook.Melee_Hook"),
		TEXT("MeleeHookAnimation"),
		FName(TEXT("Melee_Hook")),
		AttackAnimationStateName,
	},
	{
		TEXT("/Game/ProjectRequiem/Characters/Player/Animations/Combat/Unarmed/UAL2/"
			 "Melee_Hook_Rec.Melee_Hook_Rec"),
		TEXT("MeleeHookRecoveryAnimation"),
		FName(TEXT("Melee_Hook_Rec")),
		RecoveryAnimationStateName,
	},
	{
		TEXT("/Game/ProjectRequiem/Characters/Player/Animations/Combat/Unarmed/UAL2/"
			 "Melee_Uppercut.Melee_Uppercut"),
		TEXT("MeleeUppercutAnimation"),
		FName(TEXT("Melee_Uppercut")),
		AttackAnimationStateName,
	},
	{
		TEXT("/Game/ProjectRequiem/Characters/Player/Animations/Combat/Unarmed/UAL1/"
			 "PunchKick_Exit.PunchKick_Exit"),
		TEXT("CombatExitAnimation"),
		CombatExitAnimationName,
		ExitAnimationStateName,
	},
};

constexpr int32 FirstComboClipIndex = 2;
constexpr int32 ComboClipCount = 7;
constexpr float ExpectedMaximumSpeed = 500.0f;
constexpr float ExpectedMaximumAcceleration = 2000.0f;
constexpr float ExpectedWalkingBraking = 2000.0f;

struct FRunState
{
	bool bAborted = false;
	int32 AttackSerialBeforeAutoEntry = INDEX_NONE;
};

enum class ETestCombatCommand : uint8
{
	EnterManual,
	Exit,
	PrimaryAttack
};

struct FCharacterSnapshot
{
	FVector Location = FVector::ZeroVector;
	float GroundSpeed = 0.0f;
	float MaximumSpeed = 0.0f;
	float MaximumAcceleration = 0.0f;
	float WalkingBrakingDeceleration = 0.0f;
	ERequiemCombatState CombatState = ERequiemCombatState::Normal;
	FName CombatStateName = NAME_None;
	int32 AttackRequestSerial = 0;
	bool bCanBlock = true;
	ERequiemCombatState ObservedCombatState = ERequiemCombatState::Normal;
	FName ObservedCombatStateName = NAME_None;
	FName CombatAnimationStateName = NAME_None;
	FName LocomotionStateName = NAME_None;
	int32 ActiveComboAnimationIndex = INDEX_NONE;
	bool bHasActivePresentationAnimation = false;
	bool bActivePresentationIsSequence = false;
	bool bActivePresentationUsesRootMotion = false;
	FName ActivePresentationAnimationName = NAME_None;
	bool bPresentationMontageIsPlaying = false;
	FName PresentationMontageSlot = NAME_None;
	float PresentationMontageBlendOutTriggerTime = -1.0f;
	ERootMotionMode::Type RootMotionMode = ERootMotionMode::NoRootMotionExtraction;
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

ERootMotionMode::Type ReadRootMotionMode(const UAnimInstance* AnimInstance)
{
	const FByteProperty* Property =
		FindFProperty<FByteProperty>(UAnimInstance::StaticClass(), TEXT("RootMotionMode"));
	return AnimInstance && Property
		? static_cast<ERootMotionMode::Type>(Property->GetPropertyValue_InContainer(AnimInstance))
		: ERootMotionMode::NoRootMotionExtraction;
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
	URequiemCombatComponent* CombatComponent = Character
		? Character->GetCombatComponent()
		: nullptr;
	URequiemPlayerAnimInstance* AnimInstance = Character && Character->GetMesh()
		? Cast<URequiemPlayerAnimInstance>(Character->GetMesh()->GetAnimInstance())
		: nullptr;
	if (!Character || !MovementComponent || !CombatComponent || !AnimInstance)
	{
		return false;
	}

	OutSnapshot.Location = Character->GetActorLocation();
	OutSnapshot.GroundSpeed = Character->GetVelocity().Size2D();
	OutSnapshot.MaximumSpeed = MovementComponent->GetMaxSpeed();
	OutSnapshot.MaximumAcceleration = MovementComponent->GetMaxAcceleration();
	OutSnapshot.WalkingBrakingDeceleration = MovementComponent->BrakingDecelerationWalking;
	OutSnapshot.CombatState = CombatComponent->GetCombatState();
	OutSnapshot.CombatStateName = CombatComponent->GetCombatStateName();
	OutSnapshot.AttackRequestSerial = CombatComponent->GetAttackRequestSerial();
	OutSnapshot.bCanBlock = CombatComponent->CanBlock();
	OutSnapshot.ObservedCombatState = AnimInstance->GetObservedCombatState();
	OutSnapshot.ObservedCombatStateName = AnimInstance->GetObservedCombatStateName();
	OutSnapshot.CombatAnimationStateName = AnimInstance->GetCombatAnimationStateName();
	OutSnapshot.LocomotionStateName = AnimInstance->GetLocomotionStateName();
	OutSnapshot.ActiveComboAnimationIndex = AnimInstance->GetActiveComboAnimationIndex();
	const UAnimSequenceBase* ActiveAnimation = AnimInstance->GetActivePresentationAnimation();
	const UAnimSequence* ActiveSequence = Cast<UAnimSequence>(ActiveAnimation);
	const UAnimMontage* ActiveMontage = AnimInstance->GetCurrentActiveMontage();
	OutSnapshot.bHasActivePresentationAnimation = ActiveAnimation != nullptr;
	OutSnapshot.bActivePresentationIsSequence = ActiveSequence != nullptr;
	OutSnapshot.bActivePresentationUsesRootMotion =
		ActiveSequence && ActiveSequence->bEnableRootMotion;
	OutSnapshot.ActivePresentationAnimationName =
		ActiveAnimation ? ActiveAnimation->GetFName() : NAME_None;
	OutSnapshot.bPresentationMontageIsPlaying =
		ActiveMontage && AnimInstance->Montage_IsPlaying(ActiveMontage);
	OutSnapshot.PresentationMontageSlot =
		ActiveMontage && !ActiveMontage->SlotAnimTracks.IsEmpty()
			? ActiveMontage->SlotAnimTracks[0].SlotName
			: NAME_None;
	OutSnapshot.PresentationMontageBlendOutTriggerTime = ActiveMontage
		? ActiveMontage->BlendOutTriggerTime
		: -1.0f;
	OutSnapshot.RootMotionMode = ReadRootMotionMode(AnimInstance);
	return true;
}

bool HasConfiguredMovement(const FCharacterSnapshot& Snapshot)
{
	return FMath::IsNearlyEqual(Snapshot.MaximumSpeed, ExpectedMaximumSpeed, 1.0f)
		&& FMath::IsNearlyEqual(
			Snapshot.MaximumAcceleration,
			ExpectedMaximumAcceleration,
			5.0f)
		&& FMath::IsNearlyEqual(
			Snapshot.WalkingBrakingDeceleration,
			ExpectedWalkingBraking,
			5.0f);
}

bool HasValidPresentationPlayback(const FCharacterSnapshot& Snapshot)
{
	return Snapshot.bHasActivePresentationAnimation
		&& Snapshot.bActivePresentationIsSequence
		&& !Snapshot.bActivePresentationUsesRootMotion
		&& Snapshot.bPresentationMontageIsPlaying
		&& Snapshot.PresentationMontageSlot == FName(TEXT("DefaultSlot"))
		&& Snapshot.RootMotionMode == ERootMotionMode::IgnoreRootMotion;
}

bool HasValidCombatOneShotPlayback(const FCharacterSnapshot& Snapshot)
{
	return HasValidPresentationPlayback(Snapshot)
		&& FMath::IsNearlyZero(Snapshot.PresentationMontageBlendOutTriggerTime);
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

bool InvokeCombatContract(const ETestCombatCommand Command)
{
	APlayerController* PlayerController = FindPIEPlayerController();
	ARequiemCharacter* Character = PlayerController
		? Cast<ARequiemCharacter>(PlayerController->GetPawn())
		: nullptr;
	URequiemCombatComponent* CombatComponent = Character
		? Character->GetCombatComponent()
		: nullptr;
	if (!CombatComponent)
	{
		return false;
	}

	switch (Command)
	{
	case ETestCombatCommand::EnterManual:
		CombatComponent->EnterUnarmedCombat(ERequiemCombatEntryReason::Manual);
		return CombatComponent->GetCombatState() == ERequiemCombatState::CombatUnarmed;

	case ETestCombatCommand::Exit:
		CombatComponent->ExitCombat();
		return CombatComponent->GetCombatState() == ERequiemCombatState::Normal;

	case ETestCombatCommand::PrimaryAttack:
	{
		const int32 SerialBeforeRequest = CombatComponent->GetAttackRequestSerial();
		CombatComponent->RequestUnarmedAttack();
		return CombatComponent->GetCombatState() == ERequiemCombatState::CombatUnarmed
			&& CombatComponent->GetAttackRequestSerial() != SerialBeforeRequest;
	}

	default:
		return false;
	}
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
			TEXT("Unarmed combat PIE stage timed out after %.1fs: %s"),
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

			// Automation may briefly create and tear down an initial PIE world before
			// the durable play world is ready. Do not mutate gameplay state until the
			// same world and pawn have survived that handoff window.
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

			const UEnhancedInputComponent* InputComponent = Character
				? Character->FindComponentByClass<UEnhancedInputComponent>()
				: nullptr;
			const UInputAction* ToggleAction = LoadObject<UInputAction>(
				nullptr,
				ToggleCombatActionPath);
			const UInputAction* AttackAction = LoadObject<UInputAction>(
				nullptr,
				PrimaryAttackActionPath);
			const int32 ToggleBindingCount = CountStartedBindings(InputComponent, ToggleAction);
			const int32 AttackBindingCount = CountStartedBindings(InputComponent, AttackAction);
			if (InputComponent && ToggleAction && AttackAction
				&& (ToggleBindingCount != 1 || AttackBindingCount != 1))
			{
				return AbortWithError(FString::Printf(
					TEXT("Runtime combat bindings are invalid: Z Started=%d, LMB Started=%d (expected one each)."),
					ToggleBindingCount,
					AttackBindingCount));
			}

			const bool bStartsNormal =
				Snapshot.CombatState == ERequiemCombatState::Normal
				&& Snapshot.CombatStateName == NormalStateName
				&& Snapshot.ObservedCombatState == ERequiemCombatState::Normal
				&& Snapshot.ObservedCombatStateName == NormalStateName
				&& Snapshot.CombatAnimationStateName == InactiveAnimationStateName;
			if (bStartsNormal
				&& InputComponent
				&& ToggleBindingCount == 1
				&& AttackBindingCount == 1
				&& !Snapshot.bCanBlock
				&& Snapshot.RootMotionMode == ERootMotionMode::IgnoreRootMotion
				&& HasConfiguredMovement(Snapshot))
			{
				Test->AddInfo(TEXT("PIE started in Normal with unarmed blocking disabled."));
				return true;
			}
		}

		return AbortIfTimedOut(
			ElapsedSeconds,
			TEXT("waiting for a possessed Normal player with combat presentation ready"));
	}

private:
	TWeakObjectPtr<UWorld> StableWorld;
	TWeakObjectPtr<ARequiemCharacter> StableCharacter;
	double StableSinceSeconds = -1.0;
};

class FInvokeCombatContractCommand final : public FTimedCommand
{
public:
	FInvokeCombatContractCommand(
		FAutomationTestBase* InTest,
		TSharedRef<FRunState> InRunState,
		const ETestCombatCommand InCommand,
		const TCHAR* InDescription)
		: FTimedCommand(InTest, MoveTemp(InRunState), 2.0)
		, Command(InCommand)
		, Description(InDescription)
	{
	}

	virtual bool Update() override
	{
		if (ShouldSkip())
		{
			return true;
		}

		const double ElapsedSeconds = GetElapsedSeconds();
		if (!InvokeCombatContract(Command))
		{
			return AbortIfTimedOut(ElapsedSeconds, FString::Printf(
				TEXT("invoking %s"), Description));
		}

		return true;
	}

private:
	ETestCombatCommand Command;
	const TCHAR* Description;
};

class FWaitForManualCombatIdleCommand final : public FTimedCommand
{
public:
	FWaitForManualCombatIdleCommand(
		FAutomationTestBase* InTest,
		TSharedRef<FRunState> InRunState)
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
		FCharacterSnapshot Snapshot;
		if (!ReadCharacterSnapshot(Snapshot))
		{
			return AbortIfTimedOut(ElapsedSeconds, TEXT("reading manual combat entry"));
		}

		if (Snapshot.bActivePresentationUsesRootMotion
			|| Snapshot.RootMotionMode != ERootMotionMode::IgnoreRootMotion)
		{
			return AbortWithError(TEXT("Manual combat entry enabled root motion."));
		}

		bSawEnter |= Snapshot.CombatAnimationStateName == EnterAnimationStateName
			&& Snapshot.ActivePresentationAnimationName == CombatEnterAnimationName
			&& HasValidCombatOneShotPlayback(Snapshot);
		const bool bReachedCombatIdle =
			Snapshot.CombatState == ERequiemCombatState::CombatUnarmed
			&& Snapshot.CombatStateName == CombatUnarmedStateName
			&& Snapshot.ObservedCombatState == ERequiemCombatState::CombatUnarmed
			&& Snapshot.ObservedCombatStateName == CombatUnarmedStateName
			&& Snapshot.CombatAnimationStateName == IdleAnimationStateName
			&& Snapshot.ActiveComboAnimationIndex == INDEX_NONE
			&& Snapshot.ActivePresentationAnimationName == CombatIdleAnimationName
			&& HasValidPresentationPlayback(Snapshot)
			&& !Snapshot.bCanBlock;
		if (bSawEnter && bReachedCombatIdle)
		{
			Test->AddInfo(TEXT("Z played PunchKick_Enter and reached the unarmed combat idle."));
			return true;
		}

		return AbortIfTimedOut(
			ElapsedSeconds,
			FString::Printf(
				TEXT("manual entry (combat=%s, presentation=%s, animation=%s, sawEnter=%d)"),
				*Snapshot.CombatStateName.ToString(),
				*Snapshot.CombatAnimationStateName.ToString(),
				*Snapshot.ActivePresentationAnimationName.ToString(),
				bSawEnter));
	}

private:
	bool bSawEnter = false;
};

class FMoveInCombatCommand final : public FTimedCommand
{
public:
	FMoveInCombatCommand(FAutomationTestBase* InTest, TSharedRef<FRunState> InRunState)
		: FTimedCommand(InTest, MoveTemp(InRunState), 5.0)
	{
	}

	virtual bool Update() override
	{
		if (ShouldSkip())
		{
			StopContinuousAction(MoveActionPath);
			return true;
		}

		const double ElapsedSeconds = GetElapsedSeconds();
		if (!SetContinuousAction(MoveActionPath, FInputActionValue(FVector2D(1.0, 1.0))))
		{
			return AbortIfTimedOut(ElapsedSeconds, TEXT("injecting diagonal movement in combat"));
		}

		FCharacterSnapshot Snapshot;
		if (!ReadCharacterSnapshot(Snapshot))
		{
			return AbortIfTimedOut(ElapsedSeconds, TEXT("reading diagonal combat movement"));
		}
		if (!bHasStartLocation)
		{
			StartLocation = Snapshot.Location;
			bHasStartLocation = true;
		}

		if (Snapshot.CombatState != ERequiemCombatState::CombatUnarmed
			|| Snapshot.ObservedCombatState != ERequiemCombatState::CombatUnarmed
			|| Snapshot.bCanBlock)
		{
			return AbortWithError(TEXT("Diagonal movement changed the unarmed combat contract."));
		}
		if (Snapshot.bActivePresentationUsesRootMotion
			|| Snapshot.RootMotionMode != ERootMotionMode::IgnoreRootMotion)
		{
			return AbortWithError(TEXT("Combat movement used root motion."));
		}

		const float Displacement = (Snapshot.Location - StartLocation).Size2D();
		if (HasConfiguredMovement(Snapshot)
			&& Snapshot.GroundSpeed >= ExpectedMaximumSpeed * 0.95f
			&& Displacement >= 100.0f)
		{
			StopContinuousAction(MoveActionPath);
			Test->AddInfo(TEXT("W+D movement remained owned by CharacterMovement in CombatUnarmed."));
			return true;
		}

		return AbortIfTimedOut(
			ElapsedSeconds,
			FString::Printf(
				TEXT("diagonal combat movement (speed=%.1f, distance=%.1f, combat=%s)"),
				Snapshot.GroundSpeed,
				Displacement,
				*Snapshot.CombatStateName.ToString()));
	}

private:
	FVector StartLocation = FVector::ZeroVector;
	bool bHasStartLocation = false;
};

class FWaitForExitToNormalCommand final : public FTimedCommand
{
public:
	FWaitForExitToNormalCommand(
		FAutomationTestBase* InTest,
		TSharedRef<FRunState> InRunState,
		const TCHAR* InStageDescription)
		: FTimedCommand(InTest, MoveTemp(InRunState), 5.0)
		, StageDescription(InStageDescription)
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
			return AbortIfTimedOut(ElapsedSeconds, FString::Printf(
				TEXT("reading %s"), StageDescription));
		}

		if (Snapshot.bActivePresentationUsesRootMotion
			|| Snapshot.RootMotionMode != ERootMotionMode::IgnoreRootMotion)
		{
			return AbortWithError(FString::Printf(TEXT("%s used root motion."), StageDescription));
		}

		bSawExit |= Snapshot.CombatAnimationStateName == ExitAnimationStateName
			&& Snapshot.ActivePresentationAnimationName == CombatExitAnimationName
			&& HasValidCombatOneShotPlayback(Snapshot);
		const bool bReachedNormal =
			Snapshot.CombatState == ERequiemCombatState::Normal
			&& Snapshot.CombatStateName == NormalStateName
			&& Snapshot.ObservedCombatState == ERequiemCombatState::Normal
			&& Snapshot.ObservedCombatStateName == NormalStateName
			&& Snapshot.CombatAnimationStateName == InactiveAnimationStateName
			&& Snapshot.ActiveComboAnimationIndex == INDEX_NONE
			&& !Snapshot.bCanBlock;
		if (bSawExit && bReachedNormal)
		{
			Test->AddInfo(FString::Printf(TEXT("%s played PunchKick_Exit and returned to Normal."), StageDescription));
			return true;
		}

		return AbortIfTimedOut(
			ElapsedSeconds,
			FString::Printf(
				TEXT("%s (combat=%s, presentation=%s, animation=%s, sawExit=%d)"),
				StageDescription,
				*Snapshot.CombatStateName.ToString(),
				*Snapshot.CombatAnimationStateName.ToString(),
				*Snapshot.ActivePresentationAnimationName.ToString(),
				bSawExit));
	}

private:
	const TCHAR* StageDescription;
	bool bSawExit = false;
};

class FRecordAttackSerialCommand final : public FTimedCommand
{
public:
	FRecordAttackSerialCommand(
		FAutomationTestBase* InTest,
		TSharedRef<FRunState> InRunState)
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
			return AbortIfTimedOut(ElapsedSeconds, TEXT("recording the attack serial"));
		}
		if (Snapshot.CombatState != ERequiemCombatState::Normal)
		{
			return AbortWithError(TEXT("Attack serial baseline was not recorded in Normal."));
		}

		RunState->AttackSerialBeforeAutoEntry = Snapshot.AttackRequestSerial;
		return true;
	}
};

double GetQueueDelayForComboIndex(const int32 ComboIndex)
{
	switch (ComboIndex)
	{
	case 0: // Punch_Cross
		return 0.45;
	case 1: // Punch_Jab
		return 0.39;
	case 2: // Melee_Knee
		return 0.39;
	case 4: // Melee_Hook
		return 0.21;
	default:
		return -1.0;
	}
}

class FObserveAutoEntryComboCommand final : public FTimedCommand
{
public:
	FObserveAutoEntryComboCommand(
		FAutomationTestBase* InTest,
		TSharedRef<FRunState> InRunState)
		: FTimedCommand(InTest, MoveTemp(InRunState), 14.0)
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
		if (bWaitingForFinalIdle || bSawMeaningfulMovement)
		{
			StopContinuousAction(MoveActionPath);
		}
		else if (!SetContinuousAction(
			MoveActionPath,
			FInputActionValue(FVector2D(0.35, 0.35))))
		{
			return AbortIfTimedOut(ElapsedSeconds, TEXT("injecting movement during the combo"));
		}
		FCharacterSnapshot Snapshot;
		if (!ReadCharacterSnapshot(Snapshot))
		{
			return AbortIfTimedOut(ElapsedSeconds, TEXT("reading the unarmed combo"));
		}
		if (!bHasStartLocation)
		{
			StartLocation = Snapshot.Location;
			bHasStartLocation = true;
		}

		bSawMeaningfulMovement |= Snapshot.GroundSpeed >= 50.0f
			&& (Snapshot.Location - StartLocation).Size2D() >= 100.0f;
		if (Snapshot.bActivePresentationUsesRootMotion
			|| Snapshot.RootMotionMode != ERootMotionMode::IgnoreRootMotion)
		{
			return AbortWithError(FString::Printf(
				TEXT("Combo presentation %s used root motion."),
				*Snapshot.ActivePresentationAnimationName.ToString()));
		}
		if (Snapshot.bCanBlock)
		{
			return AbortWithError(TEXT("Blocking became available during unarmed combat."));
		}

		if (Snapshot.ActivePresentationAnimationName == CombatEnterAnimationName)
		{
			bSawAutomaticEnter |= Snapshot.CombatAnimationStateName == EnterAnimationStateName
				&& Snapshot.CombatState == ERequiemCombatState::CombatUnarmed
				&& HasValidCombatOneShotPlayback(Snapshot);
		}

		const int32 ComboIndex = FindExpectedComboIndex(Snapshot.ActivePresentationAnimationName);
		if (ComboIndex != INDEX_NONE && Snapshot.ActivePresentationAnimationName != LastObservedComboClip)
		{
			if (!bSawAutomaticEnter)
			{
				return AbortWithError(TEXT("LMB started a combo clip before PunchKick_Enter was observed."));
			}
			if (ComboIndex != ObservedComboClipCount)
			{
				return AbortWithError(FString::Printf(
					TEXT("Combo order mismatch: expected index %d but observed %s at index %d."),
					ObservedComboClipCount,
					*Snapshot.ActivePresentationAnimationName.ToString(),
					ComboIndex));
			}

			const FClipExpectation& Expectation = CombatClips[FirstComboClipIndex + ComboIndex];
			if (Snapshot.CombatAnimationStateName != Expectation.PresentationStateName
				|| Snapshot.ActiveComboAnimationIndex != ComboIndex
				|| !HasValidCombatOneShotPlayback(Snapshot))
			{
				return AbortWithError(FString::Printf(
					TEXT("Combo clip %s had state=%s index=%d playback=%d."),
					*Snapshot.ActivePresentationAnimationName.ToString(),
					*Snapshot.CombatAnimationStateName.ToString(),
					Snapshot.ActiveComboAnimationIndex,
					HasValidPresentationPlayback(Snapshot)));
			}

			LastObservedComboClip = Snapshot.ActivePresentationAnimationName;
			CurrentComboClipStartSeconds = ElapsedSeconds;
			++ObservedComboClipCount;
		}

		if (ComboIndex != INDEX_NONE)
		{
			QueueNextAttackIfNeeded(ComboIndex, ElapsedSeconds);
		}

		if (ObservedComboClipCount == ComboClipCount
			&& Snapshot.ActiveComboAnimationIndex == INDEX_NONE)
		{
			StopContinuousAction(MoveActionPath);
			bWaitingForFinalIdle = true;
		}

		const int32 ExpectedSerial = RunState->AttackSerialBeforeAutoEntry + 5;
		const bool bReachedFinalIdle = bWaitingForFinalIdle
			&& Snapshot.CombatState == ERequiemCombatState::CombatUnarmed
			&& Snapshot.ObservedCombatState == ERequiemCombatState::CombatUnarmed
			&& Snapshot.CombatAnimationStateName == IdleAnimationStateName
			&& Snapshot.ActiveComboAnimationIndex == INDEX_NONE
			&& Snapshot.ActivePresentationAnimationName == CombatIdleAnimationName
			&& HasValidPresentationPlayback(Snapshot)
			&& Snapshot.AttackRequestSerial == ExpectedSerial
			&& AdditionalAttackBindingsExecuted == 4
			&& bSawMeaningfulMovement
			&& HasConfiguredMovement(Snapshot);
		if (bSawAutomaticEnter && bReachedFinalIdle)
		{
			Test->AddInfo(TEXT("LMB auto-entered combat and five clicks played all seven combo clips in order."));
			return true;
		}

		return AbortIfTimedOut(
			ElapsedSeconds,
			FString::Printf(
				TEXT("auto-entry combo (combat=%s, presentation=%s, animation=%s, clips=%d/7, extraPulses=%d, serial=%d/%d, moved=%d)"),
				*Snapshot.CombatStateName.ToString(),
				*Snapshot.CombatAnimationStateName.ToString(),
				*Snapshot.ActivePresentationAnimationName.ToString(),
				ObservedComboClipCount,
				AdditionalAttackBindingsExecuted,
				Snapshot.AttackRequestSerial,
				ExpectedSerial,
				bSawMeaningfulMovement));
	}

private:
	int32 FindExpectedComboIndex(const FName AnimationName) const
	{
		for (int32 Index = 0; Index < ComboClipCount; ++Index)
		{
			if (CombatClips[FirstComboClipIndex + Index].AnimationName == AnimationName)
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}

	void QueueNextAttackIfNeeded(const int32 ComboIndex, const double ElapsedSeconds)
	{
		const double QueueDelay = GetQueueDelayForComboIndex(ComboIndex);
		if (QueueDelay < 0.0
			|| QueuedFromComboIndices.Contains(ComboIndex)
			|| ElapsedSeconds - CurrentComboClipStartSeconds < QueueDelay)
		{
			return;
		}

		if (InvokeCombatContract(ETestCombatCommand::PrimaryAttack))
		{
			QueuedFromComboIndices.Add(ComboIndex);
			++AdditionalAttackBindingsExecuted;
		}
	}

	FVector StartLocation = FVector::ZeroVector;
	TSet<int32> QueuedFromComboIndices;
	FName LastObservedComboClip = NAME_None;
	double CurrentComboClipStartSeconds = 0.0;
	int32 ObservedComboClipCount = 0;
	int32 AdditionalAttackBindingsExecuted = 0;
	bool bHasStartLocation = false;
	bool bSawAutomaticEnter = false;
	bool bSawMeaningfulMovement = false;
	bool bWaitingForFinalIdle = false;
};

class FMoveAfterCombatCommand final : public FTimedCommand
{
public:
	FMoveAfterCombatCommand(FAutomationTestBase* InTest, TSharedRef<FRunState> InRunState)
		: FTimedCommand(InTest, MoveTemp(InRunState), 5.0)
	{
	}

	virtual bool Update() override
	{
		if (ShouldSkip())
		{
			StopContinuousAction(MoveActionPath);
			return true;
		}

		const double ElapsedSeconds = GetElapsedSeconds();
		if (!SetContinuousAction(MoveActionPath, FInputActionValue(FVector2D(0.0, 1.0))))
		{
			return AbortIfTimedOut(ElapsedSeconds, TEXT("injecting movement after combat"));
		}

		FCharacterSnapshot Snapshot;
		if (!ReadCharacterSnapshot(Snapshot))
		{
			return AbortIfTimedOut(ElapsedSeconds, TEXT("reading movement after combat"));
		}
		if (!bHasStartLocation)
		{
			StartLocation = Snapshot.Location;
			bHasStartLocation = true;
		}

		if (Snapshot.CombatState != ERequiemCombatState::Normal
			|| Snapshot.ObservedCombatState != ERequiemCombatState::Normal
			|| Snapshot.CombatAnimationStateName != InactiveAnimationStateName)
		{
			return AbortWithError(TEXT("Movement after PunchKick_Exit did not remain in Normal."));
		}

		const float Displacement = (Snapshot.Location - StartLocation).Size2D();
		if (Snapshot.GroundSpeed >= ExpectedMaximumSpeed * 0.9f
			&& Displacement >= 100.0f
			&& Snapshot.LocomotionStateName == FName(TEXT("Jog"))
			&& Snapshot.ActivePresentationAnimationName == JogForwardAnimationName
			&& HasValidPresentationPlayback(Snapshot)
			&& HasConfiguredMovement(Snapshot))
		{
			StopContinuousAction(MoveActionPath);
			Test->AddInfo(TEXT("Normal Jog movement continued after leaving combat."));
			return true;
		}

		return AbortIfTimedOut(
			ElapsedSeconds,
			FString::Printf(
				TEXT("movement after combat (speed=%.1f, distance=%.1f, locomotion=%s, animation=%s)"),
				Snapshot.GroundSpeed,
				Displacement,
				*Snapshot.LocomotionStateName.ToString(),
				*Snapshot.ActivePresentationAnimationName.ToString()));
	}

private:
	FVector StartLocation = FVector::ZeroVector;
	bool bHasStartLocation = false;
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
	FRequiemUnarmedCombatPIETest,
	"ProjectRequiem.Combat.UnarmedPIE",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::NonNullRHI
		| EAutomationTestFlags::ProductFilter)

bool FRequiemUnarmedCombatPIETest::RunTest(const FString& Parameters)
{
	using namespace RequiemUnarmedCombatPIETest;

	const UInputMappingContext* MappingContext = LoadObject<UInputMappingContext>(
		nullptr,
		MappingContextPath);
	const UInputAction* ToggleCombatAction = LoadObject<UInputAction>(
		nullptr,
		ToggleCombatActionPath);
	const UInputAction* PrimaryAttackAction = LoadObject<UInputAction>(
		nullptr,
		PrimaryAttackActionPath);
	TestNotNull(TEXT("IMC_Exploration exists"), MappingContext);
	TestNotNull(TEXT("IA_ToggleCombat exists"), ToggleCombatAction);
	TestNotNull(TEXT("IA_PrimaryAttack exists"), PrimaryAttackAction);
	if (!MappingContext || !ToggleCombatAction || !PrimaryAttackAction)
	{
		return false;
	}

	TestEqual(
		TEXT("IA_ToggleCombat is Boolean"),
		ToggleCombatAction->ValueType,
		EInputActionValueType::Boolean);
	TestEqual(
		TEXT("IA_PrimaryAttack is Boolean"),
		PrimaryAttackAction->ValueType,
		EInputActionValueType::Boolean);
	TestTrue(
		TEXT("IMC_Exploration maps Z to IA_ToggleCombat"),
		HasExactMapping(MappingContext, ToggleCombatAction, EKeys::Z));
	TestTrue(
		TEXT("IMC_Exploration maps LMB to IA_PrimaryAttack"),
		HasExactMapping(MappingContext, PrimaryAttackAction, EKeys::LeftMouseButton));

	UClass* CharacterClass = LoadClass<ARequiemCharacter>(nullptr, CharacterClassPath);
	ARequiemCharacter* CharacterCDO = CharacterClass
		? Cast<ARequiemCharacter>(CharacterClass->GetDefaultObject())
		: nullptr;
	TestNotNull(TEXT("BP_CH_Player generated class exists"), CharacterClass);
	TestNotNull(TEXT("BP_CH_Player CDO exists"), CharacterCDO);
	if (CharacterCDO)
	{
		TestEqual(
			TEXT("BP_CH_Player assigns IA_ToggleCombat"),
			GetObjectPropertyValue(CharacterCDO, CharacterClass, TEXT("ToggleCombatAction")),
			static_cast<UObject*>(const_cast<UInputAction*>(ToggleCombatAction)));
		TestEqual(
			TEXT("BP_CH_Player assigns IA_PrimaryAttack"),
			GetObjectPropertyValue(CharacterCDO, CharacterClass, TEXT("PrimaryAttackAction")),
			static_cast<UObject*>(const_cast<UInputAction*>(PrimaryAttackAction)));
		URequiemCombatComponent* CombatCDO = CharacterCDO->GetCombatComponent();
		TestNotNull(TEXT("BP_CH_Player owns RequiemCombatComponent"), CombatCDO);
		if (CombatCDO)
		{
			TestEqual(
				TEXT("Combat CDO starts in Normal"),
				CombatCDO->GetCombatState(),
				ERequiemCombatState::Normal);
			TestFalse(TEXT("Unarmed combat cannot block"), CombatCDO->CanBlock());
			TestEqual(
				TEXT("Future combat auto-exit delay remains 30 seconds"),
				CombatCDO->GetAutoExitDelay(),
				30.0f);
			TestFalse(
				TEXT("Nearby enemies prevent future auto-exit"),
				CombatCDO->CanAutoExit(true));
		}
	}

	UClass* AnimInstanceClass = LoadClass<URequiemPlayerAnimInstance>(nullptr, AnimInstanceClassPath);
	URequiemPlayerAnimInstance* AnimInstanceCDO = AnimInstanceClass
		? Cast<URequiemPlayerAnimInstance>(AnimInstanceClass->GetDefaultObject())
		: nullptr;
	TestNotNull(TEXT("ABP_PR_PlayerLocomotion generated class exists"), AnimInstanceClass);
	TestNotNull(TEXT("ABP_PR_PlayerLocomotion CDO exists"), AnimInstanceCDO);
	if (AnimInstanceCDO)
	{
		TestEqual(
			TEXT("ABP ignores root motion"),
			ReadRootMotionMode(AnimInstanceCDO),
			ERootMotionMode::IgnoreRootMotion);
	}

	USkeleton* PlayerSkeleton = LoadObject<USkeleton>(nullptr, PlayerSkeletonPath);
	TestNotNull(TEXT("Player skeleton exists"), PlayerSkeleton);
	for (const FClipExpectation& Clip : CombatClips)
	{
		UAnimSequence* Animation = LoadObject<UAnimSequence>(nullptr, Clip.AssetPath);
		TestNotNull(
			*FString::Printf(TEXT("Combat animation %s exists"), *Clip.AnimationName.ToString()),
			Animation);
		if (!Animation)
		{
			continue;
		}

		TestFalse(
			*FString::Printf(TEXT("%s has root motion disabled"), *Clip.AnimationName.ToString()),
			Animation->bEnableRootMotion);
		TestEqual(
			*FString::Printf(TEXT("%s uses the player skeleton"), *Clip.AnimationName.ToString()),
			Animation->GetSkeleton(),
			PlayerSkeleton);
		if (AnimInstanceCDO && AnimInstanceClass)
		{
			TestEqual(
				*FString::Printf(TEXT("ABP assigns %s"), Clip.PropertyName),
				GetObjectPropertyValue(AnimInstanceCDO, AnimInstanceClass, Clip.PropertyName),
				static_cast<UObject*>(Animation));
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

	ADD_LATENT_AUTOMATION_COMMAND(FInvokeCombatContractCommand(
		this,
		RunState,
		ETestCombatCommand::EnterManual,
		TEXT("manual combat entry contract (Z mapping verified)")));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForManualCombatIdleCommand(this, RunState));
	ADD_LATENT_AUTOMATION_COMMAND(FMoveInCombatCommand(this, RunState));
	ADD_LATENT_AUTOMATION_COMMAND(FInvokeCombatContractCommand(
		this,
		RunState,
		ETestCombatCommand::Exit,
		TEXT("manual combat exit contract (Z mapping verified)")));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForExitToNormalCommand(
		this, RunState, TEXT("manual Z exit")));

	ADD_LATENT_AUTOMATION_COMMAND(FRecordAttackSerialCommand(this, RunState));
	ADD_LATENT_AUTOMATION_COMMAND(FInvokeCombatContractCommand(
		this,
		RunState,
		ETestCombatCommand::PrimaryAttack,
		TEXT("initial primary attack contract (LMB mapping verified)")));
	ADD_LATENT_AUTOMATION_COMMAND(FObserveAutoEntryComboCommand(this, RunState));

	ADD_LATENT_AUTOMATION_COMMAND(FInvokeCombatContractCommand(
		this,
		RunState,
		ETestCombatCommand::Exit,
		TEXT("post-combo combat exit contract (Z mapping verified)")));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForExitToNormalCommand(
		this, RunState, TEXT("post-combo Z exit")));
	ADD_LATENT_AUTOMATION_COMMAND(FMoveAfterCombatCommand(this, RunState));

	// Every failed phase only marks the shared run state, so teardown remains unconditional.
	ADD_LATENT_AUTOMATION_COMMAND(FReleaseAllInputCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForPIEEndCommand(this));
	return true;
}

#endif // WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
