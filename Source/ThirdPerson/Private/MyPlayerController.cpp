// Fill out your copyright notice in the Description page of Project Settings.


#include "MyPlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "SettingsSaveGame.h" // 或者是你创建该类时起的名字

void AMyPlayerController::BeginPlay()
{
	Super::BeginPlay();
	// 游戏开始时尝试从硬盘读取
	if (USettingsSaveGame* LoadedGame = Cast<USettingsSaveGame>(UGameplayStatics::LoadGameFromSlot("SettingsSlot", 0)))
	{
		// 通知角色（或任何监听者）当前的灵敏度
		OnSensitivityChanged.Broadcast(LoadedGame->Sensitivity);
	}
}

void AMyPlayerController::ChangeAndSaveSensitivity(float NewValue)
{
	// 1. 立即广播给角色，实现“实时改变”
	OnSensitivityChanged.Broadcast(NewValue);

	// 2. 保存到硬盘
	if (USettingsSaveGame* SaveGameInstance = Cast<USettingsSaveGame>(
		UGameplayStatics::CreateSaveGameObject(USettingsSaveGame::StaticClass())))
	{
		SaveGameInstance->Sensitivity = NewValue;
		UGameplayStatics::SaveGameToSlot(SaveGameInstance, "SettingsSlot", 0);
	}
}

void AMyPlayerController::ReturnToGame()
{
	// 1. 设置输入模式为“仅游戏”
	FInputModeGameOnly InputMode;
	SetInputMode(InputMode);

	// 2. 隐藏鼠标光标
	bShowMouseCursor = false;

	// 3. (可选) 如果你手动暂停了游戏，记得恢复
	//UGameplayStatics::SetGamePaused(GetWorld(), false);
}
