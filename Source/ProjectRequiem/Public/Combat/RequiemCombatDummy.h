// Copyright Project Requiem. All Rights Reserved.

#pragma once

#include "Components/RequiemHealthComponent.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "RequiemCombatDummy.generated.h"

class ARequiemCharacter;
class UCapsuleComponent;
class UStaticMeshComponent;

UENUM(BlueprintType)
enum class ERequiemCombatDummyState : uint8
{
	Alive,
	Reacting,
	Defeated
};

/**
 * Small test target for the current unarmed combat loop.
 *
 * The Fab mesh is assigned by a ProjectRequiem Blueprint and remains presentation-only.
 * This actor owns the temporary target health, collision and deterministic test attack.
 */
UCLASS(Blueprintable)
class PROJECTREQUIEM_API ARequiemCombatDummy : public AActor
{
	GENERATED_BODY()

public:
	ARequiemCombatDummy();

	virtual float TakeDamage(
		float DamageAmount,
		struct FDamageEvent const& DamageEvent,
		class AController* EventInstigator,
		AActor* DamageCauser) override;

	UFUNCTION(BlueprintCallable, Category = "Combat Dummy|Damage")
	ERequiemDamageOutcome ApplyRequiemDamage(const FRequiemDamageRequest& Request);

	/** Starts one telegraphed, stationary attack. It never polls, navigates or selects targets. */
	UFUNCTION(BlueprintCallable, Category = "Combat Dummy|Testing", meta = (DevelopmentOnly))
	bool RequestTestAttack(ARequiemCharacter* Target, float WindupOverrideSeconds = -1.0f);

	UFUNCTION(BlueprintCallable, Category = "Combat Dummy|Testing", meta = (DevelopmentOnly))
	void ResetForTesting();

	UFUNCTION(BlueprintPure, Category = "Combat Dummy|Components")
	UCapsuleComponent* GetHitCollision() const { return HitCollision; }

	UFUNCTION(BlueprintPure, Category = "Combat Dummy|Components")
	UStaticMeshComponent* GetVisualMesh() const { return VisualMesh; }

	UFUNCTION(BlueprintPure, Category = "Combat Dummy|Health")
	float GetMaxHealth() const { return MaxHealth; }

	UFUNCTION(BlueprintPure, Category = "Combat Dummy|Health")
	float GetCurrentHealth() const { return CurrentHealth; }

	UFUNCTION(BlueprintPure, Category = "Combat Dummy|Health")
	ERequiemCombatDummyState GetDummyState() const { return DummyState; }

	UFUNCTION(BlueprintPure, Category = "Combat Dummy|Health")
	bool IsDefeated() const { return DummyState == ERequiemCombatDummyState::Defeated; }

	UFUNCTION(BlueprintPure, Category = "Combat Dummy|Feedback")
	bool IsHitFeedbackActive() const { return bHitFeedbackActive; }

	UFUNCTION(BlueprintPure, Category = "Combat Dummy|Damage")
	float GetLastAppliedDamage() const { return LastAppliedDamage; }

	UFUNCTION(BlueprintPure, Category = "Combat Dummy|Damage")
	ERequiemDamageStrength GetLastDamageStrength() const { return LastDamageStrength; }

	UFUNCTION(BlueprintPure, Category = "Combat Dummy|Diagnostics")
	int32 GetDamageSerial() const { return DamageSerial; }

	UFUNCTION(BlueprintPure, Category = "Combat Dummy|Diagnostics")
	int32 GetHitFeedbackSerial() const { return HitFeedbackSerial; }

	UFUNCTION(BlueprintPure, Category = "Combat Dummy|Diagnostics")
	int32 GetDefeatSerial() const { return DefeatSerial; }

	UFUNCTION(BlueprintPure, Category = "Combat Dummy|Diagnostics")
	int32 GetResetSerial() const { return ResetSerial; }

	UFUNCTION(BlueprintPure, Category = "Combat Dummy|Testing")
	bool IsTestAttackPending() const { return bTestAttackPending; }

	UFUNCTION(BlueprintPure, Category = "Combat Dummy|Testing")
	int32 GetTestAttackRequestSerial() const { return TestAttackRequestSerial; }

	UFUNCTION(BlueprintPure, Category = "Combat Dummy|Testing")
	int32 GetTestAttackResolveSerial() const { return TestAttackResolveSerial; }

	UFUNCTION(BlueprintPure, Category = "Combat Dummy|Testing")
	ERequiemDamageOutcome GetLastTestAttackOutcome() const { return LastTestAttackOutcome; }

	UFUNCTION(BlueprintPure, Category = "Combat Dummy|Testing")
	float GetTestAttackDamage() const { return TestAttackDamage; }

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat Dummy|Components")
	TObjectPtr<UCapsuleComponent> HitCollision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat Dummy|Components")
	TObjectPtr<UStaticMeshComponent> VisualMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Dummy|Health", meta = (ClampMin = "1.0"))
	float MaxHealth = 100.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Dummy|Damage", meta = (ClampMin = "0.0"))
	float StrongDamageThreshold = 35.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Dummy|Feedback", meta = (ClampMin = "0.05"))
	float HitFeedbackDurationSeconds = 0.18f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Dummy|Feedback")
	float HitFeedbackTiltDegrees = 8.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Dummy|Feedback")
	float DefeatedTiltDegrees = 78.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Dummy|Test Attack", meta = (ClampMin = "0.0"))
	float TestAttackDamage = 20.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Dummy|Test Attack", meta = (ClampMin = "0.0"))
	float TestAttackWindupSeconds = 0.45f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Dummy|Test Attack", meta = (ClampMin = "0.0"))
	float TestAttackRange = 185.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Dummy|Test Attack", meta = (ClampMin = "0.0"))
	float TestAttackRadius = 65.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Dummy|Test Attack")
	float TestAttackHeight = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Dummy|Test Attack")
	ERequiemHitRegion TestAttackHitRegion = ERequiemHitRegion::Chest;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Dummy|Test Attack")
	ERequiemDamageStrength TestAttackStrength = ERequiemDamageStrength::Light;

	UFUNCTION(BlueprintImplementableEvent, Category = "Combat Dummy|Presentation")
	void OnHitFeedback(float AppliedDamage, ERequiemDamageStrength DamageStrength);

	UFUNCTION(BlueprintImplementableEvent, Category = "Combat Dummy|Presentation")
	void OnDefeated();

	UFUNCTION(BlueprintImplementableEvent, Category = "Combat Dummy|Presentation")
	void OnResetForTesting();

	UFUNCTION(BlueprintImplementableEvent, Category = "Combat Dummy|Presentation")
	void OnTestAttackTelegraphed(ARequiemCharacter* Target, float WindupSeconds);

	UFUNCTION(BlueprintImplementableEvent, Category = "Combat Dummy|Presentation")
	void OnTestAttackResolved(ERequiemDamageOutcome Outcome);

private:
	static int32 AdvanceSerial(int32 Serial);
	void BeginHitFeedback();
	void EnterDefeated();
	void ResolveTestAttack();
	void CancelPendingTestAttack();
	void RestoreVisualTransform();

	FTransform RestingVisualRelativeTransform = FTransform::Identity;
	TWeakObjectPtr<ARequiemCharacter> PendingTestAttackTarget;
	FTimerHandle TestAttackTimerHandle;
	ERequiemCombatDummyState DummyState = ERequiemCombatDummyState::Alive;
	ERequiemDamageStrength LastDamageStrength = ERequiemDamageStrength::Light;
	ERequiemDamageOutcome LastTestAttackOutcome = ERequiemDamageOutcome::IgnoredInvalid;
	FVector LastImpactDirection = FVector::ZeroVector;
	float CurrentHealth = 100.0f;
	float LastAppliedDamage = 0.0f;
	float HitFeedbackElapsedSeconds = 0.0f;
	int32 DamageSerial = 0;
	int32 HitFeedbackSerial = 0;
	int32 DefeatSerial = 0;
	int32 ResetSerial = 0;
	int32 TestAttackRequestSerial = 0;
	int32 TestAttackResolveSerial = 0;
	bool bHitFeedbackActive = false;
	bool bTestAttackPending = false;
	bool bHasCapturedVisualTransform = false;
};
