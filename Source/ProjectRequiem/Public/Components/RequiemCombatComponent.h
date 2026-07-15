// Copyright Project Requiem. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "RequiemCombatComponent.generated.h"

UENUM(BlueprintType)
enum class ERequiemCombatState : uint8
{
	Normal,
	CombatUnarmed
};

/** Source that requested combat mode. Lock-on and damage remain future callers. */
UENUM(BlueprintType)
enum class ERequiemCombatEntryReason : uint8
{
	Manual,
	Attack,
	LockOn,
	ReceivedDamage
};

/**
 * Minimal gameplay contract for the player's current combat mode.
 * Animation observes this component; it does not own movement or combat state.
 */
UCLASS(ClassGroup = (Combat), BlueprintType, meta = (BlueprintSpawnableComponent))
class PROJECTREQUIEM_API URequiemCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URequiemCombatComponent();

	UFUNCTION(BlueprintPure, Category = "Combat")
	ERequiemCombatState GetCombatState() const { return CombatState; }

	UFUNCTION(BlueprintPure, Category = "Combat")
	FName GetCombatStateName() const;

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void ToggleUnarmedCombat();

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void RequestUnarmedAttack();

	/** Shared entry point for manual input and future lock-on or damage systems. */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void EnterUnarmedCombat(ERequiemCombatEntryReason EntryReason);

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void ExitCombat();

	/** Monotonic request revision observed by presentation code so clicks are not lost. */
	UFUNCTION(BlueprintPure, Category = "Combat")
	int32 GetAttackRequestSerial() const { return AttackRequestSerial; }

	/** Unarmed combat has no blocking capability in this stage. */
	UFUNCTION(BlueprintPure, Category = "Combat")
	bool CanBlock() const { return false; }

	/**
	 * Future auto-exit eligibility only. Nothing polls or exits automatically yet,
	 * and enemy proximity remains an input supplied by a future system.
	 */
	UFUNCTION(BlueprintPure, Category = "Combat")
	bool CanAutoExit(bool bEnemyNearby) const;

	UFUNCTION(BlueprintPure, Category = "Combat")
	float GetAutoExitDelay() const { return AutoExitDelay; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Combat")
	ERequiemCombatState CombatState = ERequiemCombatState::Normal;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Tuning", meta = (ClampMin = "0.0"))
	float AutoExitDelay = 30.0f;

private:
	void MarkCombatActivity();

	int32 AttackRequestSerial = 0;
	float LastCombatActivityTimeSeconds = -1.0f;
};
