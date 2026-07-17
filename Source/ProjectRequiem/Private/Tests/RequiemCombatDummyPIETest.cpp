// Copyright Project Requiem. All Rights Reserved.

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Animation/RequiemPlayerAnimInstance.h"
#include "Characters/RequiemCharacter.h"
#include "Combat/RequiemCombatDummy.h"
#include "Components/CapsuleComponent.h"
#include "Components/RequiemCombatComponent.h"
#include "Components/RequiemDodgeComponent.h"
#include "Components/RequiemHealthComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"

namespace RequiemCombatDummyPIETest
{
constexpr TCHAR MapPackagePath[] =
	TEXT("/Game/ProjectRequiem/World/Maps/Dev/L_Dev_Foundation");
constexpr TCHAR DummyClassPath[] =
	TEXT("/Game/ProjectRequiem/Combat/Blueprints/Targets/"
		 "BP_PR_CombatDummy.BP_PR_CombatDummy_C");
constexpr TCHAR CharacterClassPath[] =
	TEXT("/Game/ProjectRequiem/Characters/Player/Blueprints/BP_CH_Player.BP_CH_Player_C");
constexpr TCHAR FabStaticMeshPath[] =
	TEXT("/Game/Fab/Medieval_Combat_Dummy/medieval_combat_dummy/StaticMeshes/"
		 "medieval_combat_dummy.medieval_combat_dummy");
constexpr float ExpectedPlayerHitDamage = 25.0f;
constexpr float ExpectedDummyAttackDamage = 20.0f;

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
	URequiemPlayerAnimInstance* AnimInstance = nullptr;
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
			&& AnimInstance
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

void ResetAndPositionPair(const FRuntimeRefs& Refs)
{
	Refs.Health->ResetForTesting();
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
		FVector(145.0f, 0.0f, 112.0f),
		FRotator(0.0f, 180.0f, 0.0f),
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
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
			TEXT("Combat dummy PIE stage timed out after %.1fs: %s"),
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
		if (Refs.IsValid()
			&& Refs.World->GetMapName().Contains(TEXT("L_Dev_Foundation"))
			&& Refs.Health->GetHealthState() == ERequiemHealthState::Alive
			&& Refs.Dummy->GetDummyState() == ERequiemCombatDummyState::Alive
			&& Refs.Dummy->GetVisualMesh()
			&& Refs.Dummy->GetVisualMesh()->GetStaticMesh()
			&& Refs.Dummy->GetHitCollision()
			&& Refs.Dummy->GetHitCollision()->GetCollisionEnabled()
				== ECollisionEnabled::QueryAndPhysics)
		{
			if (StableDummy.Get() != Refs.Dummy)
			{
				StableDummy = Refs.Dummy;
				StableSinceSeconds = ElapsedSeconds;
				return false;
			}
			if (ElapsedSeconds - StableSinceSeconds >= 0.25)
			{
				Test->AddInfo(TEXT("L_Dev_Foundation started with one configured ProjectRequiem combat dummy."));
				return true;
			}
		}

		return AbortIfTimedOut(
			ElapsedSeconds,
			FString::Printf(
				TEXT("waiting for player and exactly one dummy (count=%d)"),
				Refs.DummyCount));
	}

private:
	TWeakObjectPtr<ARequiemCombatDummy> StableDummy;
	double StableSinceSeconds = -1.0;
};

class FValidatePlayerHitCommand final : public FTimedCommand
{
public:
	FValidatePlayerHitCommand(FAutomationTestBase* InTest, TSharedRef<FRunState> InRunState)
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
		const FRuntimeRefs Refs = FindRuntimeRefs();
		if (!Refs.IsValid())
		{
			return AbortIfTimedOut(ElapsedSeconds, TEXT("recovering player-hit references"));
		}

		if (!bAttackRequested)
		{
			ResetAndPositionPair(Refs);
			HealthBefore = Refs.Dummy->GetCurrentHealth();
			DamageSerialBefore = Refs.Dummy->GetDamageSerial();
			FeedbackSerialBefore = Refs.Dummy->GetHitFeedbackSerial();
			AttemptSerialBefore = Refs.Combat->GetUnarmedHitAttemptSerial();
			ConfirmSerialBefore = Refs.Combat->GetUnarmedHitConfirmSerial();
			if (Refs.Combat->RequestUnarmedAttack()
				!= ERequiemUnarmedAttackRequestResult::InitialAccepted)
			{
				return AbortWithError(TEXT("Player primary attack was not accepted for dummy validation."));
			}
			bAttackRequested = true;
			return false;
		}

		if (Refs.Dummy->GetDamageSerial() > DamageSerialBefore + 1
			|| Refs.Combat->GetUnarmedHitAttemptSerial() > AttemptSerialBefore + 1
			|| Refs.Combat->GetUnarmedHitConfirmSerial() > ConfirmSerialBefore + 1)
		{
			return AbortWithError(TEXT("One unarmed clip applied or attempted more than one dummy hit."));
		}

		const bool bHitConfirmed =
			Refs.Dummy->GetDamageSerial() == DamageSerialBefore + 1
			&& Refs.Dummy->GetHitFeedbackSerial() == FeedbackSerialBefore + 1
			&& Refs.Combat->GetUnarmedHitAttemptSerial() == AttemptSerialBefore + 1
			&& Refs.Combat->GetUnarmedHitConfirmSerial() == ConfirmSerialBefore + 1
			&& Refs.Combat->GetLastUnarmedHitActor() == Refs.Dummy
			&& FMath::IsNearlyEqual(
				Refs.Dummy->GetCurrentHealth(),
				HealthBefore - ExpectedPlayerHitDamage,
				0.01f);
		if (bHitConfirmed)
		{
			if (HitConfirmedAtSeconds < 0.0)
			{
				HitConfirmedAtSeconds = ElapsedSeconds;
				return false;
			}
			if (ElapsedSeconds - HitConfirmedAtSeconds >= 0.35)
			{
				Test->AddInfo(TEXT("One authored unarmed strike dealt exactly one hit and triggered dummy feedback."));
				return true;
			}
		}

		return AbortIfTimedOut(
			ElapsedSeconds,
			FString::Printf(
				TEXT("player hit (health=%.1f, damage=%d/%d, attempt=%d/%d, confirm=%d/%d)"),
				Refs.Dummy->GetCurrentHealth(),
				DamageSerialBefore,
				Refs.Dummy->GetDamageSerial(),
				AttemptSerialBefore,
				Refs.Combat->GetUnarmedHitAttemptSerial(),
				ConfirmSerialBefore,
				Refs.Combat->GetUnarmedHitConfirmSerial()));
	}

private:
	float HealthBefore = 0.0f;
	int32 DamageSerialBefore = 0;
	int32 FeedbackSerialBefore = 0;
	int32 AttemptSerialBefore = 0;
	int32 ConfirmSerialBefore = 0;
	double HitConfirmedAtSeconds = -1.0;
	bool bAttackRequested = false;
};

class FValidateDummyDefeatResetCommand final : public FTimedCommand
{
public:
	FValidateDummyDefeatResetCommand(
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
		const FRuntimeRefs Refs = FindRuntimeRefs();
		if (!Refs.IsValid())
		{
			return AbortIfTimedOut(ElapsedSeconds, TEXT("recovering defeat/reset references"));
		}

		if (Phase == 0)
		{
			DamageSerialBefore = Refs.Dummy->GetDamageSerial();
			DefeatSerialBefore = Refs.Dummy->GetDefeatSerial();
			const float AppliedDamage = UGameplayStatics::ApplyPointDamage(
				Refs.Dummy,
				Refs.Dummy->GetCurrentHealth(),
				Refs.Character->GetActorForwardVector(),
				FHitResult(),
				Refs.PlayerController,
				Refs.Character,
				nullptr);
			if (AppliedDamage <= UE_KINDA_SMALL_NUMBER
				|| !Refs.Dummy->IsDefeated()
				|| Refs.Dummy->GetDamageSerial() != DamageSerialBefore + 1
				|| Refs.Dummy->GetDefeatSerial() != DefeatSerialBefore + 1
				|| Refs.Dummy->GetHitCollision()->GetCollisionEnabled()
					!= ECollisionEnabled::NoCollision)
			{
				return AbortWithError(TEXT("Player-caused lethal damage did not defeat and disable the dummy."));
			}
			Phase = 1;
			return false;
		}

		if (Phase == 1)
		{
			FRequiemDamageRequest PostDefeatRequest;
			PostDefeatRequest.DamageAmount = 10.0f;
			if (Refs.Dummy->ApplyRequiemDamage(PostDefeatRequest)
				!= ERequiemDamageOutcome::IgnoredDead
				|| Refs.Dummy->GetDamageSerial() != DamageSerialBefore + 1
				|| Refs.Dummy->GetDefeatSerial() != DefeatSerialBefore + 1)
			{
				return AbortWithError(TEXT("Damage after dummy defeat was not ignored idempotently."));
			}
			ResetSerialBefore = Refs.Dummy->GetResetSerial();
			Refs.Dummy->ResetForTesting();
			Phase = 2;
			return false;
		}

		const bool bReset =
			Refs.Dummy->GetResetSerial() == ResetSerialBefore + 1
			&& Refs.Dummy->GetDummyState() == ERequiemCombatDummyState::Alive
			&& FMath::IsNearlyEqual(
				Refs.Dummy->GetCurrentHealth(), Refs.Dummy->GetMaxHealth(), 0.01f)
			&& Refs.Dummy->GetHitCollision()->GetCollisionEnabled()
				== ECollisionEnabled::QueryAndPhysics
			&& !Refs.Dummy->IsHitFeedbackActive();
		if (bReset)
		{
			Test->AddInfo(TEXT("Dummy defeat was idempotent and its test reset restored health, state and collision."));
			return true;
		}
		return AbortIfTimedOut(ElapsedSeconds, TEXT("waiting for dummy reset"));
	}

private:
	int32 DamageSerialBefore = 0;
	int32 DefeatSerialBefore = 0;
	int32 ResetSerialBefore = 0;
	int32 Phase = 0;
};

class FValidateDummyAttackCommand final : public FTimedCommand
{
public:
	FValidateDummyAttackCommand(FAutomationTestBase* InTest, TSharedRef<FRunState> InRunState)
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
		const FRuntimeRefs Refs = FindRuntimeRefs();
		if (!Refs.IsValid())
		{
			return AbortIfTimedOut(ElapsedSeconds, TEXT("recovering dummy-attack references"));
		}

		if (!bAttackRequested)
		{
			ResetAndPositionPair(Refs);
			HealthBefore = Refs.Health->GetCurrentHealth();
			DamageSerialBefore = Refs.Health->GetDamageRequestSerial();
			ResolveSerialBefore = Refs.Dummy->GetTestAttackResolveSerial();
			if (!Refs.Dummy->RequestTestAttack(Refs.Character))
			{
				return AbortWithError(TEXT("Dummy did not accept its stationary test attack."));
			}
			bAttackRequested = true;
			return false;
		}

		const bool bResolved =
			Refs.Dummy->GetTestAttackResolveSerial() == ResolveSerialBefore + 1
			&& Refs.Dummy->GetLastTestAttackOutcome() == ERequiemDamageOutcome::Applied
			&& FMath::IsNearlyEqual(
				Refs.Health->GetCurrentHealth(),
				HealthBefore - ExpectedDummyAttackDamage,
				0.01f)
			&& Refs.Health->GetDamageRequestSerial() == DamageSerialBefore + 1
			&& Refs.Health->GetLastHitRegion() == ERequiemHitRegion::Chest
			&& Refs.Combat->GetCombatState() == ERequiemCombatState::CombatUnarmed;
		if (bResolved)
		{
			Test->AddInfo(TEXT("Dummy attack damaged the player and automatically entered CombatUnarmed."));
			return true;
		}

		return AbortIfTimedOut(
			ElapsedSeconds,
			FString::Printf(
				TEXT("dummy attack (outcome=%d, health=%.1f, serial=%d/%d, combat=%d)"),
				static_cast<int32>(Refs.Dummy->GetLastTestAttackOutcome()),
				Refs.Health->GetCurrentHealth(),
				DamageSerialBefore,
				Refs.Health->GetDamageRequestSerial(),
				static_cast<int32>(Refs.Combat->GetCombatState())));
	}

private:
	float HealthBefore = 0.0f;
	int32 DamageSerialBefore = 0;
	int32 ResolveSerialBefore = 0;
	bool bAttackRequested = false;
};

class FValidateDummyAttackIFramesCommand final : public FTimedCommand
{
public:
	FValidateDummyAttackIFramesCommand(
		FAutomationTestBase* InTest,
		TSharedRef<FRunState> InRunState)
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
		const FRuntimeRefs Refs = FindRuntimeRefs();
		if (!Refs.IsValid())
		{
			return AbortIfTimedOut(ElapsedSeconds, TEXT("recovering i-frame dummy references"));
		}

		if (!bDodgeRequested)
		{
			ResetAndPositionPair(Refs);
			HealthBefore = Refs.Health->GetCurrentHealth();
			DamageSerialBefore = Refs.Health->GetDamageRequestSerial();
			DeathSerialBefore = Refs.Health->GetDeathSerial();
			ResolveSerialBefore = Refs.Dummy->GetTestAttackResolveSerial();
			if (!Refs.Dodge->RequestDodge(Refs.Character->GetActorForwardVector()))
			{
				return AbortWithError(TEXT("Could not start dodge for dummy i-frame validation."));
			}
			bDodgeRequested = true;
			return false;
		}

		if (!bAttackRequested)
		{
			const float DodgeNormalizedTime = Refs.Dodge->GetDodgeNormalizedTime();
			const bool bInsideSafeIFrameWindow =
				Refs.Dodge->ShouldIgnoreIncomingDamage()
				&& DodgeNormalizedTime >= 0.30f
				&& DodgeNormalizedTime <= 0.45f;
			if (!bInsideSafeIFrameWindow)
			{
				return AbortIfTimedOut(
					ElapsedSeconds,
					FString::Printf(
						TEXT("waiting for safe internal i-frame margin (normalized=%.3f)"),
						DodgeNormalizedTime));
			}
			if (!Refs.Dummy->RequestTestAttack(Refs.Character, 0.01f))
			{
				return AbortWithError(TEXT("Dummy did not accept its i-frame test attack."));
			}
			bAttackRequested = true;
			return false;
		}

		if (Refs.Dummy->GetTestAttackResolveSerial() == ResolveSerialBefore + 1)
		{
			const bool bIgnored =
				Refs.Dummy->GetLastTestAttackOutcome()
					== ERequiemDamageOutcome::IgnoredInvulnerable
				&& FMath::IsNearlyEqual(Refs.Health->GetCurrentHealth(), HealthBefore, 0.01f)
				&& Refs.Health->GetDamageRequestSerial() == DamageSerialBefore
				&& Refs.Health->GetDeathSerial() == DeathSerialBefore
				&& Refs.Combat->GetCombatState() == ERequiemCombatState::Normal;
			Refs.Dodge->FinishDodge();
			if (!bIgnored)
			{
				return AbortWithError(FString::Printf(
					TEXT("Dummy attack was not fully ignored by i-frames (outcome=%d, health=%.1f, serial=%d)."),
					static_cast<int32>(Refs.Dummy->GetLastTestAttackOutcome()),
					Refs.Health->GetCurrentHealth(),
					Refs.Health->GetDamageRequestSerial()));
			}
			Test->AddInfo(TEXT("Dummy attack during dodge i-frames was ignored before health or combat changed."));
			return true;
		}

		return AbortIfTimedOut(ElapsedSeconds, TEXT("waiting for i-frame dummy attack resolution"));
	}

private:
	float HealthBefore = 0.0f;
	int32 DamageSerialBefore = 0;
	int32 DeathSerialBefore = 0;
	int32 ResolveSerialBefore = 0;
	bool bDodgeRequested = false;
	bool bAttackRequested = false;
};

class FValidatePlayerDeathFromDummyCommand final : public FTimedCommand
{
public:
	FValidatePlayerDeathFromDummyCommand(
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
			return AbortIfTimedOut(ElapsedSeconds, TEXT("recovering player-death references"));
		}

		if (Phase == 0)
		{
			ResetAndPositionPair(Refs);
			DamageSerialBefore = Refs.Health->GetDamageRequestSerial();
			DeathSerialBefore = Refs.Health->GetDeathSerial();
			LastResolveSerial = Refs.Dummy->GetTestAttackResolveSerial();
			Phase = 1;
			return false;
		}

		if (Phase == 1)
		{
			const int32 CurrentResolveSerial = Refs.Dummy->GetTestAttackResolveSerial();
			if (CurrentResolveSerial != LastResolveSerial)
			{
				LastResolveSerial = CurrentResolveSerial;
				++ResolvedAttacks;
				const ERequiemDamageOutcome ExpectedOutcome = ResolvedAttacks == 5
					? ERequiemDamageOutcome::Killed
					: ERequiemDamageOutcome::Applied;
				if (Refs.Dummy->GetLastTestAttackOutcome() != ExpectedOutcome)
				{
					return AbortWithError(FString::Printf(
						TEXT("Dummy lethal sequence attack %d returned %d instead of %d."),
						ResolvedAttacks,
						static_cast<int32>(Refs.Dummy->GetLastTestAttackOutcome()),
						static_cast<int32>(ExpectedOutcome)));
				}
			}

			if (ResolvedAttacks < 5 && !Refs.Dummy->IsTestAttackPending())
			{
				if (!Refs.Dummy->RequestTestAttack(Refs.Character, 0.01f))
				{
					return AbortWithError(TEXT("Dummy lethal sequence could not queue its next attack."));
				}
				return false;
			}

			const bool bDead =
				ResolvedAttacks == 5
				&& Refs.Health->IsDead()
				&& Refs.Health->GetDamageRequestSerial() == DamageSerialBefore + 5
				&& Refs.Health->GetDeathSerial() == DeathSerialBefore + 1
				&& Refs.Movement->MovementMode == MOVE_None
				&& Refs.AnimInstance->GetDamageAnimationState()
					== ERequiemDamageAnimationState::Death;
			if (!bDead)
			{
				return AbortIfTimedOut(
					ElapsedSeconds,
					FString::Printf(
						TEXT("waiting for dummy-caused death (attacks=%d, health=%.1f, dead=%d, move=%d, anim=%d)"),
						ResolvedAttacks,
						Refs.Health->GetCurrentHealth(),
						Refs.Health->IsDead(),
						static_cast<int32>(Refs.Movement->MovementMode),
						static_cast<int32>(Refs.AnimInstance->GetDamageAnimationState())));
			}

			AttackSerialBeforeProbe = Refs.Combat->GetAttackRequestSerial();
			DodgeSerialBeforeProbe = Refs.Dodge->GetDodgeRequestSerial();
			ProbeLocation = Refs.Character->GetActorLocation();
			const ERequiemUnarmedAttackRequestResult AttackResult =
				Refs.Combat->RequestUnarmedAttack();
			const bool bDodgeAccepted =
				Refs.Dodge->RequestDodge(Refs.Character->GetActorForwardVector());
			Refs.Character->Jump();
			Refs.Character->Crouch();
			Refs.Character->AddMovementInput(Refs.Character->GetActorForwardVector(), 1.0f);
			if (AttackResult != ERequiemUnarmedAttackRequestResult::Rejected
				|| bDodgeAccepted
				|| !Refs.Dummy->RequestTestAttack(Refs.Character, 0.01f))
			{
				return AbortWithError(TEXT("Dead player did not reject actions or accept the post-death damage probe."));
			}
			PostDeathResolveSerial = Refs.Dummy->GetTestAttackResolveSerial();
			ProbeStartedAtSeconds = ElapsedSeconds;
			Phase = 2;
			return false;
		}

		if (Phase == 2)
		{
			const bool bPostDeathResolved =
				Refs.Dummy->GetTestAttackResolveSerial() == PostDeathResolveSerial + 1;
			const bool bStillLocked =
				bPostDeathResolved
				&& Refs.Dummy->GetLastTestAttackOutcome() == ERequiemDamageOutcome::IgnoredDead
				&& Refs.Health->GetDamageRequestSerial() == DamageSerialBefore + 5
				&& Refs.Health->GetDeathSerial() == DeathSerialBefore + 1
				&& Refs.Combat->GetAttackRequestSerial() == AttackSerialBeforeProbe
				&& Refs.Dodge->GetDodgeRequestSerial() == DodgeSerialBeforeProbe
				&& !Refs.Character->CanJump()
				&& !Refs.Character->CanCrouch()
				&& !Refs.Character->IsCrouched()
				&& Refs.Movement->MovementMode == MOVE_None
				&& Refs.Character->GetVelocity().Size() <= 1.0f
				&& FVector::Dist(Refs.Character->GetActorLocation(), ProbeLocation) <= 1.0f;
			if (bStillLocked && ElapsedSeconds - ProbeStartedAtSeconds >= 0.15)
			{
				Refs.Health->ResetForTesting();
				Refs.Dummy->ResetForTesting();
				Phase = 3;
				return false;
			}
			return AbortIfTimedOut(ElapsedSeconds, TEXT("validating death action locks and post-death damage"));
		}

		const bool bReset =
			Refs.Health->GetHealthState() == ERequiemHealthState::Alive
			&& FMath::IsNearlyEqual(
				Refs.Health->GetCurrentHealth(), Refs.Health->GetMaxHealth(), 0.01f)
			&& Refs.Movement->MovementMode == MOVE_Walking
			&& Refs.Dummy->GetDummyState() == ERequiemCombatDummyState::Alive
			&& FMath::IsNearlyEqual(
				Refs.Dummy->GetCurrentHealth(), Refs.Dummy->GetMaxHealth(), 0.01f)
			&& Refs.AnimInstance->GetDamageAnimationState()
				== ERequiemDamageAnimationState::Inactive;
		if (bReset)
		{
			Test->AddInfo(TEXT("Dummy killed the player; death blocked every action, ignored later damage and reset cleanly."));
			return true;
		}
		return AbortIfTimedOut(ElapsedSeconds, TEXT("waiting for final player/dummy reset"));
	}

private:
	FVector ProbeLocation = FVector::ZeroVector;
	int32 DamageSerialBefore = 0;
	int32 DeathSerialBefore = 0;
	int32 LastResolveSerial = 0;
	int32 PostDeathResolveSerial = 0;
	int32 AttackSerialBeforeProbe = 0;
	int32 DodgeSerialBeforeProbe = 0;
	int32 ResolvedAttacks = 0;
	int32 Phase = 0;
	double ProbeStartedAtSeconds = -1.0;
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
			Test->AddError(TEXT("PIE did not finish within 5 seconds of combat-dummy teardown."));
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
	FRequiemCombatDummyPIETest,
	"ProjectRequiem.Combat.DummyPIE",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::NonNullRHI
		| EAutomationTestFlags::ProductFilter)

bool FRequiemCombatDummyPIETest::RunTest(const FString& Parameters)
{
	using namespace RequiemCombatDummyPIETest;

	UStaticMesh* FabStaticMesh = LoadObject<UStaticMesh>(nullptr, FabStaticMeshPath);
	UClass* DummyClass = LoadClass<ARequiemCombatDummy>(nullptr, DummyClassPath);
	ARequiemCombatDummy* DummyCDO = DummyClass
		? Cast<ARequiemCombatDummy>(DummyClass->GetDefaultObject())
		: nullptr;
	TestNotNull(TEXT("Inspected Fab StaticMesh exists"), FabStaticMesh);
	TestNotNull(TEXT("BP_PR_CombatDummy generated class exists"), DummyClass);
	TestNotNull(TEXT("BP_PR_CombatDummy CDO exists"), DummyCDO);
	if (DummyCDO)
	{
		TestNotNull(TEXT("Dummy owns its gameplay capsule"), DummyCDO->GetHitCollision());
		TestNotNull(TEXT("Dummy owns its presentation-only visual component"), DummyCDO->GetVisualMesh());
		if (DummyCDO->GetHitCollision())
		{
			TestEqual(
				TEXT("Dummy capsule uses Pawn object type"),
				DummyCDO->GetHitCollision()->GetCollisionObjectType(),
				ECC_Pawn);
			TestEqual(
				TEXT("Dummy capsule starts QueryAndPhysics"),
				DummyCDO->GetHitCollision()->GetCollisionEnabled(),
				ECollisionEnabled::QueryAndPhysics);
		}
		if (DummyCDO->GetVisualMesh())
		{
			TestEqual(
				TEXT("Fab mesh is assigned only to the ProjectRequiem visual"),
				DummyCDO->GetVisualMesh()->GetStaticMesh().Get(),
				FabStaticMesh);
			TestEqual(
				TEXT("Fab visual collision is disabled"),
				DummyCDO->GetVisualMesh()->GetCollisionEnabled(),
				ECollisionEnabled::NoCollision);
			TestTrue(
				TEXT("Fab visual uses the inspected 0.5 scale"),
				DummyCDO->GetVisualMesh()->GetRelativeScale3D().Equals(
					FVector(0.5f), 0.001f));
		}
		TestTrue(TEXT("Dummy max health is positive"), DummyCDO->GetMaxHealth() > 0.0f);
		TestTrue(
			TEXT("Dummy test attack damage remains 20"),
			FMath::IsNearlyEqual(
				DummyCDO->GetTestAttackDamage(), ExpectedDummyAttackDamage, 0.01f));
	}
	UClass* CharacterClass = LoadClass<ARequiemCharacter>(nullptr, CharacterClassPath);
	ARequiemCharacter* CharacterCDO = CharacterClass
		? Cast<ARequiemCharacter>(CharacterClass->GetDefaultObject())
		: nullptr;
	TestNotNull(TEXT("BP_CH_Player CDO exists for hit tuning"), CharacterCDO);
	URequiemCombatComponent* CombatCDO = CharacterCDO
		? CharacterCDO->GetCombatComponent()
		: nullptr;
	TestNotNull(TEXT("Native player CDO owns RequiemCombatComponent"), CombatCDO);
	if (CombatCDO)
	{
		TestTrue(
			TEXT("Player unarmed hit damage remains 25"),
			FMath::IsNearlyEqual(
				CombatCDO->GetUnarmedHitDamage(), ExpectedPlayerHitDamage, 0.01f));
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
	ADD_LATENT_AUTOMATION_COMMAND(FValidatePlayerHitCommand(this, RunState));
	ADD_LATENT_AUTOMATION_COMMAND(FValidateDummyDefeatResetCommand(this, RunState));
	ADD_LATENT_AUTOMATION_COMMAND(FValidateDummyAttackCommand(this, RunState));
	ADD_LATENT_AUTOMATION_COMMAND(FValidateDummyAttackIFramesCommand(this, RunState));
	ADD_LATENT_AUTOMATION_COMMAND(FValidatePlayerDeathFromDummyCommand(this, RunState));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForPIEEndCommand(this));
	return true;
}

#endif // WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
