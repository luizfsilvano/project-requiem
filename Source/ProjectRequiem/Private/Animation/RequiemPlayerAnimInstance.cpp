// Copyright Project Requiem. All Rights Reserved.

#include "Animation/RequiemPlayerAnimInstance.h"

#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSequenceBase.h"
#include "Characters/RequiemCharacter.h"
#include "Components/RequiemDodgeComponent.h"
#include "Components/RequiemHealthComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ProjectRequiem.h"

const FName URequiemPlayerAnimInstance::LocomotionSlotName(TEXT("DefaultSlot"));

namespace
{
// Dynamic montages require a finite loop count. Ten thousand cycles is effectively
// continuous for these states without risking overflow in montage length calculations.
constexpr int32 LoopingMontageCount = 10000;
constexpr float OneShotLeadTime = 0.04f;
constexpr float CombatOneShotLeadTime = 0.015f;
constexpr float MinimumLoopPlayRate = 0.35f;
constexpr float MaximumLoopPlayRate = 1.2f;
constexpr float CombatStanceMaximumSpeed = 1.0f;

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
	case ERequiemLocomotionState::Dodge:
		return TEXT("Dodge");
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
	DamageAnimationState = ERequiemDamageAnimationState::Inactive;
	ActiveComboAnimationIndex = INDEX_NONE;
	ActiveLocomotionMontage = nullptr;
	ActiveAnimation = nullptr;
	ActiveLocomotionAnimation = nullptr;
	StateElapsedSeconds = 0.0f;
	CombatAnimationElapsedSeconds = 0.0f;
	DamageAnimationElapsedSeconds = 0.0f;
	ActiveAnimationPlayRate = 1.0f;
	bNeedsInitialState = true;
	bEnterQueued = false;
	bExitQueued = false;
	bCombatStanceEstablished = false;
	bCombatAssetsInvalid = false;
	bDodgePresentationActive = false;
	bDodgeLocomotionRecoveryPresentationActive = false;
	bDeathPoseHeld = false;
	bDamageAssetsInvalid = false;
	LastObservedDamageRequestSerial =
		HealthComponent ? HealthComponent->GetDamageRequestSerial() : 0;
	LastObservedDeathSerial = HealthComponent ? HealthComponent->GetDeathSerial() : 0;
	LastObservedResetSerial = HealthComponent ? HealthComponent->GetResetSerial() : 0;
	if (CombatComponent)
	{
		CombatComponent->EndUnarmedAttackSequence();
	}
	ScheduleNextLookAround();
}

void URequiemPlayerAnimInstance::NativeUpdateAnimation(const float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (!OwningCharacter
		|| !OwningMovementComponent
		|| !CombatComponent
		|| !DodgeComponent
		|| !HealthComponent)
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
	DamageAnimationElapsedSeconds += DeltaSeconds;

	if (bNeedsInitialState)
	{
		bNeedsInitialState = false;
		TransitionTo(ERequiemLocomotionState::Idle, true);
	}

	HandleDamageReset();

	if (HealthComponent && HealthComponent->IsDead())
	{
		if (bDodgePresentationActive)
		{
			FinishDodgePresentation();
		}
		UpdateDamagePresentation();
		return;
	}

	// A committed dodge keeps gameplay priority until its clock ends. Once authored
	// root displacement is complete, held movement may hand the visual slot to Jog;
	// combat, jump, crouch and another dodge remain blocked by gameplay state.
	if (DodgeComponent && DodgeComponent->IsDodgeActive())
	{
		if (!bDodgePresentationActive)
		{
			StartDodgePresentation();
		}
		else if (bDodgeLocomotionRecoveryPresentationActive)
		{
			UpdateDodgeLocomotionRecoveryPresentation();
		}
		else
		{
			UpdateDodgePresentation();
		}
		return;
	}

	if (bDodgePresentationActive)
	{
		FinishDodgePresentation();
	}

	if (HealthComponent && HealthComponent->IsDamageReactionActive())
	{
		UpdateDamagePresentation();
		return;
	}
	if (DamageAnimationState == ERequiemDamageAnimationState::HitReaction
		|| DamageAnimationState == ERequiemDamageAnimationState::Knockback)
	{
		FinishHitReactionPresentation();
	}

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
		UpdateJogPlayRate();
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
	DodgeComponent = OwningCharacter ? OwningCharacter->GetDodgeComponent() : nullptr;
	HealthComponent = OwningCharacter ? OwningCharacter->GetHealthComponent() : nullptr;
}

void URequiemPlayerAnimInstance::HandleDamageReset()
{
	if (!HealthComponent
		|| LastObservedResetSerial == HealthComponent->GetResetSerial())
	{
		return;
	}

	LastObservedResetSerial = HealthComponent->GetResetSerial();
	LastObservedDamageRequestSerial = HealthComponent->GetDamageRequestSerial();
	LastObservedDeathSerial = HealthComponent->GetDeathSerial();
	if (ActiveLocomotionMontage)
	{
		Montage_Stop(0.0f, ActiveLocomotionMontage);
	}

	SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
	DamageAnimationState = ERequiemDamageAnimationState::Inactive;
	CombatAnimationState = ERequiemCombatAnimationState::Inactive;
	ActiveComboAnimationIndex = INDEX_NONE;
	ActiveAnimation = nullptr;
	ActiveLocomotionAnimation = nullptr;
	ActiveLocomotionMontage = nullptr;
	DamageAnimationElapsedSeconds = 0.0f;
	CombatAnimationElapsedSeconds = 0.0f;
	StateElapsedSeconds = 0.0f;
	bDeathPoseHeld = false;
	bDodgePresentationActive = false;
	bDodgeLocomotionRecoveryPresentationActive = false;
	bDamageAssetsInvalid = false;
	bEnterQueued = false;
	bExitQueued = false;
	bCombatStanceEstablished = false;
	ObservedCombatState = CombatComponent
		? CombatComponent->GetCombatState()
		: ERequiemCombatState::Normal;
	LocomotionState = ERequiemLocomotionState::Idle;
	MovementDirection = ERequiemMovementDirection::None;
	TransitionTo(ERequiemLocomotionState::Idle, true);
}

void URequiemPlayerAnimInstance::UpdateDamagePresentation()
{
	if (!HealthComponent || bDamageAssetsInvalid)
	{
		return;
	}

	if (HealthComponent->IsDead())
	{
		const bool bNeedsDeathPresentation =
			DamageAnimationState != ERequiemDamageAnimationState::Death
			|| LastObservedDeathSerial != HealthComponent->GetDeathSerial();
		if (bNeedsDeathPresentation)
		{
			StartDeathPresentation();
		}
		else if (!bDeathPoseHeld)
		{
			HoldDeathFinalPose();
		}
		return;
	}

	if (!HealthComponent->IsDamageReactionActive())
	{
		if (DamageAnimationState == ERequiemDamageAnimationState::HitReaction
			|| DamageAnimationState == ERequiemDamageAnimationState::Knockback)
		{
			FinishHitReactionPresentation();
		}
		return;
	}

	if (DamageAnimationState == ERequiemDamageAnimationState::Inactive
		|| LastObservedDamageRequestSerial
			!= HealthComponent->GetDamageRequestSerial())
	{
		StartHitReactionPresentation();
		return;
	}

	if (ActiveAnimation)
	{
		const float DurationSeconds =
			ActiveAnimation->GetPlayLength()
			/ FMath::Max(ActiveAnimationPlayRate, 0.1f);
		HealthComponent->SynchronizeDamageReactionPresentation(
			DurationSeconds,
			GetDamageAnimationNormalizedTime());
	}

	if (HasDamageOneShotFinished())
	{
		FinishHitReactionPresentation();
	}
}

void URequiemPlayerAnimInstance::StartHitReactionPresentation()
{
	if (!HealthComponent || !HealthComponent->IsDamageReactionActive())
	{
		return;
	}

	LastObservedDamageRequestSerial = HealthComponent->GetDamageRequestSerial();
	const bool bStrong =
		HealthComponent->GetLastDamageStrength() == ERequiemDamageStrength::Strong;
	UAnimSequenceBase* DamageAnimation =
		bStrong ? HitKnockbackAnimation.Get() : GetHitReactionAnimation();
	const float PlayRate = bStrong
		? FMath::Max(KnockbackPlayRate, 0.1f)
		: FMath::Max(HitReactionPlayRate, 0.1f);
	HealthComponent->BeginDamageReactionPresentation(
		DamageAnimation
			? DamageAnimation->GetPlayLength() / PlayRate
			: 0.0f);
	PlayDamageAnimation(
		bStrong
			? ERequiemDamageAnimationState::Knockback
			: ERequiemDamageAnimationState::HitReaction,
		DamageAnimation,
		bStrong);
}

void URequiemPlayerAnimInstance::StartDeathPresentation()
{
	if (!HealthComponent || !HealthComponent->IsDead())
	{
		return;
	}

	LastObservedDeathSerial = HealthComponent->GetDeathSerial();
	PlayDamageAnimation(
		ERequiemDamageAnimationState::Death,
		GetDeathAnimation(),
		false);
}

void URequiemPlayerAnimInstance::FinishHitReactionPresentation()
{
	if (ActiveLocomotionMontage)
	{
		Montage_Stop(0.06f, ActiveLocomotionMontage);
	}
	SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
	DamageAnimationState = ERequiemDamageAnimationState::Inactive;
	ActiveAnimation = nullptr;
	ActiveLocomotionAnimation = nullptr;
	ActiveLocomotionMontage = nullptr;
	DamageAnimationElapsedSeconds = 0.0f;
	bDeathPoseHeld = false;
	if (HealthComponent)
	{
		HealthComponent->FinishDamageReactionPresentation();
	}
}

void URequiemPlayerAnimInstance::PlayDamageAnimation(
	const ERequiemDamageAnimationState NewState,
	UAnimSequenceBase* NewAnimation,
	const bool bUsesRootMotion)
{
	if (CombatComponent)
	{
		CombatComponent->CancelUnarmedAttackForExternalReaction();
	}
	if (ActiveLocomotionMontage)
	{
		Montage_Stop(0.03f, ActiveLocomotionMontage);
	}

	CombatAnimationState = ERequiemCombatAnimationState::Inactive;
	ActiveComboAnimationIndex = INDEX_NONE;
	CombatAnimationElapsedSeconds = 0.0f;
	const bool bShouldRequeueCombatEnter =
		NewState != ERequiemDamageAnimationState::Death
		&& ObservedCombatState == ERequiemCombatState::CombatUnarmed
		&& !bCombatStanceEstablished;
	bEnterQueued = bShouldRequeueCombatEnter;
	bExitQueued = false;
	DamageAnimationState = NewState;
	DamageAnimationElapsedSeconds = 0.0f;
	bDeathPoseHeld = false;
	ActiveAnimationPlayRate = NewState == ERequiemDamageAnimationState::Knockback
		? FMath::Max(KnockbackPlayRate, 0.1f)
		: NewState == ERequiemDamageAnimationState::HitReaction
			? FMath::Max(HitReactionPlayRate, 0.1f)
			: 1.0f;

	const UAnimSequence* AnimationSequence = Cast<UAnimSequence>(NewAnimation);
	const bool bValidRootMotion =
		AnimationSequence && AnimationSequence->bEnableRootMotion;
	if (!NewAnimation
		|| !AnimationSequence
		|| (bUsesRootMotion && !bValidRootMotion)
		|| (!bUsesRootMotion && bValidRootMotion))
	{
		UE_LOG(
			LogProjectRequiem,
			Error,
			TEXT("Invalid damage animation for state %d (asset=%s, root_motion=%d)"),
			static_cast<int32>(NewState),
			NewAnimation ? *NewAnimation->GetPathName() : TEXT("None"),
			bValidRootMotion);
		bDamageAssetsInvalid = true;
		DamageAnimationState = ERequiemDamageAnimationState::Inactive;
		ActiveAnimation = nullptr;
		ActiveLocomotionAnimation = nullptr;
		ActiveLocomotionMontage = nullptr;
		SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
		if (HealthComponent && !HealthComponent->IsDead())
		{
			HealthComponent->FinishDamageReactionPresentation();
		}
		return;
	}

	ActiveAnimation = NewAnimation;
	ActiveLocomotionAnimation = nullptr;
	SetRootMotionMode(bUsesRootMotion
		? ERootMotionMode::RootMotionFromMontagesOnly
		: ERootMotionMode::IgnoreRootMotion);
	ActiveLocomotionMontage = PlaySlotAnimationAsDynamicMontage(
		NewAnimation,
		LocomotionSlotName,
		0.05f,
		NewState == ERequiemDamageAnimationState::Death ? 0.0f : 0.08f,
		ActiveAnimationPlayRate,
		1,
		0.0f);

	if (!ActiveLocomotionMontage)
	{
		UE_LOG(LogProjectRequiem, Error, TEXT("Failed to start damage dynamic montage"));
		bDamageAssetsInvalid = true;
		DamageAnimationState = ERequiemDamageAnimationState::Inactive;
		SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
		if (HealthComponent && !HealthComponent->IsDead())
		{
			HealthComponent->FinishDamageReactionPresentation();
		}
	}
}

void URequiemPlayerAnimInstance::HoldDeathFinalPose()
{
	if (DamageAnimationState != ERequiemDamageAnimationState::Death
		|| !ActiveAnimation
		|| !ActiveLocomotionMontage)
	{
		return;
	}

	const float Duration = ActiveAnimation->GetPlayLength();
	if (Duration <= UE_KINDA_SMALL_NUMBER)
	{
		return;
	}

	const float HoldPosition = FMath::Max(0.0f, Duration - 1.0f / 30.0f);
	if (GetDamageAnimationNormalizedTime() * Duration < HoldPosition)
	{
		return;
	}

	if (!Montage_IsActive(ActiveLocomotionMontage))
	{
		ActiveLocomotionMontage = PlaySlotAnimationAsDynamicMontage(
			ActiveAnimation,
			LocomotionSlotName,
			0.0f,
			0.0f,
			1.0f,
			1,
			0.0f,
			HoldPosition);
	}
	if (!ActiveLocomotionMontage || !Montage_IsActive(ActiveLocomotionMontage))
	{
		return;
	}

	Montage_SetPosition(ActiveLocomotionMontage, HoldPosition);
	Montage_Pause(ActiveLocomotionMontage);
	bDeathPoseHeld = Montage_IsActive(ActiveLocomotionMontage);
}

bool URequiemPlayerAnimInstance::HasDamageOneShotFinished() const
{
	if (!ActiveAnimation)
	{
		return true;
	}

	const float Duration = ActiveAnimation->GetPlayLength();
	if (Duration <= UE_KINDA_SMALL_NUMBER)
	{
		return true;
	}

	return GetDamageAnimationNormalizedTime() >= 0.999f;
}

UAnimSequenceBase* URequiemPlayerAnimInstance::GetHitReactionAnimation() const
{
	if (!HealthComponent)
	{
		return nullptr;
	}

	switch (HealthComponent->GetLastHitRegion())
	{
	case ERequiemHitRegion::Head:
		return HitHeadAnimation;
	case ERequiemHitRegion::Chest:
		return HitChestAnimation;
	case ERequiemHitRegion::Stomach:
		return HitStomachAnimation;
	case ERequiemHitRegion::ShoulderLeft:
		return HitShoulderLeftAnimation;
	case ERequiemHitRegion::ShoulderRight:
		return HitShoulderRightAnimation;
	default:
		return HitChestAnimation;
	}
}

UAnimSequenceBase* URequiemPlayerAnimInstance::GetDeathAnimation() const
{
	return HealthComponent
			&& HealthComponent->GetSelectedDeathAnimation()
				== ERequiemDeathAnimation::Death02
		? Death02Animation.Get()
		: Death01Animation.Get();
}

float URequiemPlayerAnimInstance::GetDamageAnimationNormalizedTime() const
{
	if (!ActiveAnimation)
	{
		return 0.0f;
	}

	const float Duration = ActiveAnimation->GetPlayLength();
	if (Duration <= UE_KINDA_SMALL_NUMBER)
	{
		return 1.0f;
	}

	if (ActiveLocomotionMontage && Montage_IsActive(ActiveLocomotionMontage))
	{
		return FMath::Clamp(
			Montage_GetPosition(ActiveLocomotionMontage) / Duration,
			0.0f,
			1.0f);
	}

	return FMath::Clamp(
		DamageAnimationElapsedSeconds * ActiveAnimationPlayRate / Duration,
		0.0f,
		1.0f);
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

void URequiemPlayerAnimInstance::StartDodgePresentation()
{
	if (!DodgeComponent || !DodgeComponent->IsDodgeActive())
	{
		return;
	}

	const UAnimSequence* RollSequence = Cast<UAnimSequence>(RollAnimation);
	if (!RollAnimation || !RollSequence || !RollSequence->bEnableRootMotion)
	{
		UE_LOG(
			LogProjectRequiem,
			Error,
			TEXT("Dodge requires a root-motion AnimSequence in RollAnimation"));
		DodgeComponent->FinishDodge();
		return;
	}

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
		// Input validation prevents this path. Keep the already committed combo if
		// an external caller ever violates that contract.
		DodgeComponent->FinishDodge();
		return;
	}

	if (CombatComponent)
	{
		CombatComponent->SetUnarmedAttackInputWindowOpen(false);
	}
	if (ActiveLocomotionMontage)
	{
		Montage_Stop(0.0f, ActiveLocomotionMontage);
	}

	CombatAnimationState = ERequiemCombatAnimationState::Inactive;
	ActiveComboAnimationIndex = INDEX_NONE;
	CombatAnimationElapsedSeconds = 0.0f;
	bDodgeLocomotionRecoveryPresentationActive = false;
	LocomotionState = ERequiemLocomotionState::Dodge;
	MovementDirection = ERequiemMovementDirection::None;
	StateElapsedSeconds = 0.0f;
	ActiveAnimationPlayRate = FMath::Max(DodgePlayRate, 0.1f);
	ActiveAnimation = RollAnimation;
	ActiveLocomotionAnimation = RollAnimation;
	SetRootMotionMode(ERootMotionMode::RootMotionFromMontagesOnly);
	ActiveLocomotionMontage = PlaySlotAnimationAsDynamicMontage(
		RollAnimation,
		LocomotionSlotName,
		0.04f,
		0.08f,
		ActiveAnimationPlayRate,
		1,
		0.0f);

	if (!ActiveLocomotionMontage)
	{
		UE_LOG(LogProjectRequiem, Error, TEXT("Failed to start the Roll dynamic montage"));
		DodgeComponent->FinishDodge();
		FinishDodgePresentation();
		return;
	}

	bDodgePresentationActive = true;
	DodgeComponent->SynchronizeDodgePresentation(
		RollAnimation->GetPlayLength() / ActiveAnimationPlayRate,
		0.0f);
}

void URequiemPlayerAnimInstance::UpdateDodgePresentation()
{
	if (!DodgeComponent || !RollAnimation)
	{
		return;
	}

	const float PlayRate = FMath::Max(DodgePlayRate, 0.1f);
	const float AnimationLength = RollAnimation->GetPlayLength();
	const float DurationSeconds = AnimationLength / PlayRate;
	const bool bMontagePlaying = ActiveLocomotionMontage
		&& Montage_IsPlaying(ActiveLocomotionMontage);
	if (bMontagePlaying)
	{
		Montage_SetPlayRate(ActiveLocomotionMontage, PlayRate);
		const float MontageNormalizedTime = AnimationLength > UE_KINDA_SMALL_NUMBER
			? Montage_GetPosition(ActiveLocomotionMontage) / AnimationLength
			: 1.0f;
		DodgeComponent->SynchronizeDodgePresentation(
			DurationSeconds,
			MontageNormalizedTime);
	}
	else if (DodgeComponent->GetDodgeNormalizedTime() < 0.95f)
	{
		// No gameplay montage is allowed to interrupt Roll. If an external
		// presentation stops it unexpectedly, resume from the committed clock.
		const float ResumeNormalizedTime = DodgeComponent->GetDodgeNormalizedTime();
		UE_LOG(
			LogProjectRequiem,
			Warning,
			TEXT("Roll montage stopped early at %.3f; resuming committed dodge"),
			ResumeNormalizedTime);
		ActiveLocomotionMontage = PlaySlotAnimationAsDynamicMontage(
			RollAnimation,
			LocomotionSlotName,
			0.0f,
			0.08f,
			PlayRate,
			1,
			0.0f,
			FMath::Clamp(ResumeNormalizedTime * AnimationLength, 0.0f, AnimationLength));
	}
	else
	{
		// The authored one-shot has completed; do not leave gameplay locks alive
		// for a frame after its presentation is gone.
		DodgeComponent->FinishDodge();
	}

	// Root motion owns only the authored displacement. During the stationary tail,
	// CharacterMovement receives held input and controls acceleration/displacement.
	SetRootMotionMode(DodgeComponent->IsDodgeMovementLocked()
		? ERootMotionMode::RootMotionFromMontagesOnly
		: ERootMotionMode::IgnoreRootMotion);

	if (DodgeComponent->IsDodgeActive()
		&& !DodgeComponent->IsDodgeMovementLocked()
		&& !bObservedIsFalling
		&& !bObservedIsCrouched
		&& HasMovementIntent())
	{
		StartDodgeLocomotionRecoveryPresentation();
	}
}

void URequiemPlayerAnimInstance::StartDodgeLocomotionRecoveryPresentation()
{
	if (!DodgeComponent
		|| !DodgeComponent->IsDodgeActive()
		|| DodgeComponent->IsDodgeMovementLocked()
		|| !HasMovementIntent())
	{
		return;
	}

	// From this point the component's own tick advances the committed action clock.
	// The Jog montage must never be fed back into Roll timing synchronization.
	bDodgeLocomotionRecoveryPresentationActive = true;
	SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
	if (ActiveLocomotionMontage)
	{
		Montage_Stop(FMath::Max(0.0f, DodgeJogHandoffBlendTime), ActiveLocomotionMontage);
	}

	LocomotionState = ERequiemLocomotionState::Jog;
	StateElapsedSeconds = 0.0f;
	ActiveAnimationPlayRate = JogAuthoredSpeed > UE_KINDA_SMALL_NUMBER
		? FMath::Clamp(
			ObservedGroundSpeed / JogAuthoredSpeed,
			MinimumLoopPlayRate,
			MaximumLoopPlayRate)
		: 1.0f;
	PlayStateAnimation();
	UpdateJogPlayRate();
}

void URequiemPlayerAnimInstance::UpdateDodgeLocomotionRecoveryPresentation()
{
	SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
	if (LocomotionState != ERequiemLocomotionState::Jog)
	{
		LocomotionState = ERequiemLocomotionState::Jog;
		StateElapsedSeconds = 0.0f;
		PlayStateAnimation();
	}
	else
	{
		RefreshDirectionalLoop();
	}
	UpdateJogPlayRate();
}

void URequiemPlayerAnimInstance::FinishDodgePresentation()
{
	const bool bKeepActiveJog =
		bDodgeLocomotionRecoveryPresentationActive
		&& LocomotionState == ERequiemLocomotionState::Jog
		&& ActiveLocomotionMontage
		&& Montage_IsPlaying(ActiveLocomotionMontage)
		&& (HasMovementIntent() || ObservedGroundSpeed >= DirectionalLoopMinimumSpeed);

	bDodgePresentationActive = false;
	bDodgeLocomotionRecoveryPresentationActive = false;
	CombatAnimationState = ERequiemCombatAnimationState::Inactive;
	ActiveComboAnimationIndex = INDEX_NONE;
	CombatAnimationElapsedSeconds = 0.0f;
	SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
	if (bKeepActiveJog)
	{
		// Preserve phase and montage ownership across gameplay completion so the
		// 100% boundary cannot introduce an Idle flick or restart the Jog loop.
		StateElapsedSeconds = 0.0f;
		UpdateJogPlayRate();
		return;
	}

	if (ActiveLocomotionMontage)
	{
		Montage_Stop(0.05f, ActiveLocomotionMontage);
	}
	ActiveAnimation = nullptr;
	ActiveLocomotionAnimation = nullptr;
	ActiveLocomotionMontage = nullptr;
	StateElapsedSeconds = 0.0f;

	if (bObservedIsFalling)
	{
		LocomotionState = OwningCharacter && OwningCharacter->GetVelocity().Z > 0.0f
			? ERequiemLocomotionState::JumpStart
			: ERequiemLocomotionState::JumpLoop;
	}
	else if (bObservedIsCrouched)
	{
		LocomotionState = ERequiemLocomotionState::CrouchLoop;
	}
	else
	{
		LocomotionState = HasMovementIntent()
			|| ObservedGroundSpeed >= DirectionalLoopMinimumSpeed
			? ERequiemLocomotionState::Jog
			: ERequiemLocomotionState::Idle;
	}
	PlayStateAnimation();
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
		CombatComponent->EndUnarmedAttackSequence();
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
		if (bCombatStanceEstablished)
		{
			bEnterQueued = false;
			if (CombatAnimationState == ERequiemCombatAnimationState::Exit)
			{
				if (CanPlayCombatStanceTransition())
				{
					StartCombatIdle();
				}
				else
				{
					ResumeLocomotionPresentation();
				}
			}
			return;
		}
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

	// Leaving combat cancels the single buffered follow-up. A Knee/Hook recovery already in
	// progress still finishes before PunchKick_Exit so the pose remains continuous.
	CombatComponent->SetUnarmedAttackInputWindowOpen(false);
	bEnterQueued = false;
	if (!bCombatStanceEstablished
		&& CombatAnimationState != ERequiemCombatAnimationState::Attack
		&& CombatAnimationState != ERequiemCombatAnimationState::Recovery)
	{
		bExitQueued = false;
		if (CombatAnimationState != ERequiemCombatAnimationState::Inactive)
		{
			ResumeLocomotionPresentation();
		}
		return;
	}
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
	// are deferred and an in-progress combo releases its movement commitment.
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
			CombatComponent->EndUnarmedAttackSequence();
		}

		ResumeLocomotionPresentation();
		return;
	}

	// PunchKick_Enter/Exit are full-body posture changes, not locomotion clips.
	// If movement starts while either one is playing, hand presentation back to
	// locomotion immediately and retry the posture change only after a full stop.
	if ((CombatAnimationState == ERequiemCombatAnimationState::Enter
			|| CombatAnimationState == ERequiemCombatAnimationState::Exit)
		&& !CanPlayCombatStanceTransition())
	{
		if (CombatAnimationState == ERequiemCombatAnimationState::Enter)
		{
			bEnterQueued = ObservedCombatState == ERequiemCombatState::CombatUnarmed;
		}
		else
		{
			bExitQueued = ObservedCombatState == ERequiemCombatState::Normal;
		}

		ResumeLocomotionPresentation();
		return;
	}

	UpdateCombatInputWindow();
	UpdateCombatMovementRecovery();

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
			if (!TryStartInitialUnarmedAttack() && ShouldUseCombatIdle())
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
			if (CanPlayCombatStanceTransition())
			{
				StartCombatExit();
			}
			else
			{
				bExitQueued = true;
				ResumeLocomotionPresentation();
			}
			return;
		}
		if (TryStartInitialUnarmedAttack())
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
		if (ShouldAdvanceCombatOneShot())
		{
			HandleFinishedCombatOneShot();
		}
		return;

	default:
		return;
	}
}

void URequiemPlayerAnimInstance::UpdateCombatInputWindow()
{
	if (!CombatComponent)
	{
		return;
	}

	const float NormalizedTime = GetCombatAnimationNormalizedTime();
	const bool bInsideWindow =
		NormalizedTime >= UnarmedInputWindowStartNormalized
		&& NormalizedTime <= UnarmedInputWindowEndNormalized;
	const bool bAttackCanQueue =
		CombatAnimationState == ERequiemCombatAnimationState::Attack
		&& CanQueueFollowUpFromComboIndex(ActiveComboAnimationIndex);
	const bool bRecoveryCanQueue =
		CombatAnimationState == ERequiemCombatAnimationState::Recovery
		&& (ActiveComboAnimationIndex == 3 || ActiveComboAnimationIndex == 5);
	CombatComponent->SetUnarmedAttackInputWindowOpen(
		bInsideWindow && (bAttackCanQueue || bRecoveryCanQueue));
}

void URequiemPlayerAnimInstance::UpdateCombatMovementRecovery()
{
	if (!CombatComponent
		|| CombatAnimationState != ERequiemCombatAnimationState::Attack
		|| !CombatComponent->IsUnarmedAttackMovementLocked())
	{
		return;
	}

	if (GetCombatAnimationNormalizedTime() >= UnarmedMovementUnlockNormalized)
	{
		CombatComponent->ReleaseUnarmedAttackMovementLock();
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
	if (!bCombatStanceEstablished)
	{
		bExitQueued = false;
		if (CombatAnimationState != ERequiemCombatAnimationState::Inactive)
		{
			ResumeLocomotionPresentation();
		}
		return;
	}
	if (!CanPlayCombatStanceTransition())
	{
		bExitQueued = true;
		if (CombatAnimationState != ERequiemCombatAnimationState::Inactive)
		{
			ResumeLocomotionPresentation();
		}
		return;
	}

	bExitQueued = false;
	if (CombatComponent)
	{
		CombatComponent->EndUnarmedAttackSequence();
	}
	PlayCombatAnimation(
		ERequiemCombatAnimationState::Exit,
		CombatExitAnimation,
		false);
}

void URequiemPlayerAnimInstance::StartCombatComboClip(const int32 ComboIndex)
{
	// A direct attack from locomotion is itself a complete combat presentation,
	// so a later return to Normal may still use the authored stance exit.
	bCombatStanceEstablished = true;
	const bool bRecovery = ComboIndex == 3 || ComboIndex == 5;
	if (CombatComponent)
	{
		if (bRecovery)
		{
			CombatComponent->ReleaseUnarmedAttackMovementLock();
		}
		else
		{
			CombatComponent->BeginUnarmedAttackStep(true);
		}
	}
	PlayCombatAnimation(
		bRecovery
			? ERequiemCombatAnimationState::Recovery
			: ERequiemCombatAnimationState::Attack,
		GetComboAnimation(ComboIndex),
		false,
		ComboIndex);
}

bool URequiemPlayerAnimInstance::TryStartInitialUnarmedAttack()
{
	if (!CombatComponent
		|| !CanStartUnarmedAttack()
		|| !CombatComponent->ConsumeInitialUnarmedAttackRequest())
	{
		return false;
	}

	StartCombatComboClip(0);
	return true;
}

bool URequiemPlayerAnimInstance::TryStartQueuedUnarmedFollowUp(const int32 ComboIndex)
{
	if (!CombatComponent
		|| !CanStartUnarmedAttack()
		|| !CombatComponent->ConsumeQueuedUnarmedFollowUp())
	{
		return false;
	}

	StartCombatComboClip(ComboIndex);
	return true;
}

void URequiemPlayerAnimInstance::HandleFinishedCombatOneShot()
{
	const auto ReturnToCombatPosture = [this]()
	{
		if (CombatComponent)
		{
			CombatComponent->EndUnarmedAttackSequence();
		}
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
		bCombatStanceEstablished = true;
		if (ObservedCombatState == ERequiemCombatState::Normal)
		{
			StartCombatExit();
		}
		else if (!TryStartInitialUnarmedAttack())
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
		if (ActiveComboAnimationIndex == 0 && TryStartQueuedUnarmedFollowUp(1))
		{
			return;
		}
		if (ActiveComboAnimationIndex == 1 && TryStartQueuedUnarmedFollowUp(2))
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
		if (ActiveComboAnimationIndex == 3 && TryStartQueuedUnarmedFollowUp(4))
		{
			return;
		}
		if (ActiveComboAnimationIndex == 5 && TryStartQueuedUnarmedFollowUp(6))
		{
			return;
		}
		ReturnToCombatPosture();
		return;

	case ERequiemCombatAnimationState::Exit:
		bCombatStanceEstablished = false;
		ResumeLocomotionPresentation();
		return;

	default:
		return;
	}
}

void URequiemPlayerAnimInstance::ResumeLocomotionPresentation()
{
	if (CombatComponent
		&& (CombatAnimationState == ERequiemCombatAnimationState::Attack
			|| CombatAnimationState == ERequiemCombatAnimationState::Recovery))
	{
		CombatComponent->EndUnarmedAttackSequence();
	}

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
	ActiveAnimationPlayRate = GetCombatPlayRate(NewState);

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
		if (CombatComponent)
		{
			CombatComponent->EndUnarmedAttackSequence();
		}
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
		ActiveAnimationPlayRate,
		bLooping ? LoopingMontageCount : 1,
		// The combat state machine owns one-shot handoffs. Prevent the montage from
		// fading back to the locomotion source pose before the next clip starts.
		bLooping ? -1.0f : 0.0f);
}

bool URequiemPlayerAnimInstance::ShouldUseCombatIdle() const
{
	return ObservedCombatState == ERequiemCombatState::CombatUnarmed
		&& CanPlayCombatStanceTransition();
}

bool URequiemPlayerAnimInstance::CanPlayCombatAnimation() const
{
	if (bDodgePresentationActive
		|| (DodgeComponent && DodgeComponent->IsDodgeActive())
		|| (HealthComponent && HealthComponent->AreActionsLocked())
		|| bObservedIsFalling
		|| bObservedIsCrouched)
	{
		return false;
	}

	switch (LocomotionState)
	{
	case ERequiemLocomotionState::JumpStart:
	case ERequiemLocomotionState::JumpLoop:
	case ERequiemLocomotionState::JumpLand:
	case ERequiemLocomotionState::Dodge:
	case ERequiemLocomotionState::CrouchEnter:
	case ERequiemLocomotionState::CrouchLoop:
	case ERequiemLocomotionState::CrouchExit:
		return false;
	default:
		return true;
	}
}

bool URequiemPlayerAnimInstance::CanPlayCombatStanceTransition() const
{
	return CanPlayCombatAnimation()
		&& !HasMovementIntent()
		&& ObservedGroundSpeed <= CombatStanceMaximumSpeed;
}

bool URequiemPlayerAnimInstance::CanStartUnarmedAttack() const
{
	return ObservedCombatState == ERequiemCombatState::CombatUnarmed
		&& CanPlayCombatAnimation();
}

bool URequiemPlayerAnimInstance::ShouldAdvanceCombatOneShot() const
{
	const float NormalizedTime = GetCombatAnimationNormalizedTime();
	if (CombatAnimationState == ERequiemCombatAnimationState::Attack)
	{
		if ((ActiveComboAnimationIndex == 2 || ActiveComboAnimationIndex == 4)
			&& NormalizedTime >= UnarmedAutomaticRecoveryHandoffNormalized)
		{
			return true;
		}

		if (CombatComponent
			&& CombatComponent->HasQueuedUnarmedFollowUp()
			&& (ActiveComboAnimationIndex == 0 || ActiveComboAnimationIndex == 1)
			&& NormalizedTime >= UnarmedQueuedAttackHandoffNormalized)
		{
			return true;
		}
	}
	else if (CombatAnimationState == ERequiemCombatAnimationState::Recovery
		&& CombatComponent
		&& CombatComponent->HasQueuedUnarmedFollowUp()
		&& NormalizedTime >= UnarmedQueuedRecoveryHandoffNormalized)
	{
		return true;
	}

	return HasCombatOneShotFinished();
}

bool URequiemPlayerAnimInstance::HasCombatOneShotFinished() const
{
	if (!ActiveAnimation)
	{
		return true;
	}

	const float Duration = ActiveAnimation->GetPlayLength();
	if (Duration <= UE_KINDA_SMALL_NUMBER)
	{
		return true;
	}

	const float FinishThreshold = FMath::Max(
		0.0f,
		1.0f - CombatOneShotLeadTime / Duration);
	return GetCombatAnimationNormalizedTime() >= FinishThreshold;
}

float URequiemPlayerAnimInstance::GetCombatAnimationNormalizedTime() const
{
	if (!ActiveAnimation)
	{
		return 0.0f;
	}

	const float Duration = ActiveAnimation->GetPlayLength();
	if (Duration <= UE_KINDA_SMALL_NUMBER)
	{
		return 1.0f;
	}

	if (ActiveLocomotionMontage && Montage_IsPlaying(ActiveLocomotionMontage))
	{
		return FMath::Clamp(
			Montage_GetPosition(ActiveLocomotionMontage) / Duration,
			0.0f,
			1.0f);
	}

	return FMath::Clamp(
		CombatAnimationElapsedSeconds * ActiveAnimationPlayRate / Duration,
		0.0f,
		1.0f);
}

bool URequiemPlayerAnimInstance::CanQueueFollowUpFromComboIndex(const int32 ComboIndex) const
{
	return ComboIndex == 0
		|| ComboIndex == 1
		|| ComboIndex == 2
		|| ComboIndex == 4;
}

float URequiemPlayerAnimInstance::GetCombatPlayRate(
	const ERequiemCombatAnimationState State) const
{
	if (State == ERequiemCombatAnimationState::Attack)
	{
		return FMath::Max(UnarmedAttackPlayRate, 0.1f);
	}
	if (State == ERequiemCombatAnimationState::Recovery)
	{
		return FMath::Max(UnarmedRecoveryPlayRate, 0.1f);
	}
	return 1.0f;
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
		const bool bShouldUseDirectionalCrouch = HasMovementIntent()
			|| ObservedGroundSpeed >= DirectionalLoopMinimumSpeed;
		if (bShouldUseDirectionalCrouch || HasOneShotFinished())
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

void URequiemPlayerAnimInstance::UpdateJogPlayRate()
{
	if (LocomotionState != ERequiemLocomotionState::Jog || !ActiveLocomotionMontage)
	{
		return;
	}

	ActiveAnimationPlayRate = JogAuthoredSpeed > UE_KINDA_SMALL_NUMBER
		? FMath::Clamp(
			ObservedGroundSpeed / JogAuthoredSpeed,
			MinimumLoopPlayRate,
			MaximumLoopPlayRate)
		: 1.0f;
	Montage_SetPlayRate(ActiveLocomotionMontage, ActiveAnimationPlayRate);
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
	if (AnimationSequence
		&& AnimationSequence->bEnableRootMotion
		&& LocomotionState != ERequiemLocomotionState::Dodge)
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
	const bool bMovementLocked =
		(CombatComponent && CombatComponent->IsUnarmedAttackMovementLocked())
		|| (DodgeComponent && DodgeComponent->IsDodgeMovementLocked())
		|| (HealthComponent && HealthComponent->AreActionsLocked());
	return !bMovementLocked
		&& ((OwningMovementComponent
				&& !OwningMovementComponent->GetCurrentAcceleration().IsNearlyZero(1.0f))
			|| (OwningCharacter && OwningCharacter->HasCurrentMovementInput()));
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
	case ERequiemLocomotionState::Jog:
		return bDodgeLocomotionRecoveryPresentationActive
			? FMath::Max(0.0f, DodgeJogHandoffBlendTime)
			: 0.12f;
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
	case ERequiemLocomotionState::Dodge:
		return RollAnimation;
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
