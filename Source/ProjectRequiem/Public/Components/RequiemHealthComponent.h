// Copyright Project Requiem. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "RequiemHealthComponent.generated.h"

class UDamageType;

UENUM(BlueprintType)
enum class ERequiemHitRegion : uint8
{
	Head,
	Chest,
	Stomach,
	ShoulderLeft,
	ShoulderRight
};

UENUM(BlueprintType)
enum class ERequiemDamageStrength : uint8
{
	Light,
	Strong
};

UENUM(BlueprintType)
enum class ERequiemDeathAnimation : uint8
{
	Automatic,
	Death01,
	Death02
};

UENUM(BlueprintType)
enum class ERequiemDamageOutcome : uint8
{
	Applied,
	Killed,
	IgnoredInvalid,
	IgnoredInvulnerable,
	IgnoredDead
};

UENUM(BlueprintType)
enum class ERequiemHealthState : uint8
{
	Alive,
	Reacting,
	Dead
};

/**
 * Stable damage request shared by the player, future hitboxes and future enemies.
 * Physical region detection can populate HitRegion later without changing health rules.
 */
USTRUCT(BlueprintType)
struct PROJECTREQUIEM_API FRequiemDamageRequest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage", meta = (ClampMin = "0.0"))
	float DamageAmount = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	ERequiemHitRegion HitRegion = ERequiemHitRegion::Chest;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	ERequiemDamageStrength Strength = ERequiemDamageStrength::Light;

	/** World-space direction in which the target should be pushed by a strong impact. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	FVector ImpactDirection = FVector::ZeroVector;

	/** Native UE damage-type extension point; no attribute framework is required. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	TSubclassOf<UDamageType> DamageTypeClass;

	/** Deterministic override used by tests; Automatic alternates between both death clips. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	ERequiemDeathAnimation DeathAnimation = ERequiemDeathAnimation::Automatic;
};

/**
 * Minimal player health and damage contract.
 *
 * The component owns health, i-frame rejection, action locking and idempotent death.
 * Animation observes serials and reports when a non-lethal reaction presentation finishes.
 */
UCLASS(ClassGroup = (Combat), BlueprintType, meta = (BlueprintSpawnableComponent))
class PROJECTREQUIEM_API URequiemHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URequiemHealthComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "Damage")
	ERequiemDamageOutcome ApplyDamage(const FRequiemDamageRequest& Request);

	/** Called by presentation when the queued hit reaction can take control. */
	void BeginDamageReactionPresentation(float DurationSeconds);

	/** Keeps the gameplay fallback clock aligned with the authored montage. */
	void SynchronizeDamageReactionPresentation(
		float DurationSeconds,
		float NormalizedTime);

	/** Releases the non-lethal reaction lock; combat mode remains active. */
	void FinishDamageReactionPresentation();

	/** Temporary development-only recovery path for repeatable PIE validation. */
	UFUNCTION(BlueprintCallable, Category = "Damage|Testing", meta = (DevelopmentOnly))
	void ResetForTesting();

	UFUNCTION(BlueprintPure, Category = "Health")
	float GetMaxHealth() const { return MaxHealth; }

	UFUNCTION(BlueprintPure, Category = "Health")
	float GetCurrentHealth() const { return CurrentHealth; }

	UFUNCTION(BlueprintPure, Category = "Health")
	ERequiemHealthState GetHealthState() const { return HealthState; }

	UFUNCTION(BlueprintPure, Category = "Health")
	bool IsDead() const { return HealthState == ERequiemHealthState::Dead; }

	UFUNCTION(BlueprintPure, Category = "Health")
	bool IsDamageReactionActive() const
	{
		return HealthState == ERequiemHealthState::Reacting;
	}

	UFUNCTION(BlueprintPure, Category = "Health")
	bool AreActionsLocked() const;

	UFUNCTION(BlueprintPure, Category = "Damage")
	ERequiemHitRegion GetLastHitRegion() const { return LastHitRegion; }

	UFUNCTION(BlueprintPure, Category = "Damage")
	ERequiemDamageStrength GetLastDamageStrength() const { return LastDamageStrength; }

	UFUNCTION(BlueprintPure, Category = "Damage")
	FVector GetLastImpactDirection() const { return LastImpactDirection; }

	UFUNCTION(BlueprintPure, Category = "Damage")
	float GetLastAppliedDamage() const { return LastAppliedDamage; }

	UFUNCTION(BlueprintPure, Category = "Damage")
	ERequiemDeathAnimation GetSelectedDeathAnimation() const
	{
		return SelectedDeathAnimation;
	}

	UFUNCTION(BlueprintPure, Category = "Damage")
	int32 GetDamageRequestSerial() const { return DamageRequestSerial; }

	UFUNCTION(BlueprintPure, Category = "Damage")
	int32 GetDeathSerial() const { return DeathSerial; }

	UFUNCTION(BlueprintPure, Category = "Damage|Testing")
	int32 GetResetSerial() const { return ResetSerial; }

	UFUNCTION(BlueprintPure, Category = "Damage|Tuning")
	float GetStrongDamageThreshold() const { return StrongDamageThreshold; }

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Health", meta = (ClampMin = "1.0"))
	float MaxHealth = 100.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage|Tuning", meta = (ClampMin = "0.0"))
	float StrongDamageThreshold = 35.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage|Tuning", meta = (ClampMin = "0.05"))
	float DefaultDamageReactionDurationSeconds = 1.25f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Health")
	float CurrentHealth = 100.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Health")
	ERequiemHealthState HealthState = ERequiemHealthState::Alive;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Damage")
	ERequiemHitRegion LastHitRegion = ERequiemHitRegion::Chest;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Damage")
	ERequiemDamageStrength LastDamageStrength = ERequiemDamageStrength::Light;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Damage")
	FVector LastImpactDirection = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Damage")
	float LastAppliedDamage = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Damage")
	ERequiemDeathAnimation SelectedDeathAnimation = ERequiemDeathAnimation::Automatic;

private:
	static int32 AdvanceSerial(int32 Serial);
	void EnterDeath(const FRequiemDamageRequest& Request);
	void StopCharacterForReaction();

	int32 DamageRequestSerial = 0;
	int32 DeathSerial = 0;
	int32 ResetSerial = 0;
	bool bReactionPresentationStarted = false;
	bool bPresentationClockSynchronizedSinceLastTick = false;
	float ReactionElapsedSeconds = 0.0f;
	float ActiveReactionDurationSeconds = 1.25f;
};
