// Copyright Project Requiem. All Rights Reserved.

#pragma once

#include "Animation/AnimInstance.h"
#include "CoreMinimal.h"
#include "RequiemPlayerAnimInstance.generated.h"

class ARequiemCharacter;
class UAnimMontage;
class UAnimSequenceBase;
class UCharacterMovementComponent;

UENUM(BlueprintType)
enum class ERequiemLocomotionState : uint8
{
	Idle,
	IdleLookAround,
	SprintEnter,
	SprintLoop,
	SprintExit,
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

/**
 * Presentation-only locomotion state machine for the player character.
 * It observes CharacterMovement and never owns gameplay velocity, acceleration or braking.
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
	UAnimSequenceBase* GetActiveLocomotionAnimation() const { return ActiveAnimation; }

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Locomotion|Idle")
	TObjectPtr<UAnimSequenceBase> IdleAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Locomotion|Idle")
	TObjectPtr<UAnimSequenceBase> IdleLookAroundAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Locomotion|Sprint")
	TObjectPtr<UAnimSequenceBase> SprintEnterAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Locomotion|Sprint")
	TObjectPtr<UAnimSequenceBase> SprintLoopAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Locomotion|Sprint")
	TObjectPtr<UAnimSequenceBase> SprintExitAnimation;

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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Locomotion|Tuning", meta = (ClampMin = "0.0"))
	float SprintAuthoredSpeed = 825.0f;

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

private:
	void CacheCharacterReferences();
	void UpdateObservedMovement();
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
	TObjectPtr<UAnimMontage> ActiveLocomotionMontage;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequenceBase> ActiveAnimation;

	FVector PreviousMovementIntent = FVector::ZeroVector;
	float StateElapsedSeconds = 0.0f;
	float ActiveAnimationPlayRate = 1.0f;
	float LookAroundCountdown = 0.0f;
	bool bReachedFullSprintSpeed = false;
	bool bSprintExitForReversal = false;
	bool bNeedsInitialState = true;

	static const FName LocomotionSlotName;
};
