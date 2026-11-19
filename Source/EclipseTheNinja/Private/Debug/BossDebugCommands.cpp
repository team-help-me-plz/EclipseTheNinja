#include "Character/BossEnemy.h"

#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GAS/Effects/GEBasicAttackDamage.h"
#include "GAS/StatAttributeSet.h"
#include "GameFramework/Pawn.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	void ExecuteBossDebugDamage(const TArray<FString>& Args, UWorld* World)
	{
		if (!World)
		{
			return;
		}

		float RequestedDamage = -1.0f;
		if (Args.Num() > 0)
		{
			RequestedDamage = FCString::Atof(*Args[0]);
			if (RequestedDamage <= 0.0f)
			{
				return;
			}
		}

		APawn* Player = UGameplayStatics::GetPlayerPawn(World, 0);

		for (TActorIterator<ABossEnemy> It(World); It; ++It)
		{
			ABossEnemy* Boss = *It;
			if (!IsValid(Boss) || Boss->IsDead())
			{
				continue;
			}

			UAbilitySystemComponent* ASC = Boss->GetAbilitySystemComponent();
			const UStatAttributeSet* StatSet = Boss->GetStatAttribute();
			if (!ASC || !StatSet)
			{
				continue;
			}

			// 인자가 없으면 최대 체력만큼 피해를 넣어 다음 에너지 링크 구간 체력에서 멈추고 실드가 발동하게 한다.
			const float DamageAmount = RequestedDamage > 0.0f
				? RequestedDamage
				: FMath::Max(1.0f, StatSet->GetMaxHealth());

			FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
			if (IsValid(Player))
			{
				Context.AddInstigator(Player, Player);
			}

			FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(UGEBasicAttackDamage::StaticClass(), 1.0f, Context);
			if (Spec.IsValid())
			{
				Spec.Data->SetSetByCallerMagnitude(FName(TEXT("Damage")), DamageAmount);
				ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
			}
		}
	}

	void ExecuteBossDebugBreakLink(const TArray<FString>& Args, UWorld* World)
	{
		if (!World)
		{
			return;
		}

		for (TActorIterator<ABossEnemy> It(World); It; ++It)
		{
			ABossEnemy* Boss = *It;
			if (IsValid(Boss) && !Boss->IsDead())
			{
				Boss->BreakEnergyLink();
			}
		}
	}

	FAutoConsoleCommandWithWorldAndArgs GCommandBossDebugDamage(
		TEXT("Boss.DebugDamage"),
		TEXT("보스에게 피해를 적용합니다. 인자를 생략하면 다음 에너지 링크(실드 패턴) 구간까지 체력을 깎습니다. 사용법: Boss.DebugDamage [Amount]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecuteBossDebugDamage)
	);

	FAutoConsoleCommandWithWorldAndArgs GCommandBossDebugBreakLink(
		TEXT("Boss.DebugBreakLink"),
		TEXT("현재 활성화된 보스의 에너지 링크(실드)를 해제합니다."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecuteBossDebugBreakLink)
	);
}
