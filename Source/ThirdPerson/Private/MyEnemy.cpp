#include "MyEnemy.h"

#include "AIController.h"
#include "Components/CapsuleComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"

/* ================== 构造 ================== */

AMyEnemy::AMyEnemy()
{
    PrimaryActorTick.bCanEverTick = true;

    Tags.Add("Enemy");

    GetCharacterMovement()->MaxWalkSpeed = 380.f;
    GetCharacterMovement()->bOrientRotationToMovement = true;

    // 血条
    HealthBarWidget = CreateDefaultSubobject<UWidgetComponent>("HealthBar");
    HealthBarWidget->SetupAttachment(RootComponent);

    HealthBarWidget->SetWidgetSpace(EWidgetSpace::World);
    HealthBarWidget->SetDrawSize(FVector2D(100, 12));
    HealthBarWidget->SetRelativeLocation(FVector(0,0,100));
}


/* ================== BeginPlay ================== */

void AMyEnemy::BeginPlay()
{
    Super::BeginPlay();

    CurrentHealth = MaxHealth;

    // 延迟获取 Controller
    GetWorldTimerManager().SetTimerForNextTick([this]()
    {
        CachedAIController = Cast<AAIController>(GetController());
    });
}


/* ================== Tick ================== */

void AMyEnemy::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (CurrentState == EEnemyState::Dead) return;

    // 只保留血条朝向
    if (HealthBarWidget)
    {
        FVector CamLoc =
            UGameplayStatics::GetPlayerCameraManager(GetWorld(),0)->GetCameraLocation();

        FRotator Rot =
            (CamLoc - HealthBarWidget->GetComponentLocation()).Rotation();

        HealthBarWidget->SetWorldRotation(Rot);
    }
}


/* ================== 状态切换 ================== */

void AMyEnemy::SetState(EEnemyState NewState)
{
    if (CurrentState == NewState) return;

    CurrentState = NewState;
}


/* ================== 攻击 ================== */

void AMyEnemy::TryAttack()
{
    if (!bCanAttack) return;
    if (!AttackMontage) return;

    bCanAttack = false;
    CachedAIController->StopMovement();

    // ✅ 获取 AnimInstance 并绑定蒙太奇结束委托
    UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
    if (AnimInstance)
    {
        PlayAnimMontage(AttackMontage);

        FOnMontageEnded MontageEndedDelegate;
        MontageEndedDelegate.BindUObject(this, &AMyEnemy::OnAttackMontageEnded);
        AnimInstance->Montage_SetEndDelegate(MontageEndedDelegate, AttackMontage);
    }
}

// ✅ 新增：动画放完后自动调用 Reset
void AMyEnemy::OnAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
    ResetAttack();
    StopAttackTrace(); // 动画结束确保关闭检测，防止伤害判定残留
}

void AMyEnemy::ResetAttack()
{
    bCanAttack = true;

    if (CurrentState != EEnemyState::Dead)
    {
        SetState(EEnemyState::Idle);
    }
}


/* ================== 攻击轨迹 ================== */

void AMyEnemy::StartAttackTrace()
{
    LastWeaponStart = FVector::ZeroVector;
    LastWeaponEnd   = FVector::ZeroVector;

    HitActorsThisAttack.Empty();
}


void AMyEnemy::TickAttackTrace()
{
    if (!GetMesh()) return;

    FVector CurStart = GetMesh()->GetSocketLocation(WeaponStartSocket);
    FVector CurEnd   = GetMesh()->GetSocketLocation(WeaponEndSocket);

    if (LastWeaponStart.IsZero())
    {
        LastWeaponStart = CurStart;
        LastWeaponEnd   = CurEnd;
        return;
    }

    TArray<FHitResult> Hits;

    UKismetSystemLibrary::SphereTraceMulti(
        GetWorld(),
        LastWeaponStart,
        CurEnd,
        TraceRadius,
        UEngineTypes::ConvertToTraceType(ECC_Pawn),
        false,
        {this},
        EDrawDebugTrace::None,
        Hits,
        true
    );


    for (auto& Hit : Hits)
    {
        AActor* Victim = Hit.GetActor();

        if (Victim && !HitActorsThisAttack.Contains(Victim))
        {
            HitActorsThisAttack.Add(Victim);

            ProcessHit(Victim);
        }
    }

    LastWeaponStart = CurStart;
    LastWeaponEnd   = CurEnd;
}


void AMyEnemy::StopAttackTrace()
{
    HitActorsThisAttack.Empty();
}


/* ================== 命中处理 ================== */

void AMyEnemy::ProcessHit(AActor* Victim)
{
    if (!Victim) return;

    UGameplayStatics::ApplyDamage(
        Victim,
        Damage,
        GetController(),
        this,
        UDamageType::StaticClass()
    );
}


/* ================== 受击 ================== */

float AMyEnemy::TakeDamage(
    float DamageAmount,
    FDamageEvent const& DamageEvent,
    AController* EventInstigator,
    AActor* DamageCauser
)
{
    if (CurrentState == EEnemyState::Dead) return 0.f;

    CurrentHealth -= DamageAmount;

    UpdateHealthBar();

    if (CurrentHealth <= 0.f)
    {
        Die();
        return DamageAmount;
    }


    /* ---- 硬直 ---- */

    SetState(EEnemyState::Stunned);

    bCanAttack = false;

    CachedAIController->StopMovement();

    GetWorldTimerManager().ClearTimer(AttackTimer);

    if (HitMontage)
    {
        float Len = PlayAnimMontage(HitMontage);

        GetWorldTimerManager().SetTimer(
            StunTimer,
            this,
            &AMyEnemy::OnStunFinished,
            Len,
            false
        );
    }
    else
    {
        OnStunFinished();
    }

    return DamageAmount;
}


/* ================== 硬直结束 ================== */

void AMyEnemy::OnStunFinished()
{
    if (CurrentState == EEnemyState::Dead) return;

    bCanAttack = true;

    SetState(EEnemyState::Idle);
}


/* ================== 死亡 ================== */

void AMyEnemy::Die()
{
    SetState(EEnemyState::Dead);

    Tags.Remove("Enemy");

    SetActorTickEnabled(false);

    GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    CachedAIController->StopMovement();


    if (HealthBarWidget)
    {
        HealthBarWidget->SetVisibility(false);
    }

    if (DeathMontage)
    {
        float Len = PlayAnimMontage(DeathMontage);

        SetLifeSpan(Len + 2.f);
    }
    else
    {
        Destroy();
    }
}