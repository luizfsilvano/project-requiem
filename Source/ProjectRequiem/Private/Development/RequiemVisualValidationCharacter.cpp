// Copyright Project Requiem. All Rights Reserved.

#include "Development/RequiemVisualValidationCharacter.h"

#include "AnimSequencerInstance.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "InputActionValue.h"

namespace
{
constexpr float MovementSpeedThreshold = 5.0f;
constexpr float GroundBlendDuration = 0.16f;
constexpr float SprintEnterBlendDuration = 0.12f;
constexpr float SprintLoopBlendDuration = 0.04f;
constexpr float SprintExitBlendDuration = 0.12f;
constexpr float SprintExitWalkTransitionFraction = 0.35f;
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
	GetCharacterMovement()->MaxAcceleration = 1400.0f;
	GetCharacterMovement()->BrakingDecelerationWalking = 1600.0f;
}

void ARequiemVisualValidationCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EnhancedInputComponent || !SprintAction)
	{
		return;
	}

	// IA_Sprint owns the Hold gesture. IA_Jump keeps the Tap gesture on the same key.
	EnhancedInputComponent->BindAction(
		SprintAction, ETriggerEvent::Triggered, this,
		&ARequiemVisualValidationCharacter::HandleSprintTriggered);
	EnhancedInputComponent->BindAction(
		SprintAction, ETriggerEvent::Completed, this,
		&ARequiemVisualValidationCharacter::HandleSprintReleased);
	EnhancedInputComponent->BindAction(
		SprintAction, ETriggerEvent::Canceled, this,
		&ARequiemVisualValidationCharacter::HandleSprintReleased);
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

void ARequiemVisualValidationCharacter::HandleSprintTriggered(const FInputActionValue& Value)
{
	bSprintRequested = Value.Get<bool>();
}

void ARequiemVisualValidationCharacter::HandleSprintReleased(const FInputActionValue& Value)
{
	bSprintRequested = false;
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
		const ELocomotionState GroundState = IsMovingOnGround()
			? ELocomotionState::Walk
			: ELocomotionState::Idle;
		SetLocomotionState(GroundState, GroundBlendDuration);
	}

	switch (LocomotionState)
	{
	case ELocomotionState::SprintEnter:
		if (!bSprintRequested || !HasMovementIntent())
		{
			SetLocomotionState(ELocomotionState::SprintExit, SprintExitBlendDuration);
		}
		return;

	case ELocomotionState::SprintExit:
		// SprintExit selects Walk or Idle at an authored handoff point below.
		return;

	case ELocomotionState::SprintLoop:
		if (!bSprintRequested || !HasMovementIntent())
		{
			SetLocomotionState(ELocomotionState::SprintExit, SprintExitBlendDuration);
		}
		return;

	default:
		break;
	}

	if (bSprintRequested && HasMovementIntent())
	{
		SetLocomotionState(ELocomotionState::SprintEnter, SprintEnterBlendDuration);
		return;
	}

	const ELocomotionState DesiredGroundState = IsMovingOnGround()
		? ELocomotionState::Walk
		: ELocomotionState::Idle;
	if (DesiredGroundState != LocomotionState)
	{
		SetLocomotionState(DesiredGroundState, GroundBlendDuration);
	}
}

void ARequiemVisualValidationCharacter::HandleCompletedAnimation()
{
	if (!ActiveAnimation || IsLoopingState(LocomotionState))
	{
		return;
	}

	// Sprint_Exit settles to idle after its displacement phase. When movement is
	// still requested, hand off to Walk as soon as that phase finishes instead
	// of visually idling while the capsule keeps moving.
	if (LocomotionState == ELocomotionState::SprintExit
		&& HasMovementIntent()
		&& GetNormalizedStateTime() >= SprintExitWalkTransitionFraction)
	{
		const ELocomotionState NextState = bSprintRequested
			? ELocomotionState::SprintEnter
			: ELocomotionState::Walk;
		SetLocomotionState(NextState, SprintExitBlendDuration);
		return;
	}

	if (ActiveAnimationPosition + KINDA_SMALL_NUMBER < ActiveAnimation->GetPlayLength())
	{
		return;
	}

	if (LocomotionState == ELocomotionState::SprintEnter)
	{
		const ELocomotionState NextState = bSprintRequested && HasMovementIntent()
			? ELocomotionState::SprintLoop
			: ELocomotionState::SprintExit;
		SetLocomotionState(NextState, SprintLoopBlendDuration);
		return;
	}

	if (LocomotionState == ELocomotionState::SprintExit)
	{
		if (bSprintRequested && HasMovementIntent())
		{
			SetLocomotionState(ELocomotionState::SprintEnter, SprintEnterBlendDuration);
			return;
		}

		const ELocomotionState GroundState = IsMovingOnGround() || HasMovementIntent()
			? ELocomotionState::Walk
			: ELocomotionState::Idle;
		SetLocomotionState(GroundState, GroundBlendDuration);
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

	if (NewState == ELocomotionState::SprintExit)
	{
		SprintExitStartSpeed = FMath::Clamp(GetVelocity().Size2D(), 0.0f, SprintSpeed);
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
	if (LocomotionState == ELocomotionState::Walk && WalkSpeed > UE_KINDA_SMALL_NUMBER)
	{
		ActivePlayRate = FMath::Clamp(HorizontalSpeed / WalkSpeed, 0.2f, 1.2f);
	}
	else if (LocomotionState == ELocomotionState::SprintLoop && SprintSpeed > UE_KINDA_SMALL_NUMBER)
	{
		ActivePlayRate = FMath::Clamp(HorizontalSpeed / SprintSpeed, 0.5f, 1.2f);
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
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	float DesiredMaxSpeed = WalkSpeed;

	switch (LocomotionState)
	{
	case ELocomotionState::SprintEnter:
	{
		// The authored clip walks for roughly its first half, then commits to the sprint.
		const float RampAlpha = (GetNormalizedStateTime() - 0.45f) / 0.55f;
		DesiredMaxSpeed = FMath::Lerp(WalkSpeed, SprintSpeed, SmoothStep01(RampAlpha));
		break;
	}

	case ELocomotionState::SprintLoop:
		DesiredMaxSpeed = SprintSpeed;
		break;

	case ELocomotionState::SprintExit:
	{
		// Preserve the speed present at release, then settle to Walk when movement
		// remains held or to zero when the player also lets go of movement.
		const float ExitAlpha = GetNormalizedStateTime() / SprintExitWalkTransitionFraction;
		const float ExitTargetSpeed = HasMovementIntent() ? WalkSpeed : 0.0f;
		DesiredMaxSpeed = FMath::Lerp(
			SprintExitStartSpeed, ExitTargetSpeed, SmoothStep01(ExitAlpha));
		break;
	}

	default:
		break;
	}

	Movement->MaxWalkSpeed = FMath::Max(0.0f, DesiredMaxSpeed);
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

bool ARequiemVisualValidationCharacter::IsMovingOnGround() const
{
	return !GetCharacterMovement()->IsFalling()
		&& GetVelocity().SizeSquared2D() > FMath::Square(MovementSpeedThreshold);
}

bool ARequiemVisualValidationCharacter::IsLoopingState(const ELocomotionState State) const
{
	return State == ELocomotionState::Idle
		|| State == ELocomotionState::Walk
		|| State == ELocomotionState::SprintLoop
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
	case ELocomotionState::Walk:
		return MoveAnimation;
	case ELocomotionState::SprintEnter:
		return SprintEnterAnimation;
	case ELocomotionState::SprintLoop:
		return SprintLoopAnimation;
	case ELocomotionState::SprintExit:
		return SprintExitAnimation;
	case ELocomotionState::Falling:
		return JumpAnimation;
	default:
		return nullptr;
	}
}
