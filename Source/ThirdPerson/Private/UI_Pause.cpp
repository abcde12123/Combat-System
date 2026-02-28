// Fill out your copyright notice in the Description page of Project Settings.

#include "UI_Pause.h"
#include "MyPlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "SettingsSaveGame.h"

void UUI_Pause::NativeConstruct()
{
	Super::NativeConstruct();

	// 1. 绑定滑条事件
	if (SensitivitySlider)
	{
		SensitivitySlider->OnValueChanged.AddDynamic(this, &UUI_Pause::OnSliderValueChanged);
	}

	// 2. 绑定文本框事件
	if (SensitivityInput)
	{
		SensitivityInput->OnTextCommitted.AddDynamic(this, &UUI_Pause::OnInputTextCommitted);
	}

	// 3. 初始化回显：从存档读取并设置
	float InitialValue = 1.0f;
	if (USettingsSaveGame* LoadedGame = Cast<USettingsSaveGame>(UGameplayStatics::LoadGameFromSlot("SettingsSlot", 0)))
	{
		InitialValue = LoadedGame->Sensitivity;
	}

	// 初始同步：同时更新滑条和文本框
	SyncSensitivity(InitialValue, true, true);
}

void UUI_Pause::OnSliderValueChanged(float NewValue)
{
	// 滑条在动时，同步更新文本框显示，但不更新滑条自身（防止循环调用）
	SyncSensitivity(NewValue, false, true);
}

void UUI_Pause::OnInputTextCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	// 只有在按下回车或失去焦点时才提交
	if (CommitMethod == ETextCommit::OnEnter || CommitMethod == ETextCommit::OnUserMovedFocus)
	{
		// 字符串转浮点数
		float NewVal = FCString::Atof(*Text.ToString());

		// 限制数值范围（建议根据你滑条的 Min/Max 设置，例如 0.0 到 10.0）
		NewVal = FMath::Clamp(NewVal, 0.0f, 10.0f);

		// 文本框确认后，同步更新滑条位置和文本框格式
		SyncSensitivity(NewVal, true, true);
	}
}

void UUI_Pause::SyncSensitivity(float NewValue, bool bUpdateSlider, bool bUpdateInput)
{
	// 1. 更新滑条
	if (bUpdateSlider && SensitivitySlider)
	{
		SensitivitySlider->SetValue(NewValue);
	}

	// 2. 更新文本框
	if (bUpdateInput && SensitivityInput)
	{
		// 修复报错的关键：先获取默认配置，然后创建一个可修改的副本
		FNumberFormattingOptions NumberFormat = FNumberFormattingOptions::DefaultNoGrouping();
		NumberFormat.SetMaximumFractionalDigits(2); // 设置保留两位小数
		NumberFormat.SetMinimumFractionalDigits(2); // (可选) 强制显示两位，如 1.00 而不是 1

		// 使用配置好的 NumberFormat 转换文本
		FText FormattedText = FText::AsNumber(NewValue, &NumberFormat);
		SensitivityInput->SetText(FormattedText);
	}

	// 3. 调用 PlayerController 逻辑（原有逻辑）
	if (AMyPlayerController* PC = Cast<AMyPlayerController>(GetOwningPlayer()))
	{
		PC->ChangeAndSaveSensitivity(NewValue);
	}
}
