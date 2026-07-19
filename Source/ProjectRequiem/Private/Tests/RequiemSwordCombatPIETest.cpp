// Copyright Project Requiem. All Rights Reserved.

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Animation/AnimSequence.h"
#include "Animation/RequiemPlayerAnimInstance.h"
#include "Characters/RequiemCharacter.h"
#include "Combat/RequiemCombatDummy.h"
#include "Components/RequiemCombatComponent.h"
#include "Components/RequiemDodgeComponent.h"
#include "Components/RequiemHealthComponent.h"
#include "Components/RequiemLockOnComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "EnhancedActionKeyMapping.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/DateTime.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "UnrealClient.h"
#include "UObject/UnrealType.h"

namespace RequiemSwordCombatPIETest
{
constexpr TCHAR MapPackagePath[] =
	TEXT("/Game/ProjectRequiem/World/Maps/Dev/L_Dev_Foundation");
constexpr TCHAR MappingContextPath[] =
	TEXT("/Game/ProjectRequiem/Core/Input/IMC_Exploration.IMC_Exploration");
constexpr TCHAR ToggleCombatActionPath[] =
	TEXT("/Game/ProjectRequiem/Core/Input/Actions/IA_ToggleCombat.IA_ToggleCombat");
constexpr TCHAR PrimaryAttackActionPath[] =
	TEXT("/Game/ProjectRequiem/Core/Input/Actions/IA_PrimaryAttack.IA_PrimaryAttack");
constexpr TCHAR MoveActionPath[] =
	TEXT("/Game/ProjectRequiem/Core/Input/Actions/IA_Move.IA_Move");
constexpr TCHAR CharacterClassPath[] =
	TEXT("/Game/ProjectRequiem/Characters/Player/Blueprints/BP_CH_Player.BP_CH_Player_C");
constexpr TCHAR AnimInstanceClassPath[] =
	TEXT("/Game/ProjectRequiem/Characters/Player/Animations/Locomotion/"
		 "ABP_PR_PlayerLocomotion.ABP_PR_PlayerLocomotion_C");
constexpr TCHAR DummyClassPath[] =
	TEXT("/Game/ProjectRequiem/Combat/Blueprints/Targets/"
		 "BP_PR_CombatDummy.BP_PR_CombatDummy_C");
const FName SwordHandSocketName(TEXT("Socket_Weapon_Hand_R"));
const FName SwordBackSocketName(TEXT("Socket_Weapon_Back"));
const FName SwordHandBoneName(TEXT("hand_r"));
const FName SwordBackBoneName(TEXT("spine_03"));
constexpr TCHAR CaptureAttachmentsFlag[] = TEXT("RequiemCaptureSwordAttachments");

constexpr float ExpectedChargeThresholdSeconds = 0.65f;
constexpr float ExpectedLightDamage = 35.0f;
constexpr float ExpectedHeavyDamage = 60.0f;
constexpr float ExpectedLightPlayRate = 1.0f;
constexpr float ExpectedHeavyPlayRate = 0.5f;
constexpr float ExpectedInputWindowStart = 0.30f;
constexpr float ExpectedInputWindowEnd = 0.85f;
constexpr float ExpectedMovementUnlock = 0.75f;
constexpr float ExpectedRecoveryBlendTime = 0.15f;
constexpr float MinimumRecoveryBlendTime = 0.10f;
constexpr float MaximumRecoveryBlendTime = 0.20f;
constexpr float ExpectedSwordTransitionDuration = 1.30f;
constexpr float ExpectedEnterHandAttachment = 21.0f / 39.0f;
constexpr float ExpectedExitBackAttachment = 15.0f / 39.0f;
constexpr int32 ExpectedSwordTransitionSampledKeys = 40;
constexpr int32 ExpectedSwordClipCount = 9;

const TCHAR* SwordAnimationPropertyNames[ExpectedSwordClipCount] = {
	TEXT("SwordEnterAnimation"),
	TEXT("SwordIdleAnimation"),
	TEXT("SwordExitAnimation"),
	TEXT("SwordRegularAAnimation"),
	TEXT("SwordRegularARecoveryAnimation"),
	TEXT("SwordRegularBAnimation"),
	TEXT("SwordRegularBRecoveryAnimation"),
	TEXT("SwordRegularCAnimation"),
	TEXT("SwordHeavyAttackAnimation"),
};

struct FRunState
{
	bool bAborted = false;
	bool bPresentationConfigured = false;
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
	URequiemPlayerAnimInstance* AnimInstance = nullptr;
	UStaticMeshComponent* SwordMesh = nullptr;
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
			&& AnimInstance
			&& SwordMesh
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
	Refs.SwordMesh = Refs.Character ? Refs.Character->GetSwordMesh() : nullptr;
	Refs.AnimInstance = Refs.Character && Refs.Character->GetMesh()
		? Cast<URequiemPlayerAnimInstance>(Refs.Character->GetMesh()->GetAnimInstance())
		: nullptr;
	if (Refs.World)
	{
		for (TActorIterator<ARequiemCombatDummy> Iterator(Refs.World); Iterator; ++Iterator)
		{
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
	StopContinuousAction(MoveActionPath);
}

bool IsSwordVisible(const UStaticMeshComponent* SwordMesh)
{
	return SwordMesh && SwordMesh->IsVisible() && !SwordMesh->bHiddenInGame;
}

bool IsSwordAttachedToSocket(
	const UStaticMeshComponent* SwordMesh,
	const USkeletalMeshComponent* CharacterMesh,
	const FName SocketName)
{
	return IsSwordVisible(SwordMesh)
		&& CharacterMesh
		&& SwordMesh->GetAttachParent() == CharacterMesh
		&& SwordMesh->GetAttachSocketName() == SocketName;
}

bool IsSwordStoredOnBack(const FRuntimeRefs& Refs)
{
	return IsSwordAttachedToSocket(
		Refs.SwordMesh,
		Refs.Character ? Refs.Character->GetMesh() : nullptr,
		SwordBackSocketName);
}

bool IsSwordInHand(const FRuntimeRefs& Refs)
{
	return IsSwordAttachedToSocket(
		Refs.SwordMesh,
		Refs.Character ? Refs.Character->GetMesh() : nullptr,
		SwordHandSocketName);
}

bool ShouldCaptureAttachmentScreenshots()
{
	return FParse::Param(FCommandLine::Get(), CaptureAttachmentsFlag);
}

FString RequestAttachmentScreenshot(const TCHAR* Label)
{
	const FString ScreenshotDirectory = FPaths::Combine(
		FPaths::ProjectSavedDir(),
		TEXT("Screenshots"),
		TEXT("SwordAttachmentValidation"));
	IFileManager::Get().MakeDirectory(*ScreenshotDirectory, true);
	const FString ScreenshotPath = FPaths::Combine(
		ScreenshotDirectory,
		FString::Printf(
			TEXT("%s_%lld.png"),
			Label,
			FDateTime::UtcNow().GetTicks()));
	FScreenshotRequest::RequestScreenshot(
		ScreenshotPath,
		false,
		false,
		false,
		FIntRect(),
		true);
	return ScreenshotPath;
}

void PositionCombatFixture(const FRuntimeRefs& Refs, const float DummyDistance = 145.0f)
{
	Refs.Dodge->FinishDodge();
	Refs.LockOn->ClearLockOn();
	if (Refs.Health->GetHealthState() != ERequiemHealthState::Alive)
	{
		Refs.Health->ResetForTesting();
	}
	Refs.Dummy->ResetForTesting();
	Refs.Character->UnCrouch();
	Refs.Movement->SetMovementMode(MOVE_Walking);
	Refs.Movement->StopMovementImmediately();
	Refs.Character->SetActorLocationAndRotation(
		FVector(0.0f, 0.0f, Refs.Character->GetActorLocation().Z),
		FRotator::ZeroRotator,
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
	Refs.PlayerController->SetControlRotation(FRotator::ZeroRotator);
	Refs.Dummy->SetActorLocationAndRotation(
		FVector(DummyDistance, 0.0f, 112.0f),
		FRotator(0.0f, 180.0f, 0.0f),
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
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

int32 CountBindings(
	const UEnhancedInputComponent* InputComponent,
	const UInputAction* Action,
	const ETriggerEvent TriggerEvent)
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
			&& Binding->GetTriggerEvent() == TriggerEvent)
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
		return true;
	}

	bool AbortIfTimedOut(const double ElapsedSeconds, const FString& Details)
	{
		if (ElapsedSeconds < TimeoutSeconds)
		{
			return false;
		}
		return AbortWithError(FString::Printf(
			TEXT("Sword combat PIE stage timed out after %.1fs: %s"),
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
	FWaitForPIEReadyCommand(FAutomationTestBase* InTest, TSharedRef<FRunState> InRunState)
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
		const FRuntimeRefs Refs = FindRuntimeRefs();
		if (Refs.IsValid())
		{
			if (!Refs.World->GetMapName().Contains(TEXT("L_Dev_Foundation")))
			{
				return AbortWithError(FString::Printf(
					TEXT("PIE opened the wrong map: %s"),
					*Refs.World->GetMapName()));
			}

			if (StableWorld.Get() != Refs.World || StableCharacter.Get() != Refs.Character)
			{
				StableWorld = Refs.World;
				StableCharacter = Refs.Character;
				StableSinceSeconds = ElapsedSeconds;
				return false;
			}
			if (ElapsedSeconds - StableSinceSeconds < 0.5)
			{
				return false;
			}

			const UEnhancedInputComponent* InputComponent =
				Refs.Character->FindComponentByClass<UEnhancedInputComponent>();
			const UInputAction* ToggleAction =
				LoadObject<UInputAction>(nullptr, ToggleCombatActionPath);
			const UInputAction* AttackAction =
				LoadObject<UInputAction>(nullptr, PrimaryAttackActionPath);
			const int32 ToggleStarted =
				CountBindings(InputComponent, ToggleAction, ETriggerEvent::Started);
			const int32 AttackStarted =
				CountBindings(InputComponent, AttackAction, ETriggerEvent::Started);
			const int32 AttackCompleted =
				CountBindings(InputComponent, AttackAction, ETriggerEvent::Completed);
			const int32 AttackCanceled =
				CountBindings(InputComponent, AttackAction, ETriggerEvent::Canceled);
			if (ToggleStarted != 1
				|| AttackStarted != 1
				|| AttackCompleted != 1
				|| AttackCanceled != 1)
			{
				return AbortWithError(FString::Printf(
					TEXT("Runtime sword input bindings are invalid: toggle Started=%d, "
						 "attack Started=%d Completed=%d Canceled=%d (expected one each)."),
					ToggleStarted,
					AttackStarted,
					AttackCompleted,
					AttackCanceled));
			}

			if (Refs.Health->GetHealthState() == ERequiemHealthState::Alive
				&& Refs.Dummy->GetDummyState() == ERequiemCombatDummyState::Alive
				&& Refs.Combat->GetCombatState() == ERequiemCombatState::Normal
				&& IsSwordStoredOnBack(Refs))
			{
				if (ShouldCaptureAttachmentScreenshots())
				{
					if (StoredScreenshotPath.IsEmpty())
					{
						StoredScreenshotPath = RequestAttachmentScreenshot(TEXT("SwordStoredBack"));
						return false;
					}
					if (!FPaths::FileExists(StoredScreenshotPath))
					{
						return AbortIfTimedOut(
							ElapsedSeconds,
							TEXT("waiting for the stored sword screenshot"));
					}
					Test->AddInfo(FString::Printf(
						TEXT("Stored sword screenshot: %s"),
						*StoredScreenshotPath));
				}
				Test->AddInfo(TEXT("L_Dev_Foundation started with one live dummy and the sword visible on its back socket."));
				return true;
			}
		}

		return AbortIfTimedOut(
			ElapsedSeconds,
			FString::Printf(
				TEXT("waiting for a stable player and exactly one dummy (count=%d)"),
				Refs.DummyCount));
	}

private:
	TWeakObjectPtr<UWorld> StableWorld;
	TWeakObjectPtr<ARequiemCharacter> StableCharacter;
	double StableSinceSeconds = 0.0;
	FString StoredScreenshotPath;
};

class FValidateEquipVisibilityCommand final : public FTimedCommand
{
public:
	FValidateEquipVisibilityCommand(
		FAutomationTestBase* InTest,
		TSharedRef<FRunState> InRunState)
		: FTimedCommand(InTest, MoveTemp(InRunState), 12.0)
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
		if (!Refs.IsValid())
		{
			return AbortIfTimedOut(ElapsedSeconds, TEXT("runtime references disappeared"));
		}

		if (Phase == 0)
		{
			PositionCombatFixture(Refs);
			if (!IsSwordStoredOnBack(Refs))
			{
				return AbortWithError(TEXT("Sword was not visible on its back socket before equip."));
			}
			ToggleSerialBefore = Refs.Combat->GetSwordToggleSerial();
			if (!Refs.Combat->ToggleSwordCombat()
				|| Refs.Combat->GetCombatState() != ERequiemCombatState::CombatSword
				|| Refs.Combat->GetSwordToggleSerial() != ToggleSerialBefore + 1)
			{
				return AbortWithError(TEXT("Equipping did not enter CombatSword exactly once."));
			}
			Phase = 1;
			return false;
		}

		if (Phase == 1)
		{
			if (Refs.AnimInstance->GetSwordAnimationState()
				== ERequiemSwordAnimationState::Enter)
			{
				bSawEnter = true;
				const float NormalizedTime =
					Refs.AnimInstance->GetSwordAnimationNormalizedTime();
				if (NormalizedTime < ExpectedEnterHandAttachment)
				{
					bSawEnterBeforeHandoff = true;
					if (!IsSwordStoredOnBack(Refs))
					{
						return AbortWithError(TEXT("Sword_Enter left the back socket before frame 21."));
					}
				}
				else
				{
					bSawEnterAfterHandoff = true;
					if (!IsSwordInHand(Refs))
					{
						return AbortWithError(TEXT("Sword_Enter did not attach to the hand at frame 21."));
					}
				}
			}
			const bool bEquipReady = RunState->bPresentationConfigured
				? Refs.AnimInstance->GetSwordAnimationState()
						== ERequiemSwordAnimationState::Idle
					&& IsSwordInHand(Refs)
				: ElapsedSeconds >= 0.75;
			if (!bEquipReady)
			{
				return AbortIfTimedOut(ElapsedSeconds, TEXT("waiting for sword equip presentation"));
			}
			if (RunState->bPresentationConfigured
				&& (!bSawEnter
					|| !bSawEnterBeforeHandoff
					|| !bSawEnterAfterHandoff))
			{
				return AbortWithError(TEXT("Sword_Enter did not prove both sides of its frame-21 attachment handoff."));
			}
			if (Refs.Combat->GetCombatState() != ERequiemCombatState::CombatSword)
			{
				return AbortWithError(TEXT("Sword equip did not remain in CombatSword."));
			}
			if (RunState->bPresentationConfigured && ShouldCaptureAttachmentScreenshots())
			{
				if (HandScreenshotPath.IsEmpty())
				{
					HandScreenshotPath = RequestAttachmentScreenshot(TEXT("SwordEquippedHand"));
					return false;
				}
				if (!FPaths::FileExists(HandScreenshotPath))
				{
					return AbortIfTimedOut(
						ElapsedSeconds,
						TEXT("waiting for the equipped sword screenshot"));
				}
				Test->AddInfo(FString::Printf(
					TEXT("Equipped sword screenshot: %s"),
					*HandScreenshotPath));
			}
			if (Refs.Combat->RequestUnarmedAttack()
					!= ERequiemUnarmedAttackRequestResult::Rejected
				|| Refs.Combat->HasPendingInitialUnarmedAttackRequest())
			{
				return AbortWithError(TEXT("Equipped sword allowed the unarmed attack contract to bypass its style."));
			}

			if (!Refs.Combat->ToggleSwordCombat()
				|| Refs.Combat->GetCombatState() != ERequiemCombatState::CombatUnarmed
				|| Refs.Combat->GetSwordToggleSerial() != ToggleSerialBefore + 2)
			{
				return AbortWithError(TEXT("Storing the sword did not return to CombatUnarmed."));
			}
			Phase = 2;
			return false;
		}

		if (Phase == 2)
		{
			if (Refs.AnimInstance->GetSwordAnimationState()
				== ERequiemSwordAnimationState::Exit)
			{
				bSawExit = true;
				const float NormalizedTime =
					Refs.AnimInstance->GetSwordAnimationNormalizedTime();
				if (NormalizedTime < ExpectedExitBackAttachment)
				{
					bSawExitBeforeHandoff = true;
					if (!IsSwordInHand(Refs))
					{
						return AbortWithError(TEXT("Sword_Exit left the hand before frame 15."));
					}
				}
				else
				{
					bSawExitAfterHandoff = true;
					if (!IsSwordStoredOnBack(Refs))
					{
						return AbortWithError(TEXT("Sword_Exit did not attach to the back at frame 15."));
					}
				}
			}
			const bool bStored = RunState->bPresentationConfigured
				? Refs.AnimInstance->GetSwordAnimationState()
						== ERequiemSwordAnimationState::Inactive
					&& IsSwordStoredOnBack(Refs)
				: ElapsedSeconds >= 1.25;
			if (!bStored)
			{
				return AbortIfTimedOut(ElapsedSeconds, TEXT("waiting for sword store presentation"));
			}
			if (RunState->bPresentationConfigured
				&& (!bSawExit
					|| !bSawExitBeforeHandoff
					|| !bSawExitAfterHandoff))
			{
				return AbortWithError(TEXT("Sword_Exit did not prove both sides of its frame-15 attachment handoff."));
			}
			if (!Refs.Combat->ToggleSwordCombat()
				|| Refs.Combat->GetSwordToggleSerial() != ToggleSerialBefore + 3)
			{
				return AbortWithError(TEXT("Sword could not be equipped again after storing."));
			}
			Phase = 3;
			return false;
		}

		if (RunState->bPresentationConfigured && !bRequestedAttackDuringEnter)
		{
			if (Refs.AnimInstance->GetSwordAnimationState()
				!= ERequiemSwordAnimationState::Enter)
			{
				return AbortIfTimedOut(
					ElapsedSeconds,
					TEXT("waiting for the re-equip enter before its handoff"));
			}
			if (Refs.AnimInstance->GetSwordAnimationNormalizedTime()
				>= ExpectedEnterHandAttachment)
			{
				return AbortWithError(TEXT("Re-equip passed frame 21 before the attack-skip probe."));
			}
			if (!IsSwordStoredOnBack(Refs)
				|| !Refs.Combat->BeginSwordCharge()
				|| Refs.Combat->ReleaseSwordCharge()
					!= ERequiemSwordAttackRequestResult::InitialLightAccepted)
			{
				return AbortWithError(TEXT("Attack could not skip Sword_Enter before the attachment handoff."));
			}
			bRequestedAttackDuringEnter = true;
			return false;
		}

		if (RunState->bPresentationConfigured
			&& (Refs.AnimInstance->GetSwordAnimationState()
					== ERequiemSwordAnimationState::Attack
				|| Refs.AnimInstance->GetSwordAnimationState()
					== ERequiemSwordAnimationState::Recovery))
		{
			bSawAttackSkipEnter = true;
			if (!IsSwordInHand(Refs))
			{
				return AbortWithError(TEXT("Attack skipped Sword_Enter without moving the sword to the hand."));
			}
		}

		const bool bReadyForCombat = RunState->bPresentationConfigured
			? bSawAttackSkipEnter
				&& Refs.AnimInstance->GetSwordAnimationState()
					== ERequiemSwordAnimationState::Idle
				&& IsSwordInHand(Refs)
			: Refs.Combat->GetCombatState() == ERequiemCombatState::CombatSword;
		if (bReadyForCombat)
		{
			Test->AddInfo(TEXT("Sword attachment changed at Enter frame 21 and Exit frame 15; attack still skipped Enter directly to the hand."));
			return true;
		}
		return AbortIfTimedOut(ElapsedSeconds, TEXT("waiting for the re-equipped sword"));
	}

private:
	int32 Phase = 0;
	int32 ToggleSerialBefore = 0;
	bool bSawEnter = false;
	bool bSawExit = false;
	bool bSawEnterBeforeHandoff = false;
	bool bSawEnterAfterHandoff = false;
	bool bSawExitBeforeHandoff = false;
	bool bSawExitAfterHandoff = false;
	bool bRequestedAttackDuringEnter = false;
	bool bSawAttackSkipEnter = false;
	FString HandScreenshotPath;
};

class FValidateExitDamageAttachmentCommand final : public FTimedCommand
{
public:
	FValidateExitDamageAttachmentCommand(
		FAutomationTestBase* InTest,
		TSharedRef<FRunState> InRunState)
		: FTimedCommand(InTest, MoveTemp(InRunState), 12.0)
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
		if (!Refs.IsValid())
		{
			return AbortIfTimedOut(ElapsedSeconds, TEXT("runtime references disappeared"));
		}
		if (!RunState->bPresentationConfigured)
		{
			Test->AddInfo(TEXT("Sword presentation is not configured; exit interruption attachment validation was skipped."));
			return true;
		}

		if (Phase == 0)
		{
			if (Refs.Combat->GetCombatState() != ERequiemCombatState::CombatSword
				|| Refs.AnimInstance->GetSwordAnimationState() != ERequiemSwordAnimationState::Idle
				|| !IsSwordInHand(Refs))
			{
				return AbortIfTimedOut(ElapsedSeconds, TEXT("waiting for the equipped sword before exit interruption"));
			}
			if (!Refs.Combat->ToggleSwordCombat()
				|| Refs.Combat->GetCombatState() != ERequiemCombatState::CombatUnarmed)
			{
				return AbortWithError(TEXT("Could not start Sword_Exit for damage interruption validation."));
			}
			Phase = 1;
			return false;
		}

		if (Phase == 1)
		{
			if (Refs.AnimInstance->GetSwordAnimationState() != ERequiemSwordAnimationState::Exit)
			{
				return AbortIfTimedOut(ElapsedSeconds, TEXT("waiting for Sword_Exit before applying damage"));
			}
			if (Refs.AnimInstance->GetSwordAnimationNormalizedTime()
				>= ExpectedExitBackAttachment)
			{
				return AbortWithError(TEXT("Damage probe reached Sword_Exit after its frame-15 storage handoff."));
			}
			if (!IsSwordInHand(Refs))
			{
				return AbortWithError(TEXT("Sword_Exit did not keep the sword in hand before frame 15."));
			}

			HealthBeforeDamage = Refs.Health->GetCurrentHealth();
			DamageSerialBefore = Refs.Health->GetDamageRequestSerial();
			FRequiemDamageRequest Request;
			Request.DamageAmount = 10.0f;
			Request.HitRegion = ERequiemHitRegion::Chest;
			Request.Strength = ERequiemDamageStrength::Light;
			const ERequiemDamageOutcome Outcome = Refs.Character->ApplyRequiemDamage(Request);
			if (Outcome != ERequiemDamageOutcome::Applied
				|| Refs.Combat->GetCombatState() != ERequiemCombatState::CombatUnarmed
				|| Refs.Health->GetDamageRequestSerial() != DamageSerialBefore + 1
				|| !FMath::IsNearlyEqual(
					Refs.Health->GetCurrentHealth(), HealthBeforeDamage - 10.0f, 0.01f))
			{
				return AbortWithError(TEXT("Damage did not interrupt Sword_Exit through the existing health contract."));
			}
			Phase = 2;
			return false;
		}

		if (Phase == 2)
		{
			if (Refs.AnimInstance->GetDamageAnimationState()
				!= ERequiemDamageAnimationState::Inactive)
			{
				bSawDamageReaction = true;
				if (!IsSwordStoredOnBack(Refs))
				{
					return AbortWithError(TEXT("Damage during Sword_Exit left the sword attached to the hand."));
				}
			}

			if (Refs.Health->IsDamageReactionActive()
				|| Refs.AnimInstance->GetDamageAnimationState()
					!= ERequiemDamageAnimationState::Inactive)
			{
				return AbortIfTimedOut(ElapsedSeconds, TEXT("waiting for the interrupted exit damage reaction"));
			}
			if (!bSawDamageReaction
				|| Refs.Combat->GetCombatState() != ERequiemCombatState::CombatUnarmed
				|| !IsSwordStoredOnBack(Refs))
			{
				return AbortWithError(TEXT("Interrupted Sword_Exit did not finish with the sword stored on its back."));
			}
			if (!Refs.Combat->ToggleSwordCombat())
			{
				return AbortWithError(TEXT("Sword could not be re-equipped after the interrupted exit reaction."));
			}
			Phase = 3;
			return false;
		}

		if (Refs.Combat->GetCombatState() == ERequiemCombatState::CombatSword
			&& Refs.AnimInstance->GetSwordAnimationState() == ERequiemSwordAnimationState::Idle
			&& IsSwordInHand(Refs))
		{
			Test->AddInfo(TEXT("Damage interrupted Sword_Exit, stored the sword on the back, and allowed a clean re-equip."));
			return true;
		}
		return AbortIfTimedOut(ElapsedSeconds, TEXT("waiting for re-equip after exit interruption"));
	}

private:
	int32 Phase = 0;
	int32 DamageSerialBefore = 0;
	float HealthBeforeDamage = 0.0f;
	bool bSawDamageReaction = false;
};

class FValidateLightRecoveryBlendCommand final : public FTimedCommand
{
public:
	FValidateLightRecoveryBlendCommand(
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
		const FRuntimeRefs Refs = FindRuntimeRefs();
		if (!Refs.IsValid())
		{
			return AbortTimedOutWithInputCleanup(
				ElapsedSeconds,
				TEXT("runtime references disappeared"));
		}

		if (!RunState->bPresentationConfigured)
		{
			ReleaseAllTestInput();
			Test->AddInfo(TEXT("Sword recovery blend playback check skipped because presentation assets are not configured."));
			return true;
		}

		if (Phase == 0)
		{
			ReleaseAllTestInput();
			PositionCombatFixture(Refs, 500.0f);
			Refs.Combat->CancelActiveAttackForExternalReaction();
			Refs.Combat->EnterSwordCombat(ERequiemCombatEntryReason::Manual);
			ToggleSerialBefore = Refs.Combat->GetSwordToggleSerial();
			if (!Refs.Combat->BeginSwordCharge()
				|| Refs.Combat->ReleaseSwordCharge()
					!= ERequiemSwordAttackRequestResult::InitialLightAccepted)
			{
				return AbortWithInputCleanup(
					TEXT("Could not start the isolated light recovery attack."));
			}
			Phase = 1;
			return false;
		}

		if (Phase == 1)
		{
			const ERequiemSwordAnimationState SwordState =
				Refs.AnimInstance->GetSwordAnimationState();
			if (SwordState == ERequiemSwordAnimationState::Attack
				&& Refs.AnimInstance->GetActiveSwordComboAnimationIndex() == 0)
			{
				bSawInitialAttack = true;
				if (!Refs.Combat->IsSwordAttackActive()
					|| !Refs.Combat->IsSwordAttackMovementLocked())
				{
					return AbortWithInputCleanup(
						TEXT("Sword_Regular_A was not movement-committed."));
				}
			}

			if (SwordState == ERequiemSwordAnimationState::Recovery
				&& Refs.AnimInstance->GetActiveSwordComboAnimationIndex() == 1)
			{
				if (!bSawInitialAttack
					|| !Refs.Combat->IsSwordAttackActive()
					|| !Refs.Combat->IsSwordAttackMovementLocked()
					|| Refs.Combat->HasQueuedSwordLightFollowUp())
				{
					return AbortWithInputCleanup(
						TEXT("Sword_Regular_A_Rec did not preserve the unqueued attack commitment."));
				}
				if (!SetContinuousAction(
						MoveActionPath,
						FInputActionValue(FVector2D(0.0f, 1.0f))))
				{
					return AbortWithInputCleanup(
						TEXT("Could not inject IA_Move during light recovery."));
				}
				Phase = 2;
				return false;
			}
		}
		else if (Phase == 2)
		{
			if (!SetContinuousAction(
					MoveActionPath,
					FInputActionValue(FVector2D(0.0f, 1.0f))))
			{
				return AbortWithInputCleanup(
					TEXT("IA_Move continuous injection disappeared during light recovery."));
			}

			const bool bHasRawMovementIntent =
				Refs.Character->HasCurrentMovementInput()
				&& !Refs.Character->GetCurrentMovementInputDirection().IsNearlyZero();
			const bool bBlendActive =
				Refs.AnimInstance->IsSwordLocomotionRecoveryBlendActive();
			if (bHasRawMovementIntent && !bBlendActive && !bSawBlend)
			{
				bSawLockedMovementIntentBeforeBlend = true;
				if (!Refs.Combat->IsSwordAttackActive()
					|| !Refs.Combat->IsSwordAttackMovementLocked()
					|| Refs.Movement->GetCurrentAcceleration().Size2D() > 1.0f)
				{
					return AbortWithInputCleanup(
						TEXT("Movement input escaped before the light recovery blend began."));
				}
			}

			if (bBlendActive)
			{
				if (!bSawBlend)
				{
					bSawBlend = true;
					BlendFirstObservedSeconds = FPlatformTime::Seconds();
				}
				if (!bSawLockedMovementIntentBeforeBlend
					|| !bHasRawMovementIntent
					|| !Refs.Combat->IsSwordAttackActive()
					|| !Refs.Combat->IsSwordAttackMovementLocked()
					|| Refs.AnimInstance->GetSwordAnimationState()
						!= ERequiemSwordAnimationState::Recovery
					|| Refs.AnimInstance->GetLocomotionState()
						!= ERequiemLocomotionState::Jog
					|| Refs.Movement->GetCurrentAcceleration().Size2D() > 1.0f)
				{
					return AbortWithInputCleanup(
						TEXT("Light recovery crossfade did not keep movement committed while Jog blended in."));
				}
				if (!bValidatedCommittedToggleBlock)
				{
					if (Refs.Combat->ToggleSwordCombat()
						|| Refs.Combat->GetSwordToggleSerial() != ToggleSerialBefore
						|| Refs.Combat->GetCombatState() != ERequiemCombatState::CombatSword)
					{
						return AbortWithInputCleanup(
							TEXT("Sword style toggle interrupted the committed light recovery blend."));
					}
					bValidatedCommittedToggleBlock = true;
				}
				return false;
			}

			if (bSawBlend)
			{
				ObservedBlendSeconds = static_cast<float>(
					FPlatformTime::Seconds() - BlendFirstObservedSeconds);
				ObservedUnlockNormalized =
					Refs.AnimInstance->GetSwordAnimationNormalizedTime();
				if (ObservedBlendSeconds + 0.01f < MinimumRecoveryBlendTime
					|| ObservedBlendSeconds > MaximumRecoveryBlendTime + 0.08f)
				{
					return AbortWithInputCleanup(FString::Printf(
						TEXT("Light recovery blend duration was %.3fs; expected an observed short blend near 0.10-0.20s."),
						ObservedBlendSeconds));
				}
				if (ObservedUnlockNormalized < 0.70f
					|| ObservedUnlockNormalized > 0.82f)
				{
					return AbortWithInputCleanup(FString::Printf(
						TEXT("Light movement unlocked at normalized %.3f instead of near 0.75."),
						ObservedUnlockNormalized));
				}
				if (Refs.Combat->IsSwordAttackMovementLocked()
					|| !Refs.Combat->IsSwordAttackActive()
					|| Refs.AnimInstance->GetSwordAnimationState()
						!= ERequiemSwordAnimationState::Recovery
					|| Refs.AnimInstance->GetLocomotionState()
						!= ERequiemLocomotionState::Jog)
				{
					return AbortWithInputCleanup(
						TEXT("Light recovery did not release movement only after its crossfade completed."));
				}
				PostBlendLocation = Refs.Character->GetActorLocation();
				Phase = 3;
				return false;
			}
		}
		else if (Phase == 3)
		{
			if (!SetContinuousAction(
					MoveActionPath,
					FInputActionValue(FVector2D(0.0f, 1.0f))))
			{
				return AbortWithInputCleanup(
					TEXT("IA_Move continuous injection disappeared after light recovery."));
			}
			if (Refs.Combat->IsSwordAttackMovementLocked())
			{
				return AbortWithInputCleanup(
					TEXT("Light recovery relocked movement after the blend."));
			}

			const float PlayerDrivenDistance = FVector::Dist2D(
				PostBlendLocation,
				Refs.Character->GetActorLocation());
			if (Refs.AnimInstance->GetLocomotionState() == ERequiemLocomotionState::Jog
				&& Refs.Movement->GetCurrentAcceleration().Size2D() > 1.0f
				&& PlayerDrivenDistance >= 2.0f)
			{
				bSawJogMovementAfterBlend = true;
				StopContinuousAction(MoveActionPath);
				Refs.Movement->StopMovementImmediately();
				Phase = 4;
				return false;
			}
		}
		else if (Phase == 4)
		{
			if (bSawJogMovementAfterBlend
				&& bValidatedCommittedToggleBlock
				&& !Refs.Character->HasCurrentMovementInput()
				&& !Refs.Combat->IsSwordAttackActive()
				&& !Refs.Combat->IsSwordAttackMovementLocked()
				&& Refs.AnimInstance->GetSwordAnimationState()
					== ERequiemSwordAnimationState::Idle)
			{
				ReleaseAllTestInput();
				Test->AddInfo(FString::Printf(
					TEXT("Unqueued light recovery unlocked at %.3f after a %.3fs blend, then returned gradually to Jog control."),
					ObservedUnlockNormalized,
					ObservedBlendSeconds));
				return true;
			}
		}

		return AbortTimedOutWithInputCleanup(
			ElapsedSeconds,
			FString::Printf(
				TEXT("waiting for light recovery blend (phase=%d state=%s normalized=%.2f locked=%d blend=%d)"),
				Phase,
				*Refs.AnimInstance->GetSwordAnimationStateName().ToString(),
				Refs.AnimInstance->GetSwordAnimationNormalizedTime(),
				Refs.Combat->IsSwordAttackMovementLocked(),
				Refs.AnimInstance->IsSwordLocomotionRecoveryBlendActive()));
	}

private:
	bool AbortWithInputCleanup(const FString& Message)
	{
		ReleaseAllTestInput();
		return AbortWithError(Message);
	}

	bool AbortTimedOutWithInputCleanup(
		const double ElapsedSeconds,
		const FString& Details)
	{
		const bool bTimedOut = AbortIfTimedOut(ElapsedSeconds, Details);
		if (bTimedOut)
		{
			ReleaseAllTestInput();
		}
		return bTimedOut;
	}

	int32 Phase = 0;
	int32 ToggleSerialBefore = 0;
	bool bSawInitialAttack = false;
	bool bSawLockedMovementIntentBeforeBlend = false;
	bool bSawBlend = false;
	bool bSawJogMovementAfterBlend = false;
	bool bValidatedCommittedToggleBlock = false;
	double BlendFirstObservedSeconds = 0.0;
	float ObservedBlendSeconds = 0.0f;
	float ObservedUnlockNormalized = 0.0f;
	FVector PostBlendLocation = FVector::ZeroVector;
};

class FValidateLightComboCommand final : public FTimedCommand
{
public:
	FValidateLightComboCommand(
		FAutomationTestBase* InTest,
		TSharedRef<FRunState> InRunState)
		: FTimedCommand(InTest, MoveTemp(InRunState), 15.0)
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
		if (!Refs.IsValid())
		{
			return AbortIfTimedOut(ElapsedSeconds, TEXT("runtime references disappeared"));
		}

		if (!bStarted)
		{
			Refs.Dummy->ResetForTesting();
			Refs.Dummy->SetActorLocationAndRotation(
				FVector(145.0f, 0.0f, 112.0f),
				FRotator(0.0f, 180.0f, 0.0f),
				false,
				nullptr,
				ETeleportType::TeleportPhysics);
			Refs.Character->SetActorLocationAndRotation(
				FVector(0.0f, 0.0f, Refs.Character->GetActorLocation().Z),
				FRotator::ZeroRotator,
				false,
				nullptr,
				ETeleportType::TeleportPhysics);
			Refs.Combat->CancelActiveAttackForExternalReaction();
			Refs.Combat->EnterSwordCombat(ERequiemCombatEntryReason::Manual);
			LightSerialBefore = Refs.Combat->GetSwordLightRequestSerial();
			HeavySerialBefore = Refs.Combat->GetSwordHeavyRequestSerial();
			HitAttemptBefore = Refs.Combat->GetSwordHitAttemptSerial();
			HitConfirmBefore = Refs.Combat->GetSwordHitConfirmSerial();
			DummyDamageBefore = Refs.Dummy->GetDamageSerial();
			DummyDefeatBefore = Refs.Dummy->GetDefeatSerial();
			if (!Refs.Combat->BeginSwordCharge())
			{
				return AbortWithError(TEXT("Initial light press did not start a sword charge."));
			}
			const ERequiemSwordAttackRequestResult Result = Refs.Combat->ReleaseSwordCharge();
			if (Result != ERequiemSwordAttackRequestResult::InitialLightAccepted
				|| Refs.Combat->GetPendingSwordAttackType() != ERequiemSwordAttackType::Light
				|| Refs.Combat->IsSwordChargeActive())
			{
				return AbortWithError(TEXT("A short press/release did not create exactly one light request."));
			}
			bStarted = true;
			if (!RunState->bPresentationConfigured)
			{
				return ExecuteGameplayOnlyCombo(Refs);
			}
			return false;
		}

		if (!RunState->bPresentationConfigured)
		{
			return true;
		}

		const ERequiemSwordAnimationState PresentationState =
			Refs.AnimInstance->GetSwordAnimationState();
		if (PresentationState == ERequiemSwordAnimationState::Attack
			|| PresentationState == ERequiemSwordAnimationState::Recovery)
		{
			if (!IsSwordInHand(Refs))
			{
				return AbortWithError(TEXT("Light combo lost Socket_Weapon_Hand_R attachment."));
			}
			const int32 ComboIndex = Refs.AnimInstance->GetActiveSwordComboAnimationIndex();
			if (ComboIndex < 0 || ComboIndex > 4)
			{
				return AbortWithError(FString::Printf(
					TEXT("Sword light presentation published invalid combo index %d."),
					ComboIndex));
			}
			ObservedComboMask |= 1 << ComboIndex;
			if (!FMath::IsNearlyEqual(
				Refs.AnimInstance->GetActivePresentationPlayRate(),
				ExpectedLightPlayRate,
				0.01f))
			{
				return AbortWithError(TEXT("Sword light/recovery presentation did not play at 1.0x."));
			}
		}

		if (Refs.Combat->IsSwordAttackInputWindowOpen() && BufferedFollowUps < 2)
		{
			if (!Refs.Combat->BeginSwordCharge())
			{
				return AbortWithError(TEXT("Light follow-up press was rejected inside its input window."));
			}
			const ERequiemSwordAttackRequestResult BufferResult =
				Refs.Combat->ReleaseSwordCharge();
			if (BufferResult != ERequiemSwordAttackRequestResult::FollowUpBuffered
				|| !Refs.Combat->HasQueuedSwordLightFollowUp())
			{
				return AbortWithError(TEXT("Light follow-up was not buffered inside its input window."));
			}
			++BufferedFollowUps;
			if (Refs.Combat->BeginSwordCharge())
			{
				return AbortWithError(TEXT("A second follow-up was accepted while one was already buffered."));
			}
			bSawExtraBufferRejected = true;
		}

		const bool bFinished =
			BufferedFollowUps == 2
			&& !Refs.Combat->IsSwordAttackActive()
			&& !Refs.Combat->HasPendingInitialAttackRequest()
			&& Refs.AnimInstance->GetSwordAnimationState() == ERequiemSwordAnimationState::Idle
			&& Refs.Dummy->IsDefeated();
		if (bFinished)
		{
			if (ObservedComboMask != 0x1F)
			{
				return AbortWithError(FString::Printf(
					TEXT("Light combo did not publish all five authored clips (mask=0x%02x)."),
					ObservedComboMask));
			}
			return ValidateDamageAndReset(Refs);
		}

		return AbortIfTimedOut(
			ElapsedSeconds,
			FString::Printf(
				TEXT("waiting for full light combo (buffers=%d mask=0x%02x health=%.1f)"),
				BufferedFollowUps,
				ObservedComboMask,
				Refs.Dummy->GetCurrentHealth()));
	}

private:
	bool QueueFollowUp(const FRuntimeRefs& Refs)
	{
		Refs.Combat->SetSwordAttackInputWindowOpen(true);
		if (!Refs.Combat->BeginSwordCharge()
			|| Refs.Combat->ReleaseSwordCharge()
				!= ERequiemSwordAttackRequestResult::FollowUpBuffered
			|| !Refs.Combat->HasQueuedSwordLightFollowUp())
		{
			return false;
		}
		++BufferedFollowUps;
		if (Refs.Combat->BeginSwordCharge())
		{
			return false;
		}
		bSawExtraBufferRejected = true;
		return true;
	}

	bool ExecuteGameplayOnlyCombo(const FRuntimeRefs& Refs)
	{
		if (Refs.Combat->ConsumeInitialSwordAttackRequest() != ERequiemSwordAttackType::Light)
		{
			return AbortWithError(TEXT("Gameplay-only branch could not consume initial light request."));
		}

		Refs.Combat->BeginSwordAttackStep(ERequiemSwordAttackType::Light, false, 0);
		Refs.Combat->UpdateSwordAttackHit(1.0f);
		if (!QueueFollowUp(Refs) || !Refs.Combat->ConsumeQueuedSwordLightFollowUp())
		{
			return AbortWithError(TEXT("Gameplay-only branch failed the first single-buffer rule."));
		}
		Refs.Combat->EndSwordAttackSequence();

		Refs.Combat->BeginSwordAttackStep(ERequiemSwordAttackType::Light, false, 2);
		Refs.Combat->UpdateSwordAttackHit(1.0f);
		if (!QueueFollowUp(Refs) || !Refs.Combat->ConsumeQueuedSwordLightFollowUp())
		{
			return AbortWithError(TEXT("Gameplay-only branch failed the second single-buffer rule."));
		}
		Refs.Combat->EndSwordAttackSequence();

		Refs.Combat->BeginSwordAttackStep(ERequiemSwordAttackType::Light, false, 4);
		Refs.Combat->UpdateSwordAttackHit(1.0f);
		Refs.Combat->EndSwordAttackSequence();
		ObservedComboMask = 0x1F;
		if (!Refs.Dummy->IsDefeated())
		{
			return AbortWithError(TEXT("Three gameplay light strikes did not defeat the dummy."));
		}
		return ValidateDamageAndReset(Refs);
	}

	bool ValidateDamageAndReset(const FRuntimeRefs& Refs)
	{
		const float ExpectedFinalAppliedDamage = FMath::Max(
			0.0f,
			Refs.Dummy->GetMaxHealth() - ExpectedLightDamage * 2.0f);
		if (BufferedFollowUps != 2
			|| !bSawExtraBufferRejected
			|| Refs.Combat->GetSwordLightRequestSerial() != LightSerialBefore + 3
			|| Refs.Combat->GetSwordHeavyRequestSerial() != HeavySerialBefore
			|| Refs.Combat->GetSwordHitAttemptSerial() != HitAttemptBefore + 3
			|| Refs.Combat->GetSwordHitConfirmSerial() != HitConfirmBefore + 3
			|| Refs.Dummy->GetDamageSerial() != DummyDamageBefore + 3
			|| Refs.Dummy->GetDefeatSerial() != DummyDefeatBefore + 1
			|| Refs.Combat->GetLastSwordHitActor() != Refs.Dummy
			|| !FMath::IsNearlyEqual(
				Refs.Dummy->GetLastAppliedDamage(), ExpectedFinalAppliedDamage, 0.01f))
		{
			return AbortWithError(FString::Printf(
				TEXT("Light combo result inconsistent: buffers=%d rejected=%d "
					 "light=%d/%d heavy=%d/%d attempts=%d/%d confirms=%d/%d "
					 "damage=%d/%d defeats=%d/%d last_actor=%s last_damage=%.1f/%.1f."),
				BufferedFollowUps,
				bSawExtraBufferRejected,
				Refs.Combat->GetSwordLightRequestSerial(),
				LightSerialBefore + 3,
				Refs.Combat->GetSwordHeavyRequestSerial(),
				HeavySerialBefore,
				Refs.Combat->GetSwordHitAttemptSerial(),
				HitAttemptBefore + 3,
				Refs.Combat->GetSwordHitConfirmSerial(),
				HitConfirmBefore + 3,
				Refs.Dummy->GetDamageSerial(),
				DummyDamageBefore + 3,
				Refs.Dummy->GetDefeatSerial(),
				DummyDefeatBefore + 1,
				*GetNameSafe(Refs.Combat->GetLastSwordHitActor()),
				Refs.Dummy->GetLastAppliedDamage(),
				ExpectedFinalAppliedDamage));
		}

		const int32 DamageBeforeDeadProbe = Refs.Dummy->GetDamageSerial();
		const int32 ConfirmBeforeDeadProbe = Refs.Combat->GetSwordHitConfirmSerial();
		Refs.Combat->BeginSwordAttackStep(ERequiemSwordAttackType::Light, false, 4);
		Refs.Combat->UpdateSwordAttackHit(1.0f);
		Refs.Combat->EndSwordAttackSequence();
		if (Refs.Dummy->GetDamageSerial() != DamageBeforeDeadProbe
			|| Refs.Combat->GetSwordHitConfirmSerial() != ConfirmBeforeDeadProbe)
		{
			return AbortWithError(TEXT("A defeated dummy accepted another sword hit."));
		}

		const int32 ResetSerialBefore = Refs.Dummy->GetResetSerial();
		Refs.Dummy->ResetForTesting();
		if (Refs.Dummy->GetResetSerial() != ResetSerialBefore + 1
			|| Refs.Dummy->GetDummyState() != ERequiemCombatDummyState::Alive
			|| !FMath::IsNearlyEqual(
				Refs.Dummy->GetCurrentHealth(),
				Refs.Dummy->GetMaxHealth(),
				0.01f))
		{
			return AbortWithError(TEXT("Dummy did not reset after the sword defeat check."));
		}

		Test->AddInfo(TEXT("Three-step light combo obeyed its single buffer and defeated/reset the dummy."));
		return true;
	}

	int32 LightSerialBefore = 0;
	int32 HeavySerialBefore = 0;
	int32 HitAttemptBefore = 0;
	int32 HitConfirmBefore = 0;
	int32 DummyDamageBefore = 0;
	int32 DummyDefeatBefore = 0;
	int32 BufferedFollowUps = 0;
	int32 ObservedComboMask = 0;
	bool bStarted = false;
	bool bSawExtraBufferRejected = false;
};

class FValidateHeavyReleaseCommand final : public FTimedCommand
{
public:
	FValidateHeavyReleaseCommand(
		FAutomationTestBase* InTest,
		TSharedRef<FRunState> InRunState)
		: FTimedCommand(InTest, MoveTemp(InRunState), 15.0)
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
		const FRuntimeRefs Refs = FindRuntimeRefs();
		if (!Refs.IsValid())
		{
			return AbortIfTimedOut(ElapsedSeconds, TEXT("runtime references disappeared"));
		}

		if (!bChargeStarted)
		{
			ReleaseAllTestInput();
			Refs.Dummy->ResetForTesting();
			Refs.Movement->SetMovementMode(MOVE_Walking);
			Refs.Movement->StopMovementImmediately();
			Refs.Character->SetActorLocationAndRotation(
				FVector(0.0f, 0.0f, Refs.Character->GetActorLocation().Z),
				FRotator::ZeroRotator,
				false,
				nullptr,
				ETeleportType::TeleportPhysics);
			Refs.Dummy->SetActorLocationAndRotation(
				FVector(280.0f, 0.0f, 112.0f),
				FRotator(0.0f, 180.0f, 0.0f),
				false,
				nullptr,
				ETeleportType::TeleportPhysics);
			Refs.Combat->CancelActiveAttackForExternalReaction();
			Refs.Combat->EnterSwordCombat(ERequiemCombatEntryReason::Manual);
			LightSerialBefore = Refs.Combat->GetSwordLightRequestSerial();
			HeavySerialBefore = Refs.Combat->GetSwordHeavyRequestSerial();
			AttackSerialBefore = Refs.Combat->GetAttackRequestSerial();
			HitConfirmBefore = Refs.Combat->GetSwordHitConfirmSerial();
			DummyDamageBefore = Refs.Dummy->GetDamageSerial();
			ToggleSerialBefore = Refs.Combat->GetSwordToggleSerial();
			DodgeSerialBefore = Refs.Dodge->GetDodgeRequestSerial();
			if (!Refs.Combat->BeginSwordCharge())
			{
				return AbortWithError(TEXT("Heavy hold did not start charging."));
			}
			bChargeStarted = true;
			return false;
		}

		if (!bReleased)
		{
			if (!Refs.Combat->IsSwordChargeActive()
				|| Refs.Combat->GetPendingSwordAttackType() != ERequiemSwordAttackType::None
				|| Refs.Combat->IsSwordAttackActive()
				|| Refs.Combat->GetSwordHeavyRequestSerial() != HeavySerialBefore
				|| Refs.Combat->GetAttackRequestSerial() != AttackSerialBefore
				|| Refs.Dummy->GetDamageSerial() != DummyDamageBefore)
			{
				return AbortWithError(TEXT("Heavy attack committed or dealt damage before button release."));
			}

			if (Refs.Combat->GetSwordChargeElapsedSeconds()
				< Refs.Combat->GetSwordChargeThresholdSeconds() + 0.05f)
			{
				return AbortIfTimedOut(ElapsedSeconds, TEXT("waiting to cross heavy hold threshold"));
			}

			const ERequiemSwordAttackRequestResult Result = Refs.Combat->ReleaseSwordCharge();
			if (Result != ERequiemSwordAttackRequestResult::InitialHeavyAccepted
				|| Refs.Combat->GetPendingSwordAttackType() != ERequiemSwordAttackType::Heavy
				|| Refs.Combat->IsSwordChargeActive()
				|| Refs.Combat->GetSwordHeavyRequestSerial() != HeavySerialBefore + 1)
			{
				return AbortWithError(TEXT("Release after threshold did not create one heavy request."));
			}
			HeavyStartLocation = Refs.Character->GetActorLocation();
			bReleased = true;
			if (!RunState->bPresentationConfigured)
			{
				if (Refs.Combat->ConsumeInitialSwordAttackRequest()
					!= ERequiemSwordAttackType::Heavy)
				{
					return AbortWithError(TEXT("Gameplay-only branch could not consume heavy request."));
				}
				Refs.Combat->BeginSwordAttackStep(ERequiemSwordAttackType::Heavy, false, 0);
				return ValidateCommittedHeavy(Refs, true);
			}
			return false;
		}

		if (Refs.Combat->IsSwordAttackActive())
		{
			MaxHeavyPlanarDisplacement = FMath::Max(
				MaxHeavyPlanarDisplacement,
				FVector::Dist2D(
					HeavyStartLocation,
					Refs.Character->GetActorLocation()));
		}

		if (bMovementInputInjected && !bStoppedMovementInput
			&& !SetContinuousAction(
				MoveActionPath,
				FInputActionValue(FVector2D(1.0f, 0.0f))))
		{
			return AbortHeavyWithError(
				TEXT("IA_Move continuous injection disappeared during heavy recovery."));
		}

		if (RunState->bPresentationConfigured
			&& Refs.AnimInstance->GetSwordAnimationState()
				== ERequiemSwordAnimationState::HeavyAttack)
		{
			bSawHeavyPresentation = true;
			if (!IsSwordInHand(Refs))
			{
				return AbortWithError(TEXT("Heavy attack lost Socket_Weapon_Hand_R attachment."));
			}
			if (!Refs.AnimInstance->IsSwordLocomotionRecoveryBlendActive()
				&& !FMath::IsNearlyEqual(
				Refs.AnimInstance->GetActivePresentationPlayRate(),
				ExpectedHeavyPlayRate,
				0.01f))
			{
				return AbortWithError(TEXT("Committed heavy presentation did not play at 0.5x."));
			}
			if (!bValidatedCommitLocks && ValidateCommittedHeavy(Refs, false))
			{
				bValidatedCommitLocks = true;
			}
			if (RunState->bAborted)
			{
				return true;
			}

			if (!bMovementInputInjected
				&& Refs.AnimInstance->GetSwordAnimationNormalizedTime() >= 0.55f)
			{
				if (!SetContinuousAction(
						MoveActionPath,
						FInputActionValue(FVector2D(1.0f, 0.0f))))
				{
					return AbortHeavyWithError(
						TEXT("Could not inject IA_Move during heavy recovery."));
				}
				bMovementInputInjected = true;
				return false;
			}

			if (bMovementInputInjected
				&& Refs.Character->HasCurrentMovementInput()
				&& !Refs.Character->GetCurrentMovementInputDirection().IsNearlyZero())
			{
				if (Refs.AnimInstance->IsSwordLocomotionRecoveryBlendActive())
				{
					if (!bSawHeavyRecoveryBlend)
					{
						bSawHeavyRecoveryBlend = true;
						HeavyBlendFirstObservedSeconds = FPlatformTime::Seconds();
					}
					if (!bSawLockedMovementIntentBeforeBlend
						|| !Refs.Combat->IsSwordHeavyAttackCommitted()
						|| !Refs.Combat->IsSwordAttackMovementLocked()
						|| !Refs.Combat->IsSwordAttackActive()
						|| Refs.AnimInstance->GetLocomotionState()
							!= ERequiemLocomotionState::Jog
						|| Refs.Movement->GetCurrentAcceleration().Size2D() > 1.0f)
					{
						return AbortHeavyWithError(
							TEXT("Heavy recovery crossfade did not preserve its non-cancelable movement commitment."));
					}
				}
				else
				{
					bSawLockedMovementIntentBeforeBlend = true;
					if (!Refs.Combat->IsSwordHeavyAttackCommitted()
						|| !Refs.Combat->IsSwordAttackMovementLocked()
						|| Refs.Movement->GetCurrentAcceleration().Size2D() > 1.0f)
					{
						return AbortHeavyWithError(
							TEXT("Movement input escaped before the heavy recovery blend began."));
					}
				}
			}
		}

		const bool bHeavyBlendActive =
			Refs.AnimInstance->IsSwordLocomotionRecoveryBlendActive();
		if (bSawHeavyRecoveryBlend && !bHeavyBlendActive && !bHeavyBlendCompleted)
		{
			ObservedHeavyBlendSeconds = static_cast<float>(
				FPlatformTime::Seconds() - HeavyBlendFirstObservedSeconds);
			if (ObservedHeavyBlendSeconds + 0.01f < MinimumRecoveryBlendTime
				|| ObservedHeavyBlendSeconds > MaximumRecoveryBlendTime + 0.08f)
			{
				return AbortHeavyWithError(FString::Printf(
					TEXT("Heavy recovery blend duration was %.3fs; expected an observed short blend near 0.10-0.20s."),
					ObservedHeavyBlendSeconds));
			}
			if (Refs.Combat->IsSwordAttackActive()
				|| Refs.Combat->IsSwordAttackMovementLocked()
				|| Refs.Combat->IsSwordHeavyAttackCommitted()
				|| Refs.AnimInstance->GetLocomotionState()
					!= ERequiemLocomotionState::Jog)
			{
				return AbortHeavyWithError(
					TEXT("Heavy attack released its commitment before the recovery blend completed."));
			}
			HeavyBlendCompletionLocation = Refs.Character->GetActorLocation();
			bHeavyBlendCompleted = true;
		}

		if (bHeavyBlendCompleted && !bStoppedMovementInput)
		{
			const float PlayerDrivenDistance = FVector::Dist2D(
				HeavyBlendCompletionLocation,
				Refs.Character->GetActorLocation());
			if (Refs.AnimInstance->GetLocomotionState() == ERequiemLocomotionState::Jog
				&& Refs.Movement->GetCurrentAcceleration().Size2D() > 1.0f
				&& PlayerDrivenDistance >= 2.0f)
			{
				bSawJogMovementAfterBlend = true;
				StopContinuousAction(MoveActionPath);
				Refs.Movement->StopMovementImmediately();
				bStoppedMovementInput = true;
			}
		}

		const bool bFinished = RunState->bPresentationConfigured
			&& bSawHeavyPresentation
			&& bValidatedCommitLocks
			&& bHeavyBlendCompleted
			&& bSawJogMovementAfterBlend
			&& bStoppedMovementInput
			&& !Refs.Combat->IsSwordAttackActive()
			&& Refs.AnimInstance->GetSwordAnimationState() == ERequiemSwordAnimationState::Idle
			&& Refs.Combat->GetSwordHitConfirmSerial() == HitConfirmBefore + 1;
		if (bFinished)
		{
			return ValidateHeavyResult(Refs);
		}

		const bool bTimedOut = AbortIfTimedOut(
			ElapsedSeconds,
			FString::Printf(
				TEXT("waiting for heavy completion (state=%s active=%d blend=%d moved=%d health=%.1f)"),
				*Refs.AnimInstance->GetSwordAnimationStateName().ToString(),
				Refs.Combat->IsSwordAttackActive(),
				bHeavyBlendActive,
				bSawJogMovementAfterBlend,
				Refs.Dummy->GetCurrentHealth()));
		if (bTimedOut)
		{
			ReleaseAllTestInput();
		}
		return bTimedOut;
	}

private:
	bool AbortHeavyWithError(const FString& Message)
	{
		ReleaseAllTestInput();
		return AbortWithError(Message);
	}

	bool ValidateCommittedHeavy(const FRuntimeRefs& Refs, const bool bResolveManually)
	{
		Refs.Combat->SetSwordAttackInputWindowOpen(true);
		if (!Refs.Combat->IsSwordHeavyAttackCommitted()
			|| !Refs.Combat->IsSwordAttackMovementLocked()
			|| Refs.Combat->IsSwordAttackInputWindowOpen()
			|| Refs.Combat->BeginSwordCharge()
			|| Refs.Character->CanJump()
			|| Refs.Character->CanCrouch()
			|| Refs.Combat->ToggleSwordCombat()
			|| Refs.Combat->GetSwordToggleSerial() != ToggleSerialBefore
			|| Refs.Dodge->RequestDodge(Refs.Character->GetActorForwardVector())
			|| Refs.Dodge->GetDodgeRequestSerial() != DodgeSerialBefore)
		{
			AbortWithError(TEXT("Heavy commitment did not block toggle, dodge and combo buffering."));
			return false;
		}

		if (bResolveManually)
		{
			Refs.Combat->UpdateSwordAttackHit(1.0f);
			Refs.Combat->EndSwordAttackSequence();
			bValidatedCommitLocks = true;
			return ValidateHeavyResult(Refs);
		}
		return true;
	}

	bool ValidateHeavyResult(const FRuntimeRefs& Refs)
	{
		if (Refs.Combat->GetSwordLightRequestSerial() != LightSerialBefore
			|| Refs.Combat->GetSwordHeavyRequestSerial() != HeavySerialBefore + 1
			|| Refs.Combat->GetSwordHitConfirmSerial() != HitConfirmBefore + 1
			|| Refs.Dummy->GetDamageSerial() != DummyDamageBefore + 1
			|| Refs.Dummy->IsDefeated()
			|| !FMath::IsNearlyEqual(
				Refs.Dummy->GetCurrentHealth(),
				Refs.Dummy->GetMaxHealth() - ExpectedHeavyDamage,
				0.01f)
			|| !FMath::IsNearlyEqual(
				Refs.Dummy->GetLastAppliedDamage(),
				ExpectedHeavyDamage,
				0.01f))
		{
			return AbortWithError(TEXT("Heavy release produced inconsistent damage or request serials."));
		}
		if (RunState->bPresentationConfigured && MaxHeavyPlanarDisplacement < 25.0f)
		{
			return AbortWithError(FString::Printf(
				TEXT("Sword_Attack_RM did not produce expected planar displacement (observed %.1f uu)."),
				MaxHeavyPlanarDisplacement));
		}

		ReleaseAllTestInput();
		Refs.Dummy->ResetForTesting();
		Test->AddInfo(FString::Printf(
			TEXT("Heavy committed only on release, played at 0.5x, displaced %.1f uu through root motion, held through a %.3fs blend, returned to Jog, and dealt 60 damage."),
			MaxHeavyPlanarDisplacement,
			ObservedHeavyBlendSeconds));
		return true;
	}

	int32 LightSerialBefore = 0;
	int32 HeavySerialBefore = 0;
	int32 AttackSerialBefore = 0;
	int32 HitConfirmBefore = 0;
	int32 DummyDamageBefore = 0;
	int32 ToggleSerialBefore = 0;
	int32 DodgeSerialBefore = 0;
	bool bChargeStarted = false;
	bool bReleased = false;
	bool bSawHeavyPresentation = false;
	bool bValidatedCommitLocks = false;
	bool bMovementInputInjected = false;
	bool bSawLockedMovementIntentBeforeBlend = false;
	bool bSawHeavyRecoveryBlend = false;
	bool bHeavyBlendCompleted = false;
	bool bSawJogMovementAfterBlend = false;
	bool bStoppedMovementInput = false;
	double HeavyBlendFirstObservedSeconds = 0.0;
	float ObservedHeavyBlendSeconds = 0.0f;
	FVector HeavyStartLocation = FVector::ZeroVector;
	FVector HeavyBlendCompletionLocation = FVector::ZeroVector;
	float MaxHeavyPlanarDisplacement = 0.0f;
};

class FValidateLockOnDodgeReturnCommand final : public FTimedCommand
{
public:
	FValidateLockOnDodgeReturnCommand(
		FAutomationTestBase* InTest,
		TSharedRef<FRunState> InRunState)
		: FTimedCommand(InTest, MoveTemp(InRunState), 12.0)
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
		if (!Refs.IsValid())
		{
			return AbortIfTimedOut(ElapsedSeconds, TEXT("runtime references disappeared"));
		}

		if (Phase == 0)
		{
			Refs.Combat->CancelActiveAttackForExternalReaction();
			Refs.Dummy->ResetForTesting();
			Refs.Movement->SetMovementMode(MOVE_Walking);
			Refs.Movement->StopMovementImmediately();
			Refs.Character->SetActorLocationAndRotation(
				FVector(0.0f, 0.0f, Refs.Character->GetActorLocation().Z),
				FRotator::ZeroRotator,
				false,
				nullptr,
				ETeleportType::TeleportPhysics);
			Refs.PlayerController->SetControlRotation(FRotator::ZeroRotator);
			Refs.Dummy->SetActorLocationAndRotation(
				FVector(350.0f, 0.0f, 112.0f),
				FRotator(0.0f, 180.0f, 0.0f),
				false,
				nullptr,
				ETeleportType::TeleportPhysics);
			Refs.Combat->EnterSwordCombat(ERequiemCombatEntryReason::Manual);
			if (!Refs.LockOn->TryAcquireLockOn()
				|| Refs.LockOn->GetLockOnTarget() != Refs.Dummy
				|| Refs.Combat->GetCombatState() != ERequiemCombatState::CombatSword)
			{
				return AbortWithError(TEXT("Lock-on did not acquire the live dummy while preserving CombatSword."));
			}
			DodgeSerialBefore = Refs.Dodge->GetDodgeRequestSerial();
			LightSerialBefore = Refs.Combat->GetSwordLightRequestSerial();
			if (!Refs.Combat->BeginSwordCharge()
				|| !Refs.Dodge->RequestDodge(Refs.Character->GetActorForwardVector())
				|| Refs.Combat->IsSwordChargeActive()
				|| Refs.Dodge->GetDodgeRequestSerial() != DodgeSerialBefore + 1)
			{
				return AbortWithError(TEXT("Accepted dodge did not win over and cancel an uncommitted sword hold."));
			}
			Phase = 1;
			return false;
		}

		if (Phase == 1)
		{
			if (!Refs.Dodge->IsDodgeActive()
				|| !Refs.LockOn->IsLockOnActive()
				|| Refs.LockOn->GetLockOnTarget() != Refs.Dummy
				|| Refs.Combat->GetCombatState() != ERequiemCombatState::CombatSword
				|| !IsSwordInHand(Refs)
				|| Refs.Combat->BeginSwordCharge())
			{
				return AbortWithError(TEXT("Dodge did not preserve lock-on/style or failed to reject attack input."));
			}
			Refs.Dodge->FinishDodge();
			Phase = 2;
			return false;
		}

		if (Phase == 2)
		{
			if (Refs.Dodge->IsDodgeActive()
				|| !Refs.LockOn->IsLockOnActive()
				|| !Refs.Combat->BeginSwordCharge()
				|| Refs.Combat->ReleaseSwordCharge()
					!= ERequiemSwordAttackRequestResult::InitialLightAccepted)
			{
				return AbortWithError(TEXT("Light attack could not start after lock-on dodge recovery."));
			}
			Phase = 3;
			return false;
		}

		if (Phase == 3)
		{
			if (!Refs.LockOn->IsLockOnActive()
				|| Refs.LockOn->GetLockOnTarget() != Refs.Dummy
				|| Refs.Combat->GetSwordLightRequestSerial() != LightSerialBefore + 1)
			{
				return AbortWithError(TEXT("Sword attack cleared lock-on or duplicated its request."));
			}
			Refs.Combat->CancelActiveAttackForExternalReaction();
			if (!Refs.Combat->ToggleSwordCombat()
				|| Refs.Combat->GetCombatState() != ERequiemCombatState::CombatUnarmed
				|| !Refs.LockOn->IsLockOnActive())
			{
				return AbortWithError(TEXT("Storing after lock-on did not return to CombatUnarmed."));
			}
			Phase = 4;
			return false;
		}

		const bool bSwordStored = RunState->bPresentationConfigured
			? Refs.AnimInstance->GetSwordAnimationState()
					== ERequiemSwordAnimationState::Inactive
				&& IsSwordStoredOnBack(Refs)
			: Refs.Combat->GetCombatState() == ERequiemCombatState::CombatUnarmed;
		if (bSwordStored)
		{
			const ERequiemUnarmedAttackRequestResult UnarmedResult =
				Refs.Combat->RequestUnarmedAttack();
			if (UnarmedResult != ERequiemUnarmedAttackRequestResult::InitialAccepted)
			{
				return AbortWithError(TEXT("Unarmed attack contract was not restored after storing sword."));
			}
			Refs.Combat->CancelActiveAttackForExternalReaction();
			Refs.LockOn->ClearLockOn();
			Test->AddInfo(TEXT("Lock-on survived sword attack/dodge, and storing restored CombatUnarmed."));
			return true;
		}

		return AbortIfTimedOut(ElapsedSeconds, TEXT("waiting for final sword store presentation"));
	}

private:
	int32 Phase = 0;
	int32 DodgeSerialBefore = 0;
	int32 LightSerialBefore = 0;
};

class FReleaseSwordTestInputCommand final : public IAutomationLatentCommand
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
			Test->AddError(TEXT("Sword combat PIE world did not end within 5 seconds."));
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
	FRequiemSwordCombatPIETest,
	"ProjectRequiem.Combat.SwordPIE",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::NonNullRHI
		| EAutomationTestFlags::ProductFilter)

bool FRequiemSwordCombatPIETest::RunTest(const FString& Parameters)
{
	using namespace RequiemSwordCombatPIETest;

	UInputMappingContext* MappingContext =
		LoadObject<UInputMappingContext>(nullptr, MappingContextPath);
	UInputAction* ToggleCombatAction =
		LoadObject<UInputAction>(nullptr, ToggleCombatActionPath);
	UInputAction* PrimaryAttackAction =
		LoadObject<UInputAction>(nullptr, PrimaryAttackActionPath);
	UInputAction* MoveAction =
		LoadObject<UInputAction>(nullptr, MoveActionPath);
	TestNotNull(TEXT("IMC_Exploration exists"), MappingContext);
	TestNotNull(TEXT("IA_ToggleCombat exists"), ToggleCombatAction);
	TestNotNull(TEXT("IA_PrimaryAttack exists"), PrimaryAttackAction);
	TestNotNull(TEXT("IA_Move exists"), MoveAction);
	if (!MappingContext || !ToggleCombatAction || !PrimaryAttackAction || !MoveAction)
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
	TestEqual(
		TEXT("IA_Move is Axis2D"),
		MoveAction->ValueType,
		EInputActionValueType::Axis2D);
	TestTrue(
		TEXT("IMC_Exploration maps Z to sword toggle"),
		HasExactMapping(MappingContext, ToggleCombatAction, EKeys::Z));
	TestTrue(
		TEXT("IMC_Exploration maps LMB to sword press/release"),
		HasExactMapping(MappingContext, PrimaryAttackAction, EKeys::LeftMouseButton));
	TestTrue(
		TEXT("IMC_Exploration maps W to movement used by recovery validation"),
		HasExactMapping(MappingContext, MoveAction, EKeys::W));

	UClass* CharacterClass = LoadClass<ARequiemCharacter>(nullptr, CharacterClassPath);
	ARequiemCharacter* CharacterCDO = CharacterClass
		? Cast<ARequiemCharacter>(CharacterClass->GetDefaultObject())
		: nullptr;
	TestNotNull(TEXT("BP_CH_Player generated class exists"), CharacterClass);
	TestNotNull(TEXT("BP_CH_Player CDO exists"), CharacterCDO);
	URequiemCombatComponent* CombatCDO =
		CharacterCDO ? CharacterCDO->GetCombatComponent() : nullptr;
	UStaticMeshComponent* SwordMeshCDO = CharacterCDO ? CharacterCDO->GetSwordMesh() : nullptr;
	TestNotNull(TEXT("BP_CH_Player owns RequiemCombatComponent"), CombatCDO);
	TestNotNull(TEXT("BP_CH_Player owns presentation-only SwordMesh"), SwordMeshCDO);
	if (CharacterCDO && CharacterClass)
	{
		TestEqual(
			TEXT("BP_CH_Player assigns IA_ToggleCombat"),
			GetObjectPropertyValue(CharacterCDO, CharacterClass, TEXT("ToggleCombatAction")),
			static_cast<UObject*>(ToggleCombatAction));
		TestEqual(
			TEXT("BP_CH_Player assigns IA_PrimaryAttack"),
			GetObjectPropertyValue(CharacterCDO, CharacterClass, TEXT("PrimaryAttackAction")),
			static_cast<UObject*>(PrimaryAttackAction));
	}
	if (CombatCDO)
	{
		TestTrue(
			TEXT("Sword hold threshold remains 0.65s"),
			FMath::IsNearlyEqual(
				CombatCDO->GetSwordChargeThresholdSeconds(),
				ExpectedChargeThresholdSeconds,
				0.01f));
		TestTrue(
			TEXT("Sword light damage remains 35"),
			FMath::IsNearlyEqual(
				CombatCDO->GetSwordLightHitDamage(), ExpectedLightDamage, 0.01f));
		TestTrue(
			TEXT("Sword heavy damage remains 60"),
			FMath::IsNearlyEqual(
				CombatCDO->GetSwordHeavyHitDamage(), ExpectedHeavyDamage, 0.01f));
	}
	if (SwordMeshCDO)
	{
		TestEqual(
			TEXT("Sword visual collision remains disabled"),
			SwordMeshCDO->GetCollisionEnabled(),
			ECollisionEnabled::NoCollision);
		TestEqual(
			TEXT("Sword visual starts attached to the back socket"),
			SwordMeshCDO->GetAttachSocketName(),
			SwordBackSocketName);
		TestTrue(TEXT("Stored sword starts visible"), IsSwordVisible(SwordMeshCDO));
	}
	if (CharacterCDO)
	{
		TestEqual(
			TEXT("BP_CH_Player uses the authored hand socket"),
			CharacterCDO->GetSwordHandSocketName(),
			SwordHandSocketName);
		TestEqual(
			TEXT("BP_CH_Player uses the dedicated back socket"),
			CharacterCDO->GetSwordBackSocketName(),
			SwordBackSocketName);

		USkeletalMesh* PlayerSkeletalMesh = CharacterCDO->GetMesh()
			? CharacterCDO->GetMesh()->GetSkeletalMeshAsset()
			: nullptr;
		TestNotNull(TEXT("BP_CH_Player has a skeletal mesh for sword sockets"), PlayerSkeletalMesh);
		if (PlayerSkeletalMesh)
		{
			const USkeletalMeshSocket* HandSocket =
				PlayerSkeletalMesh->FindSocket(SwordHandSocketName);
			const USkeletalMeshSocket* BackSocket =
				PlayerSkeletalMesh->FindSocket(SwordBackSocketName);
			TestNotNull(TEXT("Socket_Weapon_Hand_R exists"), HandSocket);
			TestNotNull(TEXT("Socket_Weapon_Back exists"), BackSocket);
			if (HandSocket)
			{
				TestEqual(
					TEXT("Hand socket remains parented to hand_r"),
					HandSocket->BoneName,
					SwordHandBoneName);
			}
			if (BackSocket)
			{
				TestEqual(
					TEXT("Back socket follows spine_03"),
					BackSocket->BoneName,
					SwordBackBoneName);
			}
		}
	}

	UClass* AnimInstanceClass =
		LoadClass<URequiemPlayerAnimInstance>(nullptr, AnimInstanceClassPath);
	URequiemPlayerAnimInstance* AnimInstanceCDO = AnimInstanceClass
		? Cast<URequiemPlayerAnimInstance>(AnimInstanceClass->GetDefaultObject())
		: nullptr;
	TestNotNull(TEXT("ABP_PR_PlayerLocomotion generated class exists"), AnimInstanceClass);
	TestNotNull(TEXT("ABP_PR_PlayerLocomotion CDO exists"), AnimInstanceCDO);

	bool bPresentationConfigured = false;
	if (AnimInstanceCDO && AnimInstanceClass)
	{
		TestTrue(
			TEXT("Sword light attack play rate remains 1.0x"),
			FMath::IsNearlyEqual(
				GetFloatPropertyValue(
					AnimInstanceCDO, AnimInstanceClass, TEXT("SwordLightAttackPlayRate")),
				ExpectedLightPlayRate,
				0.01f));
		TestTrue(
			TEXT("Sword light recovery play rate remains 1.0x"),
			FMath::IsNearlyEqual(
				GetFloatPropertyValue(
					AnimInstanceCDO, AnimInstanceClass, TEXT("SwordLightRecoveryPlayRate")),
				ExpectedLightPlayRate,
				0.01f));
		TestTrue(
			TEXT("Sword heavy play rate remains 0.5x"),
			FMath::IsNearlyEqual(
				GetFloatPropertyValue(
					AnimInstanceCDO, AnimInstanceClass, TEXT("SwordHeavyAttackPlayRate")),
				ExpectedHeavyPlayRate,
				0.01f));
		TestTrue(
			TEXT("Sword input window starts at 0.30"),
			FMath::IsNearlyEqual(
				GetFloatPropertyValue(
					AnimInstanceCDO, AnimInstanceClass, TEXT("SwordInputWindowStartNormalized")),
				ExpectedInputWindowStart,
				0.001f));
		TestTrue(
			TEXT("Sword input window ends at 0.85"),
			FMath::IsNearlyEqual(
				GetFloatPropertyValue(
					AnimInstanceCDO, AnimInstanceClass, TEXT("SwordInputWindowEndNormalized")),
				ExpectedInputWindowEnd,
				0.001f));
		const float MovementUnlockNormalized = GetFloatPropertyValue(
			AnimInstanceCDO,
			AnimInstanceClass,
			TEXT("SwordMovementUnlockNormalized"));
		TestTrue(
			TEXT("Sword movement unlock is 0.75"),
			FMath::IsNearlyEqual(
				MovementUnlockNormalized,
				ExpectedMovementUnlock,
				0.001f));
		TestTrue(
			TEXT("Sword movement unlock stays in the requested 70-75 percent range"),
			MovementUnlockNormalized >= 0.70f
				&& MovementUnlockNormalized <= 0.75f);
		const float RecoveryBlendTime = GetFloatPropertyValue(
			AnimInstanceCDO,
			AnimInstanceClass,
			TEXT("SwordRecoveryBlendTime"));
		TestTrue(
			TEXT("Sword recovery blend is 0.15s"),
			FMath::IsNearlyEqual(
				RecoveryBlendTime,
				ExpectedRecoveryBlendTime,
				0.001f));
		TestTrue(
			TEXT("Sword recovery blend stays in the requested 0.10-0.20s range"),
			RecoveryBlendTime >= MinimumRecoveryBlendTime
				&& RecoveryBlendTime <= MaximumRecoveryBlendTime);
		TestTrue(
			TEXT("Sword_Enter attaches to the hand at authored frame 21"),
			FMath::IsNearlyEqual(
				GetFloatPropertyValue(
					AnimInstanceCDO,
					AnimInstanceClass,
					TEXT("SwordEnterHandAttachmentNormalized")),
				ExpectedEnterHandAttachment,
				0.0001f));
		TestTrue(
			TEXT("Sword_Exit attaches to the back at authored frame 15"),
			FMath::IsNearlyEqual(
				GetFloatPropertyValue(
					AnimInstanceCDO,
					AnimInstanceClass,
					TEXT("SwordExitBackAttachmentNormalized")),
				ExpectedExitBackAttachment,
				0.0001f));

		UAnimSequence* EnterAnimation = Cast<UAnimSequence>(GetObjectPropertyValue(
			AnimInstanceCDO,
			AnimInstanceClass,
			TEXT("SwordEnterAnimation")));
		UAnimSequence* ExitAnimation = Cast<UAnimSequence>(GetObjectPropertyValue(
			AnimInstanceCDO,
			AnimInstanceClass,
			TEXT("SwordExitAnimation")));
		TestNotNull(TEXT("Sword_Enter is assigned as an AnimSequence"), EnterAnimation);
		TestNotNull(TEXT("Sword_Exit is assigned as an AnimSequence"), ExitAnimation);
		for (const TPair<const TCHAR*, UAnimSequence*> Transition : {
			TPair<const TCHAR*, UAnimSequence*>(TEXT("Sword_Enter"), EnterAnimation),
			TPair<const TCHAR*, UAnimSequence*>(TEXT("Sword_Exit"), ExitAnimation)})
		{
			if (!Transition.Value)
			{
				continue;
			}
			TestTrue(
				FString::Printf(TEXT("%s duration remains 1.30s"), Transition.Key),
				FMath::IsNearlyEqual(
					Transition.Value->GetPlayLength(),
					ExpectedSwordTransitionDuration,
					0.001f));
			TestEqual(
				FString::Printf(TEXT("%s keeps 40 sampled keys"), Transition.Key),
				Transition.Value->GetNumberOfSampledKeys(),
				ExpectedSwordTransitionSampledKeys);
			TestTrue(
				FString::Printf(TEXT("%s keeps a 30 fps sample rate"), Transition.Key),
				FMath::IsNearlyEqual(
					Transition.Value->GetSamplingFrameRate().AsDecimal(),
					30.0,
					0.001));
		}

		int32 AssignedAnimationCount = 0;
		for (const TCHAR* PropertyName : SwordAnimationPropertyNames)
		{
			AssignedAnimationCount +=
				GetObjectPropertyValue(AnimInstanceCDO, AnimInstanceClass, PropertyName)
				? 1
				: 0;
		}
		const bool bSwordMeshAssigned = SwordMeshCDO && SwordMeshCDO->GetStaticMesh();
		UAnimSequence* HeavyAnimation = Cast<UAnimSequence>(GetObjectPropertyValue(
			AnimInstanceCDO,
			AnimInstanceClass,
			TEXT("SwordHeavyAttackAnimation")));
		TestNotNull(TEXT("Sword_Attack_RM is assigned as an AnimSequence"), HeavyAnimation);
		if (HeavyAnimation)
		{
			TestTrue(TEXT("Sword_Attack_RM has root motion enabled"), HeavyAnimation->bEnableRootMotion);
		}
		const bool bAnyPresentationAsset = bSwordMeshAssigned || AssignedAnimationCount > 0;
		bPresentationConfigured =
			bSwordMeshAssigned && AssignedAnimationCount == ExpectedSwordClipCount;
		if (bAnyPresentationAsset)
		{
			TestTrue(
				TEXT("Sword presentation setup is complete, not partially assigned"),
				bPresentationConfigured);
		}
		else
		{
			AddInfo(TEXT("Sword presentation assets are not assigned yet; gameplay contracts will run and asset-dependent playback/visibility assertions are skipped."));
		}
	}

	UClass* DummyClass = LoadClass<ARequiemCombatDummy>(nullptr, DummyClassPath);
	TestNotNull(TEXT("BP_PR_CombatDummy generated class exists"), DummyClass);
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
	RunState->bPresentationConfigured = bPresentationConfigured;
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForPIEReadyCommand(this, RunState));
	ADD_LATENT_AUTOMATION_COMMAND(FValidateEquipVisibilityCommand(this, RunState));
	ADD_LATENT_AUTOMATION_COMMAND(FValidateExitDamageAttachmentCommand(this, RunState));
	ADD_LATENT_AUTOMATION_COMMAND(FValidateLightRecoveryBlendCommand(this, RunState));
	ADD_LATENT_AUTOMATION_COMMAND(FValidateLightComboCommand(this, RunState));
	ADD_LATENT_AUTOMATION_COMMAND(FValidateHeavyReleaseCommand(this, RunState));
	ADD_LATENT_AUTOMATION_COMMAND(FValidateLockOnDodgeReturnCommand(this, RunState));
	ADD_LATENT_AUTOMATION_COMMAND(FReleaseSwordTestInputCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForPIEEndCommand(this));
	return true;
}

#endif // WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
