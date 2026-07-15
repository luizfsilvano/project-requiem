// Copyright Project Requiem. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "RequiemCharacter.generated.h"

class UCameraComponent;
class UInputAction;
class URequiemCombatComponent;
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

	UFUNCTION(BlueprintPure, Category = "Combat")
	URequiemCombatComponent* GetCombatComponent() const { return CombatComponent; }

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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> JumpAction;

	/** Hold input: Started crouches; Completed/Canceled uncrouches. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> CrouchAction;

	/** Reserved for the future dodge pass. No roll behavior is bound in this stage. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> RollAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> ToggleCombatAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> PrimaryAttackAction;

private:
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void StartCrouch();
	void StopCrouch();
	void ToggleCombat();
	void PrimaryAttack();
};
