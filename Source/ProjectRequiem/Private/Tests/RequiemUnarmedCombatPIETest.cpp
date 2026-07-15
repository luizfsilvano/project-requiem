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
const FName JogBackwardAnimationName(TEXT("Jog_Bwd_Loop"));
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
constexpr float ExpectedUnarmedAttackPlayRate = 1.25f;
constexpr float ExpectedUnarmedRecoveryPlayRate = 1.35f;
constexpr float ExpectedUnarmedInputWindowStart = 0.30f;
constexpr float ExpectedUnarmedInputWindowEnd = 0.85f;
constexpr float ExpectedQueuedAttackHandoff = 0.72f;
constexpr float ExpectedAutomaticRecoveryHandoff = 0.90f;
constexpr float ExpectedQueuedRecoveryHandoff = 0.55f;
constexpr float ExpectedUnarmedMovementUnlock = 0.60f;
constexpr float MovementUnlockSamplingTolerance = 0.02f;
constexpr float ExpectedUnarmedLungeSpeed = 350.0f;
constexpr int32 InputSpamCount = 6;

struct FRunState
{
	bool bAborted = false;
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
	FVector Velocity = FVector::ZeroVector;
	FVector Acceleration = FVector::ZeroVector;
	FVector PendingMovementInput = FVector::ZeroVector;
	FVector LastMovementInput = FVector::ZeroVector;
	FVector ActorForward = FVector::ForwardVector;
	float GroundSpeed = 0.0f;
	float MaximumSpeed = 0.0f;
	float MaximumAcceleration = 0.0f;
	float WalkingBrakingDeceleration = 0.0f;
	ERequiemCombatState CombatState = ERequiemCombatState::Normal;
	FName CombatStateName = NAME_None;
	int32 AttackRequestSerial = 0;
	bool bUnarmedAttackActive = false;
	bool bUnarmedAttackInputWindowOpen = false;
	bool bQueuedUnarmedFollowUp = false;
	bool bUnarmedAttackMovementLocked = false;
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
	float PresentationMontagePlayRate = 1.0f;
	float PresentationMontagePosition = 0.0f;
	float PresentationAnimationLength = 0.0f;
	float PresentationNormalizedTime = 0.0f;
	float CombatNormalizedTime = 0.0f;
	float ActivePresentationPlayRate = 1.0f;
	bool bHasRootMotionSources = false;
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
	OutSnapshot.Velocity = Character->GetVelocity();
	OutSnapshot.Acceleration = MovementComponent->GetCurrentAcceleration();
	OutSnapshot.PendingMovementInput = Character->GetPendingMovementInputVector();
	OutSnapshot.LastMovementInput = Character->GetLastMovementInputVector();
	OutSnapshot.ActorForward = Character->GetActorForwardVector().GetSafeNormal2D();
	OutSnapshot.GroundSpeed = Character->GetVelocity().Size2D();
	OutSnapshot.MaximumSpeed = MovementComponent->GetMaxSpeed();
	OutSnapshot.MaximumAcceleration = MovementComponent->GetMaxAcceleration();
	OutSnapshot.WalkingBrakingDeceleration = MovementComponent->BrakingDecelerationWalking;
	OutSnapshot.CombatState = CombatComponent->GetCombatState();
	OutSnapshot.CombatStateName = CombatComponent->GetCombatStateName();
	OutSnapshot.AttackRequestSerial = CombatComponent->GetAttackRequestSerial();
	OutSnapshot.bUnarmedAttackActive = CombatComponent->IsUnarmedAttackActive();
	OutSnapshot.bUnarmedAttackInputWindowOpen =
		CombatComponent->IsUnarmedAttackInputWindowOpen();
	OutSnapshot.bQueuedUnarmedFollowUp = CombatComponent->HasQueuedUnarmedFollowUp();
	OutSnapshot.bUnarmedAttackMovementLocked =
		CombatComponent->IsUnarmedAttackMovementLocked();
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
	OutSnapshot.PresentationMontagePlayRate = ActiveMontage
		? AnimInstance->Montage_GetPlayRate(ActiveMontage)
		: 1.0f;
	OutSnapshot.PresentationMontagePosition = ActiveMontage
		? AnimInstance->Montage_GetPosition(ActiveMontage)
		: 0.0f;
	OutSnapshot.PresentationAnimationLength = ActiveAnimation
		? ActiveAnimation->GetPlayLength()
		: 0.0f;
	OutSnapshot.PresentationNormalizedTime =
		OutSnapshot.PresentationAnimationLength > UE_KINDA_SMALL_NUMBER
			? FMath::Clamp(
				OutSnapshot.PresentationMontagePosition
					/ OutSnapshot.PresentationAnimationLength,
				0.0f,
				1.0f)
			: 0.0f;
	OutSnapshot.CombatNormalizedTime = AnimInstance->GetCombatAnimationNormalizedTime();
	OutSnapshot.ActivePresentationPlayRate = AnimInstance->GetActivePresentationPlayRate();
	OutSnapshot.bHasRootMotionSources = MovementComponent->HasRootMotionSources();
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

bool IsCombatStanceTransitionPlaying(const FCharacterSnapshot& Snapshot)
{
	return Snapshot.CombatAnimationStateName == EnterAnimationStateName
		|| Snapshot.CombatAnimationStateName == ExitAnimationStateName
		|| Snapshot.ActivePresentationAnimationName == CombatEnterAnimationName
		|| Snapshot.ActivePresentationAnimationName == CombatExitAnimationName;
}

float GetExpectedCombatPlayRate(const int32 ComboIndex)
{
	return ComboIndex == 3 || ComboIndex == 5
		? ExpectedUnarmedRecoveryPlayRate
		: ExpectedUnarmedAttackPlayRate;
}

bool IsRecoveryComboIndex(const int32 ComboIndex)
{
	return ComboIndex == 3 || ComboIndex == 5;
}

bool IsAttackComboIndex(const int32 ComboIndex)
{
	return ComboIndex >= 0
		&& ComboIndex < ComboClipCount
		&& !IsRecoveryComboIndex(ComboIndex);
}

int32 GetPreviousAttackComboIndex(const int32 ComboIndex)
{
	switch (ComboIndex)
	{
	case 1:
		return 0;
	case 2:
		return 1;
	case 4:
		return 2;
	case 6:
		return 4;
	default:
		return INDEX_NONE;
	}
}

bool HasConsistentCombatTiming(const FCharacterSnapshot& Snapshot, const float ExpectedPlayRate)
{
	return FMath::IsNearlyEqual(
			Snapshot.ActivePresentationPlayRate,
			ExpectedPlayRate,
			0.01f)
		&& FMath::IsNearlyEqual(
			Snapshot.PresentationMontagePlayRate,
			ExpectedPlayRate,
			0.01f)
		&& FMath::IsNearlyEqual(
			Snapshot.CombatNormalizedTime,
			Snapshot.PresentationNormalizedTime,
			0.04f);
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

URequiemCombatComponent* FindPIECombatComponent()
{
	APlayerController* PlayerController = FindPIEPlayerController();
	ARequiemCharacter* Character = PlayerController
		? Cast<ARequiemCharacter>(PlayerController->GetPawn())
		: nullptr;
	return Character ? Character->GetCombatComponent() : nullptr;
}

ERequiemUnarmedAttackRequestResult RequestPrimaryAttack()
{
	URequiemCombatComponent* CombatComponent = FindPIECombatComponent();
	return CombatComponent
		? CombatComponent->RequestUnarmedAttack()
		: ERequiemUnarmedAttackRequestResult::Rejected;
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
		const ERequiemUnarmedAttackRequestResult Result =
			CombatComponent->RequestUnarmedAttack();
		return CombatComponent->GetCombatState() == ERequiemCombatState::CombatUnarmed
			&& CombatComponent->GetAttackRequestSerial() != SerialBeforeRequest
			&& Result == ERequiemUnarmedAttackRequestResult::InitialAccepted;
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

float GetFloatPropertyValue(
	const UObject* Container,
	const UClass* OwnerClass,
	const TCHAR* PropertyName)
{
	const FFloatProperty* Property = FindFProperty<FFloatProperty>(OwnerClass, PropertyName);
	return Container && Property
		? Property->GetPropertyValue_InContainer(Container)
		: TNumericLimits<float>::Lowest();
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
		if (IsCombatStanceTransitionPlaying(Snapshot)
			&& (!Snapshot.Acceleration.IsNearlyZero(1.0f)
				|| Snapshot.GroundSpeed > 1.0f))
		{
			return AbortWithError(TEXT("PunchKick_Enter started before movement fully stopped."));
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

class FEnterCombatWhileMovingCommand final : public FTimedCommand
{
public:
	FEnterCombatWhileMovingCommand(
		FAutomationTestBase* InTest,
		TSharedRef<FRunState> InRunState)
		: FTimedCommand(InTest, MoveTemp(InRunState), 6.0)
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
			return AbortIfTimedOut(ElapsedSeconds, TEXT("injecting movement before combat entry"));
		}

		FCharacterSnapshot Snapshot;
		if (!ReadCharacterSnapshot(Snapshot))
		{
			return AbortIfTimedOut(ElapsedSeconds, TEXT("reading moving combat entry"));
		}

		if (!bEntryRequested)
		{
			if (Snapshot.GroundSpeed < ExpectedMaximumSpeed * 0.9f
				|| Snapshot.LocomotionStateName != FName(TEXT("Jog"))
				|| Snapshot.ActivePresentationAnimationName != JogForwardAnimationName)
			{
				return AbortIfTimedOut(
					ElapsedSeconds,
					FString::Printf(
						TEXT("reaching Jog before combat entry (speed=%.1f, locomotion=%s, animation=%s)"),
						Snapshot.GroundSpeed,
						*Snapshot.LocomotionStateName.ToString(),
						*Snapshot.ActivePresentationAnimationName.ToString()));
			}

			if (!InvokeCombatContract(ETestCombatCommand::EnterManual))
			{
				return AbortIfTimedOut(ElapsedSeconds, TEXT("entering combat while moving"));
			}
			bEntryRequested = true;
			EntryRequestSeconds = ElapsedSeconds;
			return false;
		}

		const bool bMoving = !Snapshot.Acceleration.IsNearlyZero(1.0f)
			|| Snapshot.GroundSpeed > 1.0f;
		if (bMoving && IsCombatStanceTransitionPlaying(Snapshot))
		{
			return AbortWithError(TEXT("Combat stance Enter/Exit played while movement remained active."));
		}
		if (Snapshot.CombatState == ERequiemCombatState::CombatUnarmed
			&& Snapshot.ObservedCombatState == ERequiemCombatState::CombatUnarmed
			&& Snapshot.CombatAnimationStateName == InactiveAnimationStateName
			&& Snapshot.LocomotionStateName == FName(TEXT("Jog"))
			&& Snapshot.ActivePresentationAnimationName == JogForwardAnimationName
			&& bMoving)
		{
			++MovingLocomotionSamples;
		}

		if (MovingLocomotionSamples >= 3
			&& ElapsedSeconds - EntryRequestSeconds >= 0.2)
		{
			StopContinuousAction(MoveActionPath);
			Test->AddInfo(TEXT("CombatUnarmed entry stayed on Jog while moving and deferred PunchKick_Enter."));
			return true;
		}

		return AbortIfTimedOut(
			ElapsedSeconds,
			FString::Printf(
				TEXT("deferring combat entry while moving (combat=%s, presentation=%s, speed=%.1f, samples=%d)"),
				*Snapshot.CombatStateName.ToString(),
				*Snapshot.CombatAnimationStateName.ToString(),
				Snapshot.GroundSpeed,
				MovingLocomotionSamples));
	}

private:
	double EntryRequestSeconds = -1.0;
	int32 MovingLocomotionSamples = 0;
	bool bEntryRequested = false;
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
			Test->AddInfo(TEXT("W+D movement remained owned by CharacterMovement in CombatUnarmed; input stays held for exit validation."));
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
		const TCHAR* InStageDescription,
		const bool bInValidateMovingDeferral = false)
		: FTimedCommand(InTest, MoveTemp(InRunState), 5.0)
		, StageDescription(InStageDescription)
		, bValidateMovingDeferral(bInValidateMovingDeferral)
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

		const bool bMoving = !Snapshot.Acceleration.IsNearlyZero(1.0f)
			|| Snapshot.GroundSpeed > 1.0f;
		if (bValidateMovingDeferral)
		{
			if (bMoving && IsCombatStanceTransitionPlaying(Snapshot))
			{
				return AbortWithError(TEXT("PunchKick_Exit started while movement was still active."));
			}
			if (!bMovementReleased)
			{
				if (Snapshot.CombatState == ERequiemCombatState::Normal
					&& Snapshot.ObservedCombatState == ERequiemCombatState::Normal
					&& Snapshot.CombatAnimationStateName == InactiveAnimationStateName
					&& Snapshot.LocomotionStateName == FName(TEXT("Jog"))
					&& bMoving)
				{
					++MovingLocomotionSamples;
				}
				if (MovingLocomotionSamples >= 3 && ElapsedSeconds >= 0.2)
				{
					StopContinuousAction(MoveActionPath);
					bMovementReleased = true;
				}
			}
			else if (bMoving)
			{
				bSawBrakingWithoutExit = true;
			}
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
		const bool bMovingDeferralValidated = !bValidateMovingDeferral
			|| (bMovementReleased
				&& MovingLocomotionSamples >= 3
				&& bSawBrakingWithoutExit);
		if (bMovingDeferralValidated && bSawExit && bReachedNormal)
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
	bool bValidateMovingDeferral = false;
	bool bMovementReleased = false;
	bool bSawBrakingWithoutExit = false;
	int32 MovingLocomotionSamples = 0;
	bool bSawExit = false;
};

class FPrepareMovingAutoAttackCommand final : public FTimedCommand
{
public:
	FPrepareMovingAutoAttackCommand(
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
			return true;
		}

		const double ElapsedSeconds = GetElapsedSeconds();
		if (!SetContinuousAction(MoveActionPath, FInputActionValue(FVector2D(0.0, -1.0))))
		{
			return AbortIfTimedOut(ElapsedSeconds, TEXT("injecting movement before LMB auto-entry"));
		}

		FCharacterSnapshot Snapshot;
		if (!ReadCharacterSnapshot(Snapshot))
		{
			return AbortIfTimedOut(ElapsedSeconds, TEXT("reading movement before LMB auto-entry"));
		}
		if (Snapshot.CombatState != ERequiemCombatState::Normal
			|| Snapshot.ObservedCombatState != ERequiemCombatState::Normal)
		{
			return AbortWithError(TEXT("Moving LMB preparation did not remain in Normal."));
		}
		if (!bHasStartLocation)
		{
			StartLocation = Snapshot.Location;
			bHasStartLocation = true;
		}

		const float Displacement = (Snapshot.Location - StartLocation).Size2D();
		if (Snapshot.GroundSpeed >= ExpectedMaximumSpeed * 0.9f
			&& Displacement >= 50.0f
			&& Snapshot.LocomotionStateName == FName(TEXT("Jog"))
			&& Snapshot.ActivePresentationAnimationName == JogBackwardAnimationName
			&& HasValidPresentationPlayback(Snapshot))
		{
			Test->AddInfo(TEXT("Normal reached backward Jog before the moving LMB auto-entry."));
			return true;
		}

		return AbortIfTimedOut(
			ElapsedSeconds,
			FString::Printf(
				TEXT("preparing moving LMB auto-entry (speed=%.1f, distance=%.1f, locomotion=%s, animation=%s)"),
				Snapshot.GroundSpeed,
				Displacement,
				*Snapshot.LocomotionStateName.ToString(),
				*Snapshot.ActivePresentationAnimationName.ToString()));
	}

private:
	FVector StartLocation = FVector::ZeroVector;
	bool bHasStartLocation = false;
};

int32 FindExpectedComboIndex(const FName AnimationName)
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

bool IsInsideExpectedInputWindow(const FCharacterSnapshot& Snapshot)
{
	return Snapshot.CombatNormalizedTime >= ExpectedUnarmedInputWindowStart - 0.02f
		&& Snapshot.CombatNormalizedTime <= ExpectedUnarmedInputWindowEnd + 0.02f;
}

bool HasValidCommittedAttackPlayback(const FCharacterSnapshot& Snapshot)
{
	const bool bHasValidBasePlayback = HasValidCombatOneShotPlayback(Snapshot)
		&& HasConsistentCombatTiming(
			Snapshot,
			GetExpectedCombatPlayRate(Snapshot.ActiveComboAnimationIndex))
		&& Snapshot.bUnarmedAttackActive
		&& !Snapshot.bHasRootMotionSources;
	if (!bHasValidBasePlayback)
	{
		return false;
	}

	if (IsRecoveryComboIndex(Snapshot.ActiveComboAnimationIndex))
	{
		// Recovery remains part of the active combo/input contract, but must not
		// recommit CharacterMovement after the strike has released it.
		return Snapshot.CombatAnimationStateName == RecoveryAnimationStateName
			&& !Snapshot.bUnarmedAttackMovementLocked;
	}
	if (!IsAttackComboIndex(Snapshot.ActiveComboAnimationIndex)
		|| Snapshot.CombatAnimationStateName != AttackAnimationStateName)
	{
		return false;
	}

	const float LockedSamplingEnd =
		ExpectedUnarmedMovementUnlock - MovementUnlockSamplingTolerance;
	const float UnlockedSamplingStart =
		ExpectedUnarmedMovementUnlock + MovementUnlockSamplingTolerance;
	if (Snapshot.CombatNormalizedTime <= LockedSamplingEnd)
	{
		return Snapshot.bUnarmedAttackMovementLocked
			&& Snapshot.PendingMovementInput.IsNearlyZero(0.01f);
	}
	if (Snapshot.CombatNormalizedTime >= UnlockedSamplingStart)
	{
		return !Snapshot.bUnarmedAttackMovementLocked;
	}

	// Allow one small sampling band around 0.60 because input, movement and the
	// animation instance are updated in different engine tick groups.
	return true;
}

bool HasResumedMovementInputAfterUnlock(const FCharacterSnapshot& Snapshot)
{
	return IsAttackComboIndex(Snapshot.ActiveComboAnimationIndex)
		&& Snapshot.CombatNormalizedTime
			>= ExpectedUnarmedMovementUnlock + MovementUnlockSamplingTolerance
		&& !Snapshot.bUnarmedAttackMovementLocked
		&& !Snapshot.Acceleration.IsNearlyZero(1.0f);
}

class FObserveAutoEntrySpamCommand final : public FTimedCommand
{
public:
	FObserveAutoEntrySpamCommand(
		FAutomationTestBase* InTest,
		TSharedRef<FRunState> InRunState)
		: FTimedCommand(InTest, MoveTemp(InRunState), 10.0)
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
		FCharacterSnapshot Snapshot;
		if (!ReadCharacterSnapshot(Snapshot))
		{
			return AbortIfTimedOut(ElapsedSeconds, TEXT("reading the single-slot spam combo"));
		}

		if (Snapshot.bActivePresentationUsesRootMotion
			|| Snapshot.RootMotionMode != ERootMotionMode::IgnoreRootMotion
			|| Snapshot.bHasRootMotionSources)
		{
			return AbortWithError(TEXT("Single-slot combo used animation or movement root motion."));
		}
		if (Snapshot.bCanBlock)
		{
			return AbortWithError(TEXT("Blocking became available during unarmed combat."));
		}
		if (IsCombatStanceTransitionPlaying(Snapshot)
			&& (!Snapshot.Acceleration.IsNearlyZero(1.0f)
				|| Snapshot.GroundSpeed > 1.0f))
		{
			return AbortWithError(TEXT("Moving LMB auto-entry played a stance transition before fully stopping."));
		}

		if (!bSawAutomaticEnter
			&& Snapshot.CombatState == ERequiemCombatState::CombatUnarmed
			&& Snapshot.CombatAnimationStateName == InactiveAnimationStateName
			&& Snapshot.LocomotionStateName == FName(TEXT("Jog"))
			&& HasValidPresentationPlayback(Snapshot)
			&& Snapshot.bUnarmedAttackMovementLocked
			&& !Snapshot.bUnarmedAttackActive
			&& Snapshot.PendingMovementInput.IsNearlyZero(0.01f)
			&& Snapshot.GroundSpeed > 1.0f)
		{
			MaximumAutoEntryBrakingSpeed = FMath::Max(
				MaximumAutoEntryBrakingSpeed,
				Snapshot.GroundSpeed);
			MinimumAutoEntryBrakingSpeed = FMath::Min(
				MinimumAutoEntryBrakingSpeed,
				Snapshot.GroundSpeed);
			bSawNaturalAutoEntryBraking = true;
		}

		if (Snapshot.ActivePresentationAnimationName == CombatEnterAnimationName)
		{
			bSawAutomaticEnter |= Snapshot.CombatAnimationStateName == EnterAnimationStateName
				&& Snapshot.CombatState == ERequiemCombatState::CombatUnarmed
				&& HasValidCombatOneShotPlayback(Snapshot)
				&& HasConsistentCombatTiming(Snapshot, 1.0f);
			bSawPendingEntryMovementLock |= Snapshot.bUnarmedAttackMovementLocked
				&& !Snapshot.bUnarmedAttackActive
				&& Snapshot.PendingMovementInput.IsNearlyZero(0.01f);
		}

		const int32 ComboIndex = FindExpectedComboIndex(Snapshot.ActivePresentationAnimationName);
		if (ComboIndex != INDEX_NONE && Snapshot.ActivePresentationAnimationName != LastObservedComboClip)
		{
			if (!bSawAutomaticEnter)
			{
				return AbortWithError(TEXT("LMB started a combo clip before PunchKick_Enter was observed."));
			}
			if (ComboIndex != ObservedComboClipCount || ComboIndex > 1)
			{
				return AbortWithError(FString::Printf(
					TEXT("Input backlog escaped the single slot: expected index %d, observed %s at %d."),
					ObservedComboClipCount,
					*Snapshot.ActivePresentationAnimationName.ToString(),
					ComboIndex));
			}

			LastObservedComboClip = Snapshot.ActivePresentationAnimationName;
			++ObservedComboClipCount;
			if (ComboIndex == 0)
			{
				AttackStartLocation = Snapshot.Location;
				AttackForward = Snapshot.ActorForward;
				bHasAttackStart = true;
			}
		}

		if (ComboIndex != INDEX_NONE)
		{
			if (!HasValidCommittedAttackPlayback(Snapshot))
			{
				return AbortWithError(FString::Printf(
					TEXT("Committed clip %s did not preserve timing, its 0.60 movement release, or CharacterMovement ownership."),
					*Snapshot.ActivePresentationAnimationName.ToString()));
			}

			if (IsAttackComboIndex(ComboIndex))
			{
				if (Snapshot.CombatNormalizedTime
					<= ExpectedUnarmedMovementUnlock - MovementUnlockSamplingTolerance
					&& Snapshot.bUnarmedAttackMovementLocked
					&& Snapshot.PendingMovementInput.IsNearlyZero(0.01f)
					&& Snapshot.Acceleration.IsNearlyZero(1.0f))
				{
					LockedAttackIndices.Add(ComboIndex);
				}
				if (Snapshot.CombatNormalizedTime
					>= ExpectedUnarmedMovementUnlock + MovementUnlockSamplingTolerance
					&& !Snapshot.bUnarmedAttackMovementLocked)
				{
					UnlockedAttackIndices.Add(ComboIndex);
					if (ComboIndex == 0
						&& bBurstSubmitted
						&& Snapshot.bQueuedUnarmedFollowUp)
					{
						bSawQueuePreservedThroughUnlock = true;
					}
				}
				if (HasResumedMovementInputAfterUnlock(Snapshot))
				{
					MovementResumedAttackIndices.Add(ComboIndex);
				}
				if (ComboIndex == 1
					&& UnlockedAttackIndices.Contains(0)
					&& Snapshot.bUnarmedAttackMovementLocked)
				{
					bSawFollowUpRelock = true;
				}
			}

			if (!bInputInjectionStopped
				&& !SetContinuousAction(
					MoveActionPath,
					FInputActionValue(FVector2D(-1.0, -1.0))))
			{
				return AbortIfTimedOut(ElapsedSeconds, TEXT("injecting movement against the attack lock"));
			}

			if (ComboIndex == 1 && Snapshot.CombatNormalizedTime >= 0.80f)
			{
				StopContinuousAction(MoveActionPath);
				bInputInjectionStopped = true;
			}

			if (ComboIndex == 0 && Snapshot.bUnarmedAttackInputWindowOpen && !bBurstSubmitted)
			{
				if (!IsInsideExpectedInputWindow(Snapshot))
				{
					return AbortWithError(TEXT("The accepted Cross input window was outside 0.30-0.85."));
				}

				for (int32 RequestIndex = 0; RequestIndex < InputSpamCount; ++RequestIndex)
				{
					const ERequiemUnarmedAttackRequestResult Result = RequestPrimaryAttack();
					if (Result == ERequiemUnarmedAttackRequestResult::FollowUpBuffered)
					{
						++AcceptedBurstRequests;
					}
					else if (Result == ERequiemUnarmedAttackRequestResult::Rejected)
					{
						++RejectedBurstRequests;
					}
					else
					{
						return AbortWithError(TEXT("Spam request was misclassified as an initial attack."));
					}
				}
				bBurstSubmitted = true;

				URequiemCombatComponent* CombatComponent = FindPIECombatComponent();
				if (AcceptedBurstRequests != 1
					|| RejectedBurstRequests != InputSpamCount - 1
					|| !CombatComponent
					|| !CombatComponent->HasQueuedUnarmedFollowUp()
					|| CombatComponent->IsUnarmedAttackInputWindowOpen())
				{
					return AbortWithError(TEXT("Spam did not occupy exactly one follow-up slot."));
				}
			}
		}

		if (bHasAttackStart)
		{
			const FVector AttackDelta = Snapshot.Location - AttackStartLocation;
			const float ForwardDisplacement = FVector::DotProduct(AttackDelta, AttackForward);
			const FVector LateralDelta = AttackDelta - AttackForward * ForwardDisplacement;
			FVector PlanarVelocity = Snapshot.Velocity;
			PlanarVelocity.Z = 0.0f;
			const float ForwardSpeed = FVector::DotProduct(PlanarVelocity, Snapshot.ActorForward);
			const FVector LateralVelocity =
				PlanarVelocity - Snapshot.ActorForward * ForwardSpeed;
			MaximumForwardDisplacement = FMath::Max(
				MaximumForwardDisplacement,
				ForwardDisplacement);
			MaximumForwardSpeed = FMath::Max(MaximumForwardSpeed, ForwardSpeed);
			bSawForwardLunge |=
				(ForwardSpeed >= 75.0f && LateralVelocity.Size2D() <= 50.0f)
				|| (ForwardDisplacement >= 5.0f && LateralDelta.Size2D() <= 60.0f);
		}

		if (ObservedComboClipCount == 2 && ComboIndex == INDEX_NONE)
		{
			StopContinuousAction(MoveActionPath);
			bInputInjectionStopped = true;
		}

		const bool bReachedFinalIdle = ObservedComboClipCount == 2
			&& Snapshot.CombatState == ERequiemCombatState::CombatUnarmed
			&& Snapshot.ObservedCombatState == ERequiemCombatState::CombatUnarmed
			&& Snapshot.CombatAnimationStateName == IdleAnimationStateName
			&& Snapshot.ActiveComboAnimationIndex == INDEX_NONE
			&& Snapshot.ActivePresentationAnimationName == CombatIdleAnimationName
			&& HasValidPresentationPlayback(Snapshot)
			&& !Snapshot.bUnarmedAttackActive
			&& !Snapshot.bUnarmedAttackInputWindowOpen
			&& !Snapshot.bQueuedUnarmedFollowUp
			&& !Snapshot.bUnarmedAttackMovementLocked
			&& AcceptedBurstRequests == 1
			&& RejectedBurstRequests == InputSpamCount - 1
			&& bSawForwardLunge
			&& HasConfiguredMovement(Snapshot);
		if (bSawNaturalAutoEntryBraking
			&& bSawAutomaticEnter
			&& bSawPendingEntryMovementLock
			&& LockedAttackIndices.Num() == 2
			&& UnlockedAttackIndices.Num() == 2
			&& MovementResumedAttackIndices.Num() == 2
			&& bSawQueuePreservedThroughUnlock
			&& bSawFollowUpRelock
			&& bReachedFinalIdle)
		{
			Test->AddInfo(TEXT("Cross and Jab released movement at 0.60, the queued Jab relocked, and six clicks still occupied one slot."));
			return true;
		}

		return AbortIfTimedOut(
			ElapsedSeconds,
			FString::Printf(
				TEXT("single-slot spam (combat=%s, presentation=%s, animation=%s, clips=%d/2, accepted=%d, rejected=%d, lunged=%d, locked=%d/2, unlocked=%d/2, resumed=%d/2, relock=%d, queuePreserved=%d, brake=%.1f..%.1f, enter=%d, pendingLock=%d)"),
				*Snapshot.CombatStateName.ToString(),
				*Snapshot.CombatAnimationStateName.ToString(),
				*Snapshot.ActivePresentationAnimationName.ToString(),
				ObservedComboClipCount,
				AcceptedBurstRequests,
				RejectedBurstRequests,
				bSawForwardLunge,
				LockedAttackIndices.Num(),
				UnlockedAttackIndices.Num(),
				MovementResumedAttackIndices.Num(),
				bSawFollowUpRelock,
				bSawQueuePreservedThroughUnlock,
				MinimumAutoEntryBrakingSpeed,
				MaximumAutoEntryBrakingSpeed,
				bSawAutomaticEnter,
				bSawPendingEntryMovementLock));
	}

private:
	FVector AttackStartLocation = FVector::ZeroVector;
	FVector AttackForward = FVector::ForwardVector;
	TSet<int32> LockedAttackIndices;
	TSet<int32> UnlockedAttackIndices;
	TSet<int32> MovementResumedAttackIndices;
	FName LastObservedComboClip = NAME_None;
	int32 ObservedComboClipCount = 0;
	int32 AcceptedBurstRequests = 0;
	int32 RejectedBurstRequests = 0;
	float MaximumForwardSpeed = 0.0f;
	float MaximumForwardDisplacement = 0.0f;
	float MaximumAutoEntryBrakingSpeed = 0.0f;
	float MinimumAutoEntryBrakingSpeed = TNumericLimits<float>::Max();
	bool bHasAttackStart = false;
	bool bSawAutomaticEnter = false;
	bool bSawPendingEntryMovementLock = false;
	bool bSawNaturalAutoEntryBraking = false;
	bool bBurstSubmitted = false;
	bool bInputInjectionStopped = false;
	bool bSawForwardLunge = false;
	bool bSawQueuePreservedThroughUnlock = false;
	bool bSawFollowUpRelock = false;
};

class FObserveWindowedFullComboCommand final : public FTimedCommand
{
public:
	FObserveWindowedFullComboCommand(
		FAutomationTestBase* InTest,
		TSharedRef<FRunState> InRunState)
		: FTimedCommand(InTest, MoveTemp(InRunState), 12.0)
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
		FCharacterSnapshot Snapshot;
		if (!ReadCharacterSnapshot(Snapshot))
		{
			return AbortIfTimedOut(ElapsedSeconds, TEXT("reading the windowed full combo"));
		}

		if (Snapshot.bActivePresentationUsesRootMotion
			|| Snapshot.RootMotionMode != ERootMotionMode::IgnoreRootMotion
			|| Snapshot.bHasRootMotionSources)
		{
			return AbortWithError(TEXT("Windowed combo used animation or movement root motion."));
		}
		if (Snapshot.bCanBlock)
		{
			return AbortWithError(TEXT("Blocking became available during the full unarmed combo."));
		}

		const int32 ComboIndex = FindExpectedComboIndex(Snapshot.ActivePresentationAnimationName);
		if (ComboIndex != INDEX_NONE && Snapshot.ActivePresentationAnimationName != LastObservedComboClip)
		{
			if (ComboIndex != ObservedComboClipCount)
			{
				return AbortWithError(FString::Printf(
					TEXT("Windowed combo order mismatch: expected %d, observed %s at %d."),
					ObservedComboClipCount,
					*Snapshot.ActivePresentationAnimationName.ToString(),
					ComboIndex));
			}

			LastObservedComboClip = Snapshot.ActivePresentationAnimationName;
			++ObservedComboClipCount;
			if (ComboIndex == 0)
			{
				AttackStartLocation = Snapshot.Location;
				AttackForward = Snapshot.ActorForward;
				bHasAttackStart = true;
			}
		}

		if (ComboIndex != INDEX_NONE)
		{
			if (!HasValidCommittedAttackPlayback(Snapshot))
			{
				return AbortWithError(FString::Printf(
					TEXT("Full combo clip %s did not preserve timing, its 0.60 movement release, or CharacterMovement ownership."),
					*Snapshot.ActivePresentationAnimationName.ToString()));
			}

			if (IsAttackComboIndex(ComboIndex))
			{
				if (Snapshot.CombatNormalizedTime
					<= ExpectedUnarmedMovementUnlock - MovementUnlockSamplingTolerance
					&& Snapshot.bUnarmedAttackMovementLocked
					&& Snapshot.PendingMovementInput.IsNearlyZero(0.01f)
					&& Snapshot.Acceleration.IsNearlyZero(1.0f))
				{
					LockedAttackIndices.Add(ComboIndex);
					const int32 PreviousAttackIndex = GetPreviousAttackComboIndex(ComboIndex);
					if (PreviousAttackIndex != INDEX_NONE
						&& UnlockedAttackIndices.Contains(PreviousAttackIndex))
					{
						RelockedAttackIndices.Add(ComboIndex);
					}
				}
				if (Snapshot.CombatNormalizedTime
					>= ExpectedUnarmedMovementUnlock + MovementUnlockSamplingTolerance
					&& !Snapshot.bUnarmedAttackMovementLocked)
				{
					UnlockedAttackIndices.Add(ComboIndex);
					if ((ComboIndex == 0 || ComboIndex == 1)
						&& Snapshot.bQueuedUnarmedFollowUp
						&& !Snapshot.bUnarmedAttackInputWindowOpen)
					{
						QueuedAttackIndicesPreservedAtUnlock.Add(ComboIndex);
					}
					if ((ComboIndex == 2 || ComboIndex == 4)
						&& Snapshot.bUnarmedAttackInputWindowOpen
						&& !Snapshot.bQueuedUnarmedFollowUp)
					{
						OpenWindowAttackIndicesAtUnlock.Add(ComboIndex);
					}
				}
				if (HasResumedMovementInputAfterUnlock(Snapshot))
				{
					MovementResumedAttackIndices.Add(ComboIndex);
				}
			}
			else if (!Snapshot.bUnarmedAttackMovementLocked)
			{
				UnlockedRecoveryIndices.Add(ComboIndex);
			}

			if (!bInputInjectionStopped
				&& !SetContinuousAction(
					MoveActionPath,
					FInputActionValue(FVector2D(-1.0, -1.0))))
			{
				return AbortIfTimedOut(ElapsedSeconds, TEXT("injecting movement during the full combo"));
			}

			if (ComboIndex == 6 && Snapshot.CombatNormalizedTime >= 0.80f)
			{
				StopContinuousAction(MoveActionPath);
				bInputInjectionStopped = true;
			}

			const bool bShouldQueueHere = ComboIndex == 0
				|| ComboIndex == 1
				|| ComboIndex == 3
				|| ComboIndex == 5;
			if (bShouldQueueHere
				&& Snapshot.bUnarmedAttackInputWindowOpen
				&& !QueuedFromComboIndices.Contains(ComboIndex))
			{
				if (!IsInsideExpectedInputWindow(Snapshot))
				{
					return AbortWithError(TEXT("A full-combo request was accepted outside 0.30-0.85."));
				}

				const ERequiemUnarmedAttackRequestResult AcceptedResult = RequestPrimaryAttack();
				const ERequiemUnarmedAttackRequestResult ExtraResult = RequestPrimaryAttack();
				if (AcceptedResult != ERequiemUnarmedAttackRequestResult::FollowUpBuffered
					|| ExtraResult != ERequiemUnarmedAttackRequestResult::Rejected)
				{
					return AbortWithError(TEXT("A full-combo window accepted more than one follow-up."));
				}

				QueuedFromComboIndices.Add(ComboIndex);
				++RejectedExtraRequests;
			}
		}

		if (bHasAttackStart)
		{
			const FVector AttackDelta = Snapshot.Location - AttackStartLocation;
			const float ForwardDisplacement = FVector::DotProduct(AttackDelta, AttackForward);
			const FVector LateralDelta = AttackDelta - AttackForward * ForwardDisplacement;
			FVector PlanarVelocity = Snapshot.Velocity;
			PlanarVelocity.Z = 0.0f;
			const float ForwardSpeed = FVector::DotProduct(PlanarVelocity, Snapshot.ActorForward);
			const FVector LateralVelocity =
				PlanarVelocity - Snapshot.ActorForward * ForwardSpeed;
			MaximumForwardDisplacement = FMath::Max(
				MaximumForwardDisplacement,
				ForwardDisplacement);
			MaximumForwardSpeed = FMath::Max(MaximumForwardSpeed, ForwardSpeed);
			bSawForwardLunge |=
				(ForwardSpeed >= 75.0f && LateralVelocity.Size2D() <= 50.0f)
				|| (ForwardDisplacement >= 20.0f && LateralDelta.Size2D() <= 90.0f);
		}

		if (ObservedComboClipCount == ComboClipCount && ComboIndex == INDEX_NONE)
		{
			StopContinuousAction(MoveActionPath);
			bInputInjectionStopped = true;
		}

		const bool bReachedFinalIdle = ObservedComboClipCount == ComboClipCount
			&& Snapshot.CombatState == ERequiemCombatState::CombatUnarmed
			&& Snapshot.ObservedCombatState == ERequiemCombatState::CombatUnarmed
			&& Snapshot.CombatAnimationStateName == IdleAnimationStateName
			&& Snapshot.ActiveComboAnimationIndex == INDEX_NONE
			&& Snapshot.ActivePresentationAnimationName == CombatIdleAnimationName
			&& HasValidPresentationPlayback(Snapshot)
			&& !Snapshot.bUnarmedAttackActive
			&& !Snapshot.bUnarmedAttackInputWindowOpen
			&& !Snapshot.bQueuedUnarmedFollowUp
			&& !Snapshot.bUnarmedAttackMovementLocked
			&& QueuedFromComboIndices.Num() == 4
			&& RejectedExtraRequests == 4
			&& LockedAttackIndices.Num() == 5
			&& UnlockedAttackIndices.Num() == 5
			&& MovementResumedAttackIndices.Num() == 5
			&& RelockedAttackIndices.Num() == 4
			&& UnlockedRecoveryIndices.Num() == 2
			&& QueuedAttackIndicesPreservedAtUnlock.Num() == 2
			&& OpenWindowAttackIndicesAtUnlock.Num() == 2
			&& bSawForwardLunge
			&& HasConfiguredMovement(Snapshot);
		if (bReachedFinalIdle)
		{
			Test->AddInfo(TEXT("The finite combo released movement at 0.60, kept recoveries free, and relocked only for each next strike."));
			return true;
		}

		return AbortIfTimedOut(
			ElapsedSeconds,
			FString::Printf(
				TEXT("windowed full combo (presentation=%s, animation=%s, clips=%d/7, windows=%d/4, rejected=%d/4, locked=%d/5, unlocked=%d/5, resumed=%d/5, relocked=%d/4, recoveryFree=%d/2, queueKept=%d/2, openWindow=%d/2, lunged=%d)"),
				*Snapshot.CombatAnimationStateName.ToString(),
				*Snapshot.ActivePresentationAnimationName.ToString(),
				ObservedComboClipCount,
				QueuedFromComboIndices.Num(),
				RejectedExtraRequests,
				LockedAttackIndices.Num(),
				UnlockedAttackIndices.Num(),
				MovementResumedAttackIndices.Num(),
				RelockedAttackIndices.Num(),
				UnlockedRecoveryIndices.Num(),
				QueuedAttackIndicesPreservedAtUnlock.Num(),
				OpenWindowAttackIndicesAtUnlock.Num(),
				bSawForwardLunge));
	}

private:
	FVector AttackStartLocation = FVector::ZeroVector;
	FVector AttackForward = FVector::ForwardVector;
	TSet<int32> QueuedFromComboIndices;
	TSet<int32> LockedAttackIndices;
	TSet<int32> UnlockedAttackIndices;
	TSet<int32> MovementResumedAttackIndices;
	TSet<int32> RelockedAttackIndices;
	TSet<int32> UnlockedRecoveryIndices;
	TSet<int32> QueuedAttackIndicesPreservedAtUnlock;
	TSet<int32> OpenWindowAttackIndicesAtUnlock;
	FName LastObservedComboClip = NAME_None;
	int32 ObservedComboClipCount = 0;
	int32 RejectedExtraRequests = 0;
	float MaximumForwardSpeed = 0.0f;
	float MaximumForwardDisplacement = 0.0f;
	bool bHasAttackStart = false;
	bool bInputInjectionStopped = false;
	bool bSawForwardLunge = false;
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
		// Earlier combat and lunge stages advance toward the front edge of the
		// compact dev floor, so validate post-exit movement back toward its center.
		if (!SetContinuousAction(MoveActionPath, FInputActionValue(FVector2D(0.0, -1.0))))
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
			&& Snapshot.ActivePresentationAnimationName == JogBackwardAnimationName
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
			TestTrue(
				TEXT("Unarmed lunge target remains 350 uu/s"),
				FMath::IsNearlyEqual(
					GetFloatPropertyValue(
						CombatCDO,
						CombatCDO->GetClass(),
						TEXT("UnarmedAttackLungeSpeed")),
					ExpectedUnarmedLungeSpeed,
					0.01f));
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
		TestTrue(
			TEXT("Unarmed attack play rate remains 1.25x"),
			FMath::IsNearlyEqual(
				GetFloatPropertyValue(
					AnimInstanceCDO,
					AnimInstanceClass,
					TEXT("UnarmedAttackPlayRate")),
				ExpectedUnarmedAttackPlayRate,
				0.01f));
		TestTrue(
			TEXT("Unarmed recovery play rate remains 1.35x"),
			FMath::IsNearlyEqual(
				GetFloatPropertyValue(
					AnimInstanceCDO,
					AnimInstanceClass,
					TEXT("UnarmedRecoveryPlayRate")),
				ExpectedUnarmedRecoveryPlayRate,
				0.01f));
		TestTrue(
			TEXT("Unarmed input window starts at 0.30"),
			FMath::IsNearlyEqual(
				GetFloatPropertyValue(
					AnimInstanceCDO,
					AnimInstanceClass,
					TEXT("UnarmedInputWindowStartNormalized")),
				ExpectedUnarmedInputWindowStart,
				0.001f));
		TestTrue(
			TEXT("Unarmed input window ends at 0.85"),
			FMath::IsNearlyEqual(
				GetFloatPropertyValue(
					AnimInstanceCDO,
					AnimInstanceClass,
					TEXT("UnarmedInputWindowEndNormalized")),
				ExpectedUnarmedInputWindowEnd,
				0.001f));
		TestTrue(
			TEXT("Queued attack handoff remains 0.72"),
			FMath::IsNearlyEqual(
				GetFloatPropertyValue(
					AnimInstanceCDO,
					AnimInstanceClass,
					TEXT("UnarmedQueuedAttackHandoffNormalized")),
				ExpectedQueuedAttackHandoff,
				0.001f));
		TestTrue(
			TEXT("Automatic recovery handoff remains 0.90"),
			FMath::IsNearlyEqual(
				GetFloatPropertyValue(
					AnimInstanceCDO,
					AnimInstanceClass,
					TEXT("UnarmedAutomaticRecoveryHandoffNormalized")),
				ExpectedAutomaticRecoveryHandoff,
				0.001f));
		TestTrue(
			TEXT("Queued recovery handoff remains 0.55"),
			FMath::IsNearlyEqual(
				GetFloatPropertyValue(
					AnimInstanceCDO,
					AnimInstanceClass,
					TEXT("UnarmedQueuedRecoveryHandoffNormalized")),
				ExpectedQueuedRecoveryHandoff,
				0.001f));
		TestTrue(
			TEXT("Unarmed attacks release movement at 0.60"),
			FMath::IsNearlyEqual(
				GetFloatPropertyValue(
					AnimInstanceCDO,
					AnimInstanceClass,
					TEXT("UnarmedMovementUnlockNormalized")),
				ExpectedUnarmedMovementUnlock,
				0.001f));
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

	ADD_LATENT_AUTOMATION_COMMAND(FEnterCombatWhileMovingCommand(this, RunState));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForManualCombatIdleCommand(this, RunState));
	ADD_LATENT_AUTOMATION_COMMAND(FMoveInCombatCommand(this, RunState));
	ADD_LATENT_AUTOMATION_COMMAND(FInvokeCombatContractCommand(
		this,
		RunState,
		ETestCombatCommand::Exit,
		TEXT("manual combat exit contract (Z mapping verified)")));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForExitToNormalCommand(
		this, RunState, TEXT("manual Z exit"), true));

	ADD_LATENT_AUTOMATION_COMMAND(FPrepareMovingAutoAttackCommand(this, RunState));
	ADD_LATENT_AUTOMATION_COMMAND(FInvokeCombatContractCommand(
		this,
		RunState,
		ETestCombatCommand::PrimaryAttack,
		TEXT("initial primary attack contract (LMB mapping verified)")));
	ADD_LATENT_AUTOMATION_COMMAND(FObserveAutoEntrySpamCommand(this, RunState));

	ADD_LATENT_AUTOMATION_COMMAND(FInvokeCombatContractCommand(
		this,
		RunState,
		ETestCombatCommand::PrimaryAttack,
		TEXT("windowed full-combo initial attack")));
	ADD_LATENT_AUTOMATION_COMMAND(FObserveWindowedFullComboCommand(this, RunState));

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
