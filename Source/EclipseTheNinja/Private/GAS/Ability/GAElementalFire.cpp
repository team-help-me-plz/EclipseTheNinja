


#include "GAS/Ability/GAElementalFire.h"

#include "Abilities/Tasks/AbilityTask_WaitGameplayEffectRemoved.h"
#include "AbilitySystemComponent.h"
#include "GAS/Ability/GABaseAttack.h"
#include "GAS/Effects/GEElementalFire.h"
#include "GAS/StatAttributeSet.h"
#include "GAS/Tags/EclipseGameplayTags.h"

#include "GameFramework/Character.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "UObject/ConstructorHelpers.h"

UGAElementalFire::UGAElementalFire()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly;

	FGameplayTagContainer Tags;
	Tags.AddTag(EclipseGameplayTags::Ability_Element_Fire);
	SetAssetTags(Tags);

	// 이미 불이 붙어 있으면 다시 발동하지 않는다.
	ActivationBlockedTags.AddTag(EclipseGameplayTags::State_Weapon_Element_Fire);
	// 콤보 도중에는 속성을 바꾸지 않는다.
	ActivationBlockedTags.AddTag(UGABaseAttack::GetAttackingTag());

	FireElementEffectClass = UGEElementalFire::StaticClass();

	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> FireSwordAsset(
		TEXT("/Game/VFX/SwordEffects/Particles/NS_SwordEffect7.NS_SwordEffect7"));

	if (FireSwordAsset.Succeeded())
	{
		FireSwordSystem = FireSwordAsset.Object;
	}
}

bool UGAElementalFire::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	// GE가 없으면 쿨타임만 소모되므로 발동하지 않는다.
	if (!FireElementEffectClass) return false;

	// 시전자의 ASC, 스탯어트리뷰트, 체력이 0이하인지 확인
	const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!ASC || !ASC->GetSet<UStatAttributeSet>()) return false;
	if (ASC->GetNumericAttribute(UStatAttributeSet::GetHealthAttribute()) <= 0.0f) return false;

	return true;
}

void UGAElementalFire::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// 쿨타임은 불이 꺼진 뒤에 시작하므로 여기서는 코스트만 소모한다.
	if (!CommitAbilityCost(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, false, true);
		return;
	}

	// 불속성 GE 적용. 지속시간이 끝나면 GE가 빠지면서 Fire 태그와 칼 불꽃 Cue도 함께 사라진다.
	FireElementEffectHandle = ApplyGameplayEffectToOwner(Handle, ActorInfo, ActivationInfo,
		FireElementEffectClass->GetDefaultObject<UGameplayEffect>(), GetAbilityLevel(Handle, ActorInfo));
	if (!FireElementEffectHandle.IsValid())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, false, true);
		return;
	}

	// 불속성 GE가 끝날 때까지 기다린다.
	UAbilityTask_WaitGameplayEffectRemoved* WaitRemovedTask =
		UAbilityTask_WaitGameplayEffectRemoved::WaitForGameplayEffectRemoved(this, FireElementEffectHandle);
	WaitRemovedTask->OnRemoved.AddDynamic(this, &UGAElementalFire::OnFireElementRemoved);
	WaitRemovedTask->ReadyForActivation();

	// 불이 붙어 있는 동안 칼에 불꽃 이펙트를 붙인다. 불이 꺼지면 EndAbility에서 멈춘다.
	const ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (FireSwordSystem && Character && Character->GetMesh())
	{
		FireSwordEffect = UNiagaraFunctionLibrary::SpawnSystemAttached(FireSwordSystem, Character->GetMesh(),
			FireSwordSocketName, FireSwordLocationOffset, FireSwordRotationOffset,
			EAttachLocation::SnapToTarget, true);
	}

	OnFireElementStarted.Broadcast();
}

bool UGAElementalFire::GetFireElementTime(float& OutRemaining, float& OutDuration) const
{
	OutRemaining = 0.0f;
	OutDuration = 0.0f;

	const UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	const FActiveGameplayEffect* ActiveEffect = (ASC && FireElementEffectHandle.IsValid())
		? ASC->GetActiveGameplayEffect(FireElementEffectHandle) : nullptr;
	if (!ActiveEffect) return false;

	OutRemaining = FMath::Max(0.0f, ActiveEffect->GetTimeRemaining(ASC->GetWorld()->GetTimeSeconds()));
	OutDuration = ActiveEffect->GetDuration();
	return true;
}

FElementalFireDamageInfo UGAElementalFire::CalcFireDamage(const float AttackPower, const float Level) const
{
	FElementalFireDamageInfo Info;
	Info.HitDamage = AttackPower * FireHitDamageRatio;
	Info.TickDamage = AttackPower * FireDotRatio;

	// 화상 시간과 주기는 화상 GE에서 읽는다. Duration이 Scalable Float가 아니면(SetByCaller 등) 0으로 둔다.
	if (FireBurnEffectClass)
	{
		const UGameplayEffect* BurnEffect = FireBurnEffectClass->GetDefaultObject<UGameplayEffect>();
		const float Period = BurnEffect->Period.GetValueAtLevel(Level);
		if (BurnEffect->DurationMagnitude.GetStaticMagnitudeIfPossible(Level, Info.BurnDuration) && Period > 0.0f)
		{
			// 적용 즉시 틱은 없으므로 내림한다.
			Info.TickCount = FMath::FloorToInt(Info.BurnDuration / Period + KINDA_SMALL_NUMBER);
		}
	}

	Info.TotalDamage = Info.HitDamage + Info.TickDamage * Info.TickCount;
	return Info;
}

#if WITH_EDITOR
void UGAElementalFire::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	PreviewDamage = CalcFireDamage(PreviewAttackPower);
}
#endif

void UGAElementalFire::OnFireElementRemoved(const FGameplayEffectRemovalInfo& RemovalInfo)
{
	if (!IsActive()) return;

	// UI가 지속시간 표시를 먼저 끝낸 뒤 쿨타임 표시로 넘어가도록 순서를 지킨다.
	FireElementEffectHandle.Invalidate();
	OnFireElementEnded.Broadcast();

	// 불이 꺼진 시점부터 쿨타임을 시작한다. (쿨타임 UI도 이때 시작된다)
	CommitAbilityCooldown(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, false);
}

void UGAElementalFire::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const FActiveGameplayEffectHandle RemainingFireHandle = FireElementEffectHandle;
	FireElementEffectHandle.Invalidate();

	// 새 불꽃 생성만 멈추고, 남은 불꽃은 수명대로 사라진 뒤 컴포넌트가 스스로 정리된다.
	if (IsValid(FireSwordEffect))
	{
		FireSwordEffect->Deactivate();
	}
	FireSwordEffect = nullptr;

	// Super에서 대기 태스크가 정리된 뒤에 GE를 지워야 OnFireElementRemoved가 다시 불리지 않는다.
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);

	// 사망 등으로 어빌리티가 취소되면 불도 함께 끈다. 이 경우 쿨타임은 시작하지 않는다.
	if (bWasCancelled && ASC && RemainingFireHandle.IsValid())
	{
		ASC->RemoveActiveGameplayEffect(RemainingFireHandle);
		OnFireElementEnded.Broadcast();
	}
}
