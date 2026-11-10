#include "Component/BossEnemyGimmickComponent.h"

#include "Character/BossEnemy.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Level/BossGimmickPortal.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

UBossEnemyGimmickComponent::UBossEnemyGimmickComponent()
{
	// 보스 델리게이트로만 구동한다. 레이저 시작점은 부착으로, 끝점은 고정값으로 따라가므로 Tick이 필요 없다.
	PrimaryComponentTick.bCanEverTick = false;

	// 레이저 에셋은 보스 Blueprint의 GimmickComp 기본값에서 지정한다.
}

void UBossEnemyGimmickComponent::BeginPlay()
{
	Super::BeginPlay();

	Boss = Cast<ABossEnemy>(GetOwner());
	if (ABossEnemy* Owner = Boss.Get())
	{
		Owner->OnEnergyLinkActivated.AddDynamic(this, &UBossEnemyGimmickComponent::HandleEnergyLinkActivated);
		Owner->OnEnergyLinkBroken.AddDynamic(this, &UBossEnemyGimmickComponent::HandleEnergyLinkEnded);
		Owner->OnEnergyLinkFailed.AddDynamic(this, &UBossEnemyGimmickComponent::HandleEnergyLinkFailed);
	}
}

void UBossEnemyGimmickComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ABossEnemy* Owner = Boss.Get())
	{
		Owner->OnEnergyLinkActivated.RemoveDynamic(this, &UBossEnemyGimmickComponent::HandleEnergyLinkActivated);
		Owner->OnEnergyLinkBroken.RemoveDynamic(this, &UBossEnemyGimmickComponent::HandleEnergyLinkEnded);
		Owner->OnEnergyLinkFailed.RemoveDynamic(this, &UBossEnemyGimmickComponent::HandleEnergyLinkFailed);
	}

	Super::EndPlay(EndPlayReason);
}

bool UBossEnemyGimmickComponent::RequestBreakLink(FName LinkId)
{
	ABossEnemy* Owner = Boss.Get();
	if (!Owner || !IsLinkActive(LinkId))
	{
		return false;
	}

	// 보스의 OnEnergyLinkBroken이 HandleEnergyLinkEnded로 레이저와 포탈을 정리한다.
	Owner->BreakEnergyLink();
	return true;
}

bool UBossEnemyGimmickComponent::IsLinkActive(FName LinkId) const
{
	return !LinkId.IsNone() && GetActiveLinkId() == LinkId;
}

FName UBossEnemyGimmickComponent::GetActiveLinkId() const
{
	if (!LinkSteps.IsValidIndex(ActiveStepIndex))
	{
		return NAME_None;
	}

	return LinkSteps[ActiveStepIndex].LinkId;
}

void UBossEnemyGimmickComponent::ResetGimmick()
{
	HandleEnergyLinkEnded();
}

UBossEnemyGimmickComponent* UBossEnemyGimmickComponent::FindByLinkId(const UObject* WorldContextObject, FName LinkId)
{
	UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	if (!World || LinkId.IsNone())
	{
		return nullptr;
	}

	for (TActorIterator<ABossEnemy> It(World); It; ++It)
	{
		UBossEnemyGimmickComponent* Gimmick = It->FindComponentByClass<UBossEnemyGimmickComponent>();
		if (!Gimmick)
		{
			continue;
		}

		const bool bHasLink = Gimmick->LinkSteps.ContainsByPredicate([LinkId](const FBossGimmickLinkStep& Step)
		{
			return Step.LinkId == LinkId;
		});
		if (bHasLink)
		{
			return Gimmick;
		}
	}

	return nullptr;
}

void UBossEnemyGimmickComponent::HandleEnergyLinkActivated(int32 LinkIndex)
{
	if (!LinkSteps.IsValidIndex(LinkIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: 에너지 링크 %d에 해당하는 기믹(LinkSteps)이 없습니다."), *GetNameSafe(GetOwner()), LinkIndex);
		return;
	}

	ActiveStepIndex = LinkIndex;
	const FBossGimmickLinkStep& Step = LinkSteps[LinkIndex];

	StartLaser(Step.BeamTarget);
	NotifyPortals(Step.LinkId, true);
}

void UBossEnemyGimmickComponent::HandleEnergyLinkEnded()
{
	const FName EndedLinkId = GetActiveLinkId();
	ActiveStepIndex = INDEX_NONE;

	StopLaser();

	if (!EndedLinkId.IsNone())
	{
		NotifyPortals(EndedLinkId, false);
	}
}

void UBossEnemyGimmickComponent::HandleEnergyLinkFailed()
{
	HandleEnergyLinkEnded();

	// 실패하면 바로 처형 컷신이 나오므로 남은 레이저 입자까지 즉시 지운다.
	if (LaserComp)
	{
		LaserComp->DeactivateImmediate();
	}
}

void UBossEnemyGimmickComponent::StartLaser(const AActor* Target)
{
	ABossEnemy* Owner = Boss.Get();
	if (!Owner || !IsValid(Target))
	{
		return;
	}

	if (!LaserComp)
	{
		UNiagaraSystem* System = LaserSystem.LoadSynchronous();
		if (!System)
		{
			return;
		}

		// 보스 루트에 붙여 시작점이 보스를 따라다니게 한다.
		LaserComp = UNiagaraFunctionLibrary::SpawnSystemAttached(
			System,
			Owner->GetRootComponent(),
			NAME_None,
			BeamStartOffset,
			FRotator::ZeroRotator,
			EAttachLocation::KeepRelativeOffset,
			false,
			false,
			ENCPoolMethod::None,
			false
		);
		if (!LaserComp)
		{
			return;
		}
	}

	// 빔 끝점은 월드 좌표(Absolute Beam End)로 쓰이므로 대상 위치를 그대로 넣는다.
	LaserComp->SetVariableVec3(BeamEndParameter, Target->GetActorLocation() + BeamTargetOffset);
	LaserComp->Activate(true);
}

void UBossEnemyGimmickComponent::StopLaser()
{
	if (LaserComp)
	{
		LaserComp->Deactivate();
	}
}

void UBossEnemyGimmickComponent::NotifyPortals(FName LinkId, bool bActive) const
{
	UWorld* World = GetWorld();
	if (!World || LinkId.IsNone())
	{
		return;
	}

	for (TActorIterator<ABossGimmickPortal> It(World); It; ++It)
	{
		if (It->GetLinkId() == LinkId)
		{
			It->SetLinkActive(bActive);
		}
	}
}
