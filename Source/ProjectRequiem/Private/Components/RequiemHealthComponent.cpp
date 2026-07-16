// Copyright Project Requiem. All Rights Reserved.

#include "Components/RequiemHealthComponent.h"

#include "Characters/RequiemCharacter.h"
#include "Components/RequiemCombatComponent.h"
#include "Components/RequiemDodgeComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

URequiemHealthComponent::URequiemHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void URequiemHealthComponent::BeginPlay()
{
	Super::BeginPlay();

	MaxHealth = FMath::Max(MaxHealth, 1.0f);
	CurrentHealth = MaxHealth;
	HealthState = ERequiemHealthState::Alive;
}

void URequiemHealthComponent::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!IsDamageReactionActive())
	{
		SetComponentTickEnabled(false);
		return;
	}

	if (!bReactionPresentationStarted)
	{
		const ARequiemCharacter* Character = Cast<ARequiemCharacter>(GetOwner());
		const URequiemDodgeComponent* DodgeComponent = Character
			? Character->GetDodgeComponent()
			: nullptr;
		if (DodgeComponent && DodgeComponent->IsDodgeActive())
		{
			return;
		}

		if (ReactionElapsedSeconds <= UE_KINDA_SMALL_NUMBER)
		{
			StopCharacterForReaction();
		}
		ReactionElapsedSeconds += FMath::Max(0.0f, DeltaTime);
		if (ReactionElapsedSeconds >= ActiveReactionDurationSeconds)
		{
			FinishDamageReactionPresentation();
		}
		return;
	}

	if (bPresentationClockSynchronizedSinceLastTick)
	{
		bPresentationClockSynchronizedSinceLastTick = false;
		return;
	}

	ReactionElapsedSeconds += FMath::Max(0.0f, DeltaTime);
	if (ReactionElapsedSeconds >= ActiveReactionDurationSeconds)
	{
		FinishDamageReactionPresentation();
	}
}

ERequiemDamageOutcome URequiemHealthComponent::ApplyDamage(
	const FRequiemDamageRequest& Request)
{
	if (!FMath::IsFinite(Request.DamageAmount)
		|| Request.DamageAmount <= UE_KINDA_SMALL_NUMBER)
	{
		return ERequiemDamageOutcome::IgnoredInvalid;
	}

	if (IsDead())
	{
		return ERequiemDamageOutcome::IgnoredDead;
	}

	ARequiemCharacter* Character = Cast<ARequiemCharacter>(GetOwner());
	URequiemDodgeComponent* DodgeComponent = Character
		? Character->GetDodgeComponent()
		: nullptr;
	if (DodgeComponent && DodgeComponent->ShouldIgnoreIncomingDamage())
	{
		return ERequiemDamageOutcome::IgnoredInvulnerable;
	}

	URequiemCombatComponent* CombatComponent = Character
		? Character->GetCombatComponent()
		: nullptr;
	if (CombatComponent)
	{
		CombatComponent->CancelUnarmedAttackForExternalReaction();
		CombatComponent->EnterUnarmedCombat(ERequiemCombatEntryReason::ReceivedDamage);
	}

	LastHitRegion = Request.HitRegion;
	LastDamageStrength = Request.Strength;
	LastImpactDirection = Request.ImpactDirection.GetSafeNormal2D();
	LastAppliedDamage = FMath::Min(CurrentHealth, Request.DamageAmount);
	CurrentHealth = FMath::Clamp(CurrentHealth - LastAppliedDamage, 0.0f, MaxHealth);
	DamageRequestSerial = AdvanceSerial(DamageRequestSerial);
	bReactionPresentationStarted = false;
	bPresentationClockSynchronizedSinceLastTick = false;
	ReactionElapsedSeconds = 0.0f;
	ActiveReactionDurationSeconds =
		FMath::Max(DefaultDamageReactionDurationSeconds, 0.05f);

	if (CurrentHealth <= UE_KINDA_SMALL_NUMBER)
	{
		EnterDeath(Request);
		return ERequiemDamageOutcome::Killed;
	}

	HealthState = ERequiemHealthState::Reacting;
	SetComponentTickEnabled(true);
	return ERequiemDamageOutcome::Applied;
}

void URequiemHealthComponent::BeginDamageReactionPresentation(
	const float DurationSeconds)
{
	if (!IsDamageReactionActive())
	{
		return;
	}

	bReactionPresentationStarted = true;
	bPresentationClockSynchronizedSinceLastTick = true;
	ReactionElapsedSeconds = 0.0f;
	ActiveReactionDurationSeconds = DurationSeconds > UE_KINDA_SMALL_NUMBER
		? FMath::Max(DurationSeconds, 0.05f)
		: FMath::Max(DefaultDamageReactionDurationSeconds, 0.05f);
	SetComponentTickEnabled(true);
	StopCharacterForReaction();

	if (LastDamageStrength != ERequiemDamageStrength::Strong)
	{
		return;
	}

	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character)
	{
		return;
	}

	FVector PushDirection = LastImpactDirection.GetSafeNormal2D();
	if (PushDirection.IsNearlyZero())
	{
		PushDirection = -Character->GetActorForwardVector().GetSafeNormal2D();
	}
	if (!PushDirection.IsNearlyZero())
	{
		// Hit_Knockback moves along local backward, so face opposite the desired push.
		Character->SetActorRotation(
			FRotator(0.0f, (-PushDirection).Rotation().Yaw, 0.0f),
			ETeleportType::None);
	}
}

void URequiemHealthComponent::SynchronizeDamageReactionPresentation(
	const float DurationSeconds,
	const float NormalizedTime)
{
	if (!IsDamageReactionActive()
		|| !bReactionPresentationStarted
		|| DurationSeconds <= UE_KINDA_SMALL_NUMBER)
	{
		return;
	}

	ActiveReactionDurationSeconds = FMath::Max(DurationSeconds, 0.05f);
	ReactionElapsedSeconds = FMath::Clamp(NormalizedTime, 0.0f, 1.0f)
		* ActiveReactionDurationSeconds;
	bPresentationClockSynchronizedSinceLastTick = true;
}

void URequiemHealthComponent::FinishDamageReactionPresentation()
{
	if (!IsDamageReactionActive())
	{
		return;
	}

	HealthState = ERequiemHealthState::Alive;
	bReactionPresentationStarted = false;
	bPresentationClockSynchronizedSinceLastTick = false;
	ReactionElapsedSeconds = 0.0f;
	SetComponentTickEnabled(false);
}

bool URequiemHealthComponent::AreActionsLocked() const
{
	if (IsDead())
	{
		return true;
	}
	if (!IsDamageReactionActive())
	{
		return false;
	}

	const ARequiemCharacter* Character = Cast<ARequiemCharacter>(GetOwner());
	const URequiemDodgeComponent* DodgeComponent = Character
		? Character->GetDodgeComponent()
		: nullptr;
	// A non-lethal reaction received outside i-frames waits without changing Roll's
	// validated recovery. The lock becomes active as soon as Roll actually ends.
	return !DodgeComponent || !DodgeComponent->IsDodgeActive();
}

void URequiemHealthComponent::ResetForTesting()
{
	ARequiemCharacter* Character = Cast<ARequiemCharacter>(GetOwner());
	URequiemDodgeComponent* DodgeComponent = Character
		? Character->GetDodgeComponent()
		: nullptr;
	URequiemCombatComponent* CombatComponent = Character
		? Character->GetCombatComponent()
		: nullptr;

	if (DodgeComponent && DodgeComponent->IsDodgeActive())
	{
		DodgeComponent->FinishDodge();
	}

	CurrentHealth = FMath::Max(MaxHealth, 1.0f);
	HealthState = ERequiemHealthState::Alive;
	LastHitRegion = ERequiemHitRegion::Chest;
	LastDamageStrength = ERequiemDamageStrength::Light;
	LastImpactDirection = FVector::ZeroVector;
	LastAppliedDamage = 0.0f;
	SelectedDeathAnimation = ERequiemDeathAnimation::Automatic;
	bReactionPresentationStarted = false;
	bPresentationClockSynchronizedSinceLastTick = false;
	ReactionElapsedSeconds = 0.0f;
	ActiveReactionDurationSeconds =
		FMath::Max(DefaultDamageReactionDurationSeconds, 0.05f);
	SetComponentTickEnabled(false);
	ResetSerial = AdvanceSerial(ResetSerial);

	if (CombatComponent)
	{
		CombatComponent->CancelUnarmedAttackForExternalReaction();
		CombatComponent->ExitCombat();
	}

	if (Character)
	{
		Character->StopJumping();
		Character->UnCrouch();
		Character->ConsumeMovementInputVector();
		if (UCharacterMovementComponent* MovementComponent =
			Character->GetCharacterMovement())
		{
			MovementComponent->SetMovementMode(MOVE_Walking);
			MovementComponent->StopMovementImmediately();
		}
	}
}

int32 URequiemHealthComponent::AdvanceSerial(const int32 Serial)
{
	return Serial == MAX_int32 ? 1 : Serial + 1;
}

void URequiemHealthComponent::EnterDeath(const FRequiemDamageRequest& Request)
{
	HealthState = ERequiemHealthState::Dead;
	bReactionPresentationStarted = false;
	bPresentationClockSynchronizedSinceLastTick = false;
	SetComponentTickEnabled(false);
	DeathSerial = AdvanceSerial(DeathSerial);
	SelectedDeathAnimation = Request.DeathAnimation;
	if (SelectedDeathAnimation == ERequiemDeathAnimation::Automatic)
	{
		SelectedDeathAnimation = DeathSerial % 2 == 1
			? ERequiemDeathAnimation::Death01
			: ERequiemDeathAnimation::Death02;
	}

	ARequiemCharacter* Character = Cast<ARequiemCharacter>(GetOwner());
	if (!Character)
	{
		return;
	}

	if (URequiemDodgeComponent* DodgeComponent = Character->GetDodgeComponent();
		DodgeComponent && DodgeComponent->IsDodgeActive())
	{
		// Lethal damage outside the i-frame window is the only impact that cuts Roll short.
		DodgeComponent->FinishDodge();
	}

	Character->StopJumping();
	Character->UnCrouch();
	Character->ConsumeMovementInputVector();
	if (UCharacterMovementComponent* MovementComponent =
		Character->GetCharacterMovement())
	{
		MovementComponent->StopMovementImmediately();
		MovementComponent->DisableMovement();
	}
}

void URequiemHealthComponent::StopCharacterForReaction()
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character)
	{
		return;
	}

	Character->StopJumping();
	Character->UnCrouch();
	Character->ConsumeMovementInputVector();
	if (UCharacterMovementComponent* MovementComponent =
		Character->GetCharacterMovement())
	{
		MovementComponent->StopMovementImmediately();
	}
}
