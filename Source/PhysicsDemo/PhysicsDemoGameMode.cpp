// Copyright Epic Games, Inc. All Rights Reserved.

#include "PhysicsDemoGameMode.h"
#include "PhysicsDemoHUD.h"
#include "PhysicsDemoCharacter.h"
#include "UObject/ConstructorHelpers.h"

APhysicsDemoGameMode::APhysicsDemoGameMode()
	: Super()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnClassFinder(TEXT("/Game/FirstPersonCPP/Blueprints/FirstPersonCharacter"));
	DefaultPawnClass = PlayerPawnClassFinder.Class;

	// use our custom HUD class
	HUDClass = APhysicsDemoHUD::StaticClass();
}
