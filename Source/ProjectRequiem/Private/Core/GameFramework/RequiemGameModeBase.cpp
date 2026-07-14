// Copyright Project Requiem. All Rights Reserved.

#include "Core/GameFramework/RequiemGameModeBase.h"

#include "Characters/RequiemCharacter.h"
#include "Core/GameFramework/RequiemPlayerController.h"

ARequiemGameModeBase::ARequiemGameModeBase()
{
	DefaultPawnClass = ARequiemCharacter::StaticClass();
	PlayerControllerClass = ARequiemPlayerController::StaticClass();
}
