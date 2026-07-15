// Copyright Project Requiem. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "RequiemCharacter.generated.h"

class UCameraComponent;
class UInputAction;
class URequiemCombatComponent;
class URequiemDodgeComponent;
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

	UFUNCTION(BlueprintPure, Category = "Combat")
	URequiemCombatComponent* GetCombatComponent() const { return CombatComponent; }

	UFUNCTION(BlueprintPure, Category = "Dodge")
	URequiemDodgeComponent* GetDodgeComponent() const { return DodgeComponent; }

	/** Generic UE damage fallback; future attack resolution should query the component directly. */
	UFUNCTION(BlueprintPure, Category = "Dodge")
	bool IsDodgeInvulnerable() const;

protected:
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
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

	FVector CurrentMovementInputDirection = FVector::ZeroVector;
};
