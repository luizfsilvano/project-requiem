// Copyright Project Requiem. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "RequiemGameInstance.generated.h"

/**
 * Process-lifetime entry point for future session data.
 * Intentionally contains no gameplay or persistence logic in the foundation pass.
 */
UCLASS(BlueprintType, Blueprintable)
class PROJECTREQUIEM_API URequiemGameInstance : public UGameInstance
{
	GENERATED_BODY()
};
