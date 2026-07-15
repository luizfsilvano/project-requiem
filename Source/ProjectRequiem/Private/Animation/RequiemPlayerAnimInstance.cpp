// Copyright Project Requiem. All Rights Reserved.

#include "Animation/RequiemPlayerAnimInstance.h"

#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSequenceBase.h"
#include "Characters/RequiemCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ProjectRequiem.h"

const FName URequiemPlayerAnimInstance::LocomotionSlotName(TEXT("DefaultSlot"));

namespace
{
// Dynamic montages require a finite loop count. Ten thousand cycles is effectively
// continuous for these states without risking overflow in montage length calculations.
constexpr int32 LoopingMontageCount = 10000;
constexpr float OneShotLeadTime = 0.04f;
constexpr float MinimumLoopPlayRate = 0.35f;
constexpr float MaximumLoopPlayRate = 1.2f;

FName StateToName(const ERequiemLocomotionState State)
{
	switch (State)
	{
	case ERequiemLocomotionState::Idle:
		return TEXT("Idle");
	case ERequiemLocomotionState::IdleLookAround:
		return TEXT("Idle_LookAround");
	case ERequiemLocomotionState::Jog:
		return TEXT("Jog");
	case ERequiemLocomotionState::JumpStart:
		return TEXT("Jump_Start");
	case ERequiemLocomotionState::JumpLoop:
		return TEXT("Jump_Loop");
	case ERequiemLocomotionState::JumpLand:
		return TEXT("Jump_Land");
	case ERequiemLocomotionState::CrouchEnter:
		return TEXT("Crouch_Enter");
	case ERequiemLocomotionState::CrouchLoop:
		return TEXT("Crouch_Loop");
	case ERequiemLocomotionState::CrouchExit:
		return TEXT("Crouch_Exit");
	default:
		return NAME_None;
	}
}
}

void URequiemPlayerAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
	CacheCharacterReferences();

	LocomotionState = ERequiemLocomotionState::Idle;
	MovementDirection = ERequiemMovementDirection::None;
	ActiveLocomotionMontage = nullptr;
	ActiveAnimation = nullptr;
	StateElapsedSeconds = 0.0f;
	ActiveAnimationPlayRate = 1.0f;
	bNeedsInitialState = true;
	ScheduleNextLookAround();
}

void URequiemPlayerAnimInstance::NativeUpdateAnimation(const float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (!OwningCharacter || !OwningMovementComponent)
	{
		CacheCharacterReferences();
	}

	if (!OwningCharacter || !OwningMovementComponent)
	{
		return;
	}

	UpdateObservedMovement();
	StateElapsedSeconds += DeltaSeconds;

	if (bNeedsInitialState)
	{
		bNeedsInitialState = false;
		TransitionTo(ERequiemLocomotionState::Idle, true);
	}

	UpdateLocomotionState(DeltaSeconds);

	if (LocomotionState == ERequiemLocomotionState::Jog && ActiveLocomotionMontage)
	{
		ActiveAnimationPlayRate = JogAuthoredSpeed > UE_KINDA_SMALL_NUMBER
			? FMath::Clamp(ObservedGroundSpeed / JogAuthoredSpeed, MinimumLoopPlayRate, MaximumLoopPlayRate)
			: 1.0f;
		Montage_SetPlayRate(ActiveLocomotionMontage, ActiveAnimationPlayRate);
	}
	else if (LocomotionState == ERequiemLocomotionState::CrouchLoop && ActiveLocomotionMontage)
	{
		const float CrouchedMaximumSpeed = OwningMovementComponent->GetMaxSpeed();
		ActiveAnimationPlayRate = MovementDirection != ERequiemMovementDirection::None
			&& CrouchedMaximumSpeed > UE_KINDA_SMALL_NUMBER
			? FMath::Clamp(
				ObservedGroundSpeed / CrouchedMaximumSpeed,
				MinimumLoopPlayRate,
				MaximumLoopPlayRate)
			: 1.0f;
		Montage_SetPlayRate(ActiveLocomotionMontage, ActiveAnimationPlayRate);
	}
}

FName URequiemPlayerAnimInstance::GetLocomotionStateName() const
{
	return StateToName(LocomotionState);
}

void URequiemPlayerAnimInstance::CacheCharacterReferences()
{
	OwningCharacter = Cast<ARequiemCharacter>(TryGetPawnOwner());
	OwningMovementComponent = OwningCharacter ? OwningCharacter->GetCharacterMovement() : nullptr;
}

void URequiemPlayerAnimInstance::UpdateObservedMovement()
{
	const FVector Velocity = OwningCharacter->GetVelocity();
	ObservedGroundSpeed = Velocity.Size2D();
	bObservedIsFalling = OwningMovementComponent->IsFalling();
	bObservedIsCrouched = OwningCharacter->IsCrouched();

	const FVector DirectionSource = ObservedGroundSpeed >= DirectionalLoopMinimumSpeed
		? Velocity
		: OwningMovementComponent->GetCurrentAcceleration();
	const FVector LocalDirection = OwningCharacter->GetActorTransform().InverseTransformVectorNoScale(DirectionSource);
	const bool bHasDirection = !DirectionSource.IsNearlyZero(1.0f);
	ObservedDirectionDegrees = bHasDirection
		? FMath::RadiansToDegrees(FMath::Atan2(LocalDirection.Y, LocalDirection.X))
		: 0.0f;
	MovementDirection = QuantizeDirection(ObservedDirectionDegrees, bHasDirection);
}

void URequiemPlayerAnimInstance::UpdateLocomotionState(const float DeltaSeconds)
{
	if (bObservedIsFalling)
	{
		UpdateAirborneState();
		return;
	}

	if (bObservedIsCrouched)
	{
		UpdateCrouchedState();
		return;
	}

	if (LocomotionState == ERequiemLocomotionState::JumpStart
		|| LocomotionState == ERequiemLocomotionState::JumpLoop)
	{
		const bool bShouldJog = HasMovementIntent()
			|| ObservedGroundSpeed >= DirectionalLoopMinimumSpeed;
		TransitionTo(bShouldJog
			? ERequiemLocomotionState::Jog
			: ERequiemLocomotionState::JumpLand);
		return;
	}

	if (LocomotionState == ERequiemLocomotionState::CrouchEnter
		|| LocomotionState == ERequiemLocomotionState::CrouchLoop)
	{
		TransitionTo(ERequiemLocomotionState::CrouchExit);
		return;
	}

	UpdateGroundedState(DeltaSeconds);
}

void URequiemPlayerAnimInstance::UpdateGroundedState(const float DeltaSeconds)
{
	const bool bMovementIntent = HasMovementIntent();
	const bool bShouldJog = bMovementIntent
		|| ObservedGroundSpeed >= DirectionalLoopMinimumSpeed;

	switch (LocomotionState)
	{
	case ERequiemLocomotionState::Idle:
		if (bShouldJog)
		{
			TransitionTo(ERequiemLocomotionState::Jog);
			return;
		}

		LookAroundCountdown -= DeltaSeconds;
		if (LookAroundCountdown <= 0.0f && IdleLookAroundAnimation)
		{
			TransitionTo(ERequiemLocomotionState::IdleLookAround);
		}
		return;

	case ERequiemLocomotionState::IdleLookAround:
		if (bShouldJog)
		{
			TransitionTo(ERequiemLocomotionState::Jog);
			return;
		}
		if (HasOneShotFinished())
		{
			HandleFinishedOneShot();
		}
		return;

	case ERequiemLocomotionState::Jog:
		if (!bShouldJog)
		{
			TransitionTo(ERequiemLocomotionState::Idle);
			return;
		}
		RefreshDirectionalLoop();
		return;

	case ERequiemLocomotionState::JumpLand:
		if (bShouldJog)
		{
			TransitionTo(ERequiemLocomotionState::Jog);
			return;
		}
		if (HasOneShotFinished())
		{
			HandleFinishedOneShot();
		}
		return;

	case ERequiemLocomotionState::CrouchExit:
		if (HasOneShotFinished())
		{
			HandleFinishedOneShot();
		}
		return;

	default:
		TransitionTo(bShouldJog
			? ERequiemLocomotionState::Jog
			: ERequiemLocomotionState::Idle);
	}
}

void URequiemPlayerAnimInstance::UpdateAirborneState()
{
	if (LocomotionState != ERequiemLocomotionState::JumpStart
		&& LocomotionState != ERequiemLocomotionState::JumpLoop)
	{
		const bool bAscending = OwningCharacter->GetVelocity().Z > 0.0f;
		TransitionTo(bAscending
			? ERequiemLocomotionState::JumpStart
			: ERequiemLocomotionState::JumpLoop);
		return;
	}

	if (LocomotionState == ERequiemLocomotionState::JumpStart
		&& (OwningCharacter->GetVelocity().Z <= JumpLoopVerticalVelocity || HasOneShotFinished()))
	{
		TransitionTo(ERequiemLocomotionState::JumpLoop);
	}
}

void URequiemPlayerAnimInstance::UpdateCrouchedState()
{
	if (LocomotionState != ERequiemLocomotionState::CrouchEnter
		&& LocomotionState != ERequiemLocomotionState::CrouchLoop)
	{
		TransitionTo(ERequiemLocomotionState::CrouchEnter);
		return;
	}

	if (LocomotionState == ERequiemLocomotionState::CrouchEnter)
	{
		if (HasOneShotFinished())
		{
			TransitionTo(ERequiemLocomotionState::CrouchLoop);
		}
		return;
	}

	RefreshDirectionalLoop();
}

void URequiemPlayerAnimInstance::TransitionTo(
	const ERequiemLocomotionState NewState, const bool bForceReplay)
{
	if (NewState == LocomotionState && !bForceReplay)
	{
		return;
	}

	LocomotionState = NewState;
	StateElapsedSeconds = 0.0f;
	ActiveAnimationPlayRate = 1.0f;

	if (NewState == ERequiemLocomotionState::Idle)
	{
		ScheduleNextLookAround();
	}

	PlayStateAnimation();
}

void URequiemPlayerAnimInstance::HandleFinishedOneShot()
{
	switch (LocomotionState)
	{
	case ERequiemLocomotionState::IdleLookAround:
		TransitionTo(ERequiemLocomotionState::Idle);
		break;
	case ERequiemLocomotionState::JumpLand:
	case ERequiemLocomotionState::CrouchExit:
		TransitionTo(HasMovementIntent()
				|| ObservedGroundSpeed >= DirectionalLoopMinimumSpeed
			? ERequiemLocomotionState::Jog
			: ERequiemLocomotionState::Idle);
		break;
	default:
		break;
	}
}

void URequiemPlayerAnimInstance::RefreshDirectionalLoop()
{
	UAnimSequenceBase* DesiredAnimation = GetAnimationForCurrentState();
	if (!DesiredAnimation || DesiredAnimation == ActiveAnimation)
	{
		return;
	}

	float StartTime = 0.0f;
	if (ActiveLocomotionMontage && ActiveAnimation && ActiveAnimation->GetPlayLength() > UE_KINDA_SMALL_NUMBER)
	{
		const float NormalizedPhase = FMath::Fmod(
			Montage_GetPosition(ActiveLocomotionMontage) / ActiveAnimation->GetPlayLength(), 1.0f);
		StartTime = NormalizedPhase * DesiredAnimation->GetPlayLength();
	}

	PlayStateAnimation(StartTime);
}

void URequiemPlayerAnimInstance::PlayStateAnimation(const float StartTime)
{
	UAnimSequenceBase* NewAnimation = GetAnimationForCurrentState();
	if (!NewAnimation)
	{
		UE_LOG(
			LogProjectRequiem,
			Error,
			TEXT("Missing animation for player locomotion state %s"),
			*GetLocomotionStateName().ToString());
		ActiveAnimation = nullptr;
		ActiveLocomotionMontage = nullptr;
		return;
	}

	const UAnimSequence* AnimationSequence = Cast<UAnimSequence>(NewAnimation);
	if (AnimationSequence && AnimationSequence->bEnableRootMotion)
	{
		UE_LOG(
			LogProjectRequiem,
			Error,
			TEXT("Locomotion animation %s has root motion enabled; movement remains protected by IgnoreRootMotion"),
			*NewAnimation->GetPathName());
	}

	ActiveAnimation = NewAnimation;
	const bool bLooping = IsLoopingState(LocomotionState);
	const float BlendTime = GetStateBlendTime(LocomotionState);
	ActiveLocomotionMontage = PlaySlotAnimationAsDynamicMontage(
		NewAnimation,
		LocomotionSlotName,
		BlendTime,
		BlendTime,
		ActiveAnimationPlayRate,
		bLooping ? LoopingMontageCount : 1,
		-1.0f,
		FMath::Clamp(StartTime, 0.0f, NewAnimation->GetPlayLength()));
}

void URequiemPlayerAnimInstance::ScheduleNextLookAround()
{
	const float MinimumDelay = FMath::Max(0.0f, LookAroundMinimumDelay);
	const float MaximumDelay = FMath::Max(MinimumDelay, LookAroundMaximumDelay);
	LookAroundCountdown = FMath::FRandRange(MinimumDelay, MaximumDelay);
}

bool URequiemPlayerAnimInstance::HasMovementIntent() const
{
	return OwningMovementComponent
		&& !OwningMovementComponent->GetCurrentAcceleration().IsNearlyZero(1.0f);
}

bool URequiemPlayerAnimInstance::HasOneShotFinished() const
{
	if (!ActiveAnimation)
	{
		return true;
	}

	const float Duration = ActiveAnimation->GetPlayLength() / FMath::Max(ActiveAnimationPlayRate, UE_KINDA_SMALL_NUMBER);
	return StateElapsedSeconds >= FMath::Max(0.0f, Duration - OneShotLeadTime);
}

bool URequiemPlayerAnimInstance::IsLoopingState(const ERequiemLocomotionState State) const
{
	return State == ERequiemLocomotionState::Idle
		|| State == ERequiemLocomotionState::Jog
		|| State == ERequiemLocomotionState::JumpLoop
		|| State == ERequiemLocomotionState::CrouchLoop;
}

float URequiemPlayerAnimInstance::GetStateBlendTime(const ERequiemLocomotionState State) const
{
	switch (State)
	{
	case ERequiemLocomotionState::JumpStart:
	case ERequiemLocomotionState::JumpLoop:
	case ERequiemLocomotionState::JumpLand:
		return 0.08f;
	case ERequiemLocomotionState::CrouchEnter:
	case ERequiemLocomotionState::CrouchLoop:
	case ERequiemLocomotionState::CrouchExit:
		return 0.1f;
	default:
		return 0.12f;
	}
}

UAnimSequenceBase* URequiemPlayerAnimInstance::GetAnimationForCurrentState() const
{
	switch (LocomotionState)
	{
	case ERequiemLocomotionState::Idle:
		return IdleAnimation;
	case ERequiemLocomotionState::IdleLookAround:
		return IdleLookAroundAnimation;
	case ERequiemLocomotionState::Jog:
		return GetDirectionalJogAnimation();
	case ERequiemLocomotionState::JumpStart:
		return JumpStartAnimation;
	case ERequiemLocomotionState::JumpLoop:
		return JumpLoopAnimation;
	case ERequiemLocomotionState::JumpLand:
		return JumpLandAnimation;
	case ERequiemLocomotionState::CrouchEnter:
		return CrouchEnterAnimation;
	case ERequiemLocomotionState::CrouchLoop:
		return MovementDirection == ERequiemMovementDirection::None
			? CrouchIdleAnimation.Get()
			: GetDirectionalCrouchAnimation();
	case ERequiemLocomotionState::CrouchExit:
		return CrouchExitAnimation;
	default:
		return IdleAnimation;
	}
}

UAnimSequenceBase* URequiemPlayerAnimInstance::GetDirectionalJogAnimation() const
{
	switch (MovementDirection)
	{
	case ERequiemMovementDirection::ForwardLeft:
		return JogForwardLeftAnimation;
	case ERequiemMovementDirection::Forward:
		return JogForwardAnimation;
	case ERequiemMovementDirection::ForwardRight:
		return JogForwardRightAnimation;
	case ERequiemMovementDirection::BackwardLeft:
		return JogBackwardLeftAnimation;
	case ERequiemMovementDirection::Backward:
		return JogBackwardAnimation;
	case ERequiemMovementDirection::BackwardRight:
		return JogBackwardRightAnimation;
	case ERequiemMovementDirection::Left:
		return JogLeftAnimation;
	case ERequiemMovementDirection::Right:
		return JogRightAnimation;
	default:
		return JogForwardAnimation;
	}
}

UAnimSequenceBase* URequiemPlayerAnimInstance::GetDirectionalCrouchAnimation() const
{
	switch (MovementDirection)
	{
	case ERequiemMovementDirection::ForwardLeft:
		return CrouchForwardLeftAnimation;
	case ERequiemMovementDirection::Forward:
		return CrouchForwardAnimation;
	case ERequiemMovementDirection::ForwardRight:
		return CrouchForwardRightAnimation;
	case ERequiemMovementDirection::BackwardLeft:
		return CrouchBackwardLeftAnimation;
	case ERequiemMovementDirection::Backward:
		return CrouchBackwardAnimation;
	case ERequiemMovementDirection::BackwardRight:
		return CrouchBackwardRightAnimation;
	case ERequiemMovementDirection::Left:
		return CrouchLeftAnimation;
	case ERequiemMovementDirection::Right:
		return CrouchRightAnimation;
	default:
		return CrouchIdleAnimation;
	}
}

ERequiemMovementDirection URequiemPlayerAnimInstance::QuantizeDirection(
	const float InDirectionDegrees, const bool bHasPlanarVelocity) const
{
	if (!bHasPlanarVelocity)
	{
		return ERequiemMovementDirection::None;
	}

	const float Angle = FMath::UnwindDegrees(InDirectionDegrees);
	if (Angle >= -22.5f && Angle < 22.5f)
	{
		return ERequiemMovementDirection::Forward;
	}
	if (Angle >= 22.5f && Angle < 67.5f)
	{
		return ERequiemMovementDirection::ForwardRight;
	}
	if (Angle >= 67.5f && Angle < 112.5f)
	{
		return ERequiemMovementDirection::Right;
	}
	if (Angle >= 112.5f && Angle < 157.5f)
	{
		return ERequiemMovementDirection::BackwardRight;
	}
	if (Angle >= 157.5f || Angle < -157.5f)
	{
		return ERequiemMovementDirection::Backward;
	}
	if (Angle >= -157.5f && Angle < -112.5f)
	{
		return ERequiemMovementDirection::BackwardLeft;
	}
	if (Angle >= -112.5f && Angle < -67.5f)
	{
		return ERequiemMovementDirection::Left;
	}
	return ERequiemMovementDirection::ForwardLeft;
}
