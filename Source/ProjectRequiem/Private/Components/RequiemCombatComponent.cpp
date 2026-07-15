// Copyright Project Requiem. All Rights Reserved.

#include "Components/RequiemCombatComponent.h"

#include "Engine/World.h"

URequiemCombatComponent::URequiemCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

FName URequiemCombatComponent::GetCombatStateName() const
{
	return CombatState == ERequiemCombatState::CombatUnarmed
		? FName(TEXT("CombatUnarmed"))
		: FName(TEXT("Normal"));
}

void URequiemCombatComponent::ToggleUnarmedCombat()
{
	if (CombatState == ERequiemCombatState::CombatUnarmed)
	{
		ExitCombat();
		return;
	}

	EnterUnarmedCombat(ERequiemCombatEntryReason::Manual);
}

void URequiemCombatComponent::RequestUnarmedAttack()
{
	if (CombatState != ERequiemCombatState::CombatUnarmed)
	{
		EnterUnarmedCombat(ERequiemCombatEntryReason::Attack);
	}
	else
	{
		MarkCombatActivity();
	}

	AttackRequestSerial = AttackRequestSerial == MAX_int32
		? 1
		: AttackRequestSerial + 1;
}

void URequiemCombatComponent::EnterUnarmedCombat(const ERequiemCombatEntryReason EntryReason)
{
	if (CombatState == ERequiemCombatState::CombatUnarmed)
	{
		return;
	}

	CombatState = ERequiemCombatState::CombatUnarmed;
	MarkCombatActivity();

	// The reason is intentionally accepted by the stable contract so future
	// lock-on and damage callers do not require a second entry path.
	(void)EntryReason;
}

void URequiemCombatComponent::ExitCombat()
{
	CombatState = ERequiemCombatState::Normal;
}

bool URequiemCombatComponent::CanAutoExit(const bool bEnemyNearby) const
{
	if (CombatState != ERequiemCombatState::CombatUnarmed
		|| bEnemyNearby
		|| LastCombatActivityTimeSeconds < 0.0f)
	{
		return false;
	}

	const UWorld* World = GetWorld();
	return World
		&& World->GetTimeSeconds() - LastCombatActivityTimeSeconds >= AutoExitDelay;
}

void URequiemCombatComponent::MarkCombatActivity()
{
	if (const UWorld* World = GetWorld())
	{
		LastCombatActivityTimeSeconds = World->GetTimeSeconds();
	}
}
