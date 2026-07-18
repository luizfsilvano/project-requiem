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
#include "Engine/World.h"
#include "EngineUtils.h"
#include "EnhancedActionKeyMapping.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
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
constexpr TCHAR CharacterClassPath[] =
	TEXT("/Game/ProjectRequiem/Characters/Player/Blueprints/BP_CH_Player.BP_CH_Player_C");
constexpr TCHAR AnimInstanceClassPath[] =
	TEXT("/Game/ProjectRequiem/Characters/Player/Animations/Locomotion/"
		 "ABP_PR_PlayerLocomotion.ABP_PR_PlayerLocomotion_C");
constexpr TCHAR DummyClassPath[] =
	TEXT("/Game/ProjectRequiem/Combat/Blueprints/Targets/"
		 "BP_PR_CombatDummy.BP_PR_CombatDummy_C");

constexpr float ExpectedChargeThresholdSeconds = 0.65f;
constexpr float ExpectedLightDamage = 35.0f;
constexpr float ExpectedHeavyDamage = 60.0f;
constexpr float ExpectedLightPlayRate = 1.0f;
constexpr float ExpectedHeavyPlayRate = 0.5f;
constexpr float ExpectedInputWindowStart = 0.30f;
constexpr float ExpectedInputWindowEnd = 0.85f;
constexpr float ExpectedMovementUnlock = 0.60f;
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

bool IsSwordVisible(const UStaticMeshComponent* SwordMesh)
{
	return SwordMesh && SwordMesh->IsVisible() && !SwordMesh->bHiddenInGame;
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
				&& !IsSwordVisible(Refs.SwordMesh))
			{
				Test->AddInfo(TEXT("L_Dev_Foundation started with one live dummy and the sword stored."));
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
			if (IsSwordVisible(Refs.SwordMesh))
			{
				return AbortWithError(TEXT("Sword was visible before equip."));
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
			bSawEnter |= Refs.AnimInstance->GetSwordAnimationState()
				== ERequiemSwordAnimationState::Enter;
			const bool bEquipReady = RunState->bPresentationConfigured
				? Refs.AnimInstance->GetSwordAnimationState()
						== ERequiemSwordAnimationState::Idle
					&& IsSwordVisible(Refs.SwordMesh)
				: ElapsedSeconds >= 0.75;
			if (!bEquipReady)
			{
				return AbortIfTimedOut(ElapsedSeconds, TEXT("waiting for sword equip presentation"));
			}
			if (RunState->bPresentationConfigured && !bSawEnter)
			{
				return AbortWithError(TEXT("Configured sword equip skipped its stationary enter clip."));
			}
			if (Refs.Combat->GetCombatState() != ERequiemCombatState::CombatSword)
			{
				return AbortWithError(TEXT("Sword equip did not remain in CombatSword."));
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
			bSawExit |= Refs.AnimInstance->GetSwordAnimationState()
				== ERequiemSwordAnimationState::Exit;
			const bool bStored = RunState->bPresentationConfigured
				? Refs.AnimInstance->GetSwordAnimationState()
						== ERequiemSwordAnimationState::Inactive
					&& !IsSwordVisible(Refs.SwordMesh)
				: ElapsedSeconds >= 1.25;
			if (!bStored)
			{
				return AbortIfTimedOut(ElapsedSeconds, TEXT("waiting for sword store presentation"));
			}
			if (RunState->bPresentationConfigured && !bSawExit)
			{
				return AbortWithError(TEXT("Configured sword store skipped its stationary exit clip."));
			}
			if (!Refs.Combat->ToggleSwordCombat()
				|| Refs.Combat->GetSwordToggleSerial() != ToggleSerialBefore + 3)
			{
				return AbortWithError(TEXT("Sword could not be equipped again after storing."));
			}
			Phase = 3;
			return false;
		}

		const bool bReadyForCombat = RunState->bPresentationConfigured
			? Refs.AnimInstance->GetSwordAnimationState() == ERequiemSwordAnimationState::Idle
				&& IsSwordVisible(Refs.SwordMesh)
			: Refs.Combat->GetCombatState() == ERequiemCombatState::CombatSword;
		if (bReadyForCombat)
		{
			Test->AddInfo(TEXT("Sword equip/store state, serials and configured visibility are coherent."));
			return true;
		}
		return AbortIfTimedOut(ElapsedSeconds, TEXT("waiting for the re-equipped sword"));
	}

private:
	int32 Phase = 0;
	int32 ToggleSerialBefore = 0;
	bool bSawEnter = false;
	bool bSawExit = false;
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

		MaxHeavyPlanarDisplacement = FMath::Max(
			MaxHeavyPlanarDisplacement,
			FVector::Dist2D(HeavyStartLocation, Refs.Character->GetActorLocation()));

		if (RunState->bPresentationConfigured
			&& Refs.AnimInstance->GetSwordAnimationState()
				== ERequiemSwordAnimationState::HeavyAttack)
		{
			bSawHeavyPresentation = true;
			if (!FMath::IsNearlyEqual(
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
		}

		const bool bFinished = RunState->bPresentationConfigured
			&& bSawHeavyPresentation
			&& bValidatedCommitLocks
			&& !Refs.Combat->IsSwordAttackActive()
			&& Refs.AnimInstance->GetSwordAnimationState() == ERequiemSwordAnimationState::Idle
			&& Refs.Combat->GetSwordHitConfirmSerial() == HitConfirmBefore + 1;
		if (bFinished)
		{
			return ValidateHeavyResult(Refs);
		}

		return AbortIfTimedOut(
			ElapsedSeconds,
			FString::Printf(
				TEXT("waiting for heavy completion (state=%s active=%d health=%.1f)"),
				*Refs.AnimInstance->GetSwordAnimationStateName().ToString(),
				Refs.Combat->IsSwordAttackActive(),
				Refs.Dummy->GetCurrentHealth()));
	}

private:
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

		Refs.Dummy->ResetForTesting();
		Test->AddInfo(FString::Printf(
			TEXT("Heavy committed only on release, played at 0.5x, displaced %.1f uu through root motion, and dealt 60 damage."),
			MaxHeavyPlanarDisplacement));
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
	FVector HeavyStartLocation = FVector::ZeroVector;
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
				&& !IsSwordVisible(Refs.SwordMesh)
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
		TEXT("IMC_Exploration maps Z to sword toggle"),
		HasExactMapping(MappingContext, ToggleCombatAction, EKeys::Z));
	TestTrue(
		TEXT("IMC_Exploration maps LMB to sword press/release"),
		HasExactMapping(MappingContext, PrimaryAttackAction, EKeys::LeftMouseButton));

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
			TEXT("Sword visual stays attached to hand_r"),
			SwordMeshCDO->GetAttachSocketName(),
			FName(TEXT("hand_r")));
		TestFalse(TEXT("Sword starts visually stored"), IsSwordVisible(SwordMeshCDO));
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
		TestTrue(
			TEXT("Sword movement unlock remains 0.60"),
			FMath::IsNearlyEqual(
				GetFloatPropertyValue(
					AnimInstanceCDO, AnimInstanceClass, TEXT("SwordMovementUnlockNormalized")),
				ExpectedMovementUnlock,
				0.001f));

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
	ADD_LATENT_AUTOMATION_COMMAND(FValidateLightComboCommand(this, RunState));
	ADD_LATENT_AUTOMATION_COMMAND(FValidateHeavyReleaseCommand(this, RunState));
	ADD_LATENT_AUTOMATION_COMMAND(FValidateLockOnDodgeReturnCommand(this, RunState));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForPIEEndCommand(this));
	return true;
}

#endif // WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
