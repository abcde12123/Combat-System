#include "MyCharacter.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Animation/AnimInstance.h"
#include "MyPlayerController.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "Components/WidgetComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Character.h"

DEFINE_LOG_CATEGORY(Afight);
DEFINE_LOG_CATEGORY(Ainput);

AMyCharacter::AMyCharacter()
{
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);
	GetCharacterMovement()->JumpZVelocity = 700.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);

	PrimaryActorTick.bCanEverTick = true;

	LockOnWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("LockOnWidget"));
	LockOnWidget->SetupAttachment(RootComponent); // 初始挂在自己身上
	LockOnWidget->SetWidgetSpace(EWidgetSpace::World);
	LockOnWidget->SetDrawAtDesiredSize(true);
	LockOnWidget->SetVisibility(false); // 初始隐藏

	CurrentStamina = MaxStamina;
}

void AMyCharacter::BeginPlay()
{
	Super::BeginPlay();
	if (AMyPlayerController* PC = Cast<AMyPlayerController>(GetController()))
	{
		// 绑定订阅：PC 一广播，我的 MouseSensitivity 就变
		PC->OnSensitivityChanged.AddDynamic(this, &AMyCharacter::OnSensitivityUpdate);

		// 初始化时获取一次当前值（防止 UI 已经加载了保存的数据而角色还没拿）
	}
}

void AMyCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		EIC->BindAction(JumpAction, ETriggerEvent::Started, this, &AMyCharacter::Jump);
		EIC->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		EIC->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AMyCharacter::Move);
		EIC->BindAction(MoveAction, ETriggerEvent::Completed, this, &AMyCharacter::Move);

		EIC->BindAction(LookAction, ETriggerEvent::Triggered, this, &AMyCharacter::Look);
		EIC->BindAction(AttackAction, ETriggerEvent::Started, this, &AMyCharacter::OnAttackPressed);
		EIC->BindAction(AttackAction, ETriggerEvent::Completed, this, &AMyCharacter::OnAttackReleased);

		EIC->BindAction(LockOnAction, ETriggerEvent::Started, this, &AMyCharacter::ToggleLockOn);

		EIC->BindAction(DodgeAction, ETriggerEvent::Started, this, &AMyCharacter::Dodge);
	}
}

// --- 跳跃逻辑：增加打断支持 ---
void AMyCharacter::Jump()
{
	if (bIsDodging) return;

	if (IsPlayingAttackMontage())
	{
		// 如果打断窗口开启，按下跳跃键直接取消后摇并起跳
		if (bCanCancelAttack)
		{
			UE_LOG(Afight, Log, TEXT("跳跃打断攻击后摇"));
			StopAttackAndCleanup();
			// 注意：这里不要 return，让它继续往下走执行起跳
		}
		else
		{
			// 还在出招硬直中，跳跃无效
			return;
		}
	}

	Super::Jump();
}

// --- 移动打断逻辑 ---
void AMyCharacter::Move(const FInputActionValue& Value)
{
	FVector2D MovementVector = Value.Get<FVector2D>();
	LastRawInputVector = MovementVector;

	if (!Controller) return;

	if (IsPlayingAttackMontage())
	{
		// 意图检测：是否产生了不同于开始攻击时的操作
		if (!bHasNewInputSinceAttack)
		{
			if (MovementVector.SizeSquared() < 0.01f || (MovementVector - InputVectorOnAttackStart).SizeSquared() >
				0.1f)
			{
				bHasNewInputSinceAttack = true;
			}
		}

		// 满足窗口 + 新输入，执行移动打断
		if (bCanCancelAttack && bHasNewInputSinceAttack && MovementVector.SizeSquared() > 0.01f)
		{
			StopAttackAndCleanup();
		}
		else return;
	}

	ApplyMovement(MovementVector);
}

// --- 攻击逻辑 ---
void AMyCharacter::ExecuteLightAttack()
{
	// 【新增限制】如果正在空中（下落或跳跃中），禁止攻击
	if (GetCharacterMovement() && GetCharacterMovement()->IsFalling())
	{
		return;
	}

	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (!AnimInstance || !LightAttackMontage || bIsAttacking) return;

	GetWorldTimerManager().ClearTimer(ComboResetTimerHandle);

	if (AnimInstance->Montage_IsPlaying(LightAttackMontage))
	{
		ComboCount = (ComboCount % 3) + 1;
		FName SectionName = FName(*FString::Printf(TEXT("Attack%d"), ComboCount));
		AnimInstance->Montage_JumpToSection(SectionName, LightAttackMontage);
	}
	else
	{
		ComboCount = 1;
		PlayAnimMontage(LightAttackMontage, 1.0f, FName("Attack1"));
	}

	bIsAttacking = true;
	bCanCancelAttack = false;
	bHasNewInputSinceAttack = false;
	InputVectorOnAttackStart = LastRawInputVector;
}

// --- 辅助与重置 ---
void AMyCharacter::ResetCombo()
{
	bIsAttacking = false;
	GetWorldTimerManager().SetTimer(ComboResetTimerHandle, this, &AMyCharacter::ForcedResetCombo, 1.0f, false);
}

void AMyCharacter::EnableCancel() { bCanCancelAttack = true; }
void AMyCharacter::DisableCancel() { bCanCancelAttack = false; }

void AMyCharacter::ForcedResetCombo()
{
	ComboCount = 0;
	bIsAttacking = false;
	bCanCancelAttack = false;
}

void AMyCharacter::StopAttackAndCleanup()
{
	if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
	{
		AnimInstance->Montage_Stop(0.2f);
	}
	bIsAttacking = false;
	bCanCancelAttack = false;
	bHasNewInputSinceAttack = false;
	ComboCount = 0;
	GetWorldTimerManager().ClearTimer(ComboResetTimerHandle);
	// 重置位置，防止下一轮攻击检测出错
	LastSwordBaseLocation = FVector::ZeroVector;
	LastSwordTipLocation = FVector::ZeroVector;
}

bool AMyCharacter::IsPlayingAttackMontage() const
{
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	return AnimInstance && (AnimInstance->Montage_IsPlaying(LightAttackMontage) || AnimInstance->Montage_IsPlaying(
		HeavyAttackMontage));
}

void AMyCharacter::ApplyMovement(FVector2D MoveVector)
{
	const FRotator Rotation = Controller->GetControlRotation();
	const FRotator YawRotation(0, Rotation.Yaw, 0);
	const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	AddMovementInput(ForwardDirection, MoveVector.Y);
	AddMovementInput(RightDirection, MoveVector.X);
}

void AMyCharacter::OnAttackPressed() { PressStartTime = GetWorld()->GetTimeSeconds(); }

void AMyCharacter::OnAttackReleased()
{
	float Duration = GetWorld()->GetTimeSeconds() - PressStartTime;
	if (Duration >= 0.4f) ExecuteHeavyAttack();
	else ExecuteLightAttack();
}

void AMyCharacter::ExecuteHeavyAttack()
{
	// 【新增限制】如果正在空中（下落或跳跃中），禁止攻击
	if (GetCharacterMovement() && GetCharacterMovement()->IsFalling())
	{
		return;
	}
	if (bIsAttacking) return;
	if (HeavyAttackMontage)
	{
		bIsAttacking = true;
		PlayAnimMontage(HeavyAttackMontage);
		GetWorldTimerManager().SetTimer(ComboResetTimerHandle, this, &AMyCharacter::ForcedResetCombo, 1.5f, false);
	}
}

void AMyCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	RegenerateStamina(DeltaTime);

	if (bIsLockedOn && LockedTarget)
	{
		UpdateLockOnRotation(DeltaTime);
		CameraBoom->SocketOffset = FMath::VInterpTo(CameraBoom->SocketOffset, FVector(0, 80.f, 0), DeltaTime, 5.0f);
	}
	else
	{
		// 如果没有锁定，也要平滑地把 SocketOffset 归零
		if (!CameraBoom->SocketOffset.IsNearlyZero())
		{
			CameraBoom->SocketOffset = FMath::VInterpTo(CameraBoom->SocketOffset, FVector::ZeroVector, DeltaTime, 5.0f);
		}
	}

	if (GEngine)
	{
		// 1. 核心逻辑状态（大标题）
		FColor AttackColor = bIsAttacking ? FColor::Red : FColor::Green;
		FString AttackStatus = bIsAttacking ? TEXT("LOCKED (正在出招)") : TEXT("FREE (可行动)");
		GEngine->AddOnScreenDebugMessage(1, 0.f, AttackColor, FString::Printf(TEXT("【状态】攻击锁: %s"), *AttackStatus));

		// 2. 打断窗口状态
		FColor CancelColor = bCanCancelAttack ? FColor::Cyan : FColor::Silver;
		FString CancelStatus = bCanCancelAttack ? TEXT("OPEN (推摇杆/跳跃可打断)") : TEXT("CLOSED");
		GEngine->AddOnScreenDebugMessage(2, 0.f, CancelColor, FString::Printf(TEXT("【状态】打断窗: %s"), *CancelStatus));

		// 3. 连招信息
		GEngine->AddOnScreenDebugMessage(3, 0.f, FColor::Orange, FString::Printf(TEXT("【战斗】当前连击段数: %d"), ComboCount));

		// 4. 输入实时反馈 (验证 Move 函数是否在工作)
		FString InputDir = FString::Printf(
			TEXT("X: %.2f, Y: %.2f (强度: %.2f)"), LastRawInputVector.X, LastRawInputVector.Y, LastRawInputVector.Size());
		GEngine->AddOnScreenDebugMessage(4, 0.f, FColor::Yellow, FString::Printf(TEXT("【输入】当前摇杆: %s"), *InputDir));

		// 5. 意图快照 (验证打断算法)
		FString SnapshotDir = FString::Printf(TEXT("X: %.2f, Y: %.2f"), InputVectorOnAttackStart.X,
		                                      InputVectorOnAttackStart.Y);
		FColor IntentColor = bHasNewInputSinceAttack ? FColor::Magenta : FColor::White;
		GEngine->AddOnScreenDebugMessage(5, 0.f, IntentColor, FString::Printf(TEXT("【逻辑】起手输入快照: %s | 已产生新意图: %s"),
		                                                                      *SnapshotDir,
		                                                                      bHasNewInputSinceAttack
			                                                                      ? TEXT("YES")
			                                                                      : TEXT("NO")));
		GEngine->AddOnScreenDebugMessage(9, 0.f, FColor::Orange,
		                                 FString::Printf(TEXT("【属性】体力: %.1f / %.1f"), CurrentStamina, MaxStamina));

		// 6. 动画蒙太奇监控
		UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
		if (AnimInstance && AnimInstance->IsAnyMontagePlaying())
		{
			UAnimMontage* CurrentMontage = AnimInstance->GetCurrentActiveMontage();
			FName CurrentSection = AnimInstance->Montage_GetCurrentSection(CurrentMontage);
			float PlayTime = AnimInstance->Montage_GetPosition(CurrentMontage);

			GEngine->AddOnScreenDebugMessage(6, 0.f, FColor::Blue, FString::Printf(
				                                 TEXT("【动画】蒙太奇: %s | 当前分段: %s | 进度: %.2f秒"),
				                                 *GetNameSafe(CurrentMontage), *CurrentSection.ToString(), PlayTime));
		}
		else
		{
			GEngine->AddOnScreenDebugMessage(6, 0.f, FColor::White, TEXT("【动画】无蒙太奇播放"));
		}

		// 7. 物理状态
		FString MoveMode = "";
		if (GetCharacterMovement())
		{
			UEnum* MoveModeEnum = StaticEnum<EMovementMode>();
			MoveMode = MoveModeEnum->GetValueAsString(GetCharacterMovement()->MovementMode);
		}
		GEngine->AddOnScreenDebugMessage(7, 0.f, FColor::Green, FString::Printf(TEXT("【物理】移动模式: %s"), *MoveMode));

		FColor LockedColor = bIsLockedOn ? FColor::Cyan : FColor::Silver;
		FString LockedName = (bIsLockedOn && LockedTarget) ? LockedTarget->GetName() : TEXT("无");
		GEngine->AddOnScreenDebugMessage(8, 0.f, LockedColor, FString::Printf(TEXT("【锁定】当前目标: %s"), *LockedName));
	}
}


void AMyCharacter::Look(const FInputActionValue& Value)
{
	FVector2D LookAxisVector = Value.Get<FVector2D>();
	if (Controller)
	{
		// 在这里乘上你的灵敏度变量
		AddControllerYawInput(LookAxisVector.X * MouseSensitivity);
		AddControllerPitchInput(LookAxisVector.Y * MouseSensitivity);
	}
}

void AMyCharacter::OnSensitivityUpdate(float NewValue)
{
	MouseSensitivity = NewValue;
}

void AMyCharacter::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();
	if (APlayerController* PC = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Sub = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(
			PC->GetLocalPlayer()))
			Sub->AddMappingContext(DefaultMappingContext, 0);
	}
}

void AMyCharacter::OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PrevMovementMode, PreviousCustomMode);

	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	if (MoveComp)
	{
		if (MoveComp->MovementMode == MOVE_Falling)
		{
			// 空中允许转向，但可以适当调慢一点（例如 300），或者保持 500
			MoveComp->RotationRate = FRotator(0.0f, 300.0f, 0.0f);

			if (IsPlayingAttackMontage()) StopAttackAndCleanup();
		}
		else if (MoveComp->MovementMode == MOVE_Walking)
		{
			// 回到地面恢复正常转速
			MoveComp->RotationRate = FRotator(0.0f, 500.0f, 0.0f);
		}
	}
}

void AMyCharacter::ToggleLockOn()
{
	UE_LOG(Ainput, Log, TEXT("按下 F 键！"));
	if (bIsLockedOn)
	{
		// 取消锁定
		bIsLockedOn = false;
		LockedTarget = nullptr;
		// 恢复正常移动模式：角色朝向运动方向
		GetCharacterMovement()->bOrientRotationToMovement = true;
		GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Red, TEXT("取消锁定"));
	}
	else
	{
		// 尝试寻找目标
		LockedTarget = FindBestTarget();
		if (LockedTarget)
		{
			bIsLockedOn = true;
			// 锁定模式：角色不再自动朝向移动方向，我们要手动控制它面向敌人
			GetCharacterMovement()->bOrientRotationToMovement = false;
			GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Cyan,
			                                 FString::Printf(TEXT("锁定到: %s"), *LockedTarget->GetName()));
		}
		else
		{
			GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Yellow, TEXT("未找到有效目标"));
		}
	}
	if (bIsLockedOn && LockedTarget)
	{
		// 1. 显示图标
		LockOnWidget->SetVisibility(true);

		// 2. 将图标挂载到目标身上（或者在 Tick 中实时更新位置）
		// 推荐在 Tick 中更新，这样可以做平滑跟随
	}
	else
	{
		LockOnWidget->SetVisibility(false);
		LockOnWidget->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	}
}

AActor* AMyCharacter::FindBestTarget()
{
	TArray<AActor*> OverlappingActors;
	TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_Pawn));

	// 1. 范围搜索
	UKismetSystemLibrary::SphereOverlapActors(GetWorld(), GetActorLocation(), LockOnRadius, ObjectTypes, nullptr,
	                                          {this}, OverlappingActors);

	AActor* BestTarget = nullptr;
	float MinScore = MAX_FLT;

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC) return nullptr;

	int32 ViewportSizeX, ViewportSizeY;
	PC->GetViewportSize(ViewportSizeX, ViewportSizeY);
	FVector2D ScreenCenter(ViewportSizeX * 0.5f, ViewportSizeY * 0.5f);

	for (AActor* Actor : OverlappingActors)
	{
		// 仅检测带 Enemy 标签的物体
		if (Actor && Actor->ActorHasTag(TEXT("Enemy")))
		{
			FVector2D ScreenPosition;
			// 2. 将敌人位置投影到屏幕
			if (PC->ProjectWorldLocationToScreen(Actor->GetActorLocation(), ScreenPosition))
			{
				// 3. 计算离屏幕中心的像素距离
				float DistanceToCenter = FVector2D::Distance(ScreenPosition, ScreenCenter);

				// 增加射线检测，防止锁定墙后的敌人
				FHitResult Hit;
				FCollisionQueryParams Params;
				Params.AddIgnoredActor(this);
				bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, GetFollowCamera()->GetComponentLocation(),
				                                                 Actor->GetActorLocation(), ECC_Visibility, Params);

				if (!bHit || (Hit.GetActor() && Hit.GetActor()->IsA(Actor->GetClass())))
				{
					if (DistanceToCenter < MinScore)
					{
						MinScore = DistanceToCenter;
						BestTarget = Actor;
					}
				}
			}
		}
	}
	return BestTarget;
}

void AMyCharacter::UpdateLockOnRotation(float DeltaTime)
{
	// 1. 基础安全检查
	if (!LockedTarget || !Controller || !CameraBoom || !LockOnWidget) return;

	// --- 2. 计算目标位置与旋转 ---
	FVector EnemyLoc = LockedTarget->GetActorLocation();
	FVector LookAtPoint = EnemyLoc + FVector(0, 0, -60.0f);

	// 计算从【相机】指向【目标点】的旋转角度
	FRotator TargetRot = UKismetMathLibrary::FindLookAtRotation(GetFollowCamera()->GetComponentLocation(), LookAtPoint);

	// --- 3. 摄像机旋转平滑处理 ---
	FRotator CurrentControlRot = Controller->GetControlRotation();
	TargetRot.Pitch = FMath::Clamp(TargetRot.Pitch, -40.0f, 20.0f);

	// --- 核心修复：移除 bIsDodging 减速逻辑 ---
	// 无论是否翻滚，都使用 LockOnInterpSpeed，保证相机跟随力度
	FRotator NewControlRot = FMath::RInterpTo(CurrentControlRot, TargetRot, DeltaTime, LockOnInterpSpeed);
	Controller->SetControlRotation(NewControlRot);

	// --- 4. 越肩视角偏移 ---
	float TargetYOffset = bIsLockedOn ? 70.0f : 0.0f;
	CameraBoom->SocketOffset =
		FMath::VInterpTo(CameraBoom->SocketOffset, FVector(0, TargetYOffset, 0), DeltaTime, 5.0f);

	// --- 5. 角色朝向处理 ---
	// 只有在非翻滚状态下，才强制角色转向敌人
	if (!bIsDodging)
	{
		FRotator CurrentActorRot = GetActorRotation();
		FRotator TargetActorYaw = FRotator(0, TargetRot.Yaw, 0);
		SetActorRotation(FMath::RInterpTo(CurrentActorRot, TargetActorYaw, DeltaTime, LockOnInterpSpeed));
	}

	// --- 6. 锁定图标 (UI Widget) 逻辑 (保留你原本的所有动态缩放和自转逻辑) ---
	if (bIsLockedOn)
	{
		FVector DirectionToPlayer = (GetActorLocation() - EnemyLoc).GetSafeNormal();
		FVector IconLocation = EnemyLoc + FVector(0, 0, WidgetVerticalOffset) + (DirectionToPlayer * 50.0f);
		LockOnWidget->SetWorldLocation(IconLocation);

		CurrentIconRoll += IconRotationSpeed * DeltaTime;
		if (CurrentIconRoll > 360.f) CurrentIconRoll -= 360.f;

		FRotator CameraRot = GetFollowCamera()->GetComponentRotation();
		FRotator FinalWidgetRot = FRotator(CameraRot.Pitch, CameraRot.Yaw, CurrentIconRoll);
		LockOnWidget->SetWorldRotation(FinalWidgetRot);

		float Distance = FVector::Dist(GetActorLocation(), EnemyLoc);
		float FinalScale = FMath::GetMappedRangeValueClamped(
			FVector2D(MinScaleDistance, MaxScaleDistance),
			ScaleRange,
			Distance
		) * IconBaseScale;

		LockOnWidget->SetDrawSize(FVector2D(64.f, 64.f) * FinalScale);
		LockOnWidget->SetRelativeScale3D(FVector(FinalScale));
	}

	// --- 7. 距离自动解锁逻辑 ---
	if (FVector::Dist(GetActorLocation(), EnemyLoc) > LockOnRadius + 500.f)
	{
		ToggleLockOn();
	}
}

void AMyCharacter::Dodge()
{
	if (GetCharacterMovement()->IsFalling() || bIsDodging || CurrentStamina < DodgeStaminaCost) return;

	FVector InputDir = GetInputWorldDirection();
	float InputSize = LastRawInputVector.Size();
	UAnimMontage* SelectedMontage = nullptr;
	//UAnimMontage* SelectedMontage = DodgeForward; // 默认

	if (InputSize > 0.1f)
	{
		FVector ActorForward = GetActorForwardVector();
		FVector ActorRight = GetActorRightVector();

		float ForwardDot = FVector::DotProduct(ActorForward, InputDir);
		float RightDot = FVector::DotProduct(ActorRight, InputDir);

		// 得到 -180 到 180 的度数
		float RelativeAngle = FMath::RadiansToDegrees(FMath::Atan2(RightDot, ForwardDot));

		// --- 核心优化：调整区间边界，解决左前/右前误触 ---
		// 1. 扩大前滚区间：从 45 改为 55，给斜前方留出更多余地
		if (RelativeAngle >= -55.f && RelativeAngle <= 55.f)
		{
			SelectedMontage = DodgeForward;
			SetActorRotation(InputDir.Rotation());
		}
		// 2. 缩减侧滚区间，并再次微调后滚边界
		else if (RelativeAngle > 55.f && RelativeAngle <= 115.f) // 右
		{
			SelectedMontage = DodgeRight;
		}
		else if (RelativeAngle < -55.f && RelativeAngle >= -115.f) // 左
		{
			SelectedMontage = DodgeLeft;
		}
		else // 后 (115 到 180 或 -115 到 -180)
		{
			SelectedMontage = DodgeBackward;
			FRotator BackRot = (-InputDir).Rotation();
			SetActorRotation(FRotator(0.f, BackRot.Yaw, 0.f));
		}
	}
	else
	{
		SelectedMontage = DodgeBackward;
	}

	// --- 执行部分（保持你原有的逻辑，不作删减） ---
	if (SelectedMontage)
	{
		CurrentStamina -= DodgeStaminaCost;
		if (IsPlayingAttackMontage()) StopAttackAndCleanup();

		bIsDodging = true;
		GetCharacterMovement()->bOrientRotationToMovement = false;

		float Duration = PlayAnimMontage(SelectedMontage);
		GetWorldTimerManager().SetTimer(DodgeTimerHandle, this, &AMyCharacter::OnDodgeFinished, Duration, false);
	}
}

void AMyCharacter::OnDodgeFinished()
{
	bIsDodging = false;
	bIsInvulnerable = false; // 保险起见，结束翻滚必关无敌

	// 恢复正常移动模式
	if (!bIsLockedOn)
	{
		GetCharacterMovement()->bOrientRotationToMovement = true;
	}
}

void AMyCharacter::RegenerateStamina(float DeltaTime)
{
	// 翻滚中不恢复体力（可选）
	if (!bIsDodging && CurrentStamina < MaxStamina)
	{
		CurrentStamina = FMath::Clamp(CurrentStamina + (StaminaRegenRate * DeltaTime), 0.f, MaxStamina);
	}
}

FVector AMyCharacter::GetInputWorldDirection()
{
	if (!Controller) return GetActorForwardVector();

	const FRotator Rotation = Controller->GetControlRotation();
	const FRotator YawRotation(0, Rotation.Yaw, 0);

	const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	// 根据 WASD 输入计算最终方向向量
	FVector CombinedDir = (ForwardDirection * LastRawInputVector.Y) + (RightDirection * LastRawInputVector.X);

	return CombinedDir.IsNearlyZero() ? GetActorForwardVector() : CombinedDir.GetSafeNormal();
}


void AMyCharacter::SetInvulnerable(bool bEnabled)
{
	bIsInvulnerable = bEnabled;
}

float AMyCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator,
                               AActor* DamageCauser)
{
	if (bIsInvulnerable) return 0.f;
	return Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
}

void AMyCharacter::PerformAttackCheck(FName StartSocket, FName EndSocket, float Radius, float Damage, float PushForce)
{
    if (!GetMesh()) return;

    // --- 1. 坐标计算（保持原样） ---
    FVector CurStart = GetMesh()->GetSocketLocation(StartSocket);
    FVector CurEnd = GetMesh()->GetSocketLocation(EndSocket);
    FVector CurMid = (CurStart + CurEnd) * 0.5f;

    if (LastSwordBaseLocation.IsZero()) LastSwordBaseLocation = CurStart;
    if (LastSwordTipLocation.IsZero()) LastSwordTipLocation = CurEnd;
    FVector LastMid = (LastSwordBaseLocation + LastSwordTipLocation) * 0.5f;

    TArray<AActor*> ActorsToIgnore;
    ActorsToIgnore.Add(this);

    // --- 2. 创建一个“大篮子”收集所有的命中结果 ---
    TArray<FHitResult> AllHits;
    // 定义一个临时的篮子用来接每次检测的结果
    TArray<FHitResult> TempHits;

    // --- 3. 执行 5 次检测，并把结果全部合并 ---
    auto RunTrace = [&](FVector S, FVector E) {
        TempHits.Empty();
        UKismetSystemLibrary::SphereTraceMulti(GetWorld(), S, E, Radius, 
            UEngineTypes::ConvertToTraceType(ECC_Pawn), false, ActorsToIgnore, 
            EDrawDebugTrace::ForDuration, TempHits, true);
        AllHits.Append(TempHits); // 将结果塞进大篮子
    };

    RunTrace(LastSwordBaseLocation, CurStart); // 轨迹 1
    RunTrace(LastMid, CurMid);               // 轨迹 2
    RunTrace(LastSwordTipLocation, CurEnd);    // 轨迹 3 (刀尖轨迹)
    RunTrace(CurStart, CurMid);                // 刀身 A
    RunTrace(CurMid, CurEnd);                  // 刀身 B

    // --- 4. 统一处理“大篮子”里的所有人 ---
    if (AllHits.Num() > 0)
    {
        for (const FHitResult& Hit : AllHits)
        {
            AActor* Victim = Hit.GetActor();
            
            // 只要在 Enemy 标签内，且这一招还没打过他
            if (Victim && Victim->ActorHasTag(TEXT("Enemy")) && !HitActorsThisAttack.Contains(Victim))
            {
                HitActorsThisAttack.Add(Victim);
                HitEffect(Victim, Damage, PushForce);
                
                // 【调试打印】看看是谁触发了命中
                // GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Cyan, FString::Printf(TEXT("命中目标: %s"), *Victim->GetName()));
            }
        }
    }

    // 更新坐标
    LastSwordBaseLocation = CurStart;
    LastSwordTipLocation = CurEnd;
}

void AMyCharacter::ResetAttackTrace()
{
	LastSwordBaseLocation = FVector::ZeroVector;
	LastSwordTipLocation = FVector::ZeroVector;
	// 如果有 LastMid 也可以顺便重置，但代码里如果是实时算的就不必
	// 【新增】开始新一轮攻击判定前，清空命中名单
	HitActorsThisAttack.Empty();
}

void AMyCharacter::HitEffect(AActor* HitActor, float Damage, float PushForce)
{
	if (!HitActor) return;

	// --- 第一部分：处理数值（伤害） ---
	UGameplayStatics::ApplyDamage(
		HitActor,
		Damage,
		GetController(),
		this,
		UDamageType::StaticClass()
	);

	// --- 第二部分：处理物理（击退） ---
	ACharacter* EnemyChar = Cast<ACharacter>(HitActor);
	if (EnemyChar)
	{
		FVector LaunchDir = (HitActor->GetActorLocation() - GetActorLocation()).GetSafeNormal();
		LaunchDir.Z = 0.15f; // 给一点点向上的浮力

		// 执行击退
		EnemyChar->LaunchCharacter(LaunchDir * PushForce, true, true);

		// 速度保险
		UCharacterMovementComponent* MoveComp = EnemyChar->GetCharacterMovement();
		if (MoveComp && MoveComp->Velocity.Size() > 2500.f)
		{
			MoveComp->Velocity = MoveComp->Velocity.GetSafeNormal() * 2500.f;
		}
	}

	// --- 第三部分：处理表现（特效/音效 - 预留） ---
	// 这里以后可以写：SpawnEmitterAtLocation(...) 
}


