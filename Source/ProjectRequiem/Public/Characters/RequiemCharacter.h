// Copyright Project Requiem. All Rights Reserved.

#pragma once

#include "Components/RequiemHealthComponent.h"
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "RequiemCharacter.generated.h"

class UCameraComponent;
class UDecalComponent;
class UInputAction;
class URequiemCombatComponent;
class URequiemDodgeComponent;
class URequiemHealthComponent;
class URequiemLockOnComponent;
class USpringArmComponent;
struct FInputActionValue;

/**
 * Third-person avatar that owns movement and dispatches combat input to its component.
 * Narrative, interaction and combat rules remain outside the Character.
 */
UCLASS(Blueprintable)
class PROJECTREQUIEM_API ARequiemCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ARequiemCharacter();
	virtual float TakeDamage(
		float DamageAmount,
		struct FDamageEvent const& DamageEvent,
		class AController* EventInstigator,
		AActor* DamageCauser) override;
	virtual bool CanCrouch() const override;

	UFUNCTION(BlueprintPure, Category = "Combat")
	URequiemCombatComponent* GetCombatComponent() const { return CombatComponent; }

	UFUNCTION(BlueprintPure, Category = "Dodge")
	URequiemDodgeComponent* GetDodgeComponent() const { return DodgeComponent; }

	UFUNCTION(BlueprintPure, Category = "Health")
	URequiemHealthComponent* GetHealthComponent() const { return HealthComponent; }

	UFUNCTION(BlueprintPure, Category = "Combat|Lock On")
	URequiemLockOnComponent* GetLockOnComponent() const { return LockOnComponent; }

	UFUNCTION(BlueprintPure, Category = "Combat|Lock On")
	UDecalComponent* GetLockOnGroundIndicator() const { return LockOnGroundIndicator; }

	/** Canonical explicit damage path for future hitboxes and enemies. */
	UFUNCTION(BlueprintCallable, Category = "Damage")
	ERequiemDamageOutcome ApplyRequiemDamage(const FRequiemDamageRequest& Request);

	/** Generic UE damage fallback; future attack resolution should query the component directly. */
	UFUNCTION(BlueprintPure, Category = "Dodge")
	bool IsDodgeInvulnerable() const;

	/** Raw camera-relative movement intent, retained while a committed action owns movement. */
	UFUNCTION(BlueprintPure, Category = "Movement")
	bool HasCurrentMovementInput() const
	{
		return !CurrentMovementInputDirection.IsNearlyZero();
	}

	/** Temporary console hook: RequiemTestDamage Head 20 false. */
	UFUNCTION(Exec)
	void RequiemTestDamage(
		const FString& RegionName = TEXT("Chest"),
		float DamageAmount = 20.0f,
		bool bStrong = false);

	/** Temporary console hook: RequiemTestKill 1 (Death01) or 2 (Death02). */
	UFUNCTION(Exec)
	void RequiemTestKill(int32 DeathVariant = 1);

	/** Temporary console hook for repeatable PIE checks. */
	UFUNCTION(Exec)
	void RequiemTestReset();

	/** Temporary console hook: orders the nearest live combat dummy to attack once. */
	UFUNCTION(Exec)
	void RequiemTestDummyAttack();

	/** Temporary console hook: resets every combat dummy in the current PIE world. */
	UFUNCTION(Exec)
	void RequiemTestDummyReset();

protected:
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual bool CanJumpInternal_Implementation() const override;
	virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> FollowCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	TObjectPtr<URequiemCombatComponent> CombatComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dodge")
	TObjectPtr<URequiemDodgeComponent> DodgeComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Health")
	TObjectPtr<URequiemHealthComponent> HealthComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Lock On")
	TObjectPtr<URequiemLockOnComponent> LockOnComponent;

	/** Ground-ring presentation configured by BP_CH_Player; the lock-on component positions it. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Lock On")
	TObjectPtr<UDecalComponent> LockOnGroundIndicator;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> JumpAction;

	/** Hold input: Started crouches; Completed/Canceled uncrouches. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> CrouchAction;

	/** Committed root-motion dodge on Started. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> RollAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> ToggleCombatAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> PrimaryAttackAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> LockOnAction;

private:
	void Move(const FInputActionValue& Value);
	void StopMove();
	void Look(const FInputActionValue& Value);
	void StartJump();
	void StopJump();
	void StartCrouch();
	void StopCrouch();
	void StartDodge();
	FVector GetCurrentDodgeInputDirection() const;
	void ToggleCombat();
	void PrimaryAttack();
	void ToggleLockOn();
	bool AreDamageActionsLocked() const;

	FVector CurrentMovementInputDirection = FVector::ZeroVector;
};
