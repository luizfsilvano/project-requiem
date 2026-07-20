// Copyright Project Requiem. All Rights Reserved.

#pragma once

#include "Commandlets/Commandlet.h"
#include "CoreMinimal.h"
#include "RequiemConfigureLayeredRecoveryCommandlet.generated.h"

/** Reproducibly configures the player's presentation-only layered sword recovery graph. */
UCLASS()
class URequiemConfigureLayeredRecoveryCommandlet final : public UCommandlet
{
	GENERATED_BODY()

public:
	URequiemConfigureLayeredRecoveryCommandlet();
	virtual int32 Main(const FString& Params) override;
};
