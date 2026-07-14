// Copyright Project Requiem. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Characters/RequiemCharacter.h"
#include "RequiemVisualValidationCharacter.generated.h"

class UAnimSequenceBase;
class UAnimSequencerInstance;

/**
 * Temporary presentation-only pawn for L_Dev_Foundation.
 * Reparent BP_CH_Player to ARequiemCharacter and remove this class when the production animation layer starts.
 */
UCLASS(Blueprintable)
class PROJECTREQUIEM_API ARequiemVisualValidationCharacter : public ARequiemCharacter
{
	GENERATED_BODY()

public:
	ARequiemVisualValidationCharacter();
	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual Validation|Animation")
	TObjectPtr<UAnimSequenceBase> IdleAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual Validation|Animation")
	TObjectPtr<UAnimSequenceBase> JumpAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual Validation|Animation")
	TObjectPtr<UAnimSequenceBase> RunEnterAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual Validation|Animation")
	TObjectPtr<UAnimSequenceBase> RunLoopAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual Validation|Animation")
	TObjectPtr<UAnimSequenceBase> RunExitAnimation;

	/** Initial pace authored into the first half of UAL1 Sprint_Enter. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual Validation|Movement", meta = (ClampMin = "0.0"))
	float RunStartSpeed = 97.5f;

	/** Source-motion speed of UAL1 Sprint_Loop (5.5 m over 0.667 s). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual Validation|Movement", meta = (ClampMin = "0.0"))
	float RunSpeed = 825.0f;

private:
	enum class ELocomotionState : uint8
	{
		Idle,
		RunEnter,
		RunLoop,
		RunExit,
		Falling
	};

	void UpdateLocomotionState();
	void HandleCompletedAnimation();
	void SetLocomotionState(ELocomotionState NewState, float InBlendDuration);
	void AdvanceAnimationPlayback(float DeltaSeconds);
	void UpdateMovementSpeed();
	void UpdateAnimationBlend();
	bool HasMovementIntent() const;
	bool IsLoopingState(ELocomotionState State) const;
	float GetNormalizedStateTime() const;
	UAnimSequenceBase* GetAnimationForState(ELocomotionState State) const;

	ELocomotionState LocomotionState = ELocomotionState::Idle;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequencerInstance> VisualAnimationInstance;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequenceBase> ActiveAnimation;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequenceBase> PreviousAnimation;

	float ActiveAnimationPosition = 0.0f;
	float PreviousAnimationPosition = 0.0f;
	float BlendElapsed = 0.0f;
	float BlendDuration = 0.0f;
	float RunEnterStartSpeed = 0.0f;
	float RunExitStartSpeed = 0.0f;
	bool bPreviousAnimationLooping = false;
	int32 ActiveTrackId = 0;
	int32 PreviousTrackId = 1;
};
