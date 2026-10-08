


#include "Character/Enemy.h"
#include "GAS/StatAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Components/WidgetComponent.h"
#include "Widgets/OverheadWidget.h"
#include "Kismet/GameplayStatics.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "GAS/Ability/GABaseAttack.h"

// Sets default values
AEnemy::AEnemy()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	AIControllerClass = AAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->bUseControllerDesiredRotation = false;
	GetCharacterMovement()->bCanWalkOffLedges = false;


	// 세세한 위치는 블루프린트에서 조정
	OverHeadWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("OverheadWidgetComp"));
	OverHeadWidgetComponent->SetupAttachment(RootComponent);
	OverHeadWidgetComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

// Called when the game starts or when spawned
void AEnemy::BeginPlay()
{
	Super::BeginPlay();

	BehaviorOrigin = GetActorLocation();
	GetCharacterMovement()->SetPlaneConstraintOrigin(BehaviorOrigin);

	// 현재 위치를 기준으로 좌우 구간 설정
	const float AreaRadius = FMath::Max(1.0f, CombatAreaRadius);
	PatrolMinX = BehaviorOrigin.X + FMath::Clamp(FMath::Min(PatrolMinOffset, PatrolMaxOffset), -AreaRadius, AreaRadius);
	PatrolMaxX = BehaviorOrigin.X + FMath::Clamp(FMath::Max(PatrolMinOffset, PatrolMaxOffset), -AreaRadius, AreaRadius);

	// 체력 변화 델리게이트 바인딩
	HealthChangedDelegateHandle = ASC->GetGameplayAttributeValueChangeDelegate(UStatAttributeSet::GetHealthAttribute())
		.AddUObject(this, &AEnemy::OnHealthChanged);

	InitializeOverHeadWidget();
	if (StatAttributeSet->GetHealth() <= 0.0f)
	{
		Die();
	}
}

void AEnemy::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// ASC 바인딩 해제
	if (ASC)
	{
		ASC->GetGameplayAttributeValueChangeDelegate(UStatAttributeSet::GetHealthAttribute())
			.Remove(HealthChangedDelegateHandle);
	}

	Super::EndPlay(EndPlayReason);
}

void AEnemy::OnHealthChanged(const FOnAttributeChangeData& Data)
{
	// 체력이 0이면 사망하고, 살아 있는 상태의 체력 감소는 피격으로 처리한다.
	if (bIsDead)
	{
		return;
	}
	if (Data.NewValue <= 0.0f)
	{
		Die();
		return;
	}
	if (Data.NewValue < Data.OldValue)
	{
		PlayHitReaction();
	}
}

void AEnemy::PlayHitReaction()
{
	// 이미 죽었거나, 몽타주가 없으면 종료
	if (bIsDead || !HitReactionMontage)
	{
		return;
	}

	// 공격 쿨타임에 몽타주 재생 시간이 추가되도록 조정
	if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
	{
		// Restart on a new hit. Death is checked first and can replace this montage.
		const float Duration = AnimInstance->Montage_Play(HitReactionMontage, 1.0f, EMontagePlayReturnType::Duration);
		if (Duration > 0.0f)
		{
			StopHorizontalMovement();
			NextAttackTime = FMath::Max(NextAttackTime, GetWorld()->GetTimeSeconds() + Duration);
		}
	}
}

void AEnemy::Die()
{
	// 사망은 한 번만 처리하고 공격, AI, 이동과 충돌을 정리한다.
	if (bIsDead)
	{
		return;
	}

	//사망 처리
	bIsDead = true;
	BehaviorState = EEnemyBehaviorState::Dead;
	CombatTarget.Reset();
	ASC->CancelAllAbilities();

	// AI 컨트롤러 정지
	if (AAIController* AIController = Cast<AAIController>(GetController()))
	{
		AIController->StopMovement();
		if (UBrainComponent* Brain = AIController->GetBrainComponent())
		{
			Brain->StopLogic(TEXT("Death"));
		}
	}

	// 정지 후 움직이지 못하게 세팅
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();

	// 충돌처리 제거
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// 오버헤드 위젯 제거
	OverHeadWidgetComponent->SetVisibility(false, true);
	SetActorTickEnabled(false);

	// 사망 몽타주 재생
	float MontageDuration = 0.0f;
	if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
	{
		if (DeathMontage)
		{
			MontageDuration = AnimInstance->Montage_Play(DeathMontage, 1.0f, EMontagePlayReturnType::Duration);
			if (MontageDuration > 0.0f)
			{
				FOnMontageBlendingOutStarted BlendOutDelegate;
				BlendOutDelegate.BindUObject(this, &AEnemy::OnDeathMontageBlendingOut);
				AnimInstance->Montage_SetBlendingOutDelegate(BlendOutDelegate, DeathMontage);
			}
		}
	}

	if (MontageDuration <= 0.0f)
	{
		// A missing/incompatible montage must not leave an immortal corpse.
		GetMesh()->bPauseAnims = true;
		UE_LOG(LogTemp, Warning, TEXT("%s: Death montage was not set or could not be played."), *GetName());
	}
	// SetLifeSpan(0) cancels destruction, so keep a small positive minimum.
	SetLifeSpan(FMath::Max(0.01f, MontageDuration + FMath::Max(0.0f, DeathDespawnDelay)));
}

void AEnemy::OnDeathMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted)
{
	if (bIsDead && Montage == DeathMontage)
	{
		// Hold the death pose instead of blending back to the locomotion graph.
		GetMesh()->bPauseAnims = true;
	}
}

// Called every frame
void AEnemy::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 위젯 빌보드
	if (bFaceCamera)
	{
		UpdateOverheadWidgetRotation();
	}

	// State에 따른 행동
	if (!bIsDead)
	{
		UpdateBehavior(DeltaTime);
	}
}


bool AEnemy::IsLivingPlayer(const APawn* Player) const
{
	if (!IsValid(Player) || Player == this || !Player->IsPlayerControlled())
	{
		return false;
	}
	const IAbilitySystemInterface* AbilityOwner = Cast<IAbilitySystemInterface>(Player);
	const UAbilitySystemComponent* PlayerASC = AbilityOwner ? AbilityOwner->GetAbilitySystemComponent() : nullptr;

	// ASC가 없거나, 어트리뷰트가 없거나, 체력이 0보다 크면 살아 있다고 판단
	return !PlayerASC || !PlayerASC->GetSet<UStatAttributeSet>()
		|| PlayerASC->GetNumericAttribute(UStatAttributeSet::GetHealthAttribute()) > 0.0f;
}

bool AEnemy::IsInsideCombatArea(const FVector& Location) const
{
	return FVector::DistSquared(Location, BehaviorOrigin) <= FMath::Square(FMath::Max(1.0f, CombatAreaRadius));
}

AActor* AEnemy::GetCombatTarget() const
{
	const APawn* Target = CombatTarget.Get();
	const float RetainRadius = FMath::Max(FMath::Max(1.0f, DetectionRadius), LoseTargetRadius);
	if (bIsDead || !IsLivingPlayer(Target) || !IsInsideCombatArea(Target->GetActorLocation())
		|| FVector::DistSquared(GetActorLocation(), Target->GetActorLocation()) > FMath::Square(RetainRadius))
	{
		return nullptr;
	}
	return CombatTarget.Get();
}

bool AEnemy::IsAttackActive() const
{
	const FGameplayAbilitySpec* Spec = BasicAttackAbilityClass ? ASC->FindAbilitySpecFromClass(BasicAttackAbilityClass) : nullptr;
	return Spec && Spec->IsActive();
}

bool AEnemy::IsPlayingHitReaction() const
{
	const UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	return HitReactionMontage && AnimInstance && AnimInstance->Montage_IsPlaying(HitReactionMontage);
}

void AEnemy::StopHorizontalMovement()
{
	ConsumeMovementInputVector();
	// Keep gravity/falling velocity when cancelling horizontal movement.
	FVector Velocity = GetCharacterMovement()->Velocity;
	Velocity.X = 0.0f;
	Velocity.Y = 0.0f;
	GetCharacterMovement()->Velocity = Velocity;
}

void AEnemy::FaceDirection(float DirectionX)
{
	if (!FMath::IsNearlyZero(DirectionX))
	{
		SetActorRotation(FRotator(0.0f, DirectionX > 0.0f ? 0.0f : 180.0f, 0.0f));
	}
}

void AEnemy::MoveAlongX(float DestinationX, float Speed, float DeltaTime)
{
	const float DistanceX = DestinationX - GetActorLocation().X;
	if (FMath::Abs(DistanceX) <= 5.0f || DeltaTime <= 0.0f)
	{
		StopHorizontalMovement();
		return;
	}

	FaceDirection(DistanceX);
	GetCharacterMovement()->MaxWalkSpeed = FMath::Min(FMath::Max(1.0f, Speed), FMath::Abs(DistanceX) / DeltaTime);

	AddMovementInput(FVector::ForwardVector, FMath::Sign(DistanceX));
}

void AEnemy::SetBehaviorState(EEnemyBehaviorState NewState)
{
	if (BehaviorState == NewState)
	{
		return;
	}
	if (NewState == EEnemyBehaviorState::Idle || NewState == EEnemyBehaviorState::Patrol)
	{
		// Cancel attack before losing its tracked target. EndAbility clears montage tasks/tags.
		if (FGameplayAbilitySpec* Spec = BasicAttackAbilityClass ? ASC->FindAbilitySpecFromClass(BasicAttackAbilityClass) : nullptr)
		{
			ASC->CancelAbilityHandle(Spec->Handle);
		}
	}
	BehaviorState = NewState;
	StopHorizontalMovement();
}

void AEnemy::LoseCombatTarget()
{
	// 목표를 잃으면 순찰 상태로 돌아간다.
	SetBehaviorState(EEnemyBehaviorState::Patrol);
	CombatTarget.Reset();
}

void AEnemy::StartPatrol()
{
	if (bIsDead)
	{
		return;
	}
	SetBehaviorState(EEnemyBehaviorState::Patrol);
	CombatTarget.Reset();
	PatrolWaitRemaining = 0.0f;
}

void AEnemy::UpdatePatrol(float DeltaTime)
{
	// 양 끝에서 잠시 대기한 뒤 반대 방향으로 왕복한다.
	if (PatrolWaitRemaining > 0.0f)
	{
		PatrolWaitRemaining = FMath::Max(0.0f, PatrolWaitRemaining - DeltaTime);
		StopHorizontalMovement();
		return;
	}
	if (PatrolMaxX - PatrolMinX <= 10.0f)
	{
		StopHorizontalMovement();
		return;
	}
	const float CurrentX = GetActorLocation().X;
	const float DestinationX = bPatrolTowardsMax ? PatrolMaxX : PatrolMinX;
	if ((bPatrolTowardsMax && CurrentX >= DestinationX - 5.0f)
		|| (!bPatrolTowardsMax && CurrentX <= DestinationX + 5.0f))
	{
		bPatrolTowardsMax = !bPatrolTowardsMax;
		PatrolWaitRemaining = FMath::Max(0.0f, PatrolWaitTime);
		StopHorizontalMovement();
		return;
	}
	MoveAlongX(DestinationX, PatrolSpeed, DeltaTime);
}

void AEnemy::UpdateBehavior(float DeltaTime)
{
	// 전투 영역과 목표 유효성을 확인한 뒤 순찰, 추적, 공격을 선택한다.
	if (!IsInsideCombatArea(GetActorLocation()))
	{
		LoseCombatTarget();
		return;
	}
	APawn* Target = CombatTarget.Get();
	if (Target || BehaviorState == EEnemyBehaviorState::Chase || BehaviorState == EEnemyBehaviorState::Attack)
	{
		const float RetainRadius = FMath::Max(FMath::Max(1.0f, DetectionRadius), LoseTargetRadius);
		if (!IsLivingPlayer(Target) || !IsInsideCombatArea(Target->GetActorLocation())
			|| FVector::DistSquared(GetActorLocation(), Target->GetActorLocation()) > FMath::Square(RetainRadius))
		{
			LoseCombatTarget();
			return;
		}
	}
	else
	{
		APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
		if (IsLivingPlayer(Player) && IsInsideCombatArea(Player->GetActorLocation())
			&& FVector::DistSquared(GetActorLocation(), Player->GetActorLocation()) <= FMath::Square(FMath::Max(1.0f, DetectionRadius)))
		{
			CombatTarget = Player;
			Target = Player;
			SetBehaviorState(EEnemyBehaviorState::Chase);
		}
	}
	if (IsPlayingHitReaction())
	{
		StopHorizontalMovement();
		return;
	}
	if (!Target)
	{
		if (BehaviorState == EEnemyBehaviorState::Patrol)
		{
			UpdatePatrol(DeltaTime);
		}
		else
		{
			StopHorizontalMovement();
		}
		return;
	}
	// 진행 중인 공격이 끝난 뒤 방향 전환과 추적을 재개한다.
	if (IsAttackActive())
	{
		StopHorizontalMovement();
		return;
	}
	const float Distance = FVector::Distance(GetActorLocation(), Target->GetActorLocation());
	const float DirectionX = Target->GetActorLocation().X - GetActorLocation().X;
	if (Distance > FMath::Max(1.0f, AttackDistance))
	{
		SetBehaviorState(EEnemyBehaviorState::Chase);
		MoveAlongX(Target->GetActorLocation().X, ChaseSpeed, DeltaTime);
		return;
	}
	SetBehaviorState(EEnemyBehaviorState::Attack);
	StopHorizontalMovement();
	FaceDirection(DirectionX);
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now >= NextAttackTime)
	{
		// Also throttle failed activation (e.g. no montage assigned).
		NextAttackTime = Now + FMath::Max(0.1f, AttackInterval);
		if (BasicAttackAbilityClass)
		{
			ASC->TryActivateAbilityByClass(BasicAttackAbilityClass);
		}
	}
}

// 머리 위 체력 UI를 이 적의 GAS 정보에 연결한다.
void AEnemy::InitializeOverHeadWidget()
{
	if (!OverHeadWidgetComponent) return;

	//UE_LOG(LogTemp, Warning, TEXT("OverHeadWidgetComponent 있음"));

	if (UUserWidget* UserWidget = OverHeadWidgetComponent->GetUserWidgetObject())
	{
		//UE_LOG(LogTemp, Warning, TEXT("UserWidget 있음"));
		if (UOverheadWidget* OverheadWidget = Cast<UOverheadWidget>(UserWidget))
		{
			//UE_LOG(LogTemp, Warning, TEXT("UOverHeadWidget 캐스트 성공"));
			OverheadWidget->InitWidget(this);
		}
	}
}

void AEnemy::UpdateOverheadWidgetRotation()
{
	if (!OverHeadWidgetComponent) return;

	if (APlayerCameraManager* CameraManager = UGameplayStatics::GetPlayerCameraManager(this, 0))
	{
		// 카메라의 전방 벡터와 정확히 마주보는 방향(-CameraForward, 사이각 180도)으로 회전
		const FVector CameraForward = CameraManager->GetCameraRotation().Vector();
		FRotator WidgetRotation = (-CameraForward).Rotation();

		if (bLockWidgetPitch)
		{
			WidgetRotation.Pitch = 0.0f;
		}
		if (bLockWidgetRoll)
		{
			WidgetRotation.Roll = 0.0f;
		}

		OverHeadWidgetComponent->SetWorldRotation(WidgetRotation);
	}
}

