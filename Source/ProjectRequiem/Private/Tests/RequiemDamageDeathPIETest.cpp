// Copyright Project Requiem. All Rights Reserved.

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Animation/RequiemPlayerAnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Characters/RequiemCharacter.h"
#include "Components/RequiemCombatComponent.h"
#include "Components/RequiemDodgeComponent.h"
#include "Components/RequiemHealthComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "UObject/UnrealType.h"

namespace RequiemDamageDeathPIETest
{
constexpr TCHAR MapPackagePath[] =
	TEXT("/Game/ProjectRequiem/World/Maps/Dev/L_Dev_Foundation");
constexpr TCHAR CharacterClassPath[] =
	TEXT("/Game/ProjectRequiem/Characters/Player/Blueprints/BP_CH_Player.BP_CH_Player_C");
constexpr TCHAR AnimInstanceClassPath[] =
	TEXT("/Game/ProjectRequiem/Characters/Player/Animations/Locomotion/"
		 "ABP_PR_PlayerLocomotion.ABP_PR_PlayerLocomotion_C");
constexpr TCHAR PlayerSkeletonPath[] =
	TEXT("/Game/ProjectRequiem/Characters/Player/Meshes/Temporary/"
		 "SKEL_Temp_SuperheroFemale.SKEL_Temp_SuperheroFemale");

struct FClipExpectation
{
	const TCHAR* AssetPath;
	const TCHAR* PropertyName;
	FName AnimationName;
	bool bRootMotion;
};

const FClipExpectation DamageClips[] = {
	{
		TEXT("/Game/ProjectRequiem/Characters/Player/Animations/Damage/UAL1/"
			 "Hit_Head.Hit_Head"),
		TEXT("HitHeadAnimation"),
		FName(TEXT("Hit_Head")),
		false,
	},
	{
		TEXT("/Game/ProjectRequiem/Characters/Player/Animations/Damage/UAL1/"
			 "Hit_Chest.Hit_Chest"),
		TEXT("HitChestAnimation"),
		FName(TEXT("Hit_Chest")),
		false,
	},
	{
		TEXT("/Game/ProjectRequiem/Characters/Player/Animations/Damage/UAL1/"
			 "Hit_Stomach.Hit_Stomach"),
		TEXT("HitStomachAnimation"),
		FName(TEXT("Hit_Stomach")),
		false,
	},
	{
		TEXT("/Game/ProjectRequiem/Characters/Player/Animations/Damage/UAL1/"
			 "Hit_Shoulder_L.Hit_Shoulder_L"),
		TEXT("HitShoulderLeftAnimation"),
		FName(TEXT("Hit_Shoulder_L")),
		false,
	},
	{
		TEXT("/Game/ProjectRequiem/Characters/Player/Animations/Damage/UAL1/"
			 "Hit_Shoulder_R.Hit_Shoulder_R"),
		TEXT("HitShoulderRightAnimation"),
		FName(TEXT("Hit_Shoulder_R")),
		false,
	},
	{
		TEXT("/Game/ProjectRequiem/Characters/Player/Animations/Damage/UAL2_RM/"
			 "Hit_Knockback.Hit_Knockback"),
		TEXT("HitKnockbackAnimation"),
		FName(TEXT("Hit_Knockback")),
		true,
	},
	{
		TEXT("/Game/ProjectRequiem/Characters/Player/Animations/Death/UAL1/"
			 "Death01.Death01"),
		TEXT("Death01Animation"),
		FName(TEXT("Death01")),
		false,
	},
	{
		TEXT("/Game/ProjectRequiem/Characters/Player/Animations/Death/UAL1/"
			 "Death02.Death02"),
		TEXT("Death02Animation"),
		FName(TEXT("Death02")),
		false,
	},
};

struct FRunState
{
	bool bAborted = false;
	bool bHasStartTransform = false;
	FTransform StartTransform = FTransform::Identity;
	FRotator StartControlRotation = FRotator::ZeroRotator;
};

struct FRuntimeRefs
{
	APlayerController* PlayerController = nullptr;
	ARequiemCharacter* Character = nullptr;
	UCharacterMovementComponent* Movement = nullptr;
	URequiemCombatComponent* Combat = nullptr;
	URequiemDodgeComponent* Dodge = nullptr;
	URequiemHealthComponent* Health = nullptr;
	URequiemPlayerAnimInstance* AnimInstance = nullptr;

	bool IsValid() const
	{
		return PlayerController
			&& Character
			&& Movement
			&& Combat
			&& Dodge
			&& Health
			&& AnimInstance;
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
	UWorld* World = FindPIEWorld();
	Refs.PlayerController = World ? World->GetFirstPlayerController() : nullptr;
	Refs.Character = Refs.PlayerController
		? Cast<ARequiemCharacter>(Refs.PlayerController->GetPawn())
		: nullptr;
	Refs.Movement = Refs.Character ? Refs.Character->GetCharacterMovement() : nullptr;
	Refs.Combat = Refs.Character ? Refs.Character->GetCombatComponent() : nullptr;
	Refs.Dodge = Refs.Character ? Refs.Character->GetDodgeComponent() : nullptr;
	Refs.Health = Refs.Character ? Refs.Character->GetHealthComponent() : nullptr;
	Refs.AnimInstance = Refs.Character && Refs.Character->GetMesh()
		? Cast<URequiemPlayerAnimInstance>(
			Refs.Character->GetMesh()->GetAnimInstance())
		: nullptr;
	return Refs;
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

ERootMotionMode::Type ReadRootMotionMode(const UAnimInstance* AnimInstance)
{
	const FByteProperty* Property =
		FindFProperty<FByteProperty>(UAnimInstance::StaticClass(), TEXT("RootMotionMode"));
	return AnimInstance && Property
		? static_cast<ERootMotionMode::Type>(
			Property->GetPropertyValue_InContainer(AnimInstance))
		: ERootMotionMode::NoRootMotionExtraction;
}

FName GetActiveAnimationName(const URequiemPlayerAnimInstance* AnimInstance)
{
	const UAnimSequenceBase* Animation = AnimInstance
		? AnimInstance->GetActivePresentationAnimation()
		: nullptr;
	return Animation ? Animation->GetFName() : NAME_None;
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
		return true;
	}

	bool AbortIfTimedOut(const double ElapsedSeconds, const FString& Details)
	{
		if (ElapsedSeconds < TimeoutSeconds)
		{
			return false;
		}
		return AbortWithError(FString::Printf(
			TEXT("Damage/death PIE stage timed out after %.1fs: %s"),
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
		UWorld* World = FindPIEWorld();
		const FRuntimeRefs Refs = FindRuntimeRefs();
		if (World && Refs.IsValid())
		{
			if (!World->GetMapName().Contains(TEXT("L_Dev_Foundation")))
			{
				return AbortWithError(FString::Printf(
					TEXT("PIE opened the wrong map: %s"),
					*World->GetMapName()));
			}

			if (StableCharacter.Get() != Refs.Character)
			{
				StableCharacter = Refs.Character;
				StableSinceSeconds = ElapsedSeconds;
				return false;
			}
			if (ElapsedSeconds - StableSinceSeconds < 0.4)
			{
				return false;
			}

			if (!RunState->bHasStartTransform)
			{
				RunState->StartTransform = Refs.Character->GetActorTransform();
				RunState->StartControlRotation =
					Refs.PlayerController->GetControlRotation();
				RunState->bHasStartTransform = true;
			}

			const bool bReady =
				Refs.Health->GetHealthState() == ERequiemHealthState::Alive
				&& FMath::IsNearlyEqual(
					Refs.Health->GetCurrentHealth(),
					Refs.Health->GetMaxHealth(),
					0.01f)
				&& Refs.Combat->GetCombatState() == ERequiemCombatState::Normal
				&& !Refs.Dodge->IsDodgeActive()
				&& Refs.Movement->MovementMode == MOVE_Walking;
			if (bReady)
			{
				Test->AddInfo(TEXT("PIE player started alive with health, combat and dodge components."));
				return true;
			}
		}

		return AbortIfTimedOut(ElapsedSeconds, TEXT("waiting for the possessed damage-ready player"));
	}

private:
	TWeakObjectPtr<ARequiemCharacter> StableCharacter;
	double StableSinceSeconds = -1.0;
};

class FResetCharacterCommand final : public FTimedCommand
{
public:
	FResetCharacterCommand(
		FAutomationTestBase* InTest,
		TSharedRef<FRunState> InRunState,
		const TCHAR* InLabel)
		: FTimedCommand(InTest, MoveTemp(InRunState), 5.0)
		, Label(InLabel)
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
		if (!Refs.IsValid() || !RunState->bHasStartTransform)
		{
			return AbortIfTimedOut(ElapsedSeconds, TEXT("recovering reset references"));
		}

		if (!bResetRequested)
		{
			ResetSerialBefore = Refs.Health->GetResetSerial();
			Refs.Health->ResetForTesting();
			bResetRequested = true;
			StableSinceSeconds = ElapsedSeconds;
			return false;
		}

		const bool bPresentationReset =
			Refs.Health->GetResetSerial() != ResetSerialBefore
				&& Refs.Health->GetHealthState() == ERequiemHealthState::Alive
				&& !Refs.Health->IsDead()
				&& Refs.AnimInstance->GetDamageAnimationState()
					== ERequiemDamageAnimationState::Inactive
				&& !Refs.AnimInstance->IsDeathPoseHeld()
				&& ReadRootMotionMode(Refs.AnimInstance)
					== ERootMotionMode::IgnoreRootMotion;
		if (bPresentationReset && !bTransformRestored)
		{
			Refs.Movement->StopMovementImmediately();
			Refs.Character->SetActorLocationAndRotation(
				RunState->StartTransform.GetLocation(),
				RunState->StartTransform.Rotator(),
				false,
				nullptr,
				ETeleportType::TeleportPhysics);
			Refs.PlayerController->SetControlRotation(RunState->StartControlRotation);
			bTransformRestored = true;
			StableSinceSeconds = ElapsedSeconds;
			return false;
		}

		const float DistanceFromStart = FVector::Dist2D(
			Refs.Character->GetActorLocation(),
			RunState->StartTransform.GetLocation());
		const bool bReset =
			bPresentationReset
				&& bTransformRestored
				&& FMath::IsNearlyEqual(
					Refs.Health->GetCurrentHealth(),
					Refs.Health->GetMaxHealth(),
					0.01f)
				&& Refs.Combat->GetCombatState() == ERequiemCombatState::Normal
				&& !Refs.Combat->IsUnarmedAttackActive()
				&& !Refs.Dodge->IsDodgeActive()
				&& Refs.Movement->MovementMode == MOVE_Walking
				&& DistanceFromStart <= 2.0f;
		if (bReset)
		{
			if (ElapsedSeconds - StableSinceSeconds < 0.12)
			{
				return false;
			}
			Test->AddInfo(FString::Printf(
				TEXT("%s restored full health, Alive, Normal and MOVE_Walking."),
				Label));
			return true;
		}

		StableSinceSeconds = ElapsedSeconds;
		return AbortIfTimedOut(
			ElapsedSeconds,
			FString::Printf(
				TEXT("%s (health=%.1f, state=%d, combat=%d, move=%d, damageAnim=%d, rootMode=%d, serial=%d/%d, attack=%d, dodge=%d, deathPose=%d, transform=%d, distance=%.1f)"),
				Label,
				Refs.Health->GetCurrentHealth(),
				static_cast<int32>(Refs.Health->GetHealthState()),
				static_cast<int32>(Refs.Combat->GetCombatState()),
				static_cast<int32>(Refs.Movement->MovementMode),
				static_cast<int32>(Refs.AnimInstance->GetDamageAnimationState()),
				static_cast<int32>(ReadRootMotionMode(Refs.AnimInstance)),
				ResetSerialBefore,
				Refs.Health->GetResetSerial(),
				Refs.Combat->IsUnarmedAttackActive(),
				Refs.Dodge->IsDodgeActive(),
				Refs.AnimInstance->IsDeathPoseHeld(),
				bTransformRestored,
				DistanceFromStart));
	}

private:
	const TCHAR* Label;
	int32 ResetSerialBefore = 0;
	double StableSinceSeconds = -1.0;
	bool bResetRequested = false;
	bool bTransformRestored = false;
};

class FValidateRegionDamageCommand final : public FTimedCommand
{
public:
	FValidateRegionDamageCommand(
		FAutomationTestBase* InTest,
		TSharedRef<FRunState> InRunState,
		const ERequiemHitRegion InRegion,
		const FName InExpectedAnimation)
		: FTimedCommand(InTest, MoveTemp(InRunState), 4.0)
		, Region(InRegion)
		, ExpectedAnimation(InExpectedAnimation)
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
			return AbortIfTimedOut(ElapsedSeconds, TEXT("reading regional damage references"));
		}

		if (!bDamageApplied)
		{
			HealthBefore = Refs.Health->GetCurrentHealth();
			DamageSerialBefore = Refs.Health->GetDamageRequestSerial();
			FRequiemDamageRequest Request;
			Request.DamageAmount = 10.0f;
			Request.HitRegion = Region;
			Request.Strength = ERequiemDamageStrength::Light;
			const ERequiemDamageOutcome Outcome =
				Refs.Character->ApplyRequiemDamage(Request);
			if (Outcome != ERequiemDamageOutcome::Applied)
			{
				return AbortWithError(FString::Printf(
					TEXT("%s damage returned outcome %d instead of Applied."),
					*ExpectedAnimation.ToString(),
					static_cast<int32>(Outcome)));
			}
			bDamageApplied = true;
			return false;
		}

		const bool bGameplayValid =
			Refs.Health->GetDamageRequestSerial() != DamageSerialBefore
				&& Refs.Health->GetLastHitRegion() == Region
				&& FMath::IsNearlyEqual(
					Refs.Health->GetCurrentHealth(),
					HealthBefore - 10.0f,
					0.01f)
				&& Refs.Combat->GetCombatState()
					== ERequiemCombatState::CombatUnarmed;
		const bool bPresentationValid =
			Refs.AnimInstance->GetDamageAnimationState()
					== ERequiemDamageAnimationState::HitReaction
				&& GetActiveAnimationName(Refs.AnimInstance) == ExpectedAnimation;
		if (bGameplayValid && bPresentationValid)
		{
			Test->AddInfo(FString::Printf(
				TEXT("%s selected the matching region reaction and entered CombatUnarmed."),
				*ExpectedAnimation.ToString()));
			return true;
		}

		return AbortIfTimedOut(
			ElapsedSeconds,
			FString::Printf(
				TEXT("%s (health=%.1f, region=%d, combat=%d, animState=%d, animation=%s)"),
				*ExpectedAnimation.ToString(),
				Refs.Health->GetCurrentHealth(),
				static_cast<int32>(Refs.Health->GetLastHitRegion()),
				static_cast<int32>(Refs.Combat->GetCombatState()),
				static_cast<int32>(Refs.AnimInstance->GetDamageAnimationState()),
				*GetActiveAnimationName(Refs.AnimInstance).ToString()));
	}

private:
	ERequiemHitRegion Region;
	FName ExpectedAnimation;
	float HealthBefore = 0.0f;
	int32 DamageSerialBefore = 0;
	bool bDamageApplied = false;
};

class FValidateStrongDamageCommand final : public FTimedCommand
{
public:
	FValidateStrongDamageCommand(
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
			return AbortIfTimedOut(ElapsedSeconds, TEXT("reading knockback references"));
		}

		if (!bDamageApplied)
		{
			StartLocation = Refs.Character->GetActorLocation();
			PushDirection =
				-Refs.Character->GetActorForwardVector().GetSafeNormal2D();
			HealthBefore = Refs.Health->GetCurrentHealth();
			FRequiemDamageRequest Request;
			Request.DamageAmount = 20.0f;
			Request.HitRegion = ERequiemHitRegion::Chest;
			Request.Strength = ERequiemDamageStrength::Strong;
			Request.ImpactDirection = PushDirection;
			const ERequiemDamageOutcome Outcome =
				Refs.Character->ApplyRequiemDamage(Request);
			if (Outcome != ERequiemDamageOutcome::Applied)
			{
				return AbortWithError(FString::Printf(
					TEXT("Strong damage returned outcome %d instead of Applied."),
					static_cast<int32>(Outcome)));
			}
			bDamageApplied = true;
			return false;
		}

		const FVector Displacement =
			(Refs.Character->GetActorLocation() - StartLocation).GetSafeNormal2D();
		const float Distance = FVector::Dist2D(
			Refs.Character->GetActorLocation(),
			StartLocation);
		const bool bPresentationValid =
			Refs.AnimInstance->GetDamageAnimationState()
					== ERequiemDamageAnimationState::Knockback
				&& GetActiveAnimationName(Refs.AnimInstance)
					== FName(TEXT("Hit_Knockback"))
				&& ReadRootMotionMode(Refs.AnimInstance)
					== ERootMotionMode::RootMotionFromMontagesOnly;
		const bool bDisplaced =
			Distance >= 5.0f
				&& FVector::DotProduct(Displacement, PushDirection) >= 0.65f;
		if (bPresentationValid
			&& bDisplaced
			&& FMath::IsNearlyEqual(
				Refs.Health->GetCurrentHealth(),
				HealthBefore - 20.0f,
				0.01f))
		{
			Test->AddInfo(FString::Printf(
				TEXT("Strong damage played Hit_Knockback with root motion and moved %.1f uu."),
				Distance));
			return true;
		}

		return AbortIfTimedOut(
			ElapsedSeconds,
			FString::Printf(
				TEXT("knockback (distance=%.1f, dot=%.2f, animState=%d, animation=%s, rootMode=%d)"),
				Distance,
				FVector::DotProduct(Displacement, PushDirection),
				static_cast<int32>(Refs.AnimInstance->GetDamageAnimationState()),
				*GetActiveAnimationName(Refs.AnimInstance).ToString(),
				static_cast<int32>(ReadRootMotionMode(Refs.AnimInstance))));
	}

private:
	FVector StartLocation = FVector::ZeroVector;
	FVector PushDirection = FVector::ZeroVector;
	float HealthBefore = 0.0f;
	bool bDamageApplied = false;
};

class FValidateIFramesCommand final : public FTimedCommand
{
public:
	FValidateIFramesCommand(
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
			return AbortIfTimedOut(ElapsedSeconds, TEXT("reading dodge i-frame references"));
		}

		if (!bDodgeRequested)
		{
			if (!Refs.Dodge->RequestDodge(Refs.Character->GetActorForwardVector()))
			{
				return AbortWithError(TEXT("Could not start the i-frame validation dodge."));
			}
			bDodgeRequested = true;
			return false;
		}

		if (!Refs.Dodge->ShouldIgnoreIncomingDamage())
		{
			return AbortIfTimedOut(
				ElapsedSeconds,
				FString::Printf(
					TEXT("waiting for dodge i-frame window (normalized=%.3f)"),
					Refs.Dodge->GetDodgeNormalizedTime()));
		}

		const float HealthBefore = Refs.Health->GetCurrentHealth();
		const int32 DamageSerialBefore = Refs.Health->GetDamageRequestSerial();
		const int32 DeathSerialBefore = Refs.Health->GetDeathSerial();
		FRequiemDamageRequest Request;
		Request.DamageAmount = 50.0f;
		Request.HitRegion = ERequiemHitRegion::Head;
		Request.Strength = ERequiemDamageStrength::Strong;
		const ERequiemDamageOutcome Outcome =
			Refs.Character->ApplyRequiemDamage(Request);
		const bool bIgnored =
			Outcome == ERequiemDamageOutcome::IgnoredInvulnerable
				&& FMath::IsNearlyEqual(
					Refs.Health->GetCurrentHealth(),
					HealthBefore,
					0.01f)
				&& Refs.Health->GetDamageRequestSerial() == DamageSerialBefore
				&& Refs.Health->GetDeathSerial() == DeathSerialBefore;
		Refs.Dodge->FinishDodge();
		if (!bIgnored)
		{
			return AbortWithError(FString::Printf(
				TEXT("I-frame damage was not fully ignored (outcome=%d, health=%.1f, serial=%d)."),
				static_cast<int32>(Outcome),
				Refs.Health->GetCurrentHealth(),
				Refs.Health->GetDamageRequestSerial()));
		}

		Test->AddInfo(TEXT("Damage during dodge i-frames was IgnoredInvulnerable without health or serial changes."));
		return true;
	}

private:
	bool bDodgeRequested = false;
};

class FValidateDeathCommand final : public FTimedCommand
{
public:
	FValidateDeathCommand(
		FAutomationTestBase* InTest,
		TSharedRef<FRunState> InRunState,
		const ERequiemDeathAnimation InDeathAnimation,
		const FName InExpectedAnimation)
		: FTimedCommand(InTest, MoveTemp(InRunState), 8.0)
		, DeathAnimation(InDeathAnimation)
		, ExpectedAnimation(InExpectedAnimation)
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
			return AbortIfTimedOut(ElapsedSeconds, TEXT("reading death references"));
		}

		if (!bKilled)
		{
			FRequiemDamageRequest Request;
			Request.DamageAmount = Refs.Health->GetCurrentHealth() + 1.0f;
			Request.HitRegion = ERequiemHitRegion::Chest;
			Request.Strength = ERequiemDamageStrength::Strong;
			Request.DeathAnimation = DeathAnimation;
			const ERequiemDamageOutcome Outcome =
				Refs.Character->ApplyRequiemDamage(Request);
			if (Outcome != ERequiemDamageOutcome::Killed)
			{
				return AbortWithError(FString::Printf(
					TEXT("%s lethal damage returned outcome %d instead of Killed."),
					*ExpectedAnimation.ToString(),
					static_cast<int32>(Outcome)));
			}
			DamageSerialAfterDeath = Refs.Health->GetDamageRequestSerial();
			DeathSerialAfterDeath = Refs.Health->GetDeathSerial();
			bKilled = true;
			return false;
		}

		const bool bDeathPresentation =
			Refs.Health->IsDead()
				&& Refs.Health->GetCurrentHealth() <= UE_KINDA_SMALL_NUMBER
				&& Refs.Health->GetSelectedDeathAnimation() == DeathAnimation
				&& Refs.AnimInstance->GetDamageAnimationState()
					== ERequiemDamageAnimationState::Death
				&& GetActiveAnimationName(Refs.AnimInstance) == ExpectedAnimation
				&& Refs.Movement->MovementMode == MOVE_None;
		if (!bDeathPresentation)
		{
			return AbortIfTimedOut(
				ElapsedSeconds,
				FString::Printf(
					TEXT("%s presentation (dead=%d, move=%d, state=%d, animation=%s)"),
					*ExpectedAnimation.ToString(),
					Refs.Health->IsDead(),
					static_cast<int32>(Refs.Movement->MovementMode),
					static_cast<int32>(Refs.AnimInstance->GetDamageAnimationState()),
					*GetActiveAnimationName(Refs.AnimInstance).ToString()));
		}

		if (!bLocksProbed)
		{
			ProbeLocation = Refs.Character->GetActorLocation();
			AttackSerialBeforeProbe = Refs.Combat->GetAttackRequestSerial();
			DodgeSerialBeforeProbe = Refs.Dodge->GetDodgeRequestSerial();

			const ERequiemUnarmedAttackRequestResult AttackResult =
				Refs.Combat->RequestUnarmedAttack();
			const bool bDodgeAccepted =
				Refs.Dodge->RequestDodge(Refs.Character->GetActorForwardVector());
			Refs.Character->Jump();
			Refs.Character->Crouch();
			Refs.Character->AddMovementInput(
				Refs.Character->GetActorForwardVector(),
				1.0f);

			FRequiemDamageRequest PostDeathRequest;
			PostDeathRequest.DamageAmount = 10.0f;
			PostDeathRequest.HitRegion = ERequiemHitRegion::Head;
			const ERequiemDamageOutcome PostDeathOutcome =
				Refs.Character->ApplyRequiemDamage(PostDeathRequest);

			if (AttackResult != ERequiemUnarmedAttackRequestResult::Rejected
				|| bDodgeAccepted
				|| PostDeathOutcome != ERequiemDamageOutcome::IgnoredDead
				|| Refs.Health->GetDamageRequestSerial() != DamageSerialAfterDeath
				|| Refs.Health->GetDeathSerial() != DeathSerialAfterDeath)
			{
				return AbortWithError(FString::Printf(
					TEXT("%s did not reject attack/dodge/post-death damage idempotently."),
					*ExpectedAnimation.ToString()));
			}

			bLocksProbed = true;
			ProbeTimeSeconds = ElapsedSeconds;
			return false;
		}

		const bool bActionsStillBlocked =
			Refs.Combat->GetAttackRequestSerial() == AttackSerialBeforeProbe
				&& Refs.Dodge->GetDodgeRequestSerial() == DodgeSerialBeforeProbe
				&& !Refs.Combat->IsUnarmedAttackActive()
				&& !Refs.Dodge->IsDodgeActive()
				&& !Refs.Character->CanJump()
				&& !Refs.Character->CanCrouch()
				&& !Refs.Character->IsCrouched()
				&& !Refs.Movement->IsFalling()
				&& Refs.Movement->MovementMode == MOVE_None
				&& Refs.Character->GetVelocity().Size() <= 1.0f
				&& FVector::Dist(
					Refs.Character->GetActorLocation(),
					ProbeLocation) <= 1.0f
				&& Refs.Health->GetDamageRequestSerial() == DamageSerialAfterDeath
				&& Refs.Health->GetDeathSerial() == DeathSerialAfterDeath;
		if (bActionsStillBlocked
			&& Refs.AnimInstance->IsDeathPoseHeld()
			&& ElapsedSeconds - ProbeTimeSeconds >= 0.15)
		{
			Test->AddInfo(FString::Printf(
				TEXT("%s held its final pose; death blocked movement, attack, jump, crouch and dodge, and remained idempotent."),
				*ExpectedAnimation.ToString()));
			return true;
		}

		return AbortIfTimedOut(
			ElapsedSeconds,
			FString::Printf(
				TEXT("%s action locks (blocked=%d, poseHeld=%d, distance=%.2f, attackSerial=%d, dodgeSerial=%d)"),
				*ExpectedAnimation.ToString(),
				bActionsStillBlocked,
				Refs.AnimInstance->IsDeathPoseHeld(),
				FVector::Dist(Refs.Character->GetActorLocation(), ProbeLocation),
				Refs.Combat->GetAttackRequestSerial(),
				Refs.Dodge->GetDodgeRequestSerial()));
	}

private:
	ERequiemDeathAnimation DeathAnimation;
	FName ExpectedAnimation;
	FVector ProbeLocation = FVector::ZeroVector;
	int32 DamageSerialAfterDeath = 0;
	int32 DeathSerialAfterDeath = 0;
	int32 AttackSerialBeforeProbe = 0;
	int32 DodgeSerialBeforeProbe = 0;
	double ProbeTimeSeconds = -1.0;
	bool bKilled = false;
	bool bLocksProbed = false;
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
	FRequiemDamageDeathPIETest,
	"ProjectRequiem.Combat.DamageDeathPIE",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::NonNullRHI
		| EAutomationTestFlags::ProductFilter)

bool FRequiemDamageDeathPIETest::RunTest(const FString& Parameters)
{
	using namespace RequiemDamageDeathPIETest;

	UClass* CharacterClass = LoadClass<ARequiemCharacter>(nullptr, CharacterClassPath);
	ARequiemCharacter* CharacterCDO = CharacterClass
		? Cast<ARequiemCharacter>(CharacterClass->GetDefaultObject())
		: nullptr;
	TestNotNull(TEXT("BP_CH_Player generated class exists"), CharacterClass);
	TestNotNull(TEXT("BP_CH_Player CDO exists"), CharacterCDO);
	URequiemHealthComponent* HealthCDO =
		CharacterCDO ? CharacterCDO->GetHealthComponent() : nullptr;
	TestNotNull(TEXT("BP_CH_Player owns RequiemHealthComponent"), HealthCDO);
	if (HealthCDO)
	{
		TestTrue(
			TEXT("Player max health is positive"),
			HealthCDO->GetMaxHealth() > 0.0f);
		TestTrue(
			TEXT("Strong damage threshold is positive and below max health"),
			HealthCDO->GetStrongDamageThreshold() > 0.0f
				&& HealthCDO->GetStrongDamageThreshold() < HealthCDO->GetMaxHealth());
		TestEqual(
			TEXT("Health CDO starts Alive"),
			HealthCDO->GetHealthState(),
			ERequiemHealthState::Alive);
	}

	UClass* AnimInstanceClass = LoadClass<URequiemPlayerAnimInstance>(
		nullptr,
		AnimInstanceClassPath);
	URequiemPlayerAnimInstance* AnimInstanceCDO = AnimInstanceClass
		? Cast<URequiemPlayerAnimInstance>(AnimInstanceClass->GetDefaultObject())
		: nullptr;
	TestNotNull(TEXT("ABP_PR_PlayerLocomotion generated class exists"), AnimInstanceClass);
	TestNotNull(TEXT("ABP_PR_PlayerLocomotion CDO exists"), AnimInstanceCDO);
	if (AnimInstanceCDO)
	{
		TestEqual(
			TEXT("ABP defaults to ignoring root motion"),
			ReadRootMotionMode(AnimInstanceCDO),
			ERootMotionMode::IgnoreRootMotion);
	}

	USkeleton* PlayerSkeleton = LoadObject<USkeleton>(nullptr, PlayerSkeletonPath);
	TestNotNull(TEXT("Player skeleton exists"), PlayerSkeleton);
	for (const FClipExpectation& Clip : DamageClips)
	{
		UAnimSequence* Animation = LoadObject<UAnimSequence>(nullptr, Clip.AssetPath);
		TestNotNull(
			*FString::Printf(TEXT("%s exists"), *Clip.AnimationName.ToString()),
			Animation);
		if (!Animation)
		{
			continue;
		}

		TestEqual(
			*FString::Printf(
				TEXT("%s root-motion flag matches its role"),
				*Clip.AnimationName.ToString()),
			Animation->bEnableRootMotion,
			Clip.bRootMotion);
		TestEqual(
			*FString::Printf(
				TEXT("%s uses the player skeleton"),
				*Clip.AnimationName.ToString()),
			Animation->GetSkeleton(),
			PlayerSkeleton);
		TestTrue(
			*FString::Printf(
				TEXT("%s has a positive duration"),
				*Clip.AnimationName.ToString()),
			Animation->GetPlayLength() > UE_KINDA_SMALL_NUMBER);
		if (AnimInstanceCDO && AnimInstanceClass)
		{
			TestEqual(
				*FString::Printf(
					TEXT("ABP assigns %s"),
					*Clip.AnimationName.ToString()),
				GetObjectPropertyValue(
					AnimInstanceCDO,
					AnimInstanceClass,
					Clip.PropertyName),
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

	struct FRegionCase
	{
		ERequiemHitRegion Region;
		FName AnimationName;
		const TCHAR* ResetLabel;
	};
	const FRegionCase RegionCases[] = {
		{ERequiemHitRegion::Head, FName(TEXT("Hit_Head")), TEXT("reset before Head")},
		{ERequiemHitRegion::Chest, FName(TEXT("Hit_Chest")), TEXT("reset before Chest")},
		{ERequiemHitRegion::Stomach, FName(TEXT("Hit_Stomach")), TEXT("reset before Stomach")},
		{ERequiemHitRegion::ShoulderLeft, FName(TEXT("Hit_Shoulder_L")), TEXT("reset before ShoulderLeft")},
		{ERequiemHitRegion::ShoulderRight, FName(TEXT("Hit_Shoulder_R")), TEXT("reset before ShoulderRight")},
	};
	for (const FRegionCase& RegionCase : RegionCases)
	{
		ADD_LATENT_AUTOMATION_COMMAND(FResetCharacterCommand(
			this,
			RunState,
			RegionCase.ResetLabel));
		ADD_LATENT_AUTOMATION_COMMAND(FValidateRegionDamageCommand(
			this,
			RunState,
			RegionCase.Region,
			RegionCase.AnimationName));
	}

	ADD_LATENT_AUTOMATION_COMMAND(FResetCharacterCommand(
		this, RunState, TEXT("reset before strong damage")));
	ADD_LATENT_AUTOMATION_COMMAND(FValidateStrongDamageCommand(this, RunState));

	ADD_LATENT_AUTOMATION_COMMAND(FResetCharacterCommand(
		this, RunState, TEXT("reset before i-frames")));
	ADD_LATENT_AUTOMATION_COMMAND(FValidateIFramesCommand(this, RunState));

	ADD_LATENT_AUTOMATION_COMMAND(FResetCharacterCommand(
		this, RunState, TEXT("reset before Death01")));
	ADD_LATENT_AUTOMATION_COMMAND(FValidateDeathCommand(
		this,
		RunState,
		ERequiemDeathAnimation::Death01,
		FName(TEXT("Death01"))));
	ADD_LATENT_AUTOMATION_COMMAND(FResetCharacterCommand(
		this, RunState, TEXT("reset after Death01")));

	ADD_LATENT_AUTOMATION_COMMAND(FValidateDeathCommand(
		this,
		RunState,
		ERequiemDeathAnimation::Death02,
		FName(TEXT("Death02"))));
	ADD_LATENT_AUTOMATION_COMMAND(FResetCharacterCommand(
		this, RunState, TEXT("final reset after Death02")));

	// Every failed phase only marks the shared state, so teardown remains unconditional.
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForPIEEndCommand(this));
	return true;
}

#endif // WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
