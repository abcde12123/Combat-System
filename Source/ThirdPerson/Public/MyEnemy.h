#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "MyEnemy.generated.h"


class UWidgetComponent;
class AAIController;


// ---------------- 状态 ----------------
UENUM(BlueprintType)
enum class EEnemyState : uint8
{
    Idle        UMETA(DisplayName="Idle"),
    Chasing     UMETA(DisplayName="Chasing"),
    Attacking   UMETA(DisplayName="Attacking"),
    Stunned     UMETA(DisplayName="Stunned"),
    Dead        UMETA(DisplayName="Dead")
};

// ---------------- Enemy ----------------
UCLASS()
class AMyEnemy : public ACharacter
{
    GENERATED_BODY()

public:
    AMyEnemy();

protected:
    virtual void BeginPlay() override;

public:
    virtual void Tick(float DeltaTime) override;


/* ================== 基础属性 ================== */

public:

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Stats")
    float MaxHealth = 100.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Enemy|Stats")
    float CurrentHealth = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Stats")
    float DetectionRange = 1200.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Stats")
    float AttackRange = 180.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Stats")
    float Damage = 15.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Stats")
    float AttackCooldown = 1.2f;


/* ================== 组件 ================== */

protected:

    UPROPERTY(VisibleAnywhere,BlueprintReadOnly, Category="Enemy|UI",meta = (AllowPrivateAccess = "true"))
    UWidgetComponent* HealthBarWidget;


/* ================== AI ================== */

protected:

    UPROPERTY()
    AAIController* CachedAIController = nullptr;

    UPROPERTY()
    AActor* TargetActor = nullptr;

    EEnemyState CurrentState = EEnemyState::Idle;


/* ================== 动画 ================== */

protected:

    UPROPERTY(EditDefaultsOnly, Category="Enemy|Anim")
    UAnimMontage* AttackMontage;

    UPROPERTY(EditDefaultsOnly, Category="Enemy|Anim")
    UAnimMontage* HitMontage;

    UPROPERTY(EditDefaultsOnly, Category="Enemy|Anim")
    UAnimMontage* DeathMontage;


/* ================== 攻击判定 ================== */

protected:

    // 武器 Socket
    UPROPERTY(EditDefaultsOnly, Category="Enemy|Weapon")
    FName WeaponStartSocket = "Hand_R";

    UPROPERTY(EditDefaultsOnly, Category="Enemy|Weapon")
    FName WeaponEndSocket = "Hand_R_Tip";

    UPROPERTY(EditDefaultsOnly, Category="Enemy|Weapon")
    float TraceRadius = 35.f;

    FVector LastWeaponStart;
    FVector LastWeaponEnd;
    
    UPROPERTY()
    TSet<AActor*> HitActorsThisAttack;

    void StartAttackTrace();
    void TickAttackTrace();
    void StopAttackTrace();

    void ProcessHit(AActor* Victim);

    


/* ================== 战斗 ================== */

protected:

    bool bCanAttack = true;

    FTimerHandle AttackTimer;
    FTimerHandle StunTimer;
    
    UFUNCTION(BlueprintCallable, Category = "Enemy|Combat")
    void TryAttack();
    
    void ResetAttack();

    // ✅ 新增：动画结束后的回调函数
    UFUNCTION()
    void OnAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted);

    void OnStunFinished();

    virtual float TakeDamage(
        float DamageAmount,
        struct FDamageEvent const& DamageEvent,
        class AController* EventInstigator,
        AActor* DamageCauser
    ) override;

    void Die();


/* ================== 状态 ================== */

protected:

    void SetState(EEnemyState NewState);


/* ================== 工具 ================== */

protected:
    UFUNCTION(BlueprintImplementableEvent)
    void UpdateHealthBar();


};