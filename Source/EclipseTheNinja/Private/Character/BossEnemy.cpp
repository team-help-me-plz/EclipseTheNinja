#include "Character/BossEnemy.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Component/AssassinationDropComponent.h"
#include "Component/EnemyDetectionComponent.h"
#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GAS/Ability/GABossPatternBase.h"
#include "GAS/Effects/GEBasicAttackDamage.h"
#include "GAS/Effects/GEDefaultStats.h"
#include "GAS/StatAttributeSet.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

ABossEnemy::ABossEnemy()
{
	DefaultStatsEffect = UGEBossEnemyDefaultStats::StaticClass();

	// 보스는 덩치가 커서 일반 적보다 먼 거리에서 발도를 시작한다.
	AttackDistance = 250.0f;

	// 덩치 큰 보스가 순간적으로 뒤돌지 않도록 반 바퀴를 0.5초에 돈다.
	TurnRate = 360.0f;

	// 보스는 암살로 처치되어도 회복 아이템을 떨어뜨리지 않는다.
	AssassinationDropComp->SetDropActorClass(nullptr);
}

void ABossEnemy::BeginPlay()
{
	Super::BeginPlay();

	// 높은 체력 구간부터 순서대로 발동하도록 정렬한다.
	EnergyLinkThresholds.Sort([](float A, float B)
	{
		return A > B;
	});
}

void ABossEnemy::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(EnergyLinkTimer);

	Super::EndPlay(EndPlayReason);
}

void ABossEnemy::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

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

void ABossEnemy::SetDrawDebugAttack(bool bEnable)
{
	bDrawDebugAttack = bEnable;
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
	ASC->SetLooseGameplayTagCount(UStatAttributeSet::GetInvulnerableTag(), 1);

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

	UE_LOG(LogTemp, Log, TEXT("%s: 에너지 링크 해제 실패, 플레이어 처치"), *GetName());
	OnEnergyLinkFailed.Broadcast();

	// 플레이어의 체력을 0으로 만들어 플레이어 쪽 사망 처리를 그대로 사용한다.
	APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	UAbilitySystemComponent* PlayerASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Player);
	if (PlayerASC && PlayerASC->GetSet<UStatAttributeSet>())
	{
		PlayerASC->SetNumericAttributeBase(UStatAttributeSet::GetHealthAttribute(), 0.0f);
	}
}

void ABossEnemy::ClearEnergyLinkState()
{
	bEnergyLinkActive = false;
	GetWorldTimerManager().ClearTimer(EnergyLinkTimer);

	if (ASC)
	{
		ASC->SetLooseGameplayTagCount(UStatAttributeSet::GetInvulnerableTag(), 0);
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
	ClearEnergyLinkState();
	NextEnergyLinkIndex = 0;

	// 리스폰한 보스는 모든 패턴을 바로 쓸 수 있게 패턴 쿨타임을 지운다.
	PatternReadyTimes.Reset();

	Super::ResetToInitial_Implementation();

	if (BrokenEnergyLinkCount != 0)
	{
		BrokenEnergyLinkCount = 0;
		OnPhaseChanged.Broadcast(GetPhase());
	}
}

float ABossEnemy::GetMoveSpeedLimit() const
{
	// 달리기 전 페이즈에서는 자세를 잡고 천천히 다가간다. 순찰·수색처럼 원래 느린 이동은 그대로다.
	return IsRunPhase() ? Super::GetMoveSpeedLimit() : StanceWalkSpeed;
}

bool ABossEnemy::CanStartAIAttack(AActor* Target) const
{
	if (!PatternTable)
	{
		return Super::CanStartAIAttack(Target);
	}

	if (!IsValidAITarget(Target))
	{
		return false;
	}

	TArray<TPair<FName, const FBossPatternRow*>> Candidates;
	float TotalWeight = 0.0f;
	GatherUsablePatterns(Target, Candidates, TotalWeight);
	return !Candidates.IsEmpty();
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
	if (!PatternTable)
	{
		// 테이블이 없으면 기존처럼 입력 ID로 지정한 기본 패턴을 실행한다.
		const FGameplayAbilitySpec* Spec = FindAbilitySpecByInputID(GetAIAttackInputID());
		CurrentPatternHandle = Spec ? Spec->Handle : FGameplayAbilitySpecHandle();
		return TryActivateAbilityByInputID(GetAIAttackInputID());
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
	for (const TPair<FName, uint8*>& Pair : PatternTable->GetRowMap())
	{
		const FBossPatternRow* Row = reinterpret_cast<const FBossPatternRow*>(Pair.Value);
		if (!Row || !IsPatternUsable(Pair.Key, *Row, Distance, bAssumeInAttackDistance))
		{
			continue;
		}

		OutCandidates.Emplace(Pair.Key, Row);
		OutTotalWeight += Row->Weight;
	}
}

bool ABossEnemy::IsPatternUsable(
	FName RowName,
	const FBossPatternRow& Row,
	float DistanceToTarget,
	bool bAssumeInAttackDistance
) const
{
	// 가중치가 있고 어빌리티가 지정된 행만 후보로 삼는다.
	if (Row.Weight <= 0.0f || !Row.AbilityClass)
	{
		return false;
	}

	if (GetPhase() < Row.MinPhase)
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
	float Roll = FMath::FRandRange(0.0f, OutTotalWeight);
	for (const TPair<FName, const FBossPatternRow*>& Candidate : Candidates)
	{
		Roll -= Candidate.Value->Weight;
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
	const FString Message = FString::Printf(
		TEXT("[보스 패턴] %d페이즈 %s (%s) - 가중치 %.1f / 합 %.1f (%.1f%%), 쿨타임 %.1f초"),
		GetPhase(),
		*Row.DisplayName.ToString(),
		*RowName.ToString(),
		Row.Weight,
		TotalWeight,
		TotalWeight > 0.0f ? Row.Weight / TotalWeight * 100.0f : 0.0f,
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
}
#endif
