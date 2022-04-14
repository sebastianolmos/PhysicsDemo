// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "PhysicsDemoHUD.generated.h"

UCLASS()
class APhysicsDemoHUD : public AHUD
{
	GENERATED_BODY()

public:
	APhysicsDemoHUD();

	/** Primary draw call for the HUD */
	virtual void DrawHUD() override;

private:
	/** Crosshair asset pointer */
	class UTexture2D* CrosshairTex;

};

