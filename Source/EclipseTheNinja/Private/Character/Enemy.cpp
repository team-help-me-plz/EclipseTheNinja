


#include "Character/Enemy.h"
#include "Clone/CloneCharacter.h"
#include "Component/EnemyDetectionComponent.h"
#include "GAS/StatAttributeSet.h"
#include "GAS/Effects/GEDefaultStats.h"
#include "GAS/Effects/GEBasicAttackDamage.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffectExtension.h"
#include "Components/WidgetComponent.h"
#include "NiagaraSystem.h"
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
#include "Component/AssassinationDropComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "ObjectPool/ObjectPoolSubsystem.h"
#include "Components/SphereComponent.h"
#include "Noise/NoiseTypes.h"
#include "GAS/Tags/EclipseGameplayTags.h"
#include "Util/EclipseCollisionChannels.h"
#include "Component/EnemySpacingComponent.h"

// Sets default values
AEnemy::AEnemy()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	EnemyDetectComponent = CreateDefaultSubobject<UEnemyDetectionComponent>(TEXT("EnemyDetectionComp"));

	// 추격·공격 중 간격 유지, 포위, 낭떠러지 검사
	SpacingComp = CreateDefaultSubobject<UEnemySpacingComponent>(TEXT("SpacingComp"));

	AIControllerClass = AAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	// 기본 적은 근거리 적 스탯으로 시작한다.
	DefaultStatsEffect = UGEMeleeEnemyDefaultStats::StaticClass();

	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->bUseControllerDesiredRotation = false;
	GetCharacterMovement()->bCanWalkOffLedges = false;


	// 세세한 위치는 블루프린트에서 조정
	OverHeadWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("OverheadWidgetComp"));
	OverHeadWidgetComponent->SetupAttachment(RootComponent);
	OverHeadWidgetComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	CheckpointBound = CreateDefaultSubobject<UCheckpointBoundComponent>(TEXT("CheckpointBound"));
	AssassinateTargetComt = CreateDefaultSubobject<UAssassinateTargetComponent>(TEXT("AssassinateTargetComp"));

	// 기본으로 모든 적이 암살 처치 시 회복 아이템을 떨어뜨린다.
	AssassinationDropComp = CreateDefaultSubobject<UAssassinationDropComponent>(TEXT("AssassinationDropComp"));
	static ConstructorHelpers::FClassFinder<AActor> HealPickupClass(TEXT("/Game/Blueprint/Item/BP_HealPickup"));
	if (HealPickupClass.Succeeded())
	{
		AssassinationDropComp->SetDropActorClass(HealPickupClass.Class);
	}
	NoiseDetectComponent = CreateDefaultSubobject<USphereComponent>(TEXT("NoiseDetectComponent"));
	NoiseDetectComponent->SetupAttachment(RootComponent);
	NoiseDetectComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	NoiseDetectComponent->SetCollisionObjectType(ECC_Noise);
	NoiseDetectComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	NoiseDetectComponent->SetCollisionResponseToChannel(ECC_Noise, ECR_Overlap);
	NoiseDetectComponent->SetGenerateOverlapEvents(false);
}

// Called when the game starts or when spawned
void AEnemy::BeginPlay()
{
	Super::BeginPlay();

	InitializeBehaviorArea();

	if (bOverlapWithCharacters)
	{
		// 몬스터 캡슐은 Monster 채널을 쓰고 몬스터끼리는 통과한다. 간격은 MoveAlongX가 유지한다.
		// Blueprint에 저장된 캡슐 충돌 설정보다 우선하도록 런타임에 지정한다.
		GetCapsuleComponent()->SetCollisionObjectType(ECC_Monster);
		GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Monster, ECR_Ignore);

		// 메시도 캐릭터 캡슐을 막지 않게 한다. 막으면 다른 몬스터 메시를 밟고 올라가 JumpOff로 튄다.
		GetMesh()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		GetMesh()->SetCollisionResponseToChannel(ECC_Monster, ECR_Ignore);
	}

	if (SpacingComp->IsAvoidingLedges())
	{
		// Blueprint에 예전 기본값(true)이 저장된 적도 낭떠러지에서 떨어지지 않게 런타임에 다시 지정한다.
		GetCharacterMovement()->bCanWalkOffLedges = false;
	}

	// 리스폰 시 복원할 충돌 설정 저장
	// 풀은 액터 충돌을 끈 채 BeginPlay를 부르므로, 액터 설정을 빼고 컴포넌트 자체 값을 읽는다.
	InitialCapsuleCollision = GetCapsuleComponent()->BodyInstance.GetCollisionEnabled(false);
	InitialMeshCollision = GetMesh()->BodyInstance.GetCollisionEnabled(false);
	InitialMeshRelativeLocation = GetMesh()->GetRelativeLocation();

	if (bOverlapWithCharacters)
	{
		// 몬스터가 돌아설 때마다 메시 오프셋을 다시 맞춘다.
		GetRootComponent()->TransformUpdated.AddUObject(this, &AEnemy::OnRootTransformUpdated);
		UpdateMeshYOffset();
	}

	// 체력 변화 델리게이트 바인딩
	HealthChangedDelegateHandle = ASC->GetGameplayAttributeValueChangeDelegate(UStatAttributeSet::GetHealthAttribute())
		.AddUObject(this, &AEnemy::OnHealthChanged);

	// 대미지 수신 델리게이트 바인딩
	StatAttributeSet->OnDamageReceived.AddUObject(this, &AEnemy::OnDamageReceived);
	StatAttributeSet->OnDotDamageReceived.AddUObject(this, &AEnemy::OnDotDamageReceived);

	// 무적(보스 에너지 링크 등)으로 막힌 공격도 공격자를 알아채도록 같은 감지 갱신을 한다.
	StatAttributeSet->OnDamageBlocked.AddUObject(this, &AEnemy::OnDamageReceived);

	InitializeOverHeadWidget();
	if (StatAttributeSet->GetHealth() <= 0.0f)
	{
		Die();
	}

	// 적 감지상태 변화 델리게이트 바인딩
	EnemyDetectComponent->OnAwarenessChanged.AddDynamic(this, &AEnemy::HandleAwarenessChanged);
	// 소리 감지 원 크기 초기화
	SyncNoiseDetectRadius(this->LastNoiseSyncLevel);
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

	// 메시 Y 오프셋용 루트 트랜스폼 바인딩 해제
	if (GetRootComponent())
	{
		GetRootComponent()->TransformUpdated.RemoveAll(this);
	}

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
		StatAttributeSet->OnDotDamageReceived.RemoveAll(this);
		StatAttributeSet->OnDamageBlocked.RemoveAll(this);
	}

	Super::EndPlay(EndPlayReason);
}

void AEnemy::OnHealthChanged(const FOnAttributeChangeData& Data)
{
	// 도트데미지인지 아닌지 바로 확인하고 초기화
	const bool bIsDoT = bIsDotDamage;
	bIsDotDamage = false;

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
		// 화상 데미지가 아닐 때만 피격 애니메이션을 실행한다.
		if (!bIsDoT)
		{
			PlayHitReaction();
		}
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

void AEnemy::OnDotDamageReceived(const FGameplayEffectContextHandle& Context)
{
	bIsDotDamage = true;

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

void AEnemy::ReceiveAssassination(UAnimMontage* Montage, float InDamage)
{
	if (bIsDead) return;
	if (!IsValid(ASC)) return;
	if (!FMath::IsFinite(InDamage) || InDamage <= 0.0f) return;

	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(UGEBasicAttackDamage::StaticClass(), 1.0f, Context);
	if (!Spec.IsValid()) return;

	Spec.Data->SetSetByCallerMagnitude(FName(TEXT("Damage")), InDamage);

	// 피해로 체력이 0이 되면 OnHealthChanged에서 암살 몽타주로 사망 처리한다.
	PendingAssassinationMontage = Montage;
	ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());

	if (!bIsDead)
	{
		// 생존 리액션이 없으면 사망 몽타주로 대체하지 않는다.
		if (IsValid(Montage))
		{
			if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
			{
				AnimInstance->Montage_Play(Montage);
			}
		}
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

	if (ASC->HasMatchingGameplayTag(EclipseGameplayTags::State_Debuff_Burning))
	{
		ASC->RemoveActiveEffectsWithGrantedTags(FGameplayTagContainer(EclipseGameplayTags::State_Debuff_Burning));
		if (!ASC->IsGameplayCueActive(EclipseGameplayTags::GameplayCue_Enemy_Burn))
		{
			ASC->AddGameplayCue(EclipseGameplayTags::GameplayCue_Enemy_Burn);
		}
	}

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

	// PendingAssassinationMontage는 ReceiveAssassination 안에서만 유효하므로 여기서 암살 여부를 함께 알린다.
	OnDied.Broadcast(this, IsValid(PendingAssassinationMontage));
}

void AEnemy::DeactivateAfterDeath()
{
	if (!bIsDead)
	{
		return;
	}

	// 풀 반환 시 대기 위치로 옮겨지므로, 사라지기 전 위치에서 알린다.
	OnDeathFinished.Broadcast(this);

	// 풀에서 스폰된 적이면 풀에 반환한다. (레벨에 배치된 적은 풀이 관리하지 않으므로 false가 반환된다)
	if (UObjectPoolSubsystem* PoolSubsystem = GetWorld()->GetSubsystem<UObjectPoolSubsystem>())
	{
		if (PoolSubsystem->ReturnToPool(this))
		{
			return;
		}
	}

	// 화상 상태 및 화상 틱 제거
	ASC->RemoveActiveEffectsWithGrantedTags(FGameplayTagContainer(EclipseGameplayTags::State_Debuff_Burning));
	ASC->RemoveGameplayCue(EclipseGameplayTags::GameplayCue_Enemy_Burn);
	
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
	ParryStunEndTime = 0.0;
	NextAttackTime = 0.0;
	MonategeDuration = 0.0f;

	// 체력 회복. 증가 변화이므로 OnHealthChanged에서 피격/사망으로 처리되지 않고, 오버헤드 위젯은 델리게이트로 갱신된다.
	ASC->SetNumericAttributeBase(UStatAttributeSet::GetHealthAttribute(), StatAttributeSet->GetMaxHealth());

	// 화상 상태 및 화상 큐 제거
	ASC->RemoveActiveEffectsWithGrantedTags(FGameplayTagContainer(EclipseGameplayTags::State_Debuff_Burning));
	ASC->RemoveGameplayCue(EclipseGameplayTags::GameplayCue_Enemy_Burn);

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
	AppliedMeshYOffset = FVector::ZeroVector;
	UpdateMeshYOffset();

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

	// 화상 상태 및 화상 큐 제거
	ASC->RemoveActiveEffectsWithGrantedTags(FGameplayTagContainer(EclipseGameplayTags::State_Debuff_Burning));
	ASC->RemoveGameplayCue(EclipseGameplayTags::GameplayCue_Enemy_Burn);

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

void AEnemy::SuspendAIForScriptedMove()
{
	// BT가 이동 입력을 넣지 않도록 먼저 멈춘다.
	if (AAIController* AIController = Cast<AAIController>(GetController()))
	{
		AIController->StopMovement();

		if (UBrainComponent* Brain = AIController->GetBrainComponent())
		{
			Brain->StopLogic(TEXT("ScriptedMove"));
		}
	}

	StopHorizontalMovement();
	BehaviorState = EEnemyBehaviorState::Idle;
	CombatTarget.Reset();

	// 연출 중에는 발각 범위 UI와 감지를 끈다.
	EnemyDetectComponent->StopDetection();
}

void AEnemy::ResumeAIAfterScriptedMove(AActor* CombatActor)
{
	if (bIsDead)
	{
		return;
	}

	// 연출로 옮겨 온 자리를 새 활동 영역과 이동 평면의 기준으로 삼는다.
	InitializeBehaviorArea();

	// 감지를 다시 켜고, 대상이 있으면 맞았을 때처럼 바로 전투 상태로 만든다.
	EnemyDetectComponent->ResetDetection();

	if (CombatActor)
	{
		EnemyDetectComponent->NotifyDamaged(CombatActor);
	}

	RestartAI();
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
	// 회수되어 풀에서 대기 중인 분신은 대상이 아니다.
	const ACloneCharacter* Clone = Cast<ACloneCharacter>(Player);
	if (Clone && !Clone->IsSpawned())
	{
		return false;
	}
	const IAbilitySystemInterface* AbilityOwner = Cast<IAbilitySystemInterface>(Player);
	const UAbilitySystemComponent* PlayerASC = AbilityOwner ? AbilityOwner->GetAbilitySystemComponent() : nullptr;

	// ASC가 없거나, 어트리뷰트가 없거나, 체력이 0보다 크면 살아 있다고 판단
	return !PlayerASC || !PlayerASC->GetSet<UStatAttributeSet>()
		|| PlayerASC->GetNumericAttribute(UStatAttributeSet::GetHealthAttribute()) > 0.0f;
}

void AEnemy::HandleAwarenessChanged(float Gauge, EEnemyAlertLevel Level)
{
	if (Level != LastNoiseSyncLevel)
	{
		SyncNoiseDetectRadius(Level);
	}
}

void AEnemy::SyncNoiseDetectRadius(EEnemyAlertLevel Level)
{
	LastNoiseSyncLevel = Level;
	NoiseDetectComponent->SetSphereRadius(EnemyDetectComponent->GetCurrentDetectionRadius(), false);
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
	const FGameplayAbilitySpec* Spec = FindAbilitySpecByInputID(GetAIAttackInputID());
	return Spec && Spec->IsActive();
}

bool AEnemy::IsPlayingHitReaction() const
{
	if (GetWorld()->GetTimeSeconds() < ParryStunEndTime) return true;
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

void AEnemy::UpdateMeshYOffset()
{
	if (!bOverlapWithCharacters)
	{
		return;
	}

	// 액터 회전을 상쇄한 오프셋으로 바꿔 넣는다. 이전에 넣은 값만 빼서 다른 연출이 옮긴 위치는 유지한다.
	const FVector NewOffset = GetActorQuat().UnrotateVector(FVector(0.0f, MeshYOffset, 0.0f));

	if (NewOffset.Equals(AppliedMeshYOffset, 0.01f))
	{
		return;
	}

	GetMesh()->SetRelativeLocation(GetMesh()->GetRelativeLocation() - AppliedMeshYOffset + NewOffset);
	AppliedMeshYOffset = NewOffset;
}

void AEnemy::OnRootTransformUpdated(USceneComponent* UpdatedComponent, EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport)
{
	UpdateMeshYOffset();
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
	const float RequestedDistanceX = DestinationX - GetActorLocation().X;

	// 추격 중에만 간격을 유지한다. 순찰·수색에서 앞 몬스터에 막혀 목적지에 영영 못 가는 일을 막는다.
	// 반대편으로 건너가는 동안은 대상 옆의 몬스터를 지나가야 하므로 간격을 보지 않는다.
	// 대상 근처에서 공격할 수 있게 되면 앞 몬스터에 막혀 서 있지 않고 공격하러 간다.
	const bool bUseSpacing = SpacingComp->IsKeepingMonsterSpacing()
		&& BehaviorState == EEnemyBehaviorState::Chase
		&& !SpacingComp->IsCrossingSurroundSide()
		&& !SpacingComp->IsPrioritizingAttack();

	if (bUseSpacing)
	{
		DestinationX = SpacingComp->ApplyMonsterSpacing(DestinationX);
	}
	else
	{
		SpacingComp->ClearSpacingHold();
	}

	if (GetWorld()->GetTimeSeconds() < ParryStunEndTime)
	{
		StopHorizontalMovement();
		return;
	}
	const float DistanceX = DestinationX - GetActorLocation().X;
	if (FMath::Abs(DistanceX) <= MoveArriveTolerance || DeltaTime <= 0.0f)
	{
		// 간격 때문에 멈췄으면 원래 가려던 쪽을 바라보고 선다.
		if (SpacingComp->IsHeldBySpacing())
		{
			FaceDirection(RequestedDistanceX);
		}

		// 추격 대상이 있으면 공격 태스크와 같은 기준(대상에서 먼 쪽이 양보)으로 비켜 선다.
		// 기준이 다르면 추격 중인 몬스터와 공격 대기 중인 몬스터가 서로 상대가 비키기를 기다리며 겹친 채 멈춘다.
		const AActor* SeparateFaceTarget = EnemyDetectComponent->GetCurrentTarget();

		if (bUseSpacing && DeltaTime > 0.0f && SpacingComp->TrySeparateFromMonsters(Speed, DeltaTime, SeparateFaceTarget))
		{
			return;
		}

		StopHorizontalMovement();
		return;
	}

	FaceDirection(DistanceX);

	// 가려는 쪽이 낭떠러지면 가장자리 앞에서 멈춰 그쪽을 바라보고 선다.
	if (SpacingComp->IsLedgeAhead(DistanceX))
	{
		StopHorizontalMovement();
		return;
	}

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
		CancelAIBaseAttack();
	}
	BehaviorState = NewState;
	StopHorizontalMovement();

	// 추격·공격이 끝나면 정해 둔 자리를 비운다. 다음 추격에서 다시 정한다.
	if (NewState != EEnemyBehaviorState::Chase && NewState != EEnemyBehaviorState::Attack)
	{
		SpacingComp->ResetSurround();
	}
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

bool AEnemy::CanStartAIAttack(AActor* Target) const
{
	// 반대편으로 건너가는 중에는 공격하지 않는다. 지나가다 공격하면 다시 한쪽에 몰린다.
	return IsWithinAttackRange(Target) && !SpacingComp->IsCrossingSurroundSide();
}

void AEnemy::UpdateCombatMovement(AActor* Target, float DeltaSeconds)
{
	if (!IsValidAITarget(Target) || DeltaSeconds <= 0.0f)
	{
		return;
	}

	// 대상 간격과 포위 자리를 반영한 목적지를 받는다.
	const float	TargetX = Target->GetActorLocation().X;
	const float	DestinationX = SpacingComp->UpdateCombatDestinationX(Target);

	// 대상이 다가와 목적지가 내 뒤로 넘어가도 돌아서 걷지 않고, 그 자리에서 대상을 보고 버틴다.
	const float	MyX = GetActorLocation().X;
	const bool	bDestinationBehind = (DestinationX - MyX) * (TargetX - MyX) < 0.0f;

	if (!SpacingComp->IsCrossingSurroundSide() && bDestinationBehind)
	{
		StopHorizontalMovement();
		FaceDirection(TargetX - MyX);
		return;
	}

	MoveAlongX(DestinationX, ChaseSpeed, DeltaSeconds);
}

float AEnemy::GetCombatSightRadius() const
{
	return EnemyDetectComponent ? EnemyDetectComponent->GetCurrentDiscoveryRadius() : 0.0f;
}

bool AEnemy::ActivateAIAttack()
{
	return BasicAttackMontage && TryActivateAbilityByInputID(GetAIAttackInputID());
}

void AEnemy::FaceLocation(const FVector& Location)
{
	FaceDirection(Location.X - GetActorLocation().X);
}

void AEnemy::NotifyNoise(const FVector& InLocation, const float InRadius, const float InStrength)
{
	this->EnemyDetectComponent->NotifyNoise(InLocation, InRadius, InStrength);
}

bool AEnemy::IsAIAttackCooldownReady() const
{
	return GetWorld()->GetTimeSeconds() >= NextAttackTime;
}

bool AEnemy::TryStartAIBaseAttack(AActor* Target)
{
	if (!CanStartAIAttack(Target)) return false;
	if (IsPlayingHitReaction() || !ASC || IsAttackActive()) return false;
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now < NextAttackTime) return false;
	CombatTarget = Cast<APawn>(Target);
	SetBehaviorState(EEnemyBehaviorState::Attack);
	StopHorizontalMovement();
	FaceLocation(Target->GetActorLocation());
	const bool bActivated = ActivateAIAttack();
	if (bActivated)
	{
		NextAttackTime = Now + FMath::Max(0.1f, GetAIAttackCooldown());
	}
	else
	{
		// 활성화 조건(지상 여부, 어빌리티/몽타주 설정 등)이 계속 실패해도 공격 분기에 갇히지 않도록 재시도를 늦춘다.
		// 쿨타임으로 공격 준비 여부를 판단하는 적은 이 시간 동안 이동 분기로 돌아간다.
		NextAttackTime = Now + FMath::Max(0.1f, AttackActivationRetryDelay);
	}

	return bActivated;
}

void AEnemy::CancelAIBaseAttack()
{
	if (const FGameplayAbilitySpec* Spec = FindAbilitySpecByInputID(GetAIAttackInputID()))
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


void AEnemy::ReceiveParry(float FallbackStunDuration)
{
	if (bIsDead) return;

	// 공격 패턴과 루트 모션을 중단하고 다른 유지형 어빌리티는 보존한다.
	// 공격 몽타주가 멈춘 뒤에 패링 피격 몽타주를 재생해야 덮어써지지 않는다.
	if (ASC)
	{
		FGameplayTagContainer AttackTags(EclipseGameplayTags::Ability_Attack);
		ASC->CancelAbilities(&AttackTags);
	}

	StopHorizontalMovement();

	// 패링 피격 몽타주 중 하나를 무작위로 골라 그 길이만큼, 없으면 패링 쪽에서 정한 시간만큼 경직된다.
	// 비어 있는 칸은 후보에서 뺀다.
	float StunDuration = FallbackStunDuration;
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	TArray<UAnimMontage*, TInlineAllocator<4>> Candidates;
	for (UAnimMontage* Montage : ParryHitMontages)
	{
		if (Montage) Candidates.Add(Montage);
	}
	if (AnimInstance && Candidates.Num() > 0)
	{
		UAnimMontage* Montage = Candidates[FMath::RandRange(0, Candidates.Num() - 1)];
		const float PlayLength = AnimInstance->Montage_Play(Montage);
		if (PlayLength > 0.0f) StunDuration = PlayLength;
	}

	ParryStunEndTime = FMath::Max(ParryStunEndTime, GetWorld()->GetTimeSeconds() + FMath::Max(0.01f, StunDuration));
}

UNiagaraSystem* AEnemy::GetParrySparkEffect() const
{
	return ParrySparkEffect;
}
