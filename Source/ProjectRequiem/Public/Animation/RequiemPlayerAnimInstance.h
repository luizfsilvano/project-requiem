// Copyright Project Requiem. All Rights Reserved.

#pragma once

#include "Animation/AnimInstance.h"
#include "Components/RequiemCombatComponent.h"
#include "CoreMinimal.h"
#include "RequiemPlayerAnimInstance.generated.h"

class ARequiemCharacter;
class UAnimMontage;
class UAnimSequenceBase;
class UCharacterMovementComponent;
class URequiemCombatComponent;

UENUM(BlueprintType)
enum class ERequiemLocomotionState : uint8
{
	Idle,
	IdleLookAround,
	Jog,
	JumpStart,
	JumpLoop,
	JumpLand,
	CrouchEnter,
	CrouchLoop,
	CrouchExit
};

UENUM(BlueprintType)
enum class ERequiemMovementDirection : uint8
{
	None,
	Forward,
	ForwardRight,
	Right,
	BackwardRight,
	Backward,
	BackwardLeft,
	Left,
	ForwardLeft
};

/** Presentation phase for unarmed combat. Gameplay mode remains owned by the combat component. */
UENUM(BlueprintType)
enum class ERequiemCombatAnimationState : uint8
{
	Inactive,
	Enter,
	Idle,
	Attack,
	Recovery,
	Exit
};

/**
 * Presentation-only locomotion and unarmed-combat state machine for the player character.
 * It observes gameplay state and never owns velocity, acceleration or braking.
 */
UCLASS(Blueprintable, BlueprintType)
class PROJECTREQUIEM_API URequiemPlayerAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	UFUNCTION(BlueprintPure, Category = "Locomotion")
	ERequiemLocomotionState GetLocomotionState() const { return LocomotionState; }

	UFUNCTION(BlueprintPure, Category = "Locomotion")
	FName GetLocomotionStateName() const;

	UFUNCTION(BlueprintPure, Category = "Locomotion")
	ERequiemMovementDirection GetMovementDirection() const { return MovementDirection; }

	UFUNCTION(BlueprintPure, Category = "Locomotion")
	UAnimSequenceBase* GetActiveLocomotionAnimation() const { return ActiveLocomotionAnimation; }

	UFUNCTION(BlueprintPure, Category = "Combat")
	ERequiemCombatState GetObservedCombatState() const { return ObservedCombatState; }

	UFUNCTION(BlueprintPure, Category = "Combat")
	FName GetObservedCombatStateName() const;

	UFUNCTION(BlueprintPure, Category = "Combat")
	ERequiemCombatAnimationState GetCombatAnimationState() const { return CombatAnimationState; }

	UFUNCTION(BlueprintPure, Category = "Combat")
	FName GetCombatAnimationStateName() const;

	/** Current animation on the shared presentation slot, whether locomotion or combat owns it. */
	UFUNCTION(BlueprintPure, Category = "Animation")
	UAnimSequenceBase* GetActivePresentationAnimation() const { return ActiveAnimation; }

	/** Zero-based index in the requested seven-clip sequence, or INDEX_NONE outside the combo. */
	UFUNCTION(BlueprintPure, Category = "Combat")
	int32 GetActiveComboAnimationIndex() const { return ActiveComboAnimationIndex; }

	UFUNCTION(BlueprintPure, Category = "Combat")
	float GetActivePresentationPlayRate() const { return ActiveAnimationPlayRate; }

	UFUNCTION(BlueprintPure, Category = "Combat")
	float GetCombatAnimationNormalizedTime() const;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Locomotion|Idle")
	TObjectPtr<UAnimSequenceBase> IdleAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Locomotion|Idle")
	TObjectPtr<UAnimSequenceBase> IdleLookAroundAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Locomotion|Direction")
	TObjectPtr<UAnimSequenceBase> JogForwardLeftAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Locomotion|Direction")
	TObjectPtr<UAnimSequenceBase> JogForwardAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Locomotion|Direction")
	TObjectPtr<UAnimSequenceBase> JogForwardRightAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Locomotion|Direction")
	TObjectPtr<UAnimSequenceBase> JogBackwardLeftAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Locomotion|Direction")
	TObjectPtr<UAnimSequenceBase> JogBackwardAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Locomotion|Direction")
	TObjectPtr<UAnimSequenceBase> JogBackwardRightAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Locomotion|Direction")
	TObjectPtr<UAnimSequenceBase> JogLeftAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Locomotion|Direction")
	TObjectPtr<UAnimSequenceBase> JogRightAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Locomotion|Jump")
	TObjectPtr<UAnimSequenceBase> JumpStartAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Locomotion|Jump")
	TObjectPtr<UAnimSequenceBase> JumpLoopAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Locomotion|Jump")
	TObjectPtr<UAnimSequenceBase> JumpLandAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Locomotion|Crouch")
	TObjectPtr<UAnimSequenceBase> CrouchEnterAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Locomotion|Crouch")
	TObjectPtr<UAnimSequenceBase> CrouchIdleAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Locomotion|Crouch")
	TObjectPtr<UAnimSequenceBase> CrouchForwardLeftAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Locomotion|Crouch")
	TObjectPtr<UAnimSequenceBase> CrouchForwardAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Locomotion|Crouch")
	TObjectPtr<UAnimSequenceBase> CrouchForwardRightAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Locomotion|Crouch")
	TObjectPtr<UAnimSequenceBase> CrouchBackwardLeftAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Locomotion|Crouch")
	TObjectPtr<UAnimSequenceBase> CrouchBackwardAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Locomotion|Crouch")
	TObjectPtr<UAnimSequenceBase> CrouchBackwardRightAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Locomotion|Crouch")
	TObjectPtr<UAnimSequenceBase> CrouchLeftAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Locomotion|Crouch")
	TObjectPtr<UAnimSequenceBase> CrouchRightAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Locomotion|Crouch")
	TObjectPtr<UAnimSequenceBase> CrouchExitAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Unarmed")
	TObjectPtr<UAnimSequenceBase> CombatEnterAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Unarmed")
	TObjectPtr<UAnimSequenceBase> CombatIdleAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Unarmed")
	TObjectPtr<UAnimSequenceBase> CombatExitAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Unarmed|Combo")
	TObjectPtr<UAnimSequenceBase> PunchCrossAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Unarmed|Combo")
	TObjectPtr<UAnimSequenceBase> PunchJabAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Unarmed|Combo")
	TObjectPtr<UAnimSequenceBase> MeleeKneeAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Unarmed|Combo")
	TObjectPtr<UAnimSequenceBase> MeleeKneeRecoveryAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Unarmed|Combo")
	TObjectPtr<UAnimSequenceBase> MeleeHookAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Unarmed|Combo")
	TObjectPtr<UAnimSequenceBase> MeleeHookRecoveryAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Unarmed|Combo")
	TObjectPtr<UAnimSequenceBase> MeleeUppercutAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Unarmed|Tuning", meta = (ClampMin = "0.1"))
	float UnarmedAttackPlayRate = 1.25f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Unarmed|Tuning", meta = (ClampMin = "0.1"))
	float UnarmedRecoveryPlayRate = 1.35f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Unarmed|Tuning", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float UnarmedInputWindowStartNormalized = 0.30f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Unarmed|Tuning", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float UnarmedInputWindowEndNormalized = 0.85f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Unarmed|Tuning", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float UnarmedQueuedAttackHandoffNormalized = 0.72f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Unarmed|Tuning", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float UnarmedAutomaticRecoveryHandoffNormalized = 0.90f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Unarmed|Tuning", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float UnarmedQueuedRecoveryHandoffNormalized = 0.55f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Locomotion|Tuning", meta = (ClampMin = "0.0"))
	float JogAuthoredSpeed = 500.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Locomotion|Tuning", meta = (ClampMin = "0.0"))
	float DirectionalLoopMinimumSpeed = 20.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Locomotion|Tuning")
	float JumpLoopVerticalVelocity = 100.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Locomotion|Tuning", meta = (ClampMin = "0.0"))
	float LookAroundMinimumDelay = 8.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Locomotion|Tuning", meta = (ClampMin = "0.0"))
	float LookAroundMaximumDelay = 15.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Locomotion|Runtime")
	ERequiemLocomotionState LocomotionState = ERequiemLocomotionState::Idle;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Locomotion|Runtime")
	ERequiemMovementDirection MovementDirection = ERequiemMovementDirection::None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Locomotion|Runtime")
	float ObservedGroundSpeed = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Locomotion|Runtime")
	float ObservedDirectionDegrees = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Locomotion|Runtime")
	bool bObservedIsFalling = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Locomotion|Runtime")
	bool bObservedIsCrouched = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Combat|Runtime")
	ERequiemCombatState ObservedCombatState = ERequiemCombatState::Normal;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Combat|Runtime")
	ERequiemCombatAnimationState CombatAnimationState = ERequiemCombatAnimationState::Inactive;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Combat|Runtime")
	int32 ActiveComboAnimationIndex = INDEX_NONE;

private:
	void CacheCharacterReferences();
	void UpdateObservedMovement();
	void HandleCombatStateChange();
	void UpdateCombatPresentation();
	void UpdateCombatInputWindow();
	void StartCombatEnter();
	void StartCombatIdle();
	void StartCombatExit();
	void StartCombatComboClip(int32 ComboIndex);
	bool TryStartInitialUnarmedAttack();
	bool TryStartQueuedUnarmedFollowUp(int32 ComboIndex);
	void HandleFinishedCombatOneShot();
	void ResumeLocomotionPresentation();
	void PlayCombatAnimation(
		ERequiemCombatAnimationState NewState,
		UAnimSequenceBase* NewAnimation,
		bool bLooping,
		int32 ComboIndex = INDEX_NONE);
	bool ShouldUseCombatIdle() const;
	bool CanPlayCombatAnimation() const;
	bool CanPlayCombatStanceTransition() const;
	bool CanStartUnarmedAttack() const;
	bool ShouldAdvanceCombatOneShot() const;
	bool HasCombatOneShotFinished() const;
	bool CanQueueFollowUpFromComboIndex(int32 ComboIndex) const;
	float GetCombatPlayRate(ERequiemCombatAnimationState State) const;
	UAnimSequenceBase* GetComboAnimation(int32 ComboIndex) const;
	void UpdateLocomotionState(float DeltaSeconds);
	void UpdateGroundedState(float DeltaSeconds);
	void UpdateAirborneState();
	void UpdateCrouchedState();
	void TransitionTo(ERequiemLocomotionState NewState, bool bForceReplay = false);
	void HandleFinishedOneShot();
	void RefreshDirectionalLoop();
	void PlayStateAnimation(float StartTime = 0.0f);
	void ScheduleNextLookAround();
	bool HasMovementIntent() const;
	bool HasOneShotFinished() const;
	bool IsLoopingState(ERequiemLocomotionState State) const;
	float GetStateBlendTime(ERequiemLocomotionState State) const;
	UAnimSequenceBase* GetAnimationForCurrentState() const;
	UAnimSequenceBase* GetDirectionalJogAnimation() const;
	UAnimSequenceBase* GetDirectionalCrouchAnimation() const;
	ERequiemMovementDirection QuantizeDirection(float InDirectionDegrees, bool bHasPlanarVelocity) const;

	UPROPERTY(Transient)
	TObjectPtr<ARequiemCharacter> OwningCharacter;

	UPROPERTY(Transient)
	TObjectPtr<UCharacterMovementComponent> OwningMovementComponent;

	UPROPERTY(Transient)
	TObjectPtr<URequiemCombatComponent> CombatComponent;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveLocomotionMontage;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequenceBase> ActiveAnimation;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequenceBase> ActiveLocomotionAnimation;

	float StateElapsedSeconds = 0.0f;
	float CombatAnimationElapsedSeconds = 0.0f;
	float ActiveAnimationPlayRate = 1.0f;
	float LookAroundCountdown = 0.0f;
	bool bNeedsInitialState = true;
	bool bEnterQueued = false;
	bool bExitQueued = false;
	bool bCombatStanceEstablished = false;
	bool bCombatAssetsInvalid = false;

	static const FName LocomotionSlotName;
};
