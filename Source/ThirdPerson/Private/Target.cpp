#include "Target.h"
#include "Components/CapsuleComponent.h"

ATarget::ATarget()
{
	PrimaryActorTick.bCanEverTick = false;

	// 1. 核心步骤：添加标签，确保 AMyCharacter::FindBestTarget 能找到它
	Tags.Add(FName("Enemy"));

	// 设置碰撞频道，确保能被射线检测到
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
}

void ATarget::BeginPlay()
{
	Super::BeginPlay();
	Health = MaxHealth;
}

float ATarget::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (bIsDead) return 0.f;

	float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
    
	Health -= ActualDamage;
	UE_LOG(LogTemp, Warning, TEXT("靶子剩余血量: %f"), Health);

	// 2. 播放受击反馈动画
	if (HitReactMontage && !bIsDead)
	{
		PlayAnimMontage(HitReactMontage);
	}

	if (Health <= 0)
	{
		Die();
	}

	return ActualDamage;
}

void ATarget::Die()
{
	if (bIsDead) return;
	bIsDead = true;

	UE_LOG(LogTemp, Error, TEXT("靶子已被击毁"));

	// 3. 简单的死亡处理：可以销毁或禁用碰撞
	// 如果被锁定了，Destroy 会自动导致 AMyCharacter 的 LockedTarget 变为空，
	// 从而触发你 Tick 里的安全检查
	Destroy();
}

