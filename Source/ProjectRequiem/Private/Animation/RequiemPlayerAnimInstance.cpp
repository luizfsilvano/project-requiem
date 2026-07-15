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
constexpr int32 MaximumBufferedAttackRequests = 8;
constexpr float OneShotLeadTime = 0.04f;
constexpr float CombatOneShotLeadTime = 0.015f;
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

FName CombatAnimationStateToName(const ERequiemCombatAnimationState State)
{
	switch (State)
	{
	case ERequiemCombatAnimationState::Inactive:
		return TEXT("Inactive");
	case ERequiemCombatAnimationState::Enter:
		return TEXT("Enter");
	case ERequiemCombatAnimationState::Idle:
		return TEXT("Idle");
	case ERequiemCombatAnimationState::Attack:
		return TEXT("Attack");
	case ERequiemCombatAnimationState::Recovery:
		return TEXT("Recovery");
	case ERequiemCombatAnimationState::Exit:
		return TEXT("Exit");
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
	ObservedCombatState = ERequiemCombatState::Normal;
	CombatAnimationState = ERequiemCombatAnimationState::Inactive;
	ActiveComboAnimationIndex = INDEX_NONE;
	ActiveLocomotionMontage = nullptr;
	ActiveAnimation = nullptr;
	ActiveLocomotionAnimation = nullptr;
	StateElapsedSeconds = 0.0f;
	CombatAnimationElapsedSeconds = 0.0f;
	ActiveAnimationPlayRate = 1.0f;
	LastConsumedAttackRequestSerial = CombatComponent
		? CombatComponent->GetAttackRequestSerial()
		: 0;
	PendingAttackRequestCount = 0;
	bNeedsInitialState = true;
	bEnterQueued = false;
	bExitQueued = false;
	bCombatAssetsInvalid = false;
	ScheduleNextLookAround();
}

void URequiemPlayerAnimInstance::NativeUpdateAnimation(const float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (!OwningCharacter || !OwningMovementComponent || !CombatComponent)
	{
		CacheCharacterReferences();
	}

	if (!OwningCharacter || !OwningMovementComponent)
	{
		return;
	}

	UpdateObservedMovement();
	StateElapsedSeconds += DeltaSeconds;
	CombatAnimationElapsedSeconds += DeltaSeconds;

	if (bNeedsInitialState)
	{
		bNeedsInitialState = false;
		TransitionTo(ERequiemLocomotionState::Idle, true);
	}

	ObserveCombatRequests();
	HandleCombatStateChange();
	UpdateCombatPresentation();

	if (CombatAnimationState == ERequiemCombatAnimationState::Inactive)
	{
		UpdateLocomotionState(DeltaSeconds);
	}

	if (CombatAnimationState == ERequiemCombatAnimationState::Inactive
		&& LocomotionState == ERequiemLocomotionState::Jog
		&& ActiveLocomotionMontage)
	{
		ActiveAnimationPlayRate = JogAuthoredSpeed > UE_KINDA_SMALL_NUMBER
			? FMath::Clamp(ObservedGroundSpeed / JogAuthoredSpeed, MinimumLoopPlayRate, MaximumLoopPlayRate)
			: 1.0f;
		Montage_SetPlayRate(ActiveLocomotionMontage, ActiveAnimationPlayRate);
	}
	else if (CombatAnimationState == ERequiemCombatAnimationState::Inactive
		&& LocomotionState == ERequiemLocomotionState::CrouchLoop
		&& ActiveLocomotionMontage)
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

FName URequiemPlayerAnimInstance::GetObservedCombatStateName() const
{
	return ObservedCombatState == ERequiemCombatState::CombatUnarmed
		? FName(TEXT("CombatUnarmed"))
		: FName(TEXT("Normal"));
}

FName URequiemPlayerAnimInstance::GetCombatAnimationStateName() const
{
	return CombatAnimationStateToName(CombatAnimationState);
}

void URequiemPlayerAnimInstance::CacheCharacterReferences()
{
	OwningCharacter = Cast<ARequiemCharacter>(TryGetPawnOwner());
	OwningMovementComponent = OwningCharacter ? OwningCharacter->GetCharacterMovement() : nullptr;
	CombatComponent = OwningCharacter ? OwningCharacter->GetCombatComponent() : nullptr;
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

void URequiemPlayerAnimInstance::ObserveCombatRequests()
{
	if (!CombatComponent)
	{
		return;
	}

	const int32 CurrentSerial = CombatComponent->GetAttackRequestSerial();
	if (CombatComponent->GetCombatState() == ERequiemCombatState::Normal)
	{
		// Covers an attack followed by an immediate toggle before this AnimInstance
		// observes either edge; the stale request must not fire on the next entry.
		LastConsumedAttackRequestSerial = CurrentSerial;
		PendingAttackRequestCount = 0;
		return;
	}

	if (CurrentSerial == LastConsumedAttackRequestSerial)
	{
		return;
	}

	// A lower value is only possible when the component deliberately wraps MAX_int32 to one.
	const int32 NewRequestCount = CurrentSerial > LastConsumedAttackRequestSerial
		? CurrentSerial - LastConsumedAttackRequestSerial
		: 1;
	PendingAttackRequestCount = FMath::Min(
		PendingAttackRequestCount + NewRequestCount,
		MaximumBufferedAttackRequests);
	LastConsumedAttackRequestSerial = CurrentSerial;
}

void URequiemPlayerAnimInstance::HandleCombatStateChange()
{
	if (!CombatComponent)
	{
		return;
	}

	const ERequiemCombatState CurrentState = CombatComponent->GetCombatState();
	if (CurrentState == ObservedCombatState)
	{
		return;
	}

	ObservedCombatState = CurrentState;
	if (bCombatAssetsInvalid)
	{
		PendingAttackRequestCount = 0;
		bEnterQueued = false;
		bExitQueued = false;
		if (CombatAnimationState != ERequiemCombatAnimationState::Inactive)
		{
			ResumeLocomotionPresentation();
		}
		return;
	}

	if (ObservedCombatState == ERequiemCombatState::CombatUnarmed)
	{
		bExitQueued = false;
		if (CombatAnimationState == ERequiemCombatAnimationState::Attack
			|| CombatAnimationState == ERequiemCombatAnimationState::Recovery)
		{
			// Re-entering before an authored attack/recovery finishes cancels the
			// queued exit without cutting the current clip.
			bEnterQueued = false;
			return;
		}
		if (!CanPlayCombatStanceTransition())
		{
			bEnterQueued = true;
			if (CombatAnimationState != ERequiemCombatAnimationState::Inactive)
			{
				ResumeLocomotionPresentation();
			}
			return;
		}

		bEnterQueued = false;
		StartCombatEnter();
		return;
	}

	// Leaving combat cancels buffered follow-ups. A Knee/Hook recovery already in
	// progress still finishes before PunchKick_Exit so the pose remains continuous.
	PendingAttackRequestCount = 0;
	bEnterQueued = false;
	if (CombatAnimationState == ERequiemCombatAnimationState::Attack
		|| CombatAnimationState == ERequiemCombatAnimationState::Recovery
		|| !CanPlayCombatStanceTransition())
	{
		bExitQueued = true;
		if (CombatAnimationState != ERequiemCombatAnimationState::Attack
			&& CombatAnimationState != ERequiemCombatAnimationState::Recovery
			&& CombatAnimationState != ERequiemCombatAnimationState::Inactive)
		{
			ResumeLocomotionPresentation();
		}
		return;
	}

	if (CombatAnimationState == ERequiemCombatAnimationState::Exit)
	{
		return;
	}

	bExitQueued = false;
	StartCombatExit();
}

void URequiemPlayerAnimInstance::UpdateCombatPresentation()
{
	if (bCombatAssetsInvalid)
	{
		return;
	}

	// Airborne and crouched locomotion have immediate visual priority. Enter/Exit
	// are deferred and an in-progress combo is safely abandoned without affecting
	// CharacterMovement.
	if ((bObservedIsFalling || bObservedIsCrouched)
		&& CombatAnimationState != ERequiemCombatAnimationState::Inactive)
	{
		if (CombatAnimationState == ERequiemCombatAnimationState::Enter)
		{
			bEnterQueued = ObservedCombatState == ERequiemCombatState::CombatUnarmed;
		}
		else if (CombatAnimationState == ERequiemCombatAnimationState::Exit)
		{
			bExitQueued = ObservedCombatState == ERequiemCombatState::Normal;
		}
		else if (CombatAnimationState == ERequiemCombatAnimationState::Attack
			|| CombatAnimationState == ERequiemCombatAnimationState::Recovery)
		{
			PendingAttackRequestCount = 0;
		}

		ResumeLocomotionPresentation();
		return;
	}

	switch (CombatAnimationState)
	{
	case ERequiemCombatAnimationState::Inactive:
		if (ObservedCombatState == ERequiemCombatState::CombatUnarmed)
		{
			if (bEnterQueued)
			{
				if (CanPlayCombatStanceTransition())
				{
					StartCombatEnter();
				}
				return;
			}
			if (!TryConsumeAttackRequest(0) && ShouldUseCombatIdle())
			{
				StartCombatIdle();
			}
		}
		else if (bExitQueued && CanPlayCombatStanceTransition())
		{
			StartCombatExit();
		}
		return;

	case ERequiemCombatAnimationState::Idle:
		if (ObservedCombatState == ERequiemCombatState::Normal)
		{
			StartCombatExit();
			return;
		}
		if (TryConsumeAttackRequest(0))
		{
			return;
		}
		if (!ShouldUseCombatIdle())
		{
			ResumeLocomotionPresentation();
		}
		return;

	case ERequiemCombatAnimationState::Enter:
	case ERequiemCombatAnimationState::Attack:
	case ERequiemCombatAnimationState::Recovery:
	case ERequiemCombatAnimationState::Exit:
		if (HasCombatOneShotFinished())
		{
			HandleFinishedCombatOneShot();
		}
		return;

	default:
		return;
	}
}

void URequiemPlayerAnimInstance::StartCombatEnter()
{
	bEnterQueued = false;
	PlayCombatAnimation(
		ERequiemCombatAnimationState::Enter,
		CombatEnterAnimation,
		false);
}

void URequiemPlayerAnimInstance::StartCombatIdle()
{
	PlayCombatAnimation(
		ERequiemCombatAnimationState::Idle,
		CombatIdleAnimation,
		true);
}

void URequiemPlayerAnimInstance::StartCombatExit()
{
	bExitQueued = false;
	PlayCombatAnimation(
		ERequiemCombatAnimationState::Exit,
		CombatExitAnimation,
		false);
}

void URequiemPlayerAnimInstance::StartCombatComboClip(const int32 ComboIndex)
{
	const bool bRecovery = ComboIndex == 3 || ComboIndex == 5;
	PlayCombatAnimation(
		bRecovery
			? ERequiemCombatAnimationState::Recovery
			: ERequiemCombatAnimationState::Attack,
		GetComboAnimation(ComboIndex),
		false,
		ComboIndex);
}

bool URequiemPlayerAnimInstance::TryConsumeAttackRequest(const int32 ComboIndex)
{
	if (PendingAttackRequestCount <= 0 || !CanStartUnarmedAttack())
	{
		return false;
	}

	--PendingAttackRequestCount;
	StartCombatComboClip(ComboIndex);
	return true;
}

void URequiemPlayerAnimInstance::HandleFinishedCombatOneShot()
{
	const auto ReturnToCombatPosture = [this]()
	{
		if (ShouldUseCombatIdle())
		{
			StartCombatIdle();
		}
		else
		{
			ResumeLocomotionPresentation();
		}
	};

	switch (CombatAnimationState)
	{
	case ERequiemCombatAnimationState::Enter:
		if (ObservedCombatState == ERequiemCombatState::Normal)
		{
			StartCombatExit();
		}
		else if (!TryConsumeAttackRequest(0))
		{
			ReturnToCombatPosture();
		}
		return;

	case ERequiemCombatAnimationState::Attack:
		// Knee and Hook always play their authored recovery before any next decision.
		if (ActiveComboAnimationIndex == 2)
		{
			StartCombatComboClip(3);
			return;
		}
		if (ActiveComboAnimationIndex == 4)
		{
			StartCombatComboClip(5);
			return;
		}
		if (bExitQueued || ObservedCombatState == ERequiemCombatState::Normal)
		{
			bExitQueued = false;
			StartCombatExit();
			return;
		}
		if (ActiveComboAnimationIndex == 0 && TryConsumeAttackRequest(1))
		{
			return;
		}
		if (ActiveComboAnimationIndex == 1 && TryConsumeAttackRequest(2))
		{
			return;
		}
		if (ActiveComboAnimationIndex == 6 && TryConsumeAttackRequest(0))
		{
			return;
		}
		ReturnToCombatPosture();
		return;

	case ERequiemCombatAnimationState::Recovery:
		if (bExitQueued || ObservedCombatState == ERequiemCombatState::Normal)
		{
			bExitQueued = false;
			StartCombatExit();
			return;
		}
		if (ActiveComboAnimationIndex == 3 && TryConsumeAttackRequest(4))
		{
			return;
		}
		if (ActiveComboAnimationIndex == 5 && TryConsumeAttackRequest(6))
		{
			return;
		}
		ReturnToCombatPosture();
		return;

	case ERequiemCombatAnimationState::Exit:
		ResumeLocomotionPresentation();
		return;

	default:
		return;
	}
}

void URequiemPlayerAnimInstance::ResumeLocomotionPresentation()
{
	CombatAnimationState = ERequiemCombatAnimationState::Inactive;
	ActiveComboAnimationIndex = INDEX_NONE;
	CombatAnimationElapsedSeconds = 0.0f;

	// Combat and locomotion intentionally share DefaultSlot. Force the currently
	// observed locomotion state back onto it before normal locomotion updates resume.
	TransitionTo(LocomotionState, true);
}

void URequiemPlayerAnimInstance::PlayCombatAnimation(
	const ERequiemCombatAnimationState NewState,
	UAnimSequenceBase* NewAnimation,
	const bool bLooping,
	const int32 ComboIndex)
{
	CombatAnimationState = NewState;
	ActiveComboAnimationIndex = ComboIndex;
	CombatAnimationElapsedSeconds = 0.0f;
	ActiveAnimationPlayRate = 1.0f;

	if (!NewAnimation)
	{
		UE_LOG(
			LogProjectRequiem,
			Error,
			TEXT("Missing animation for player combat presentation state %s"),
			*GetCombatAnimationStateName().ToString());
		CombatAnimationState = ERequiemCombatAnimationState::Inactive;
		ActiveComboAnimationIndex = INDEX_NONE;
		bCombatAssetsInvalid = true;
		ActiveAnimation = nullptr;
		ActiveLocomotionMontage = nullptr;
		TransitionTo(LocomotionState, true);
		return;
	}

	const UAnimSequence* AnimationSequence = Cast<UAnimSequence>(NewAnimation);
	if (AnimationSequence && AnimationSequence->bEnableRootMotion)
	{
		UE_LOG(
			LogProjectRequiem,
			Error,
			TEXT("Combat animation %s has root motion enabled; movement remains protected by IgnoreRootMotion"),
			*NewAnimation->GetPathName());
	}

	float BlendTime = 0.08f;
	if (NewState == ERequiemCombatAnimationState::Idle)
	{
		BlendTime = 0.1f;
	}
	else if (NewState == ERequiemCombatAnimationState::Attack)
	{
		BlendTime = 0.06f;
	}
	else if (NewState == ERequiemCombatAnimationState::Recovery)
	{
		BlendTime = 0.03f;
	}

	ActiveAnimation = NewAnimation;
	ActiveLocomotionMontage = PlaySlotAnimationAsDynamicMontage(
		NewAnimation,
		LocomotionSlotName,
		BlendTime,
		BlendTime,
		1.0f,
		bLooping ? LoopingMontageCount : 1,
		// The combat state machine owns one-shot handoffs. Prevent the montage from
		// fading back to the locomotion source pose before the next clip starts.
		bLooping ? -1.0f : 0.0f);
}

bool URequiemPlayerAnimInstance::ShouldUseCombatIdle() const
{
	return ObservedCombatState == ERequiemCombatState::CombatUnarmed
		&& CanPlayCombatStanceTransition()
		&& !HasMovementIntent()
		&& ObservedGroundSpeed < DirectionalLoopMinimumSpeed;
}

bool URequiemPlayerAnimInstance::CanPlayCombatStanceTransition() const
{
	if (bObservedIsFalling || bObservedIsCrouched)
	{
		return false;
	}

	switch (LocomotionState)
	{
	case ERequiemLocomotionState::JumpStart:
	case ERequiemLocomotionState::JumpLoop:
	case ERequiemLocomotionState::JumpLand:
	case ERequiemLocomotionState::CrouchEnter:
	case ERequiemLocomotionState::CrouchLoop:
	case ERequiemLocomotionState::CrouchExit:
		return false;
	default:
		return true;
	}
}

bool URequiemPlayerAnimInstance::CanStartUnarmedAttack() const
{
	return ObservedCombatState == ERequiemCombatState::CombatUnarmed
		&& CanPlayCombatStanceTransition();
}

bool URequiemPlayerAnimInstance::HasCombatOneShotFinished() const
{
	if (!ActiveAnimation)
	{
		return true;
	}

	const float Duration = ActiveAnimation->GetPlayLength();
	return CombatAnimationElapsedSeconds >= FMath::Max(0.0f, Duration - CombatOneShotLeadTime);
}

UAnimSequenceBase* URequiemPlayerAnimInstance::GetComboAnimation(const int32 ComboIndex) const
{
	switch (ComboIndex)
	{
	case 0:
		return PunchCrossAnimation;
	case 1:
		return PunchJabAnimation;
	case 2:
		return MeleeKneeAnimation;
	case 3:
		return MeleeKneeRecoveryAnimation;
	case 4:
		return MeleeHookAnimation;
	case 5:
		return MeleeHookRecoveryAnimation;
	case 6:
		return MeleeUppercutAnimation;
	default:
		return nullptr;
	}
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
		ActiveLocomotionAnimation = nullptr;
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
	ActiveLocomotionAnimation = NewAnimation;
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
