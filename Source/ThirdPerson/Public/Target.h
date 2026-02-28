#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Target.generated.h"

UCLASS()
class ATarget : public ACharacter
{
	GENERATED_BODY()

public:
	ATarget();

protected:
	virtual void BeginPlay() override;

public:
	// 处理受击的核心函数
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;

	// 受击时播放的蒙太奇（在编辑器中指定）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	UAnimMontage* HitReactMontage;

	// 基础属性
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	float Health = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	float MaxHealth = 100.f;

private:
	bool bIsDead = false;

	void Die();

};