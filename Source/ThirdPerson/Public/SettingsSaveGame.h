// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "SettingsSaveGame.generated.h"

/**
 * 
 */
UCLASS()
class THIRDPERSON_API USettingsSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	float Sensitivity = 1.0f;

};
