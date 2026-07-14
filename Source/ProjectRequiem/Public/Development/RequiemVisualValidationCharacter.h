// Copyright Project Requiem. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Characters/RequiemCharacter.h"
#include "RequiemVisualValidationCharacter.generated.h"

class UAnimationAsset;

/**
 * Temporary presentation-only pawn for L_Dev_Foundation.
 * Reparent BP_CH_Player to ARequiemCharacter and remove this class when the real animation layer starts.
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
	TObjectPtr<UAnimationAsset> IdleAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual Validation|Animation")
	TObjectPtr<UAnimationAsset> MoveAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual Validation|Animation")
	TObjectPtr<UAnimationAsset> JumpAnimation;

private:
	void UpdateVisualValidationAnimation();

	UPROPERTY(Transient)
	TObjectPtr<UAnimationAsset> ActiveVisualValidationAnimation;
};
