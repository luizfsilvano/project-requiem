// Copyright Project Requiem. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "RequiemDodgeComponent.generated.h"

/**
 * Gameplay contract for the player's committed dodge.
 *
 * The component captures one world-space direction, owns the normalized action
 * clock and exposes the invulnerability window. Animation supplies the authored
 * duration and root-motion presentation, but it never owns the dodge rules.
 */
UCLASS(ClassGroup = (Movement), BlueprintType, meta = (BlueprintSpawnableComponent))
class PROJECTREQUIEM_API URequiemDodgeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URequiemDodgeComponent();

	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Attempts one grounded dodge and captures Direction exactly once on success. */
	UFUNCTION(BlueprintCallable, Category = "Dodge")
	bool RequestDodge(const FVector& Direction);

	/** Aligns gameplay timing and i-frames with the authored Roll presentation. */
	void SynchronizeDodgePresentation(float DurationSeconds, float NormalizedTime);

	/** Emergency-safe completion used if the presentation asset cannot be played. */
	void FinishDodge();

	UFUNCTION(BlueprintPure, Category = "Dodge")
	bool IsDodgeActive() const { return bDodgeActive; }

	/** Move input returns at recovery; orientation remains committed until the clip ends. */
	UFUNCTION(BlueprintPure, Category = "Dodge")
	bool IsDodgeMovementLocked() const;

	/** Attack, jump, crouch and another dodge remain locked for the entire action. */
	UFUNCTION(BlueprintPure, Category = "Dodge")
	bool AreDodgeRestrictedActionsLocked() const { return bDodgeActive; }

	UFUNCTION(BlueprintPure, Category = "Dodge")
	bool IsDodgeInvulnerable() const;

	/** Future hit/damage entry points must consult this before committing an impact. */
	UFUNCTION(BlueprintPure, Category = "Dodge")
	bool ShouldIgnoreIncomingDamage() const { return IsDodgeInvulnerable(); }

	UFUNCTION(BlueprintPure, Category = "Dodge")
	FVector GetDodgeDirection() const { return DodgeDirection; }

	UFUNCTION(BlueprintPure, Category = "Dodge")
	float GetDodgeNormalizedTime() const;

	/** Monotonic serial for accepted dodges only; rejected repeated inputs do not advance it. */
	UFUNCTION(BlueprintPure, Category = "Dodge")
	int32 GetDodgeRequestSerial() const { return DodgeRequestSerial; }

	UFUNCTION(BlueprintPure, Category = "Dodge|Tuning")
	float GetIFrameStartNormalized() const { return IFrameStartNormalized; }

	UFUNCTION(BlueprintPure, Category = "Dodge|Tuning")
	float GetIFrameEndNormalized() const { return IFrameEndNormalized; }

	UFUNCTION(BlueprintPure, Category = "Dodge|Tuning")
	float GetMovementControlRecoveryNormalized() const
	{
		return MovementControlRecoveryNormalized;
	}

protected:
	/** Safe fallback if presentation cannot publish the authored duration immediately. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dodge|Tuning", meta = (ClampMin = "0.05"))
	float DefaultDodgeDurationSeconds = 1.1f;

	/** Central invulnerability window. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dodge|Tuning", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float IFrameStartNormalized = 0.25f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dodge|Tuning", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float IFrameEndNormalized = 0.65f;

	/** Movement resumes just after Roll's authored root displacement; actions stay locked to 100%. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dodge|Tuning", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MovementControlRecoveryNormalized = 0.62f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Dodge|Runtime")
	bool bDodgeActive = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Dodge|Runtime")
	FVector DodgeDirection = FVector::ForwardVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Dodge|Runtime")
	float DodgeElapsedSeconds = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Dodge|Runtime")
	float ActiveDodgeDurationSeconds = 1.1f;

private:
	void ApplyMovementRecoveryIfNeeded();
	void RestoreCharacterRotationPolicy();

	int32 DodgeRequestSerial = 0;
	bool bSavedUseControllerDesiredRotation = true;
	bool bSavedOrientRotationToMovement = false;
	bool bSavedUseControllerRotationYaw = false;
	bool bMovementRecoveryApplied = false;
};
