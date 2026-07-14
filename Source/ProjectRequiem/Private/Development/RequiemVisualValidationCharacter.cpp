// Copyright Project Requiem. All Rights Reserved.

#include "Development/RequiemVisualValidationCharacter.h"

#include "Animation/AnimationAsset.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

ARequiemVisualValidationCharacter::ARequiemVisualValidationCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ARequiemVisualValidationCharacter::BeginPlay()
{
	Super::BeginPlay();
	UpdateVisualValidationAnimation();
}

void ARequiemVisualValidationCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateVisualValidationAnimation();
}

void ARequiemVisualValidationCharacter::UpdateVisualValidationAnimation()
{
	UAnimationAsset* DesiredAnimation = IdleAnimation;
	if (GetCharacterMovement()->IsFalling())
	{
		DesiredAnimation = JumpAnimation;
	}
	else if (!GetVelocity().IsNearlyZero())
	{
		DesiredAnimation = MoveAnimation;
	}

	if (!DesiredAnimation || DesiredAnimation == ActiveVisualValidationAnimation)
	{
		return;
	}

	GetMesh()->PlayAnimation(DesiredAnimation, true);
	ActiveVisualValidationAnimation = DesiredAnimation;
}
