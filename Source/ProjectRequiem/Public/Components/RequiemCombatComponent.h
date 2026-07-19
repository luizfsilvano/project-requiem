// Copyright Project Requiem. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "RequiemCombatComponent.generated.h"

UENUM(BlueprintType)
enum class ERequiemCombatState : uint8
{
	Normal,
	CombatUnarmed,
	CombatSword
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

UENUM(BlueprintType)
enum class ERequiemSwordAttackType : uint8
{
	None,
	Light,
	Heavy
};

UENUM(BlueprintType)
enum class ERequiemSwordAttackRequestResult : uint8
{
	InitialLightAccepted,
	InitialHeavyAccepted,
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

	/** Equips or unequips the sword without leaving combat. A committed sword attack cannot be interrupted. */
	UFUNCTION(BlueprintCallable, Category = "Combat|Sword")
	bool ToggleSwordCombat();

	UFUNCTION(BlueprintCallable, Category = "Combat")
	ERequiemUnarmedAttackRequestResult RequestUnarmedAttack();

	/** Shared entry point for manual input, lock-on and accepted damage. */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void EnterUnarmedCombat(ERequiemCombatEntryReason EntryReason);

	UFUNCTION(BlueprintCallable, Category = "Combat|Sword")
	void EnterSwordCombat(ERequiemCombatEntryReason EntryReason);

	/** Keeps the equipped sword style when damage or lock-on requests combat entry. */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void EnterCurrentCombat(ERequiemCombatEntryReason EntryReason);

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

	/** Starts a sword charge. No attack is requested until ReleaseSwordCharge. */
	UFUNCTION(BlueprintCallable, Category = "Combat|Sword")
	bool BeginSwordCharge();

	/** Resolves the held duration into a light or heavy initial request. */
	UFUNCTION(BlueprintCallable, Category = "Combat|Sword")
	ERequiemSwordAttackRequestResult ReleaseSwordCharge();

	/** Clears a held input without creating an attack request. */
	UFUNCTION(BlueprintCallable, Category = "Combat|Sword")
	void CancelSwordCharge();

	/** Consumes the one pending sword request used to start a light or heavy attack. */
	ERequiemSwordAttackType ConsumeInitialSwordAttackRequest();

	/** Starts one authored sword strike. Heavy movement is left entirely to presentation root motion. */
	void BeginSwordAttackStep(
		ERequiemSwordAttackType AttackType,
		bool bApplyForwardLunge,
		int32 ComboAnimationIndex);

	/** Presentation publishes the authored strike phase; gameplay resolves at most one hit. */
	void UpdateSwordAttackHit(float NormalizedTime);

	/** Publishes the single light-combo follow-up window. Heavy attacks never open it. */
	void SetSwordAttackInputWindowOpen(bool bOpen);

	/** Consumes the only buffered continuation for the current light-combo step. */
	bool ConsumeQueuedSwordLightFollowUp();

	/** Returns light-attack movement to CharacterMovement without ending the sequence. */
	void ReleaseSwordAttackMovementLock();

	/**
	 * Completes the authored recovery handoff without zeroing the planar velocity that
	 * CharacterMovement must carry into locomotion. Heavy attacks may use this only
	 * after their non-cancelable recovery blend has completed.
	 */
	void CompleteSwordAttackRecovery();

	/** Clears all runtime state for the current sword attack sequence. */
	void EndSwordAttackSequence();

	/** Cancels held input, pending requests and either active attack style. */
	void CancelActiveAttackForExternalReaction();

	UFUNCTION(BlueprintPure, Category = "Combat")
	bool IsAnyAttackActive() const
	{
		return bUnarmedAttackActive || bSwordAttackActive;
	}

	UFUNCTION(BlueprintPure, Category = "Combat")
	bool IsAnyAttackMovementLocked() const
	{
		return bUnarmedAttackMovementLocked || bSwordAttackMovementLocked;
	}

	UFUNCTION(BlueprintPure, Category = "Combat")
	bool HasPendingInitialAttackRequest() const
	{
		return bInitialUnarmedAttackRequested
			|| PendingSwordAttackType != ERequiemSwordAttackType::None;
	}

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

	UFUNCTION(BlueprintPure, Category = "Combat|Sword")
	bool IsSwordEquipped() const
	{
		return CombatState == ERequiemCombatState::CombatSword;
	}

	UFUNCTION(BlueprintPure, Category = "Combat|Sword")
	bool IsSwordChargeActive() const { return bSwordChargeActive; }

	UFUNCTION(BlueprintPure, Category = "Combat|Sword")
	float GetSwordChargeElapsedSeconds() const;

	UFUNCTION(BlueprintPure, Category = "Combat|Sword|Tuning")
	float GetSwordChargeThresholdSeconds() const { return SwordChargeThresholdSeconds; }

	UFUNCTION(BlueprintPure, Category = "Combat|Sword")
	bool IsSwordAttackActive() const { return bSwordAttackActive; }

	UFUNCTION(BlueprintPure, Category = "Combat|Sword")
	bool IsSwordHeavyAttackCommitted() const
	{
		return (bSwordAttackActive
				&& ActiveSwordAttackType == ERequiemSwordAttackType::Heavy)
			|| PendingSwordAttackType == ERequiemSwordAttackType::Heavy;
	}

	UFUNCTION(BlueprintPure, Category = "Combat|Sword")
	ERequiemSwordAttackType GetActiveSwordAttackType() const
	{
		return ActiveSwordAttackType;
	}

	UFUNCTION(BlueprintPure, Category = "Combat|Sword")
	ERequiemSwordAttackType GetPendingSwordAttackType() const
	{
		return PendingSwordAttackType;
	}

	UFUNCTION(BlueprintPure, Category = "Combat|Sword")
	int32 GetActiveSwordAttackIndex() const { return ActiveSwordAttackIndex; }

	UFUNCTION(BlueprintPure, Category = "Combat|Sword")
	bool IsSwordAttackInputWindowOpen() const
	{
		return bSwordAttackActive
			&& ActiveSwordAttackType == ERequiemSwordAttackType::Light
			&& bSwordAttackInputWindowOpen
			&& !bQueuedSwordLightFollowUp;
	}

	UFUNCTION(BlueprintPure, Category = "Combat|Sword")
	bool HasQueuedSwordLightFollowUp() const { return bQueuedSwordLightFollowUp; }

	UFUNCTION(BlueprintPure, Category = "Combat|Sword")
	bool IsSwordAttackMovementLocked() const { return bSwordAttackMovementLocked; }

	UFUNCTION(BlueprintPure, Category = "Combat|Sword")
	int32 GetSwordToggleSerial() const { return SwordToggleSerial; }

	UFUNCTION(BlueprintPure, Category = "Combat|Sword")
	int32 GetSwordChargeRequestSerial() const { return SwordChargeRequestSerial; }

	UFUNCTION(BlueprintPure, Category = "Combat|Sword")
	int32 GetSwordLightRequestSerial() const { return SwordLightRequestSerial; }

	UFUNCTION(BlueprintPure, Category = "Combat|Sword")
	int32 GetSwordHeavyRequestSerial() const { return SwordHeavyRequestSerial; }

	UFUNCTION(BlueprintPure, Category = "Combat|Sword")
	int32 GetSwordHitAttemptSerial() const { return SwordHitAttemptSerial; }

	UFUNCTION(BlueprintPure, Category = "Combat|Sword")
	int32 GetSwordHitConfirmSerial() const { return SwordHitConfirmSerial; }

	UFUNCTION(BlueprintPure, Category = "Combat|Sword")
	float GetSwordLightHitDamage() const { return SwordLightHitDamage; }

	UFUNCTION(BlueprintPure, Category = "Combat|Sword")
	float GetSwordHeavyHitDamage() const { return SwordHeavyHitDamage; }

	UFUNCTION(BlueprintPure, Category = "Combat|Sword")
	AActor* GetLastSwordHitActor() const { return LastSwordHitActor.Get(); }

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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Sword|Tuning", meta = (ClampMin = "0.05"))
	float SwordChargeThresholdSeconds = 0.65f;

	/** Target planar speed for light strikes. Heavy displacement comes only from root motion. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Sword|Tuning", meta = (ClampMin = "0.0"))
	float SwordLightAttackLungeSpeed = 350.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Sword|Hit", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SwordLightHitMomentNormalized = 0.4f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Sword|Hit", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SwordHeavyHitMomentNormalized = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Sword|Hit", meta = (ClampMin = "0.0"))
	float SwordLightHitDamage = 35.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Sword|Hit", meta = (ClampMin = "0.0"))
	float SwordHeavyHitDamage = 60.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Sword|Hit", meta = (ClampMin = "0.0"))
	float SwordHitRange = 180.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Sword|Hit", meta = (ClampMin = "0.0"))
	float SwordHitRadius = 55.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Sword|Hit")
	float SwordHitHeight = 70.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Combat|Sword|Runtime")
	bool bSwordChargeActive = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Combat|Sword|Runtime")
	bool bSwordAttackActive = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Combat|Sword|Runtime")
	bool bSwordAttackInputWindowOpen = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Combat|Sword|Runtime")
	bool bQueuedSwordLightFollowUp = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Combat|Sword|Runtime")
	bool bSwordAttackMovementLocked = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Combat|Sword|Runtime")
	ERequiemSwordAttackType ActiveSwordAttackType = ERequiemSwordAttackType::None;

private:
	void MarkCombatActivity();
	void ResolveUnarmedAttackHit();
	void ResolveSwordAttackHit();
	ERequiemSwordAttackRequestResult RequestSwordAttack(ERequiemSwordAttackType AttackType);
	void CancelSwordAttack();
	static int32 AdvanceSerial(int32 Serial);

	int32 AttackRequestSerial = 0;
	int32 ActiveUnarmedAttackIndex = INDEX_NONE;
	int32 UnarmedHitAttemptSerial = 0;
	int32 UnarmedHitConfirmSerial = 0;
	float LastCombatActivityTimeSeconds = -1.0f;
	bool bInitialUnarmedAttackRequested = false;
	bool bUnarmedHitAttempted = false;
	TWeakObjectPtr<AActor> LastUnarmedHitActor;

	ERequiemSwordAttackType PendingSwordAttackType = ERequiemSwordAttackType::None;
	int32 ActiveSwordAttackIndex = INDEX_NONE;
	int32 SwordToggleSerial = 0;
	int32 SwordChargeRequestSerial = 0;
	int32 SwordLightRequestSerial = 0;
	int32 SwordHeavyRequestSerial = 0;
	int32 SwordHitAttemptSerial = 0;
	int32 SwordHitConfirmSerial = 0;
	float SwordChargeStartTimeSeconds = -1.0f;
	bool bSwordHitAttempted = false;
	TWeakObjectPtr<AActor> LastSwordHitActor;
};
