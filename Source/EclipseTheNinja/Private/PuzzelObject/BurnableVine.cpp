


#include "PuzzelObject/BurnableVine.h"
#include "Checkpoint/CheckpointBoundComponent.h"
#include "Util/EclipseCollisionChannels.h"
#include "GAS/Damage/FireDamageType.h"
#include "Interface/VineLockable.h"

#include "Components/BoxComponent.h"
#include "Components/ChildActorComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "UObject/ConstructorHelpers.h"

// Sets default values
ABurnableVine::ABurnableVine()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	this->RootScene = CreateDefaultSubobject<USceneComponent>(TEXT("RootScene"));
	SetRootComponent(this->RootScene);

	this->CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
	this->CollisionBox->SetupAttachment(GetRootComponent());
	this->CollisionBox->SetCollisionObjectType(ECC_Vine);
	this->CollisionBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	this->CollisionBox->SetCollisionResponseToAllChannels(ECR_Ignore);

	this->IdleMeshAssembly = CreateDefaultSubobject<UChildActorComponent>(TEXT("IdleMeshAssembly"));
	this->IdleMeshAssembly->SetupAttachment(GetRootComponent());

	this->BurnedMeshAssembly = CreateDefaultSubobject<UChildActorComponent>(TEXT("BurnedMeshAssembly"));
	this->BurnedMeshAssembly->SetupAttachment(GetRootComponent());

	// 불이 붙을 때만 켜므로 자동 재생은 끈다.
	this->BurnEffect = CreateDefaultSubobject<UNiagaraComponent>(TEXT("BurnEffect"));
	this->BurnEffect->SetupAttachment(GetRootComponent());
	this->BurnEffect->bAutoActivate = false;

	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> BurnSystem(
		TEXT("/Game/VFX/3D_Fire_and_Explosions/Effects/P_Fire_4.P_Fire_4"));

	if (BurnSystem.Succeeded())
	{
		this->BurnEffect->SetAsset(BurnSystem.Object);
	}

	this->CheckpointBoundComponent = CreateDefaultSubobject<UCheckpointBoundComponent>(TEXT("CheckpointBoundComponent"));
}

// Called when the game starts or when spawned
void ABurnableVine::BeginPlay()
{
	Super::BeginPlay();

	// 외형 메시는 충돌 없이 보여주기만 하고, 다 탄 외형은 숨겨 둔다.
	this->DisableAssemblyCollision(this->IdleMeshAssembly);
	this->DisableAssemblyCollision(this->BurnedMeshAssembly);
	this->SetAssemblyVisible(this->IdleMeshAssembly, true);
	this->SetAssemblyVisible(this->BurnedMeshAssembly, false);

	if (this->bIsBlockPath)
	{
		this->CollisionBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	}
	if (this->TargetActor && this->TargetActor->Implements<UVineLockable>())
	{
		IVineLockable::Execute_AddInteractLock(TargetActor);
	}

	this->CollectOverlappingVines();
}

// Called every frame
void ABurnableVine::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

float ABurnableVine::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	float RetDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator,DamageCauser);

	if (DamageEvent.DamageTypeClass && DamageEvent.DamageTypeClass->IsChildOf(UFireDamageType::StaticClass()))
	{
		this->Ignite();
	}
	return (RetDamage);
}

void ABurnableVine::ResetToInitial_Implementation()
{
	this->SetAssemblyVisible(this->IdleMeshAssembly, true);
	this->SetAssemblyVisible(this->BurnedMeshAssembly, false);
	this->CollisionBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	if (this->bIsBlockPath)
	{
		this->CollisionBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	}
	// 리셋은 즉시 원래 모습이어야 하므로 남은 파티클까지 바로 지운다.
	this->BurnEffect->DeactivateImmediate();
	if (this->bIsBurned)
	{
		this->bIsBurned = false;
		if (this->TargetActor && this->TargetActor->Implements<UVineLockable>())
		{
			IVineLockable::Execute_AddInteractLock(TargetActor);
		}
	}
	this->bIsBurning = false;
	GetWorld()->GetTimerManager().ClearTimer(this->BurnTimer);
	GetWorld()->GetTimerManager().ClearTimer(this->SpreadTimer);
}

void ABurnableVine::OnAfterReset_Implementation()
{
}

void ABurnableVine::Ignite()
{
	if (this->bIsBurned || this->bIsBurning)
	{
		return ;
	}
	this->bIsBurning = true;
	this->BurnEffect->Activate(true);
	GetWorld()->GetTimerManager().SetTimer(
		this->BurnTimer,
		this,
		&ABurnableVine::FinishBurn,
		this->BurnDuration,
		false
	);
	GetWorld()->GetTimerManager().SetTimer(
		this->SpreadTimer,
		this,
		&ABurnableVine::SpreadFire,
		this->SpreadDelay,
		false
	);
}

void ABurnableVine::FinishBurn()
{
	this->bIsBurning = false;
	this->bIsBurned = true;
	this->CollisionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (this->bIsBlockPath)
	{
		this->CollisionBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	}
	if (this->TargetActor && this->TargetActor->Implements<UVineLockable>())
	{
		IVineLockable::Execute_RemoveInteractLock(TargetActor);
	}
	// 다 탄 외형이 지정되지 않았으면 SetAssemblyVisible이 아무것도 하지 않으므로 덩굴은 그냥 사라진다.
	this->SetAssemblyVisible(this->IdleMeshAssembly, false);
	this->SetAssemblyVisible(this->BurnedMeshAssembly, true);
	// 새 파티클 생성만 멈추고, 남은 불꽃은 수명대로 자연스럽게 사라지게 한다.
	this->BurnEffect->Deactivate();
}

void ABurnableVine::SpreadFire()
{
	for (ABurnableVine* Vine : this->NeighborVines)
	{
		if (IsValid(Vine))
		{
			Vine->Ignite();
		}
	}
}

void ABurnableVine::SetAssemblyVisible(UChildActorComponent* InAssembly, bool bVisible)
{
	AActor* AssemblyActor = InAssembly ? InAssembly->GetChildActor() : nullptr;
	if (!IsValid(AssemblyActor))
	{
		return;
	}
	AssemblyActor->SetActorHiddenInGame(!bVisible);
}

void ABurnableVine::CollectOverlappingVines()
{
	// CollisionBox는 Vine 채널에 Ignore라 겹침 이벤트로는 찾을 수 없으므로, 박스 모양으로 Vine 오브젝트를 직접 검색한다.
	// 오브젝트 타입 쿼리는 응답 설정과 관계없이 Query가 켜진 Vine 콜리전을 모두 찾는다.
	TArray<FOverlapResult> Overlaps;
	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_Vine);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BurnableVineNeighbor), false, this);

	GetWorld()->OverlapMultiByObjectType(
		Overlaps,
		this->CollisionBox->GetComponentLocation(),
		this->CollisionBox->GetComponentQuat(),
		ObjectParams,
		FCollisionShape::MakeBox(this->CollisionBox->GetScaledBoxExtent()),
		QueryParams
	);

	for (const FOverlapResult& Overlap : Overlaps)
	{
		ABurnableVine* Vine = Cast<ABurnableVine>(Overlap.GetActor());
		if (IsValid(Vine) && Vine != this)
		{
			this->NeighborVines.AddUnique(Vine);
		}
	}
}

void ABurnableVine::DisableAssemblyCollision(UChildActorComponent* InAssembly)
{
	AActor* AssemblyActor = InAssembly ? InAssembly->GetChildActor() : nullptr;
	if (!IsValid(AssemblyActor))
	{
		return;
	}

	TArray<UStaticMeshComponent*> Meshes;
	AssemblyActor->GetComponents<UStaticMeshComponent>(Meshes);

	for (UStaticMeshComponent* Mesh : Meshes)
	{
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

