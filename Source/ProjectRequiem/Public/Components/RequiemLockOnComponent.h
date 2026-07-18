// Copyright Project Requiem. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "RequiemLockOnComponent.generated.h"

class ARequiemCharacter;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FRequiemLockOnTargetChangedSignature,
	AActor*,
	PreviousTarget,
	AActor*,
	NewTarget);

/**
 * Basic single-target lock-on owned by the player.
 *
 * Acquisition is explicit and target eligibility comes from
 * URequiemLockOnTargetInterface. While locked, this component owns only camera
 * tracking and target-facing control rotation; CharacterMovement continues to
 * own physical movement and the dodge component keeps its committed rotation.
 */
UCLASS(ClassGroup = (Combat), BlueprintType, meta = (BlueprintSpawnableComponent))
class PROJECTREQUIEM_API URequiemLockOnComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URequiemLockOnComponent();

	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Clears an existing lock even while the owner is action-locked; otherwise attempts acquisition. */
	UFUNCTION(BlueprintCallable, Category = "Combat|Lock On")
	bool ToggleLockOn();

	/** Finds the nearest valid target in the forward cone. */
	UFUNCTION(BlueprintCallable, Category = "Combat|Lock On")
	bool TryAcquireLockOn();

	/** Ends target tracking without changing the player's current combat state. */
	UFUNCTION(BlueprintCallable, Category = "Combat|Lock On")
	void ClearLockOn();

	UFUNCTION(BlueprintPure, Category = "Combat|Lock On")
	bool IsLockOnActive() const { return LockOnTarget.IsValid(); }

	UFUNCTION(BlueprintPure, Category = "Combat|Lock On")
	AActor* GetLockOnTarget() const { return LockOnTarget.Get(); }

	UFUNCTION(BlueprintPure, Category = "Combat|Lock On")
	FVector GetLockOnFocusLocation() const;

	UFUNCTION(BlueprintPure, Category = "Combat|Lock On|Tuning")
	float GetAcquisitionRange() const { return AcquisitionRange; }

	UFUNCTION(BlueprintPure, Category = "Combat|Lock On|Tuning")
	float GetBreakRange() const { return BreakRange; }

	UFUNCTION(BlueprintPure, Category = "Combat|Lock On|Tuning")
	float GetAcquisitionConeHalfAngleDegrees() const
	{
		return AcquisitionConeHalfAngleDegrees;
	}

	UPROPERTY(BlueprintAssignable, Category = "Combat|Lock On")
	FRequiemLockOnTargetChangedSignature OnLockOnTargetChanged;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Lock On|Tuning", meta = (ClampMin = "0.0"))
	float AcquisitionRange = 1200.0f;

	/** Slightly wider than acquisition range to avoid boundary flicker. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Lock On|Tuning", meta = (ClampMin = "0.0"))
	float BreakRange = 1500.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Lock On|Tuning", meta = (ClampMin = "0.0", ClampMax = "89.0"))
	float AcquisitionConeHalfAngleDegrees = 55.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Lock On|Camera", meta = (ClampMin = "0.0"))
	float CameraTrackingInterpSpeed = 12.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Lock On|Indicator", meta = (ClampMin = "0.0"))
	float IndicatorRadius = 90.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Lock On|Indicator", meta = (ClampMin = "0.1"))
	float IndicatorProjectionDepth = 8.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Lock On|Indicator", meta = (ClampMin = "0.0"))
	float IndicatorGroundOffset = 3.0f;

private:
	ARequiemCharacter* GetOwnerCharacter() const;
	AActor* FindBestTarget() const;
	bool IsTargetValid(const AActor* Candidate) const;
	bool HasClearAcquisitionPath(const FVector& FocusLocation, const AActor* Candidate) const;
	void BeginLockOn(AActor* NewTarget);
	void UpdateCameraTracking(float DeltaTime);
	void UpdateGroundIndicator();
	void HideGroundIndicator();

	TWeakObjectPtr<AActor> LockOnTarget;
	float LockedCameraPitch = 0.0f;
	bool bHasLockedCameraPitch = false;
};
