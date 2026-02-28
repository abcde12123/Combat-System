// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Slider.h"
#include "Components/EditableText.h" // 必须包含，否则 UEditableText 无法识别
#include "UI_Pause.generated.h"

UCLASS()
class THIRDPERSON_API UUI_Pause : public UUserWidget
{
	GENERATED_BODY()

protected:
	// BindWidget 会自动在蓝图中寻找名字叫 "SensitivitySlider" 的滑条
	UPROPERTY(meta = (BindWidget))
	class USlider* SensitivitySlider;

	// 必须在蓝图中放置一个 EditableText 并命名为 SensitivityInput
	UPROPERTY(meta = (BindWidget))
	class UEditableText* SensitivityInput;

	// UI 初始化时执行
	virtual void NativeConstruct() override;

	// 滑条变动时的回调函数
	UFUNCTION()
	void OnSliderValueChanged(float NewValue);

	// 文本框输入完成（按回车或失去焦点）时的回调
	UFUNCTION()
	void OnInputTextCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	// 核心：同步函数，用于保证滑条和文本永远一致
	// bUpdateSlider: 是否更新滑条 UI；bUpdateInput: 是否更新文本框 UI
	void SyncSensitivity(float NewValue, bool bUpdateSlider, bool bUpdateInput);
};
