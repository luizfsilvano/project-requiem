// Copyright Project Requiem. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "RequiemGameModeBase.generated.h"

/** Minimal framework configuration shared by Project Requiem maps. */
UCLASS(Blueprintable)
class PROJECTREQUIEM_API ARequiemGameModeBase : public AGameModeBase
{
	GENERATED_BODY()

public:
	ARequiemGameModeBase();
};
