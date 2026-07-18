// Copyright Project Requiem. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "RequiemCombatComponent.generated.h"

UENUM(BlueprintType)
enum class ERequiemCombatState : uint8
{
	Normal,
	CombatUnarmed
};

/** Source that requested combat mode. Damage and lock-on reuse the same entry contract. */
UENUM(BlueprintType)
enum class ERequiemCombatEntryReason : uint8
{
	Manual,
	Attack,
	LockOn,
	ReceivedDamage
};

UENUM(BlueprintType)
enum class ERequiemUnarmedAttackRequestResult : uint8
{
	InitialAccepted,
	FollowUpBuffered,
	Rejected
};

/**
 * Minimal gameplay contract for the player's current combat mode.
 * Animation observes this component; it does not own movement or combat state.
 */
UCLASS(ClassGroup = (Combat), BlueprintType, meta = (BlueprintSpawnableComponent))
class PROJECTREQUIEM_API URequiemCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URequiemCombatComponent();

	UFUNCTION(BlueprintPure, Category = "Combat")
	ERequiemCombatState GetCombatState() const { return CombatState; }

	UFUNCTION(BlueprintPure, Category = "Combat")
	FName GetCombatStateName() const;

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void ToggleUnarmedCombat();

	UFUNCTION(BlueprintCallable, Category = "Combat")
	ERequiemUnarmedAttackRequestResult RequestUnarmedAttack();

	/** Shared entry point for manual input, lock-on and accepted damage. */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void EnterUnarmedCombat(ERequiemCombatEntryReason EntryReason);

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void ExitCombat();

	/** Monotonic diagnostic revision for every primary-attack input attempt. */
	UFUNCTION(BlueprintPure, Category = "Combat")
	int32 GetAttackRequestSerial() const { return AttackRequestSerial; }

	/** Consumes the single initial attack request used to start a combo. */
	bool ConsumeInitialUnarmedAttackRequest();

	/** Starts one committed attack step and optionally applies its short forward lunge. */
	void BeginUnarmedAttackStep(bool bApplyForwardLunge, int32 ComboAnimationIndex);

	/** Presentation publishes the authored strike phase; gameplay resolves at most one hit. */
	void UpdateUnarmedAttackHit(float NormalizedTime);

	/** Presentation publishes its normalized input window through this gameplay contract. */
	void SetUnarmedAttackInputWindowOpen(bool bOpen);

	/** Consumes at most one accepted follow-up for the current combo step. */
	bool ConsumeQueuedUnarmedFollowUp();

	/** Returns movement to CharacterMovement without ending the active combo step. */
	void ReleaseUnarmedAttackMovementLock();

	/** Clears the current attack-step runtime state. */
	void EndUnarmedAttackSequence();

	/** Explicit damage/death interruption that also clears an unconsumed initial request. */
	void CancelUnarmedAttackForExternalReaction();

	UFUNCTION(BlueprintPure, Category = "Combat|Unarmed")
	bool IsUnarmedAttackActive() const { return bUnarmedAttackActive; }

	UFUNCTION(BlueprintPure, Category = "Combat|Unarmed")
	bool IsUnarmedAttackInputWindowOpen() const
	{
		return bUnarmedAttackActive
			&& bUnarmedAttackInputWindowOpen
			&& !bQueuedUnarmedFollowUp;
	}

	UFUNCTION(BlueprintPure, Category = "Combat|Unarmed")
	bool HasQueuedUnarmedFollowUp() const { return bQueuedUnarmedFollowUp; }

	UFUNCTION(BlueprintPure, Category = "Combat|Unarmed")
	bool IsUnarmedAttackMovementLocked() const { return bUnarmedAttackMovementLocked; }

	UFUNCTION(BlueprintPure, Category = "Combat|Unarmed")
	int32 GetUnarmedHitAttemptSerial() const { return UnarmedHitAttemptSerial; }

	UFUNCTION(BlueprintPure, Category = "Combat|Unarmed")
	int32 GetUnarmedHitConfirmSerial() const { return UnarmedHitConfirmSerial; }

	UFUNCTION(BlueprintPure, Category = "Combat|Unarmed")
	float GetUnarmedHitDamage() const { return UnarmedHitDamage; }

	UFUNCTION(BlueprintPure, Category = "Combat|Unarmed")
	AActor* GetLastUnarmedHitActor() const { return LastUnarmedHitActor.Get(); }

	UFUNCTION(BlueprintPure, Category = "Combat|Unarmed")
	bool HasPendingInitialUnarmedAttackRequest() const
	{
		return bInitialUnarmedAttackRequested;
	}

	/** Unarmed combat has no blocking capability in this stage. */
	UFUNCTION(BlueprintPure, Category = "Combat")
	bool CanBlock() const { return false; }

	/**
	 * Future auto-exit eligibility only. Nothing polls or exits automatically yet,
	 * and enemy proximity remains an input supplied by a future system.
	 */
	UFUNCTION(BlueprintPure, Category = "Combat")
	bool CanAutoExit(bool bEnemyNearby) const;

	UFUNCTION(BlueprintPure, Category = "Combat")
	float GetAutoExitDelay() const { return AutoExitDelay; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Combat")
	ERequiemCombatState CombatState = ERequiemCombatState::Normal;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Tuning", meta = (ClampMin = "0.0"))
	float AutoExitDelay = 30.0f;

	/** Target planar speed for the collision-aware CharacterMovement attack commitment. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Unarmed|Tuning", meta = (ClampMin = "0.0"))
	float UnarmedAttackLungeSpeed = 350.0f;

	/** Authored normalized phase at which the active attack performs its single query. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Unarmed|Hit", meta = (ClampMin = "0.0", ClampMax = "0.70"))
	float UnarmedHitMomentNormalized = 0.4f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Unarmed|Hit", meta = (ClampMin = "0.0"))
	float UnarmedHitDamage = 25.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Unarmed|Hit", meta = (ClampMin = "0.0"))
	float UnarmedHitRange = 135.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Unarmed|Hit", meta = (ClampMin = "0.0"))
	float UnarmedHitRadius = 55.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Unarmed|Hit")
	float UnarmedHitHeight = 70.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Combat|Unarmed|Runtime")
	bool bUnarmedAttackActive = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Combat|Unarmed|Runtime")
	bool bUnarmedAttackInputWindowOpen = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Combat|Unarmed|Runtime")
	bool bQueuedUnarmedFollowUp = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Combat|Unarmed|Runtime")
	bool bUnarmedAttackMovementLocked = false;

private:
	void MarkCombatActivity();
	void ResolveUnarmedAttackHit();

	int32 AttackRequestSerial = 0;
	int32 ActiveUnarmedAttackIndex = INDEX_NONE;
	int32 UnarmedHitAttemptSerial = 0;
	int32 UnarmedHitConfirmSerial = 0;
	float LastCombatActivityTimeSeconds = -1.0f;
	bool bInitialUnarmedAttackRequested = false;
	bool bUnarmedHitAttempted = false;
	TWeakObjectPtr<AActor> LastUnarmedHitActor;
};
