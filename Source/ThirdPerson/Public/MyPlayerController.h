// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "MyPlayerController.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSensitivityChanged, float, NewValue);

/**
 * 
 */
UCLASS()
class THIRDPERSON_API AMyPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable)
	FOnSensitivityChanged OnSensitivityChanged;

	// UI 调用这个函数
	UFUNCTION(BlueprintCallable)
	void ChangeAndSaveSensitivity(float NewValue);

	UFUNCTION(BlueprintCallable, Category = "UI")
	void ReturnToGame();

protected:
	virtual void BeginPlay() override;
};
