#include "GAS/Animation/AnimNotifyState_SwordTrail.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GAS/Tags/EclipseGameplayTags.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "UObject/ConstructorHelpers.h"

UAnimNotifyState_SwordTrail::UAnimNotifyState_SwordTrail()
{
#if WITH_EDITORONLY_DATA
	NotifyColor = FColor(220, 40, 60);
#endif

	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> TrailAsset(
		TEXT("/Game/VFX/SwordEffects/Particles/NS_SwordTrail_Simple.NS_SwordTrail_Simple"));

	if (TrailAsset.Succeeded())
	{
		TrailSystem = TrailAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> TrailMaterialAsset(
		TEXT("/Game/VFX/SwordEffects/Materials/MI_SwordTrail_Simple.MI_SwordTrail_Simple"));

	if (TrailMaterialAsset.Succeeded())
	{
		TrailMaterial = TrailMaterialAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> FireTrailMaterialAsset(
		TEXT("/Game/VFX/SwordEffects/Materials/MI_SwordTrail_Fire.MI_SwordTrail_Fire"));

	if (FireTrailMaterialAsset.Succeeded())
	{
		FireTrailMaterial = FireTrailMaterialAsset.Object;
	}
}

void UAnimNotifyState_SwordTrail::NotifyBegin(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	float TotalDuration,
	const FAnimNotifyEventReference& EventReference
)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (!MeshComp)
	{
		return;
	}

	// 칼에 불이 붙어 있으면 불 트레일을 쓴다. 불 이펙트를 비워 두면 기본 이펙트에 불 머티리얼만 바꿔 쓴다.
	const bool bFire = IsWeaponOnFire(MeshComp);
	UNiagaraSystem* System = (bFire && FireTrailSystem) ? FireTrailSystem.Get() : TrailSystem.Get();
	UMaterialInterface* Material = (bFire && FireTrailMaterial) ? FireTrailMaterial.Get() : TrailMaterial.Get();
	if (!System)
	{
		return;
	}

	// 구간이 겹치면 이전 트레일을 먼저 멈춘다.
	TWeakObjectPtr<UNiagaraComponent> Previous;
	if (ActiveTrails.RemoveAndCopyValue(MeshComp, Previous) && Previous.IsValid())
	{
		Previous->Deactivate();
	}

	USceneComponent* AttachComponent = FindAttachComponent(MeshComp);
	if (!AttachComponent)
	{
		return;
	}

	// 첫 프레임부터 폭과 머티리얼이 적용되도록 값을 넣은 뒤에 활성화한다.
	// (먼저 켜면 이펙트 기본 머티리얼로 한두 프레임 그려져 칼 전체에 흰 선이 남는다)
	UNiagaraComponent* Trail = UNiagaraFunctionLibrary::SpawnSystemAttached(System, AttachComponent,
		SocketName, FVector(0.0f, 0.0f, TrailOffset), FRotator::ZeroRotator,
		EAttachLocation::SnapToTarget, true, false);
	if (!Trail)
	{
		return;
	}

	Trail->SetVariableFloat(TEXT("User.TrailSize"), TrailWidth);
	if (Material)
	{
		Trail->SetVariableMaterial(TEXT("User.TrailMaterial"), Material);
	}
	Trail->Activate(true);

	ActiveTrails.Add(MeshComp, Trail);
}

void UAnimNotifyState_SwordTrail::NotifyEnd(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference
)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	// 새 리본 생성만 멈추고, 남은 리본은 수명대로 사라진 뒤 컴포넌트가 스스로 정리된다.
	TWeakObjectPtr<UNiagaraComponent> Trail;
	if (MeshComp && ActiveTrails.RemoveAndCopyValue(MeshComp, Trail) && Trail.IsValid())
	{
		Trail->Deactivate();
	}
}

FString UAnimNotifyState_SwordTrail::GetNotifyName_Implementation() const
{
	return TEXT("Sword Trail");
}

bool UAnimNotifyState_SwordTrail::IsWeaponOnFire(const USkeletalMeshComponent* MeshComp) const
{
	const UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(MeshComp->GetOwner());
	return ASC && ASC->HasMatchingGameplayTag(EclipseGameplayTags::State_Weapon_Element_Fire);
}

USceneComponent* UAnimNotifyState_SwordTrail::FindAttachComponent(USkeletalMeshComponent* MeshComp) const
{
	// 애니메이션 에디터 미리보기처럼 메시 자체에 소켓이 있으면 그대로 쓴다.
	if (MeshComp->DoesSocketExist(SocketName))
	{
		return MeshComp;
	}

	const AActor* Owner = MeshComp->GetOwner();
	if (!Owner)
	{
		return nullptr;
	}

	// 칼은 캐릭터 메시와 별도의 메시 컴포넌트이므로 소켓을 가진 메시를 찾는다.
	// 소켓이 없는 캐릭터(칼이 없는 적 등)는 트레일을 붙이지 않는다.
	TInlineComponentArray<UMeshComponent*> Meshes(Owner);
	for (UMeshComponent* Mesh : Meshes)
	{
		if (Mesh && Mesh->DoesSocketExist(SocketName))
		{
			return Mesh;
		}
	}
	return nullptr;
}
