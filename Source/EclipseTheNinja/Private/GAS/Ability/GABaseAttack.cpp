


#include "GAS/Ability/GABaseAttack.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GAS/Effects/GEBasicAttackDamage.h"
#include "GAS/StatAttributeSet.h"
#include "NativeGameplayTags.h"
#include "Character/BaseCharacter.h"
#include "Character/Enemy.h"
#include "GameFramework/Character.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_BasicAttack, "Ability.Attack.Basic");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Attacking, "State.Attacking");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_BasicAttackHit, "Event.Attack.Basic.Hit");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ComboInput, "Event.Attack.Basic.ComboInput");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ComboEnd, "Event.Attack.Basic.ComboEnd");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_AttackStart, "Event.Attack.Basic.Start");

UAnimMontage* GetBasicAttackMontage(const ACharacter* Character)
{
	if (const ABaseCharacter* CombatCharacter = Cast<ABaseCharacter>(Character))
	{
		return CombatCharacter->GetBasicAttackMontage();
	}
	return nullptr;
}

UGABaseAttack::UGABaseAttack()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly;

	FGameplayTagContainer Tags;
	Tags.AddTag(TAG_BasicAttack);
	SetAssetTags(Tags);

	// 활성화되면 획득할 태그
	ActivationOwnedTags.AddTag(TAG_Attacking);

	// 가지고 있으면 활성화되지 않는 태그
	ActivationBlockedTags.AddTag(TAG_Attacking);
}

FGameplayTag UGABaseAttack::GetHitEventTag()
{
	return TAG_BasicAttackHit;
}

FGameplayTag UGABaseAttack::GetComboInputEventTag()
{
	return TAG_ComboInput;
}

FGameplayTag UGABaseAttack::GetComboEndEventTag()
{
	return TAG_ComboEnd;
}

FGameplayTag UGABaseAttack::GetAttackStartEventTag()
{
	return TAG_AttackStart;
}

FGameplayTag UGABaseAttack::GetAttackingTag()
{
	return TAG_Attacking;
}

bool UGABaseAttack::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	// 캐릭터, 기본 공격 몽타주, 애님 인스턴스 확인
	const ACharacter* Character = ActorInfo ? Cast<ACharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (!Character) return false;
	if (!GetBasicAttackMontage(Character)) return false;
	if (!Character->GetMesh()->GetAnimInstance()) return false;

	// 시전자의 ASC, 스탯어트리뷰트, 체력이 0이하인지 확인
	const UAbilitySystemComponent* SourceASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!SourceASC) return false;
	if (!SourceASC->GetSet<UStatAttributeSet>()) return false;
	if (SourceASC->GetNumericAttribute(UStatAttributeSet::GetHealthAttribute()) <= 0.0f) return false;

	// 적, 적이 죽었는지, 공격 상태가 아닌지, 타겟이 있는지 확인
	const AEnemy* Enemy = Cast<AEnemy>(Character);
	if (Enemy)
	{
		if (Enemy->IsDead() || Enemy->GetBehaviorState() != EEnemyBehaviorState::Attack) return false;
		if (!Enemy->GetCombatTarget()) return false;
	}


	return true;
}

void UGABaseAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	ACharacter* Character = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
	UAnimMontage* AttackMontage = GetBasicAttackMontage(Character);

	// 캐릭터나 몽타주가 없으면 실패
	if (!Character || !AttackMontage || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, false, true);
		return;
	}

	// 공격 대상을 담을 Set 초기화
	HitActors.Reset();

	// 콤보용 
	CurrentAttackMontage = AttackMontage;
	ComboIndex = 1;
	bNextAttackPending = false;
	bAcceptComboInput = true;

	// TAG_BasicAttackHit 태그를 달고 오는 이벤트를 기다리는 테스트 (해당 이벤트가 타격 로직을 실행함)
	// 몽타주의 시작 부분에 노티파이가 있을 수 있기 때문에 재생보다 먼저 와야함
	UAbilityTask_WaitGameplayEvent* HitTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, TAG_BasicAttackHit);
	HitTask->EventReceived.AddDynamic(this, &UGABaseAttack::OnHitEvent);
	HitTask->ReadyForActivation();

	// TAG_ComboInput 태그를 달고 오는 이벤트를 기다리는 테스크 (해당 이벤트가 타격 로직을 실행함)
	// 공격중 추가 입력을 기다리는 Task
	UAbilityTask_WaitGameplayEvent* InputTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, TAG_ComboInput);
	InputTask->EventReceived.AddDynamic(this, &UGABaseAttack::OnComboInput);
	InputTask->ReadyForActivation();

	// 다음 타 연결을 결정하는 Notify 이벤트를 받는다. (이 노티파이 이후로 들어오는 입력은 콤보에 영향을 주지 않는다)
	UAbilityTask_WaitGameplayEvent* CheckTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, TAG_ComboEnd);
	CheckTask->EventReceived.AddDynamic(this, &UGABaseAttack::OnComboCheck);
	CheckTask->ReadyForActivation();

	// 각 타가 시작되는 Notify 이벤트를 받는다.
	UAbilityTask_WaitGameplayEvent* StartTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, TAG_AttackStart);
	StartTask->EventReceived.AddDynamic(this, &UGABaseAttack::OnAttackStart);
	StartTask->ReadyForActivation();

	// 몽타주를 재생을 기다리는 태스크
	// 끝날면, 중단되면, 취소되면에 맞는 함수를 바인딩할 수 있다.
	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this, NAME_None, AttackMontage, FMath::Max(0.01f, PlayRate), FName(TEXT("Attack1")), true,
		1.0f, 0.0f, true);
	MontageTask->OnCompleted.AddDynamic(this, &UGABaseAttack::OnAttackCompleted);
	MontageTask->OnInterrupted.AddDynamic(this, &UGABaseAttack::OnAttackInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &UGABaseAttack::OnAttackInterrupted);
	MontageTask->ReadyForActivation();


	if (IsActive())
	{
		// 각 섹션의 다음 섹션을 초기화
		if (UAnimInstance* Anim = Character->GetMesh()->GetAnimInstance())
		{
			Anim->Montage_SetNextSection(
				TEXT("Attack1"), NAME_None, AttackMontage);

			Anim->Montage_SetNextSection(
				TEXT("Attack2"), NAME_None, AttackMontage);

			Anim->Montage_SetNextSection(
				TEXT("Attack3"), NAME_None, AttackMontage);
		}
	}

}

void UGABaseAttack::OnAttackCompleted()
{
	if (IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, false);
	}
}

void UGABaseAttack::OnAttackInterrupted()
{
	if (IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, true);
	}
}

void UGABaseAttack::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	// 공격 대상을 담을 Set 초기화
	HitActors.Reset();

	// 콤보 초기화
	ComboIndex = 1;
	bNextAttackPending = false;
	bAcceptComboInput = false;
	CurrentAttackMontage = nullptr;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGABaseAttack::OnHitEvent(FGameplayEventData Payload)
{
	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	// 활성화중 아니고 캐릭터 있고, ASC 있고, 현재 ASC에서 몽타주를 재생하고 있는게 나인지, 어트리뷰트 있는지
	if (!IsActive() || !Character || !SourceASC
		|| SourceASC->GetAnimatingAbility() != this || !SourceASC->GetSet<UStatAttributeSet>())
	{
		return;
	}

	// 공격자의 체력이 0이하 이거나, 적이 죽은 상태이면
	// 중단
	const AEnemy* Enemy = Cast<AEnemy>(Character);
	if (SourceASC->GetNumericAttribute(UStatAttributeSet::GetHealthAttribute()) <= 0.0f || (Enemy && Enemy->IsDead()))
	{
		OnAttackInterrupted();
		return;
	}

	//데미지가 유효한 숫자인지 확인
	const float Damage = SourceASC->GetNumericAttribute(UStatAttributeSet::GetAttackPowerAttribute());
	if (!FMath::IsFinite(Damage) || Damage <= 0.0f)
	{
		return;
	}

	// 판정 구의 반지름
	const float Radius = FMath::Max(1.0f, AttackRadius);

	// 캐릭터의 앞으로 반지름 만큼 이동한 위치
	const FVector Start = Character->GetActorLocation() + Character->GetActorForwardVector() * Radius;
	// 캐릭터의 앞으로 공격 범위만큼 이동한 위치
	const FVector End = Character->GetActorLocation() + Character->GetActorForwardVector() * FMath::Max(Radius, AttackRange);

	// 폰이랑 쿼리 확인
	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BasicAttack), false, Character);

	// Sweep
	TArray<FHitResult> Hits;

	// 구를 만들어서 Start 부터 End까지 Sweep
	Character->GetWorld()->SweepMultiByObjectType(Hits, Start, End, FQuat::Identity,
		ObjectParams, FCollisionShape::MakeSphere(Radius), QueryParams);

	// 충돌체들을 순회하면서 확인
	for (const FHitResult& Hit : Hits)
	{
		// 충돌한 액터를 가져옴
		AActor* Target = Hit.GetActor();

		// 본인 이거나 이미 포함된 친구는 생략
		if (!IsValid(Target) || Target == Character || HitActors.Contains(Target))
		{
			continue;
		}

		// Enemy attacks only its currently tracked player, never another enemy.
		// 에너미가 에너미를 공격하지 않게 
		if (Enemy && Target != Enemy->GetCombatTarget())
		{
			continue;
		}

		UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target);
		// 타겟이 어트리뷰트 없거나, 체력이 0이하면 생략
		if (!TargetASC || !TargetASC->GetSet<UStatAttributeSet>()
			|| TargetASC->GetNumericAttribute(UStatAttributeSet::GetHealthAttribute()) <= 0.0f)
		{
			continue;
		}

		// 이펙트 핸들 생성
		FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
		Context.AddHitResult(Hit);

		// 스펙 생성
		FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(UGEBasicAttackDamage::StaticClass(), GetAbilityLevel(), Context);

		// SetbyCaller를 통한 모디파이어 조정 후 이펙트 적용
		if (Spec.IsValid())
		{
			Spec.Data->SetSetByCallerMagnitude(FName(TEXT("Damage")), Damage);
			HitActors.Add(Target);
			SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
		}
	}
}

void UGABaseAttack::OnComboInput(FGameplayEventData Payload)
{
	// 활성화 상태가 아니거나, 콤보를 안받는 상태거나, 이미 3타 이상을 쳤으면
	if (!IsActive() || !bAcceptComboInput || ComboIndex >= 3)
	{
		return;
	}

	// 여러 번 눌러도 다음 타 하나만 예약한다.
	bNextAttackPending = true;
}

void UGABaseAttack::OnComboCheck(FGameplayEventData Payload)
{
	UAbilitySystemComponent* SourceASC =GetAbilitySystemComponentFromActorInfo();

	// 콤보를 재생 하려면 ASC도 있어야 하지만
	// 현재 재생중인 몽타주의 어빌리티가 반드시 '나'여야 함
	if (!IsActive() || !SourceASC || SourceASC->GetAnimatingAbility() != this) return;

	// 이번 타의 입력 접수는 여기서 끝난다.
	bAcceptComboInput = false;

	// 입력이 없거나 이미 3타 했으면 종료
	if (!bNextAttackPending || ComboIndex >= 3) return;

	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	UAnimInstance* Anim = Character ? Character->GetMesh()->GetAnimInstance() : nullptr;
	if (!Anim || !CurrentAttackMontage)	return;

	// 현재 섹션 이름
	const FName CurrentSection(*FString::Printf(TEXT("Attack%d"), ComboIndex));
	// 다음 섹션 이름
	const FName NextSection(*FString::Printf(TEXT("Attack%d"), ComboIndex + 1));

	// 다음 섹션으로 이동
	// 현재 섹션이 끝나기를 기다리지 않고 즉시 이동한다.
	Anim->Montage_JumpToSection(
		NextSection, CurrentAttackMontage);

	// 입력 소비
	bNextAttackPending = false;
}

void UGABaseAttack::OnAttackStart(FGameplayEventData Payload)
{
	UAbilitySystemComponent* SourceASC =GetAbilitySystemComponentFromActorInfo();

	// 콤보를 재생 하려면 ASC도 있어야 하지만
	// 현재 재생중인 몽타주의 어빌리티가 반드시 '나'여야 함
	if (!IsActive() || !SourceASC || SourceASC->GetAnimatingAbility() != this)	return;

	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	UAnimInstance* Anim = Character ? Character->GetMesh()->GetAnimInstance() : nullptr;
	if (!Anim || !CurrentAttackMontage)	return;

	// 현재 섹션 이름
	const FName Section = Anim->Montage_GetCurrentSection(CurrentAttackMontage);

	// 섹션 따라 현재 인덱스 저장
	if (Section == FName(TEXT("Attack1")))
	{
		ComboIndex = 1;
	}
	else if (Section == FName(TEXT("Attack2")))
	{
		ComboIndex = 2;
	}
	else if (Section == FName(TEXT("Attack3")))
	{
		ComboIndex = 3;
	}
	else
	{
		return;
	}

	// 같은 적이 이번 타에도 데미지를 받을 수 있게 한다.
	HitActors.Reset();

	// 입력 소비
	bNextAttackPending = false;
	// 콤보를 받을 수 있는지 설정
	bAcceptComboInput = ComboIndex < 3;
}

