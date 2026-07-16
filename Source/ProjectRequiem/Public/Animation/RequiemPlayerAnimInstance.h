// Copyright Project Requiem. All Rights Reserved.

#pragma once

#include "Animation/AnimInstance.h"
#include "Components/RequiemCombatComponent.h"
#include "Components/RequiemHealthComponent.h"
#include "CoreMinimal.h"
#include "RequiemPlayerAnimInstance.generated.h"

class ARequiemCharacter;
class UAnimMontage;
class UAnimSequenceBase;
class UCharacterMovementComponent;
class URequiemCombatComponent;
class URequiemDodgeComponent;
class URequiemHealthComponent;

UENUM(BlueprintType)
enum class ERequiemLocomotionState : uint8
{
	Idle,
	IdleLookAround,
	Jog,
	JumpStart,
	JumpLoop,
	JumpLand,
	Dodge,
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

UENUM(BlueprintType)
enum class ERequiemDamageAnimationState : uint8
{
	Inactive,
	HitReaction,
	Knockback,
	Death
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

	UFUNCTION(BlueprintPure, Category = "Locomotion|Tuning")
	float GetJogAuthoredSpeed() const { return JogAuthoredSpeed; }

	UFUNCTION(BlueprintPure, Category = "Combat")
	float GetCombatAnimationNormalizedTime() const;

	UFUNCTION(BlueprintPure, Category = "Dodge")
	bool IsDodgePresentationActive() const { return bDodgePresentationActive; }

	UFUNCTION(BlueprintPure, Category = "Damage")
	ERequiemDamageAnimationState GetDamageAnimationState() const
	{
		return DamageAnimationState;
	}

	UFUNCTION(BlueprintPure, Category = "Damage")
	float GetDamageAnimationNormalizedTime() const;

	UFUNCTION(BlueprintPure, Category = "Damage")
	bool IsDeathPoseHeld() const { return bDeathPoseHeld; }

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

	/** Roll from UAL1_RM. This is the only current locomotion-adjacent root-motion asset. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dodge")
	TObjectPtr<UAnimSequenceBase> RollAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dodge|Tuning", meta = (ClampMin = "0.1"))
	float DodgePlayRate = 1.35f;

	/** Short visual handoff from Roll's completed displacement into directional Jog. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dodge|Tuning", meta = (ClampMin = "0.0"))
	float DodgeJogHandoffBlendTime = 0.05f;

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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage|Hit Reaction")
	TObjectPtr<UAnimSequenceBase> HitHeadAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage|Hit Reaction")
	TObjectPtr<UAnimSequenceBase> HitChestAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage|Hit Reaction")
	TObjectPtr<UAnimSequenceBase> HitStomachAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage|Hit Reaction")
	TObjectPtr<UAnimSequenceBase> HitShoulderLeftAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage|Hit Reaction")
	TObjectPtr<UAnimSequenceBase> HitShoulderRightAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage|Knockback")
	TObjectPtr<UAnimSequenceBase> HitKnockbackAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage|Death")
	TObjectPtr<UAnimSequenceBase> Death01Animation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage|Death")
	TObjectPtr<UAnimSequenceBase> Death02Animation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage|Tuning", meta = (ClampMin = "0.1"))
	float HitReactionPlayRate = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage|Tuning", meta = (ClampMin = "0.1"))
	float KnockbackPlayRate = 1.0f;

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

	/** Movement returns before the visual follow-through ends; the attack and combo remain committed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Unarmed|Tuning", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float UnarmedMovementUnlockNormalized = 0.60f;

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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Damage|Runtime")
	ERequiemDamageAnimationState DamageAnimationState =
		ERequiemDamageAnimationState::Inactive;

private:
	void CacheCharacterReferences();
	void UpdateObservedMovement();
	void HandleDamageReset();
	void UpdateDamagePresentation();
	void StartHitReactionPresentation();
	void StartDeathPresentation();
	void FinishHitReactionPresentation();
	void PlayDamageAnimation(
		ERequiemDamageAnimationState NewState,
		UAnimSequenceBase* NewAnimation,
		bool bUsesRootMotion);
	void HoldDeathFinalPose();
	bool HasDamageOneShotFinished() const;
	UAnimSequenceBase* GetHitReactionAnimation() const;
	UAnimSequenceBase* GetDeathAnimation() const;
	void StartDodgePresentation();
	void UpdateDodgePresentation();
	void StartDodgeLocomotionRecoveryPresentation();
	void UpdateDodgeLocomotionRecoveryPresentation();
	void FinishDodgePresentation();
	void HandleCombatStateChange();
	void UpdateCombatPresentation();
	void UpdateCombatInputWindow();
	void UpdateCombatMovementRecovery();
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
	void UpdateJogPlayRate();
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
	TObjectPtr<URequiemDodgeComponent> DodgeComponent;

	UPROPERTY(Transient)
	TObjectPtr<URequiemHealthComponent> HealthComponent;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveLocomotionMontage;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequenceBase> ActiveAnimation;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequenceBase> ActiveLocomotionAnimation;

	float StateElapsedSeconds = 0.0f;
	float CombatAnimationElapsedSeconds = 0.0f;
	float DamageAnimationElapsedSeconds = 0.0f;
	float ActiveAnimationPlayRate = 1.0f;
	float LookAroundCountdown = 0.0f;
	bool bNeedsInitialState = true;
	bool bEnterQueued = false;
	bool bExitQueued = false;
	bool bCombatStanceEstablished = false;
	bool bCombatAssetsInvalid = false;
	bool bDodgePresentationActive = false;
	bool bDodgeLocomotionRecoveryPresentationActive = false;
	bool bDeathPoseHeld = false;
	bool bDamageAssetsInvalid = false;
	int32 LastObservedDamageRequestSerial = 0;
	int32 LastObservedDeathSerial = 0;
	int32 LastObservedResetSerial = 0;

	static const FName LocomotionSlotName;
};
