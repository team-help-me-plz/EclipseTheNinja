#include "Character/BossEnemy.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Character/PlayerCharacter.h"
#include "Component/AssassinationDropComponent.h"
#include "Component/EnemyDetectionComponent.h"
#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GAS/Ability/GABossExecution.h"
#include "GAS/Ability/GABossPatternBase.h"
#include "GAS/Effects/GEBasicAttackDamage.h"
#include "GAS/Effects/GEBossEnemyDefaultStats.h"
#include "GAS/StatAttributeSet.h"
#include "GAS/Tags/EclipseGameplayTags.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Widgets/BossHealthBarWidget.h"

ABossEnemy::ABossEnemy()
{
	DefaultStatsEffect = UGEBossEnemyDefaultStats::StaticClass();

	// 보스는 덩치가 커서 일반 적보다 먼 거리에서 발도를 시작한다.
	AttackDistance = 250.0f;

	// 보스는 암살로 처치되어도 회복 아이템을 떨어뜨리지 않는다.
	AssassinationDropComp->SetDropActorClass(nullptr);

	// 기믹 실패 시 처형 연출. 플레이어 피격 모션은 일반 암살의 피격자 몽타주를 그대로 쓴다.
	ExecutionAbilityClass = UGABossExecution::StaticClass();
	ExecutionVictimMontage = TSoftObjectPtr<UAnimMontage>(
		FSoftObjectPath(TEXT("/Game/Anims/Montage/AM_Assassination_Victim.AM_Assassination_Victim"))
	);
}

void ABossEnemy::BeginPlay()
{
	Super::BeginPlay();

	// 높은 체력 구간부터 순서대로 발동하도록 정렬한다.
	EnergyLinkThresholds.Sort([](float A, float B)
	{
		return A > B;
	});

	// 패턴이 끝난 시점부터 휴식 시간을 센다.
	if (ASC)
	{
		PatternEndedHandle = ASC->OnAbilityEnded.AddUObject(this, &ABossEnemy::OnPatternAbilityEnded);
	}
}

void ABossEnemy::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	HideBossHealthBar();
	GetWorldTimerManager().ClearTimer(EnergyLinkTimer);
	GetWorldTimerManager().ClearTimer(ExecutionWaitTimer);

	if (ASC)
	{
		ASC->OnAbilityEnded.Remove(PatternEndedHandle);
	}

	Super::EndPlay(EndPlayReason);
}

void ABossEnemy::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UpdateTurning(DeltaTime);

	if (!bIsDead && !BossHealthBar && EnemyDetectComponent
		&& EnemyDetectComponent->GetCurrentAlertLevel() == EEnemyAlertLevel::Combat)
	{
		ShowBossHealthBar();
	}

	// A failed attempt must not leave a boss HUD on the respawn screen.
	if (BossHealthBar)
	{
		const APlayerController* PC = BossHealthBar->GetOwningPlayer();
		const ABaseCharacter* Player = PC ? Cast<ABaseCharacter>(PC->GetPawn()) : nullptr;
		if (!Player || !Player->GetStatAttribute() || Player->GetStatAttribute()->GetHealth() <= 0.0f)
		{
			BossHealthBar->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

#if WITH_EDITOR
	ProcessDebugRequests();
#endif

	// 실드 이펙트가 생기기 전까지 실드 상태를 눈으로 확인하기 위한 표시
	if (bDrawDebugShield && bEnergyLinkActive)
	{
		const float Radius = GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * 1.3f;
		DrawDebugSphere(
			GetWorld(),
			GetActorLocation(),
			Radius,
			16,
			FColor::Cyan,
			false,
			-1.0f,
			0,
			2.0f
		);
	}
}

float ABossEnemy::GetHealthRatio() const
{
	if (!StatAttributeSet || StatAttributeSet->GetMaxHealth() <= 0.0f)
	{
		return 0.0f;
	}

	return StatAttributeSet->GetHealth() / StatAttributeSet->GetMaxHealth();
}

void ABossEnemy::ShowBossHealthBar()
{
	if (BossHealthBar || bIsDead || !BossHealthBarClass || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (PC && PC->IsLocalController() && PC->GetPawn())
	{
		BossHealthBar = CreateWidget<UBossHealthBarWidget>(PC, BossHealthBarClass);
		if (BossHealthBar)
		{
			BossHealthBar->AddToViewport(10);
			BossHealthBar->InitBoss(this, BossDisplayName);
		}
	}
}

void ABossEnemy::HideBossHealthBar()
{
	if (BossHealthBar)
	{
		BossHealthBar->RemoveFromParent();
		BossHealthBar = nullptr;
	}
}

void ABossEnemy::SetDrawDebugAttack(bool bEnable)
{
	bDrawDebugAttack = bEnable;
}

float ABossEnemy::GetEnergyLinkTimeRemaining() const
{
	if (!bEnergyLinkActive || EnergyLinkTimeLimit <= 0.0f)
	{
		return -1.0f;
	}

	return FMath::Max(0.0f, GetWorldTimerManager().GetTimerRemaining(EnergyLinkTimer));
}

void ABossEnemy::OnHealthChanged(const FOnAttributeChangeData& Data)
{
	// 실드 중이거나 체력이 늘어난 경우는 일반 처리만 한다.
	if (bIsDead || bEnergyLinkActive || Data.NewValue >= Data.OldValue
		|| !EnergyLinkThresholds.IsValidIndex(NextEnergyLinkIndex))
	{
		Super::OnHealthChanged(Data);
		return;
	}

	const float ThresholdHealth = EnergyLinkThresholds[NextEnergyLinkIndex] * StatAttributeSet->GetMaxHealth();
	if (Data.NewValue > ThresholdHealth)
	{
		Super::OnHealthChanged(Data);
		return;
	}

	// 아래의 체력 보정이 이 함수를 다시 호출하므로, 체력을 바꾸기 전에 실드를 먼저 켠다.
	const int32 LinkIndex = NextEnergyLinkIndex++;
	ActivateEnergyLink(LinkIndex);

	// 큰 피해로 구간을 넘어가도 이번 구간 체력에서 멈춘다. 같은 이유로 이 피해로는 죽지 않는다.
	if (Data.NewValue < ThresholdHealth)
	{
		ASC->SetNumericAttributeBase(UStatAttributeSet::GetHealthAttribute(), ThresholdHealth);
	}

	// 피격 반응과 감지 갱신은 보정된 체력 기준으로 일반 적 처리에 맡긴다.
	FOnAttributeChangeData ClampedData = Data;
	ClampedData.NewValue = ThresholdHealth;
	Super::OnHealthChanged(ClampedData);
}

void ABossEnemy::ActivateEnergyLink(int32 LinkIndex)
{
	bEnergyLinkActive = true;
	ASC->SetLooseGameplayTagCount(EclipseGameplayTags::State_Invincible, 1);

	if (EnergyLinkTimeLimit > 0.0f)
	{
		GetWorldTimerManager().SetTimer(
			EnergyLinkTimer,
			this,
			&ABossEnemy::OnEnergyLinkTimeExpired,
			EnergyLinkTimeLimit,
			false
		);
	}

	UE_LOG(LogTemp, Log, TEXT("%s: 에너지 링크 %d 발동 (체력 %.0f%%)"), *GetName(), LinkIndex, EnergyLinkThresholds[LinkIndex] * 100.0f);
	OnEnergyLinkActivated.Broadcast(LinkIndex);
}

void ABossEnemy::BreakEnergyLink()
{
	if (!bEnergyLinkActive)
	{
		return;
	}

	ClearEnergyLinkState();
	++BrokenEnergyLinkCount;

	UE_LOG(LogTemp, Log, TEXT("%s: 에너지 링크 해제, %d페이즈"), *GetName(), GetPhase());
	OnEnergyLinkBroken.Broadcast();
	OnPhaseChanged.Broadcast(GetPhase());
}

void ABossEnemy::OnEnergyLinkTimeExpired()
{
	if (!bEnergyLinkActive || bIsDead)
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("%s: 에너지 링크 해제 실패, 플레이어 처형"), *GetName());
	OnEnergyLinkFailed.Broadcast();

	// 진행 중인 패턴을 멈추고 처형으로 넘어간다.
	CancelAIBaseAttack();

	// 처형 전까지 플레이어가 움직이지 못하게 한다. 벽 매달리기 등은 취소해 땅으로 내려오게 한다.
	APlayerCharacter* Player = Cast<APlayerCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (IsValid(Player))
	{
		if (UAbilitySystemComponent* PlayerASC = Player->GetAbilitySystemComponent())
		{
			PlayerASC->CancelAllAbilities();
		}

		if (APlayerController* PlayerController = Cast<APlayerController>(Player->GetController()))
		{
			Player->DisableInput(PlayerController);
		}
	}

	ExecutionWaitStartTime = GetWorld()->GetTimeSeconds();
	TryStartExecution();
}

void ABossEnemy::TryStartExecution()
{
	if (bIsDead)
	{
		return;
	}

	APlayerCharacter* Player = Cast<APlayerCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	UAbilitySystemComponent* PlayerASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Player);
	if (!IsValid(Player) || !PlayerASC || PlayerASC->GetNumericAttribute(UStatAttributeSet::GetHealthAttribute()) <= 0.0f)
	{
		return;
	}

	// 공중에서 피격자 몽타주를 재생하면 허공에 멈춘 채 쓰러지므로, 착지할 때까지 잠시 기다린다.
	const bool bWaitTimedOut = GetWorld()->GetTimeSeconds() - ExecutionWaitStartTime >= ExecutionGroundWaitTime;
	if (!Player->GetCharacterMovement()->IsMovingOnGround() && !bWaitTimedOut)
	{
		GetWorldTimerManager().SetTimer(
			ExecutionWaitTimer,
			this,
			&ABossEnemy::TryStartExecution,
			0.1f,
			false
		);
		return;
	}

	if (ActivateExecutionAbility())
	{
		return;
	}

	// 처형을 시작하지 못하면 플레이어의 체력을 0으로 만들어 플레이어 쪽 사망 처리를 그대로 사용한다.
	PlayerASC->SetNumericAttributeBase(UStatAttributeSet::GetHealthAttribute(), 0.0f);
}

bool ABossEnemy::ActivateExecutionAbility()
{
	if (!ExecutionAbilityClass || !ASC)
	{
		return false;
	}

	// 처음 실행할 때 부여한다.
	FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromClass(ExecutionAbilityClass);
	if (!Spec && HasAuthority())
	{
		const FGameplayAbilitySpecHandle NewHandle = ASC->GiveAbility(FGameplayAbilitySpec(ExecutionAbilityClass, 1));
		Spec = ASC->FindAbilitySpecFromHandle(NewHandle);
	}

	if (!Spec)
	{
		return false;
	}

	return ASC->TryActivateAbility(Spec->Handle);
}

void ABossEnemy::ClearEnergyLinkState()
{
	bEnergyLinkActive = false;
	GetWorldTimerManager().ClearTimer(EnergyLinkTimer);

	// 착지를 기다리던 중에 실드가 해제되면 처형을 취소하고 플레이어 입력을 되돌린다.
	if (GetWorldTimerManager().IsTimerActive(ExecutionWaitTimer))
	{
		GetWorldTimerManager().ClearTimer(ExecutionWaitTimer);

		APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
		if (APlayerController* PlayerController = Player ? Cast<APlayerController>(Player->GetController()) : nullptr)
		{
			Player->EnableInput(PlayerController);
		}
	}

	if (ASC)
	{
		ASC->SetLooseGameplayTagCount(EclipseGameplayTags::State_Invincible, 0);
	}
}

bool ABossEnemy::CanBeAssassinated() const
{
	return !bEnergyLinkActive;
}

void ABossEnemy::ReceiveAssassination(UAnimMontage* Montage)
{
	if (bIsDead || !IsValid(ASC) || bEnergyLinkActive)
	{
		return;
	}

	// 체력이 충분히 낮으면 일반 적처럼 즉시 처치한다.
	const float Ratio = GetHealthRatio();
	if (Ratio <= AssassinationKillRatio + KINDA_SMALL_NUMBER)
	{
		Super::ReceiveAssassination(Montage);
		return;
	}

	// 현재 체력보다 낮은 가장 가까운 에너지 링크 체력까지 깎는다.
	// 남은 에너지 링크가 없으면 즉시 처치 체력까지 깎아, 다음 암살로 처치할 수 있게 한다.
	float TargetRatio = AssassinationKillRatio;
	for (int32 Index = NextEnergyLinkIndex; Index < EnergyLinkThresholds.Num(); ++Index)
	{
		if (EnergyLinkThresholds[Index] < Ratio)
		{
			TargetRatio = FMath::Max(EnergyLinkThresholds[Index], AssassinationKillRatio);
			break;
		}
	}

	// 체력 변경 델리게이트가 OnHealthChanged를 호출해 에너지 링크 발동까지 처리한다.
	ASC->SetNumericAttributeBase(UStatAttributeSet::GetHealthAttribute(), TargetRatio * StatAttributeSet->GetMaxHealth());
}

void ABossEnemy::ToggleActivate_Implementation()
{
	BreakEnergyLink();
}

void ABossEnemy::ResetToInitial_Implementation()
{
	HideBossHealthBar();
	ClearEnergyLinkState();
	NextEnergyLinkIndex = 0;

	// 리스폰한 보스는 모든 패턴을 바로 쓸 수 있게 패턴 쿨타임을 지운다.
	PatternReadyTimes.Reset();

	bTurning = false;

	Super::ResetToInitial_Implementation();

	if (BrokenEnergyLinkCount != 0)
	{
		BrokenEnergyLinkCount = 0;
		OnPhaseChanged.Broadcast(GetPhase());
	}
}

void ABossEnemy::FaceDirection(float DirectionX)
{
	if (TurnRate <= 0.0f)
	{
		Super::FaceDirection(DirectionX);
		return;
	}

	if (FMath::IsNearlyZero(DirectionX))
	{
		return;
	}

	// 실제 회전은 Tick에서 TurnRate로 진행한다.
	TargetFacingYaw = DirectionX > 0.0f ? 0.0f : 180.0f;
	bTurning = !FMath::IsNearlyZero(FRotator::NormalizeAxis(TargetFacingYaw - GetActorRotation().Yaw), 1.0f);
}

void ABossEnemy::UpdateTurning(float DeltaTime)
{
	if (!bTurning)
	{
		return;
	}

	const float CurrentYaw = GetActorRotation().Yaw;
	const float NewYaw = FMath::FixedTurn(CurrentYaw, TargetFacingYaw, TurnRate * DeltaTime);
	SetActorRotation(FRotator(0.0f, NewYaw, 0.0f));

	if (FMath::IsNearlyZero(FRotator::NormalizeAxis(TargetFacingYaw - NewYaw), 1.0f))
	{
		SetActorRotation(FRotator(0.0f, TargetFacingYaw, 0.0f));
		bTurning = false;
	}
}

void ABossEnemy::MoveAlongX(float DestinationX, float Speed, float DeltaTime)
{
	// 달리기 전 페이즈에서는 자세를 잡고 천천히 다가간다. 순찰·수색처럼 원래 느린 이동은 그대로다.
	const float LimitedSpeed = IsRunPhase()
		? Speed
		: FMath::Min(Speed, StanceWalkSpeed);

	// 이동 방향으로 도는 것은 FaceDirection 재정의가 맡는다.
	Super::MoveAlongX(DestinationX, LimitedSpeed, DeltaTime);

	// 크게 도는 중에는 뒷걸음질치지 않도록 방금 넣은 이동 입력을 버리고 제자리에서 다 돈 뒤 이동한다.
	if (bTurning && FMath::Abs(FRotator::NormalizeAxis(TargetFacingYaw - GetActorRotation().Yaw)) > 45.0f)
	{
		StopHorizontalMovement();
	}
}

bool ABossEnemy::CanStartAIAttack(AActor* Target) const
{
	// 패턴 사이 휴식 중에는 공격 분기로 넘어가지 않고 추격 분기에서 대상을 바라보며 거리를 맞춘다.
	if (IsRestingBetweenPatterns())
	{
		return false;
	}

	if (!PatternTable || !IsValidAITarget(Target))
	{
		return false;
	}

	TArray<TPair<FName, const FBossPatternRow*>> Candidates;
	float TotalWeight = 0.0f;
	GatherUsablePatterns(Target, Candidates, TotalWeight);
	return !Candidates.IsEmpty();
}

bool ABossEnemy::IsRestingBetweenPatterns() const
{
	return GetWorld()->GetTimeSeconds() < NextAttackTime;
}

void ABossEnemy::UpdateCombatMovement(AActor* Target, float DeltaSeconds)
{
	if (!IsRestingBetweenPatterns())
	{
		Super::UpdateCombatMovement(Target, DeltaSeconds);
		return;
	}

	if (!IsValidAITarget(Target) || DeltaSeconds <= 0.0f)
	{
		return;
	}

	const float OffsetX = Target->GetActorLocation().X - GetActorLocation().X;

	// 휴식 거리보다 멀면 대상 앞 휴식 거리 지점까지 다가간다. 다가가는 동안의 회전은 MoveAlongX가 맡는다.
	if (FMath::Abs(OffsetX) > PatternRestKeepDistance)
	{
		const float DestinationX = Target->GetActorLocation().X - FMath::Sign(OffsetX) * PatternRestKeepDistance;
		MoveAlongX(DestinationX, ChaseSpeed, DeltaSeconds);
		return;
	}

	// 충분히 가까우면 제자리에서 대상을 바라보며 다음 패턴을 기다린다.
	StopHorizontalMovement();
	FaceLocation(Target->GetActorLocation());
}

float ABossEnemy::GetCombatSightRadius() const
{
	if (!EnemyDetectComponent)
	{
		return Super::GetCombatSightRadius();
	}

	return FMath::Max(Super::GetCombatSightRadius(), EnemyDetectComponent->GetCurrentDetectionRadius());
}

bool ABossEnemy::ActivateAIAttack()
{
	// 대상 쪽으로 다 돌기 전에는 공격하지 않는다. Attack 태스크가 AttackActivationRetryDelay 뒤에 다시 시도한다.
	if (bTurning)
	{
		return false;
	}

	// 패턴은 모두 패턴 테이블로 고른다. 테이블이 없으면 공격하지 않는다.
	if (!PatternTable)
	{
		return false;
	}

	FName RowName;
	float TotalWeight = 0.0f;
	const FBossPatternRow* Pattern = PickNextPattern(GetCombatTarget(), RowName, TotalWeight);
	if (!Pattern)
	{
		// 모든 패턴이 쿨타임이거나 사용 거리 밖이다.
		return false;
	}

	// 처음 고른 어빌리티는 그때 부여한다. 같은 어빌리티를 쓰는 패턴끼리는 스펙을 공유한다.
	FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromClass(Pattern->AbilityClass);
	if (!Spec && HasAuthority())
	{
		const FGameplayAbilitySpecHandle NewHandle = ASC->GiveAbility(FGameplayAbilitySpec(Pattern->AbilityClass, 1));
		Spec = ASC->FindAbilitySpecFromHandle(NewHandle);
	}

	if (!Spec)
	{
		return false;
	}

	CurrentPatternHandle = Spec->Handle;
	const bool bActivated = ASC->TryActivateAbility(CurrentPatternHandle);
	if (bActivated)
	{
		// 쿨타임은 패턴을 시작한 시점부터 센다.
		if (Pattern->Cooldown > 0.0f)
		{
			PatternReadyTimes.Add(RowName, GetWorld()->GetTimeSeconds() + Pattern->Cooldown);
		}

		LogPatternChoice(RowName, *Pattern, TotalWeight);
	}

	return bActivated;
}

void ABossEnemy::GatherUsablePatterns(
	const AActor* Target,
	TArray<TPair<FName, const FBossPatternRow*>>& OutCandidates,
	float& OutTotalWeight
) const
{
	OutCandidates.Reset();
	OutTotalWeight = 0.0f;
	if (!PatternTable || !ASC || !Target)
	{
		return;
	}

	const float Distance = FVector::Dist(GetActorLocation(), Target->GetActorLocation());
	const bool bAssumeInAttackDistance = IsChasingFleeingTarget(Target);
	const int32 Phase = GetPhase();
	for (const TPair<FName, uint8*>& Pair : PatternTable->GetRowMap())
	{
		const FBossPatternRow* Row = reinterpret_cast<const FBossPatternRow*>(Pair.Value);
		if (!Row || !IsPatternUsable(Pair.Key, *Row, Distance, bAssumeInAttackDistance))
		{
			continue;
		}

		OutCandidates.Emplace(Pair.Key, Row);
		OutTotalWeight += Row->GetWeightForPhase(Phase);
	}
}

bool ABossEnemy::IsPatternUsable(
	FName RowName,
	const FBossPatternRow& Row,
	float DistanceToTarget,
	bool bAssumeInAttackDistance
) const
{
	// 지금 페이즈의 가중치가 있고 어빌리티가 지정된 행만 후보로 삼는다.
	if (Row.GetWeightForPhase(GetPhase()) <= 0.0f || !Row.AbilityClass)
	{
		return false;
	}

	if (IsPatternOnCooldown(RowName))
	{
		return false;
	}

	// 어빌리티 자체에 GAS 쿨타임이 걸려 있으면 어차피 활성화되지 않으므로 뺀다.
	const UGameplayAbility* AbilityCDO = Row.AbilityClass->GetDefaultObject<UGameplayAbility>();
	const FGameplayTagContainer* CooldownTags = AbilityCDO ? AbilityCDO->GetCooldownTags() : nullptr;
	if (CooldownTags && !CooldownTags->IsEmpty() && ASC->HasAnyMatchingGameplayTags(*CooldownTags))
	{
		return false;
	}

	// 사용 거리는 어빌리티 CDO의 설정을 읽는다. 보스 패턴 계열이 아니면 기본 공격 거리를 쓴다.
	const UGABossPatternBase* PatternCDO = Cast<UGABossPatternBase>(AbilityCDO);
	const float MinDistance = PatternCDO ? PatternCDO->GetMinUseDistance() : 0.0f;
	const float MaxDistance = PatternCDO && PatternCDO->GetMaxUseDistance() > 0.0f
		? PatternCDO->GetMaxUseDistance()
		: AttackDistance;

	auto IsInRange = [MinDistance, MaxDistance](float Distance)
	{
		return Distance >= MinDistance && Distance <= MaxDistance;
	};

	if (IsInRange(DistanceToTarget))
	{
		return true;
	}

	// 달아나는 대상을 쫓는 중이면 공격 거리에 들어온 것으로 보고 다시 검사한다.
	return bAssumeInAttackDistance && IsInRange(AttackDistance);
}

bool ABossEnemy::IsPatternOnCooldown(FName RowName) const
{
	const double* ReadyTime = PatternReadyTimes.Find(RowName);
	if (!ReadyTime)
	{
		return false;
	}

	return GetWorld()->GetTimeSeconds() < *ReadyTime;
}

bool ABossEnemy::IsChasingFleeingTarget(const AActor* Target) const
{
	if (!bAssumeAttackRangeWhileChasing || !Target || !EnemyDetectComponent)
	{
		return false;
	}

	// 안쪽 감지 범위(발각 반경) 안이어야 한다.
	const FVector ToTarget = Target->GetActorLocation() - GetActorLocation();
	if (ToTarget.SizeSquared() > FMath::Square(EnemyDetectComponent->GetCurrentDiscoveryRadius()))
	{
		return false;
	}

	// 횡스크롤이라 X축만 본다. 대상이 보스 앞쪽에 있고, 보스가 바라보는 방향으로 달아나고 있어야 한다.
	const float FacingSign = GetActorForwardVector().X >= 0.0f ? 1.0f : -1.0f;
	if (ToTarget.X * FacingSign <= 0.0f)
	{
		return false;
	}

	return Target->GetVelocity().X * FacingSign >= ChaseAssumeMinTargetSpeed;
}

const FBossPatternRow* ABossEnemy::PickNextPattern(const AActor* Target, FName& OutRowName, float& OutTotalWeight) const
{
	TArray<TPair<FName, const FBossPatternRow*>> Candidates;
	GatherUsablePatterns(Target, Candidates, OutTotalWeight);
	if (Candidates.IsEmpty())
	{
		return nullptr;
	}

	// 0 ~ 후보 가중치 합 사이 값을 뽑아 누적 가중치가 그 값을 넘는 행을 고른다.
	const int32 Phase = GetPhase();
	float Roll = FMath::FRandRange(0.0f, OutTotalWeight);
	for (const TPair<FName, const FBossPatternRow*>& Candidate : Candidates)
	{
		Roll -= Candidate.Value->GetWeightForPhase(Phase);
		if (Roll <= 0.0f)
		{
			OutRowName = Candidate.Key;
			return Candidate.Value;
		}
	}

	OutRowName = Candidates.Last().Key;
	return Candidates.Last().Value;
}

void ABossEnemy::LogPatternChoice(FName RowName, const FBossPatternRow& Row, float TotalWeight) const
{
	const float Weight = Row.GetWeightForPhase(GetPhase());
	const FString Message = FString::Printf(
		TEXT("[보스 패턴] %d페이즈 %s (%s) - 가중치 %.1f / 합 %.1f (%.1f%%), 쿨타임 %.1f초"),
		GetPhase(),
		*Row.DisplayName.ToString(),
		*RowName.ToString(),
		Weight,
		TotalWeight,
		TotalWeight > 0.0f ? Weight / TotalWeight * 100.0f : 0.0f,
		Row.Cooldown
	);

	UE_LOG(LogTemp, Log, TEXT("%s"), *Message);

	if (bPrintPatternOnScreen && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Orange, Message);
	}
}

bool ABossEnemy::IsAttackActive() const
{
	if (const FGameplayAbilitySpec* Spec = ASC ? ASC->FindAbilitySpecFromHandle(CurrentPatternHandle) : nullptr)
	{
		return Spec->IsActive();
	}

	return Super::IsAttackActive();
}

void ABossEnemy::OnPatternAbilityEnded(const FAbilityEndedData& EndedData)
{
	if (bIsDead || !CurrentPatternHandle.IsValid() || EndedData.AbilitySpecHandle != CurrentPatternHandle)
	{
		return;
	}

	const float RestTime = FMath::FRandRange(
		PatternRestTimeMin,
		FMath::Max(PatternRestTimeMin, PatternRestTimeMax)
	);

	// 피격 경직 등으로 이미 더 늦게 잡힌 공격 시각은 앞당기지 않는다.
	NextAttackTime = FMath::Max(NextAttackTime, GetWorld()->GetTimeSeconds() + RestTime);
}

void ABossEnemy::CancelAIBaseAttack()
{
	if (ASC && CurrentPatternHandle.IsValid())
	{
		ASC->CancelAbilityHandle(CurrentPatternHandle);
		return;
	}

	Super::CancelAIBaseAttack();
}

#if WITH_EDITOR
void ABossEnemy::ProcessDebugRequests()
{
	if (DebugDamage > 0.0f)
	{
		const float Amount = DebugDamage;
		DebugDamage = 0.0f;

		// 플레이어가 때린 것처럼 Context를 만들어 일반 피해 경로(무적 검사, 감지 갱신)를 그대로 탄다.
		FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
		APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
		Context.AddInstigator(Player, Player);

		FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(UGEBasicAttackDamage::StaticClass(), 1.0f, Context);
		if (Spec.IsValid())
		{
			Spec.Data->SetSetByCallerMagnitude(FName(TEXT("Damage")), Amount);
			ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
		}
	}

	if (bDebugAssassinate)
	{
		bDebugAssassinate = false;
		ReceiveAssassination(nullptr);
	}

	if (bDebugBreakEnergyLink)
	{
		bDebugBreakEnergyLink = false;
		BreakEnergyLink();
	}

	if (bDebugExpireEnergyLink)
	{
		bDebugExpireEnergyLink = false;
		GetWorldTimerManager().ClearTimer(EnergyLinkTimer);
		OnEnergyLinkTimeExpired();
	}
}
#endif
