


#include "Character/Enemy.h"
#include "Clone/CloneCharacter.h"
#include "Component/EnemyDetectionComponent.h"
#include "GAS/StatAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffectExtension.h"
#include "Components/WidgetComponent.h"
#include "Widgets/OverheadWidget.h"
#include "Kismet/GameplayStatics.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKey.h"
#include "Checkpoint/CheckpointBoundComponent.h"
#include "TimerManager.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "GAS/Ability/GABaseAttack.h"
#include "Component/AssassinateTargetComponent.h"
#include "ObjectPool/ObjectPoolSubsystem.h"

// Sets default values
AEnemy::AEnemy()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	EnemyDetectComponent = CreateDefaultSubobject<UEnemyDetectionComponent>(TEXT("EnemyDetectionComp"));
	AIControllerClass = AAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->bUseControllerDesiredRotation = false;
	GetCharacterMovement()->bCanWalkOffLedges = false;


	// 세세한 위치는 블루프린트에서 조정
	OverHeadWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("OverheadWidgetComp"));
	OverHeadWidgetComponent->SetupAttachment(RootComponent);
	OverHeadWidgetComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	CheckpointBound = CreateDefaultSubobject<UCheckpointBoundComponent>(TEXT("CheckpointBound"));
	AssassinateTargetComt = CreateDefaultSubobject<UAssassinateTargetComponent>(TEXT("AssassinateTargetComp"));
}

// Called when the game starts or when spawned
void AEnemy::BeginPlay()
{
	Super::BeginPlay();

	InitializeBehaviorArea();

	// 리스폰 시 복원할 충돌 설정 저장
	InitialCapsuleCollision = GetCapsuleComponent()->GetCollisionEnabled();
	InitialMeshCollision = GetMesh()->GetCollisionEnabled();
	InitialMeshRelativeLocation = GetMesh()->GetRelativeLocation();

	// 체력 변화 델리게이트 바인딩
	HealthChangedDelegateHandle = ASC->GetGameplayAttributeValueChangeDelegate(UStatAttributeSet::GetHealthAttribute())
		.AddUObject(this, &AEnemy::OnHealthChanged);

	// 대미지 수신 델리게이트 바인딩
	StatAttributeSet->OnDamageReceived.AddUObject(this, &AEnemy::OnDamageReceived);

	InitializeOverHeadWidget();
	if (StatAttributeSet->GetHealth() <= 0.0f)
	{
		Die();
	}
}

void AEnemy::InitializeBehaviorArea()
{
	BehaviorOrigin = GetActorLocation();
	GetCharacterMovement()->SetPlaneConstraintOrigin(BehaviorOrigin);

	// 현재 위치를 기준으로 좌우 구간 설정
	const float AreaRadius = FMath::Max(1.0f, CombatAreaRadius);
	PatrolMinX = BehaviorOrigin.X + FMath::Clamp(FMath::Min(PatrolMinOffset, PatrolMaxOffset), -AreaRadius, AreaRadius);
	PatrolMaxX = BehaviorOrigin.X + FMath::Clamp(FMath::Max(PatrolMinOffset, PatrolMaxOffset), -AreaRadius, AreaRadius);
}

void AEnemy::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(DeathDeactivateTimer);

	// ASC 바인딩 해제
	if (ASC)
	{
		ASC->GetGameplayAttributeValueChangeDelegate(UStatAttributeSet::GetHealthAttribute())
			.Remove(HealthChangedDelegateHandle);
	}

	// 대미지 수신 델리게이트 바인딩 해제
	if (StatAttributeSet)
	{
		StatAttributeSet->OnDamageReceived.RemoveAll(this);
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

		// Health를 직접 수정하는 경우에도 감지 상태를 갱신한다.
		if (Data.GEModData)
		{
			OnDamageReceived(Data.GEModData->EffectSpec.GetContext());
		}
	}
}

void AEnemy::OnDamageReceived(const FGameplayEffectContextHandle& Context)
{
	// 치명타는 OnHealthChanged에서 이미 사망 처리했으므로 감지/AI를 다시 켜지 않는다.
	if (!IsReadyForAI() || !EnemyDetectComponent)
	{
		return;
	}

	AActor* DamageInstigator = IsValid(Context.GetInstigator()) ? Context.GetInstigator() : Context.GetEffectCauser();

	EnemyDetectComponent->NotifyDamaged(DamageInstigator);
}

void AEnemy::PlayHitReaction()
{
	Super::PlayHitReaction();

	if (MonategeDuration > 0.0f)
	{
		StopHorizontalMovement();
		NextAttackTime = FMath::Max(NextAttackTime, GetWorld()->GetTimeSeconds() + MonategeDuration);
	}
}

void AEnemy::ReceiveAssassination(UAnimMontage* Montage)
{
	if (bIsDead) return;
	if (!IsValid(ASC)) return;

	// 기존 체력/사망 경로를 사용하고 이번 사망 연출만 암살 몽타주로 바꾼다.
	PendingAssassinationMontage = Montage;
	//TODO Damage를 통해서 체력 수정하도록 변경
	ASC->SetNumericAttributeBase(UStatAttributeSet::GetHealthAttribute(), 0.0f);
	if (!bIsDead)
	{
		Die();
	}
	PendingAssassinationMontage = nullptr;
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
	UAnimMontage* MontageToPlay = DeathMontage;
	if (IsValid(PendingAssassinationMontage))
	{
		MontageToPlay = PendingAssassinationMontage;
	}

	float MontageDuration = 0.0f;
	if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
	{
		if (MontageToPlay)
		{
			MontageDuration = AnimInstance->Montage_Play(MontageToPlay, 1.0f, EMontagePlayReturnType::Duration);
			if (MontageDuration > 0.0f)
			{
				FOnMontageBlendingOutStarted BlendOutDelegate;
				BlendOutDelegate.BindWeakLambda(this, [this, MontageToPlay](UAnimMontage* Montage, bool bInterrupted)
				{
					if (!bIsDead) return;
					if (Montage != MontageToPlay) return;
					GetMesh()->bPauseAnims = true;
				});
				AnimInstance->Montage_SetBlendingOutDelegate(BlendOutDelegate, MontageToPlay);
			}
		}
	}

	if (MontageDuration <= 0.0f)
	{
		// A missing/incompatible montage must not leave an immortal corpse.
		GetMesh()->bPauseAnims = true;
		UE_LOG(LogTemp, Warning, TEXT("%s: Death montage was not set or could not be played."), *GetName());
	}
	// 체크포인트 리스폰으로 되살릴 수 있도록 파괴하지 않고, 연출이 끝나면 숨긴다.
	GetWorldTimerManager().SetTimer(DeathDeactivateTimer, this, &AEnemy::DeactivateAfterDeath,
		FMath::Max(0.01f, MontageDuration + FMath::Max(0.0f, DeathDespawnDelay)), false);
}

void AEnemy::DeactivateAfterDeath()
{
	if (!bIsDead)
	{
		return;
	}

	// 풀에서 스폰된 적이면 풀에 반환한다. (레벨에 배치된 적은 풀이 관리하지 않으므로 false가 반환된다)
	if (UObjectPoolSubsystem* PoolSubsystem = GetWorld()->GetSubsystem<UObjectPoolSubsystem>())
	{
		if (PoolSubsystem->ReturnToPool(this))
		{
			return;
		}
	}

	// 충돌, 이동, 틱, AI는 Die()에서 이미 정지했으므로 외형만 숨긴다.
	SetActorHiddenInGame(true);
	GetMesh()->bPauseAnims = true;
}

void AEnemy::ResetToInitial_Implementation()
{
	// 트랜스폼은 CheckpointBound 컴포넌트가 이미 복원했다. 여기서는 자기 자신의 상태만 초기화한다.
	GetWorldTimerManager().ClearTimer(DeathDeactivateTimer);

	// 진행 중인 공격 어빌리티 정리 (EndAbility가 몽타주 태스크와 태그를 정리)
	ASC->CancelAllAbilities();

	// 사망/전투 상태 초기화
	bIsDead = false;
	BehaviorState = EEnemyBehaviorState::Patrol;
	CombatTarget.Reset();
	bPatrolTowardsMax = true;
	NextAttackTime = 0.0;
	MonategeDuration = 0.0f;

	// 체력 회복. 증가 변화이므로 OnHealthChanged에서 피격/사망으로 처리되지 않고, 오버헤드 위젯은 델리게이트로 갱신된다.
	ASC->SetNumericAttributeBase(UStatAttributeSet::GetHealthAttribute(), StatAttributeSet->GetMaxHealth());

	// 사망 몽타주와 정지된 포즈 해제
	GetMesh()->bPauseAnims = false;
	if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
	{
		AnimInstance->StopAllMontages(0.0f);
	}

	// 충돌, 이동 복원
	GetCapsuleComponent()->SetCollisionEnabled(InitialCapsuleCollision);
	GetMesh()->SetCollisionEnabled(InitialMeshCollision);
	GetMesh()->SetRelativeLocation(InitialMeshRelativeLocation);

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->StopMovementImmediately();
	Movement->SetDefaultMovementMode();
	Movement->SetPlaneConstraintOrigin(BehaviorOrigin);

	// 외형, UI, 틱 복원
	SetActorHiddenInGame(false);
	OverHeadWidgetComponent->SetVisibility(true, true);
	SetActorTickEnabled(true);

	// 감지 게이지/경계 단계 초기화
	EnemyDetectComponent->ResetDetection();
}

void AEnemy::OnAfterReset_Implementation()
{
	RestartAI();
}

void AEnemy::OnSpawnFromPool_Implementation()
{
	// 트랜스폼은 풀이 이미 설정했다. 꺼낸 위치를 새 활동 영역의 중심으로 삼는다.
	InitializeBehaviorArea();

	// 체크포인트 리스폰과 같은 2단계 초기화 (BP 오버라이드 포함)
	ICheckpointResettable::Execute_ResetToInitial(this);
	ICheckpointResettable::Execute_OnAfterReset(this);
}

void AEnemy::OnReturnToPool_Implementation()
{
	GetWorldTimerManager().ClearTimer(DeathDeactivateTimer);

	// 진행 중인 공격 어빌리티 정리
	ASC->CancelAllAbilities();

	// 풀 대기 중에는 AI 판정(IsReadyForAI)과 피격 처리에서 제외되도록 사망 상태로 둔다. (스폰 시 ResetToInitial에서 해제)
	bIsDead = true;
	BehaviorState = EEnemyBehaviorState::Dead;
	CombatTarget.Reset();

	// AI 컨트롤러는 별도 액터라 풀의 공통 비활성화 대상이 아니므로 직접 정지
	if (AAIController* AIController = Cast<AAIController>(GetController()))
	{
		AIController->StopMovement();
		if (UBrainComponent* Brain = AIController->GetBrainComponent())
		{
			Brain->StopLogic(TEXT("ReturnToPool"));
		}
	}

	if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
	{
		AnimInstance->StopAllMontages(0.0f);
	}

	// 뷰포트에 붙은 발각 범위 UI는 액터 숨김으로 가려지지 않으므로 직접 제거
	EnemyDetectComponent->StopDetection();
}

void AEnemy::RestartAI()
{
	AAIController* AIController = Cast<AAIController>(GetController());
	if (!AIController)
	{
		return;
	}

	AIController->StopMovement();

	UBehaviorTreeComponent* BTComp = Cast<UBehaviorTreeComponent>(AIController->GetBrainComponent());
	if (!BTComp)
	{
		return;
	}

	// 실행 중인 태스크를 즉시 중단 (사망 시 이미 정지된 경우에도 안전)
	BTComp->StopTree(EBTStopMode::Forced);

	// 블랙보드 값을 모두 비운 뒤 SelfActor와 감지 상태를 다시 기록한다.
	// (같은 에셋으로 InitializeBlackboard를 호출하면 기존 값이 유지되므로 직접 비운다)
	if (UBlackboardComponent* BB = AIController->GetBlackboardComponent())
	{
		for (int32 KeyIndex = 0; KeyIndex < BB->GetNumKeys(); ++KeyIndex)
		{
			BB->ClearValue(FBlackboard::FKey(KeyIndex));
		}
		BB->SetValueAsObject(FBlackboard::KeySelf, this);
	}
	EnemyDetectComponent->RefreshBlackboard();

	// 정지된 트리는 마지막으로 실행한 에셋으로 루트부터 다시 시작한다.
	BTComp->RestartTree();
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
}


bool AEnemy::IsLivingPlayer(const APawn* Player) const
{
	// Keep the Blueprint-facing function name; clones do not have a player controller.
	if (!IsValid(Player) || Player->IsActorBeingDestroyed() || Player == this
		|| (!Player->IsPlayerControlled() && !Player->IsA<ACloneCharacter>()))
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
	// 공격을 시작한 타겟이 여전히 추적 가능한 경우에만 반환한다.
	APawn* Target = CombatTarget.Get();
	return IsValidAITarget(Target) ? Target : nullptr;
}

bool AEnemy::IsAttackActive() const
{
	const FGameplayAbilitySpec* Spec = FindAbilitySpecByInputID(EAbilityInputID::BasicAttack);
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
		if (const FGameplayAbilitySpec* Spec = FindAbilitySpecByInputID(EAbilityInputID::BasicAttack))
		{
			ASC->CancelAbilityHandle(Spec->Handle);
		}
	}
	BehaviorState = NewState;
	StopHorizontalMovement();
}

void AEnemy::StartPatrol()
{
	if (bIsDead)
	{
		return;
	}
	SetBehaviorState(EEnemyBehaviorState::Patrol);
	CombatTarget.Reset();
}

FVector AEnemy::ChooseNextPatrolLocation()
{
	if (!IsReadyForAI()) 
		return GetActorLocation();

	StartPatrol();
	const float X = GetActorLocation().X;
	if (bPatrolTowardsMax && X >= PatrolMaxX - 10.0f) 
		bPatrolTowardsMax = false;
	else if (!bPatrolTowardsMax && X <= PatrolMinX + 10.0f) 
		bPatrolTowardsMax = true;

	FVector Location = BehaviorOrigin;
	Location.X = bPatrolTowardsMax ? PatrolMaxX : PatrolMinX;
	return Location;
}

bool AEnemy::IsValidAITarget(AActor* Target) const
{
	// 활동 영역은 검사하지 않는다. 시야를 잃으면 마지막 목격 위치를 거쳐 순찰 구간으로 돌아온다.
	return IsReadyForAI() && IsLivingPlayer(Cast<APawn>(Target));
}

bool AEnemy::IsWithinAttackRange(AActor* Target) const
{
	return IsValidAITarget(Target)
		&& FVector::DistSquared(GetActorLocation(), Target->GetActorLocation()) <= FMath::Square(FMath::Max(1.0f, AttackDistance));
}

void AEnemy::FaceLocation(const FVector& Location)
{
	FaceDirection(Location.X - GetActorLocation().X);
}

bool AEnemy::TryStartAIBaseAttack(AActor* Target)
{
	if (!IsWithinAttackRange(Target) || IsPlayingHitReaction() || !ASC || IsAttackActive()) return false;
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now < NextAttackTime) return false;
	CombatTarget = Cast<APawn>(Target);
	SetBehaviorState(EEnemyBehaviorState::Attack);
	StopHorizontalMovement();
	FaceLocation(Target->GetActorLocation());
	NextAttackTime = Now + FMath::Max(0.1f, AttackInterval);
	return BasicAttackMontage && TryActivateAbilityByInputID(EAbilityInputID::BasicAttack);
}

void AEnemy::CancelAIBaseAttack()
{
	if (const FGameplayAbilitySpec* Spec = FindAbilitySpecByInputID(EAbilityInputID::BasicAttack))
	{
		ASC->CancelAbilityHandle(Spec->Handle);
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

