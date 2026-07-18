// Copyright Project Requiem. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "RequiemLockOnTargetInterface.generated.h"

/** Explicit contract for actors that may be selected by the basic lock-on system. */
UINTERFACE(BlueprintType)
class PROJECTREQUIEM_API URequiemLockOnTargetInterface : public UInterface
{
	GENERATED_BODY()
};

class PROJECTREQUIEM_API IRequiemLockOnTargetInterface
{
	GENERATED_BODY()

public:
	/** Runtime validity remains owned by the target, not by collision or actor class checks. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Combat|Lock On")
	bool IsValidLockOnTarget() const;

	/** World-space point used for acquisition, camera tracking and the temporary indicator. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Combat|Lock On")
	FVector GetLockOnFocusLocation() const;
};
