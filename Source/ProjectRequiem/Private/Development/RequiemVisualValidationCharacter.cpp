// Copyright Project Requiem. All Rights Reserved.

#include "Development/RequiemVisualValidationCharacter.h"

#include "AnimSequencerInstance.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace
{
constexpr float GroundBlendDuration = 0.16f;
constexpr float RunEnterBlendDuration = 0.12f;
constexpr float RunLoopBlendDuration = 0.04f;
constexpr float RunExitBlendDuration = 0.12f;
constexpr float RunEnterCommitFraction = 0.45f;
constexpr float RunLoopMinimumPlayRate = 0.35f;
constexpr float FallingBlendDuration = 0.08f;

float SmoothStep01(const float Value)
{
	const float ClampedValue = FMath::Clamp(Value, 0.0f, 1.0f);
	return ClampedValue * ClampedValue * (3.0f - 2.0f * ClampedValue);
}
}

ARequiemVisualValidationCharacter::ARequiemVisualValidationCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	// Temporary tuning based on the source motion contained in UAL1 Pro.
	GetCharacterMovement()->MaxAcceleration = 1200.0f;
	GetCharacterMovement()->BrakingDecelerationWalking = 1200.0f;
	GetCharacterMovement()->GroundFriction = 6.0f;
}

void ARequiemVisualValidationCharacter::BeginPlay()
{
	Super::BeginPlay();

	// UAnimSequencerInstance gives this temporary pawn two weighted sequence tracks,
	// allowing short crossfades without introducing a production Animation Blueprint yet.
	GetMesh()->SetAnimInstanceClass(UAnimSequencerInstance::StaticClass());
	VisualAnimationInstance = Cast<UAnimSequencerInstance>(GetMesh()->GetAnimInstance());
	GetMesh()->AddTickPrerequisiteActor(this);

	SetLocomotionState(ELocomotionState::Idle, 0.0f);
	UpdateMovementSpeed();
	UpdateAnimationBlend();
}

void ARequiemVisualValidationCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!VisualAnimationInstance)
	{
		VisualAnimationInstance = Cast<UAnimSequencerInstance>(GetMesh()->GetAnimInstance());
	}

	UpdateLocomotionState();
	AdvanceAnimationPlayback(DeltaSeconds);
	HandleCompletedAnimation();
	UpdateMovementSpeed();
	UpdateAnimationBlend();
}

void ARequiemVisualValidationCharacter::UpdateLocomotionState()
{
	if (GetCharacterMovement()->IsFalling())
	{
		if (LocomotionState != ELocomotionState::Falling)
		{
			SetLocomotionState(ELocomotionState::Falling, FallingBlendDuration);
		}
		return;
	}

	if (LocomotionState == ELocomotionState::Falling)
	{
		const float HorizontalSpeed = GetVelocity().Size2D();
		ELocomotionState GroundState = HorizontalSpeed > RunStartSpeed
			? ELocomotionState::RunExit
			: ELocomotionState::Idle;
		if (HasMovementIntent())
		{
			GroundState = HorizontalSpeed >= RunSpeed * RunLoopMinimumPlayRate
				? ELocomotionState::RunLoop
				: ELocomotionState::RunEnter;
		}
		SetLocomotionState(GroundState, GroundBlendDuration);
	}

	switch (LocomotionState)
	{
	case ELocomotionState::RunEnter:
		if (!HasMovementIntent())
		{
			const bool bCommittedToRun =
				GetNormalizedStateTime() >= RunEnterCommitFraction;
			SetLocomotionState(
				bCommittedToRun ? ELocomotionState::RunExit : ELocomotionState::Idle,
				bCommittedToRun ? RunExitBlendDuration : GroundBlendDuration);
		}
		return;

	case ELocomotionState::RunExit:
		if (HasMovementIntent())
		{
			const ELocomotionState ResumeState =
				GetVelocity().Size2D() >= RunSpeed * RunLoopMinimumPlayRate
					? ELocomotionState::RunLoop
					: ELocomotionState::RunEnter;
			SetLocomotionState(ResumeState, RunExitBlendDuration);
		}
		return;

	case ELocomotionState::RunLoop:
		if (!HasMovementIntent())
		{
			SetLocomotionState(ELocomotionState::RunExit, RunExitBlendDuration);
		}
		return;

	default:
		break;
	}

	if (HasMovementIntent())
	{
		SetLocomotionState(ELocomotionState::RunEnter, RunEnterBlendDuration);
		return;
	}

	if (LocomotionState != ELocomotionState::Idle)
	{
		SetLocomotionState(ELocomotionState::Idle, GroundBlendDuration);
	}
}

void ARequiemVisualValidationCharacter::HandleCompletedAnimation()
{
	if (!ActiveAnimation || IsLoopingState(LocomotionState))
	{
		return;
	}

	if (ActiveAnimationPosition + KINDA_SMALL_NUMBER < ActiveAnimation->GetPlayLength())
	{
		return;
	}

	if (LocomotionState == ELocomotionState::RunEnter)
	{
		const ELocomotionState NextState = HasMovementIntent()
			? ELocomotionState::RunLoop
			: ELocomotionState::RunExit;
		SetLocomotionState(NextState, RunLoopBlendDuration);
		return;
	}

	if (LocomotionState == ELocomotionState::RunExit)
	{
		if (HasMovementIntent())
		{
			SetLocomotionState(ELocomotionState::RunEnter, RunEnterBlendDuration);
			return;
		}

		SetLocomotionState(ELocomotionState::Idle, GroundBlendDuration);
	}
}

void ARequiemVisualValidationCharacter::SetLocomotionState(
	const ELocomotionState NewState, const float InBlendDuration)
{
	UAnimSequenceBase* NewAnimation = GetAnimationForState(NewState);
	if (!NewAnimation || (NewState == LocomotionState && ActiveAnimation))
	{
		return;
	}

	if (ActiveAnimation)
	{
		PreviousAnimation = ActiveAnimation;
		PreviousAnimationPosition = ActiveAnimationPosition;
		bPreviousAnimationLooping = IsLoopingState(LocomotionState);
		PreviousTrackId = ActiveTrackId;
		ActiveTrackId = ActiveTrackId == 0 ? 1 : 0;
	}
	else
	{
		PreviousAnimation = nullptr;
		PreviousAnimationPosition = 0.0f;
		bPreviousAnimationLooping = false;
	}

	LocomotionState = NewState;
	ActiveAnimation = NewAnimation;
	ActiveAnimationPosition = 0.0f;
	BlendElapsed = 0.0f;
	BlendDuration = PreviousAnimation ? FMath::Max(0.0f, InBlendDuration) : 0.0f;
}

void ARequiemVisualValidationCharacter::AdvanceAnimationPlayback(const float DeltaSeconds)
{
	auto AdvancePosition = [DeltaSeconds](
		float& Position, const UAnimSequenceBase* Animation, const bool bLooping,
		const float PlayRate)
	{
		if (!Animation)
		{
			return;
		}

		const float Length = Animation->GetPlayLength();
		if (Length <= UE_KINDA_SMALL_NUMBER)
		{
			Position = 0.0f;
			return;
		}

		Position += DeltaSeconds * PlayRate;
		Position = bLooping
			? FMath::Fmod(Position, Length)
			: FMath::Min(Position, Length);
	};

	float ActivePlayRate = 1.0f;
	const float HorizontalSpeed = GetVelocity().Size2D();
	if (LocomotionState == ELocomotionState::RunLoop && RunAnimationAuthoredSpeed > UE_KINDA_SMALL_NUMBER)
	{
		ActivePlayRate = FMath::Clamp(
			HorizontalSpeed / RunAnimationAuthoredSpeed, RunLoopMinimumPlayRate, 1.2f);
	}

	AdvancePosition(
		ActiveAnimationPosition, ActiveAnimation, IsLoopingState(LocomotionState),
		ActivePlayRate);
	AdvancePosition(
		PreviousAnimationPosition, PreviousAnimation, bPreviousAnimationLooping, 1.0f);

	BlendElapsed += DeltaSeconds;
	if (PreviousAnimation && (BlendDuration <= UE_KINDA_SMALL_NUMBER || BlendElapsed >= BlendDuration))
	{
		PreviousAnimation = nullptr;
		PreviousAnimationPosition = 0.0f;
		bPreviousAnimationLooping = false;
	}
}

void ARequiemVisualValidationCharacter::UpdateMovementSpeed()
{
	// Movement feel belongs to CharacterMovement. The animation state machine only
	// presents enter/loop/exit clips and never introduces a second speed curve.
	GetCharacterMovement()->MaxWalkSpeed = FMath::Max(0.0f, RunSpeed);
}

void ARequiemVisualValidationCharacter::UpdateAnimationBlend()
{
	if (!VisualAnimationInstance || !ActiveAnimation)
	{
		return;
	}

	VisualAnimationInstance->ResetNodes();

	const float BlendAlpha = PreviousAnimation && BlendDuration > UE_KINDA_SMALL_NUMBER
		? SmoothStep01(BlendElapsed / BlendDuration)
		: 1.0f;

	if (PreviousAnimation && BlendAlpha < 1.0f)
	{
		VisualAnimationInstance->UpdateAnimTrack(
			PreviousAnimation, PreviousTrackId, PreviousAnimationPosition,
			1.0f - BlendAlpha, false);
	}

	VisualAnimationInstance->UpdateAnimTrack(
		ActiveAnimation, ActiveTrackId, ActiveAnimationPosition, BlendAlpha, false);
}

bool ARequiemVisualValidationCharacter::HasMovementIntent() const
{
	return !GetLastMovementInputVector().IsNearlyZero(0.05f);
}

bool ARequiemVisualValidationCharacter::IsLoopingState(const ELocomotionState State) const
{
	return State == ELocomotionState::Idle
		|| State == ELocomotionState::RunLoop
		|| State == ELocomotionState::Falling;
}

float ARequiemVisualValidationCharacter::GetNormalizedStateTime() const
{
	if (!ActiveAnimation || ActiveAnimation->GetPlayLength() <= UE_KINDA_SMALL_NUMBER)
	{
		return 0.0f;
	}
	return FMath::Clamp(ActiveAnimationPosition / ActiveAnimation->GetPlayLength(), 0.0f, 1.0f);
}

UAnimSequenceBase* ARequiemVisualValidationCharacter::GetAnimationForState(
	const ELocomotionState State) const
{
	switch (State)
	{
	case ELocomotionState::Idle:
		return IdleAnimation;
	case ELocomotionState::RunEnter:
		return RunEnterAnimation;
	case ELocomotionState::RunLoop:
		return RunLoopAnimation;
	case ELocomotionState::RunExit:
		return RunExitAnimation;
	case ELocomotionState::Falling:
		return JumpAnimation;
	default:
		return nullptr;
	}
}
