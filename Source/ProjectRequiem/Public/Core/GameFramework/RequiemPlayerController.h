// Copyright Project Requiem. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "RequiemPlayerController.generated.h"

class UInputMappingContext;

/** Player-facing input context owner. */
UCLASS(Blueprintable)
class PROJECTREQUIEM_API ARequiemPlayerController : public APlayerController
{
	GENERATED_BODY()

protected:
	virtual void SetupInputComponent() override;

	/** Mapping context selected by the composing Blueprint. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;
};
