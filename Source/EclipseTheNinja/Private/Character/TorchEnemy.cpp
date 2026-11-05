#include "Character/TorchEnemy.h"

#include "Component/AllySummonComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GAS/Ability/GASummonAllies.h"
#include "NiagaraComponent.h"

ATorchEnemy::ATorchEnemy()
{
	AllySummonComp = CreateDefaultSubobject<UAllySummonComponent>(TEXT("AllySummonComp"));

	// 횃불 애니가 왼손으로 들고 있으므로 왼손에 붙이고, 불은 횃불 메시를 따라간다. 위치는 BP에서 맞춘다.
	TorchMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TorchMesh"));
	TorchMesh->SetupAttachment(GetMesh(), TEXT("hand_l"));
	TorchMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TorchMesh->SetGenerateOverlapEvents(false);

	TorchFlame = CreateDefaultSubobject<UNiagaraComponent>(TEXT("TorchFlame"));
	TorchFlame->SetupAttachment(TorchMesh);

	// 소환 어빌리티를 기본으로 부여한다. 기본 공격 항목은 기존 적과 같이 블루프린트에서 지정한다.
	FAbilityInfo& SummonInfo = AbilityClassArray.AddDefaulted_GetRef();
	SummonInfo.AbilityClass = UGASummonAllies::StaticClass();
	SummonInfo.InputID = EAbilityInputID::SummonAllies;
}

void ATorchEnemy::ResetToInitial_Implementation()
{
	Super::ResetToInitial_Implementation();

	AllySummonComp->ResetSummon();
}
