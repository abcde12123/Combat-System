#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h" // 处理 FInputActionValue 的关键包含
#include "Logging/LogMacros.h"
#include "InputMappingContext.h"
#include "EnhancedInputComponent.h" // 如果你用到了输入组件也要加这个
#include "MyCharacter.generated.h"

// 前置声明，加快编译速度
class USpringArmComponent;
class UCameraComponent;
class UInputMappingContext;
class UInputAction;
class UWidgetComponent;
class UAnimMontage;

DECLARE_LOG_CATEGORY_EXTERN(Afight, Log, All);

DECLARE_LOG_CATEGORY_EXTERN(Ainput, Log, All);

UCLASS(config=Game)
class AMyCharacter : public ACharacter
{
	GENERATED_BODY()

	/** --- 核心组件 --- */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;

	/** --- 增强输入 Action --- */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputMappingContext* DefaultMappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* JumpAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* MoveAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* LookAction;

	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* AttackAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* LockOnAction;

	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* DodgeAction;

public:
	AMyCharacter();
	virtual void Tick(float DeltaTime) override;

protected:
	/** --- 引擎生命周期与回调 --- */
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	virtual void NotifyControllerChanged() override;
	virtual void OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode = 0) override;
	virtual void BeginPlay() override;
	virtual void Jump() override;

	/** --- 核心输入处理 --- */
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void OnAttackPressed();
	void OnAttackReleased();
	void ToggleLockOn();
	void Dodge();

	/** --- 连招与战斗逻辑 --- */
	void ExecuteLightAttack();
	void ExecuteHeavyAttack();
	void StopAttackAndCleanup();
	void ForcedResetCombo();
	void ApplyMovement(FVector2D MoveVector);
	bool IsPlayingAttackMontage() const;

	UFUNCTION(BlueprintCallable, Category="Combat")
	void EnableCancel();
	UFUNCTION(BlueprintCallable, Category="Combat")
	void DisableCancel();
	UFUNCTION(BlueprintCallable, Category="Combat")
	void ResetCombo();

	UPROPERTY(EditAnywhere, Category="Combat")
	UAnimMontage* LightAttackMontage;

	UPROPERTY(EditAnywhere, Category="Combat")
	UAnimMontage* HeavyAttackMontage;

	UPROPERTY(EditAnywhere, Category="Combat")
	UAnimMontage* DodgeMontage;

	// 战斗状态变量
	int32 ComboCount = 0;
	bool bIsAttacking = false;
	bool bCanCancelAttack = false;
	float PressStartTime = 0.f;
	FTimerHandle ComboResetTimerHandle;

	// 输入历史数据
	FVector2D LastRawInputVector = FVector2D::ZeroVector;
	FVector2D InputVectorOnAttackStart = FVector2D::ZeroVector;
	bool bHasNewInputSinceAttack = false;

	/** --- 锁定系统 --- */
	UPROPERTY(VisibleAnywhere, Category = "Combat")
	UWidgetComponent* LockOnWidget;

	UPROPERTY(EditAnywhere, Category = "Combat")
	float WidgetVerticalOffset = 20.f;

	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	bool bIsLockedOn = false;

	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	AActor* LockedTarget = nullptr;

	UPROPERTY(EditAnywhere, Category = "Combat")
	float LockOnRadius = 2000.f;

	UPROPERTY(EditAnywhere, Category = "Combat")
	float LockOnInterpSpeed = 12.0f;

	// 锁定UI详细参数 (保留原始变量名，解决解析失败问题)
	UPROPERTY(EditAnywhere, Category = "Combat|LockOnUI")
	float IconRotationSpeed = 100.0f;

	float CurrentIconRoll = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Combat|LockOnUI")
	float IconBaseScale = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Combat|LockOnUI")
	float MinScaleDistance = 500.f;

	UPROPERTY(EditAnywhere, Category = "Combat|LockOnUI")
	float MaxScaleDistance = 2000.f;

	UPROPERTY(EditAnywhere, Category = "Combat|LockOnUI")
	FVector2D ScaleRange = FVector2D(1.2f, 0.5f);

	AActor* FindBestTarget();
	void UpdateLockOnRotation(float DeltaTime);

	/** --- 体力与属性 --- */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attributes")
	float MaxStamina = 100.f;

	UPROPERTY(BlueprintReadOnly, Category="Attributes")
	float CurrentStamina = 100.f;

	UPROPERTY(EditAnywhere, Category="Attributes")
	float DodgeStaminaCost = 25.f;

	UPROPERTY(EditAnywhere, Category="Attributes")
	float StaminaRegenRate = 15.f;

	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	bool bIsDodging = false;

	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	bool bIsInvulnerable = false;

	void RegenerateStamina(float DeltaTime);

	UFUNCTION(BlueprintCallable, Category="Combat")
	void OnDodgeFinished();

	UFUNCTION(BlueprintPure, Category="Attributes")
	float GetStaminaPercent() const { return CurrentStamina / MaxStamina; }

	/** --- 核心工具 --- */
	FVector GetInputWorldDirection();

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void SetInvulnerable(bool bEnabled);

	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent,
	                         class AController* EventInstigator, AActor* DamageCauser) override;

	/** 翻滚蒙太奇资产 */
	UPROPERTY(EditAnywhere, Category = "MyCharacter|Animation")
	class UAnimMontage* DodgeForward;

	UPROPERTY(EditAnywhere, Category = "MyCharacter|Animation")
	class UAnimMontage* DodgeBackward;

	UPROPERTY(EditAnywhere, Category = "MyCharacter|Animation")
	class UAnimMontage* DodgeLeft;

	UPROPERTY(EditAnywhere, Category = "MyCharacter|Animation")
	class UAnimMontage* DodgeRight;

	/** 翻滚用的独立计时器句柄 */
	FTimerHandle DodgeTimerHandle;

	// 在 MyCharacter.h 的 private 或 protected 里添加
private:
	FVector LastSwordBaseLocation;
	FVector LastSwordTipLocation;
	bool bIsFirstAttackTick; // 用来标记是否是攻击的第一帧
	UPROPERTY()
	TArray<AActor*> HitActorsThisAttack;

public:
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }

	UPROPERTY(BlueprintReadWrite, Category = "Input")
	float MouseSensitivity = 1.0f;

	UFUNCTION()
	void OnSensitivityUpdate(float NewValue);

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void ResetAttackTrace(); // 用来清空坐标的函数
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void PerformAttackCheck(FName StartSocket, FName EndSocket, float Radius, float Damage, float PushForce);
	void HitEffect(AActor* HitActor, float Damage, float PushForce);

	
};
