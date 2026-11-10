


#include "PuzzelObject/BreakablePlatform.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Interface/Interactor.h"
#include "GameFramework/Character.h"
#include "Checkpoint/CheckpointBoundComponent.h"

#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"
#include "Util/EclipseCollisionChannels.h"

// Sets default values
ABreakablePlatform::ABreakablePlatform()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	this->RootScene = CreateDefaultSubobject<USceneComponent>(TEXT("RootScene"));
	SetRootComponent(this->RootScene);

	this->CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
	this->CollisionBox->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Overlap);
	this->CollisionBox->SetupAttachment(GetRootComponent());

	this->MeshAssembly = CreateDefaultSubobject<UChildActorComponent>(TEXT("MeshAssembly"));
	this->MeshAssembly->SetupAttachment(GetRootComponent());

	this->CheckpointBound = CreateDefaultSubobject<UCheckpointBoundComponent>(TEXT("CheckpointBound"));

	// 무너질 때만 켜므로 자동 재생은 끈다.
	this->BreakEffect = CreateDefaultSubobject<UNiagaraComponent>(TEXT("BreakEffect"));
	this->BreakEffect->SetupAttachment(GetRootComponent());
	this->BreakEffect->bAutoActivate = false;

	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> BreakSystem(
		TEXT("/Game/VFX/Vefects/Sand_VFX/VFX/SandFalling/Looped/NS_Sand_Ceiling_04_Looped_Platform.NS_Sand_Ceiling_04_Looped_Platform"));

	if (BreakSystem.Succeeded())
	{
		this->BreakEffect->SetAsset(BreakSystem.Object);
	}
}

void ABreakablePlatform::ResetToInitial_Implementation()
{
	GetWorldTimerManager().ClearTimer(this->PlatformBreakTimer);
	GetWorldTimerManager().ClearTimer(this->PlatformRestoreTimer);
	GetWorldTimerManager().ClearTimer(this->PlatformRestoreTryTimer);
	GetWorldTimerManager().ClearTimer(this->BreakEffectTimer);
	this->ClearStandCandidates();
	// 리셋은 즉시 원래 모습이어야 하므로 남은 파티클까지 바로 지운다.
	this->BreakEffect->DeactivateImmediate();
	this->RestorePlatform();
}

// Called when the game starts or when spawned
void ABreakablePlatform::BeginPlay()
{
	Super::BeginPlay();

	// 기본 이미터는 컴포넌트 X축을 향하는 판이라 사이드뷰 카메라에서는 옆면만 보인다. 카메라를 향하는 이미터를 쓴다.
	this->BreakEffect->SetVariableBool(TEXT("User.FaceCamera"), true);
	this->BreakEffect->SetVariableVec2(TEXT("User.ScaleXY"), this->BreakEffectScale);

	AActor* AssemblyActor = this->MeshAssembly->GetChildActor();
	if (!IsValid(AssemblyActor))
	{
		return;
	}

	TArray<UStaticMeshComponent*> Meshes;
	AssemblyActor->GetComponents<UStaticMeshComponent>(Meshes);

	for (UStaticMeshComponent* Mesh : Meshes)
	{
		Mesh->SetCollisionObjectType(ECC_WorldDynamic);

		Mesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
		Mesh->SetCollisionResponseToChannel(ECC_Monster, ECR_Block);
	}
}

// Called every frame
void ABreakablePlatform::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void ABreakablePlatform::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	this->CollisionBox->OnComponentBeginOverlap.AddDynamic(this, &ABreakablePlatform::OnCollisionBeginOverlap);
	this->CollisionBox->OnComponentEndOverlap.AddDynamic(this, &ABreakablePlatform::OnCollisionEndOverlap);
}

void ABreakablePlatform::OnCollisionBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!OtherActor)
	{
		return ;
	}
	if (OtherActor == this)
	{
		return ;
	}
	this->OverlapCount++;
	if (!OtherActor->Implements<UInteractor>())
	{
		return ;
	}
	// 박스에 들어온 것만으로는 부수지 않는다. 메시를 실제로 밟았는지 확인할 후보로 등록한다
	ACharacter*	Character = Cast<ACharacter>(OtherActor);
	if (!Character)
	{
		return ;
	}
	this->StandCandidates.Add(Character);
	if (this->State != EBreakablePlatformState::Ready)
	{
		return ;
	}
	this->CheckStandingOnMesh();
	if (this->State == EBreakablePlatformState::Ready
		&& !GetWorldTimerManager().IsTimerActive(this->StandCheckTimer))
	{
		GetWorldTimerManager().SetTimer(
			this->StandCheckTimer,
			this,
			&ABreakablePlatform::CheckStandingOnMesh,
			this->StandCheckInterval,
			true
		);
	}
}

void ABreakablePlatform::CheckStandingOnMesh()
{
	if (this->State != EBreakablePlatformState::Ready)
	{
		GetWorldTimerManager().ClearTimer(this->StandCheckTimer);
		return ;
	}
	for (auto It = this->StandCandidates.CreateIterator(); It; ++It)
	{
		const ACharacter*	Character = It->Get();
		if (!Character)
		{
			It.RemoveCurrent();
			continue ;
		}
		if (this->IsStandingOnMesh(Character))
		{
			this->StartBreaking();
			return ;
		}
	}
	if (this->StandCandidates.IsEmpty())
	{
		GetWorldTimerManager().ClearTimer(this->StandCheckTimer);
	}
}

bool ABreakablePlatform::IsStandingOnMesh(const ACharacter* InCharacter) const
{
	// 캐릭터가 서 있는 컴포넌트(발밑)가 이 발판 외형의 메시인지 확인한다
	const UPrimitiveComponent*	MovementBase = InCharacter ? Cast<UPrimitiveComponent>(InCharacter->GetMovementBaseObject()) : nullptr;
	const AActor*				AssemblyActor = this->MeshAssembly->GetChildActor();
	return MovementBase && AssemblyActor && MovementBase->GetOwner() == AssemblyActor;
}

void ABreakablePlatform::ClearStandCandidates()
{
	GetWorldTimerManager().ClearTimer(this->StandCheckTimer);
	this->StandCandidates.Reset();
}

void ABreakablePlatform::StartBreaking()
{
	if (this->State != EBreakablePlatformState::Ready)
	{
		return ;
	}
	GetWorldTimerManager().ClearTimer(this->StandCheckTimer);
	this->State = EBreakablePlatformState::Breaking;
	// 플레이어를 감지한 순간 무너질 것을 알리는 이펙트를 짧게 재생한다.
	this->BreakEffect->Activate(true);
	GetWorldTimerManager().SetTimer(
		this->BreakEffectTimer,
		this,
		&ABreakablePlatform::StopBreakEffect,
		this->BreakEffectDuration,
		false
	);
	GetWorldTimerManager().SetTimer(
		this->PlatformBreakTimer,
		this,
		&ABreakablePlatform::BreakPlatform,
		this->BreakTime,
		false
	);
}

void ABreakablePlatform::OnCollisionEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (OtherActor == this)
	{
		return;
	}
	this->OverlapCount--;
	// 캡슐/메시 등 여러 컴포넌트가 겹칠 수 있으므로, 액터가 박스에서 완전히 나갔을 때만 후보에서 뺀다
	if (OtherActor && !this->CollisionBox->IsOverlappingActor(OtherActor))
	{
		this->StandCandidates.Remove(Cast<ACharacter>(OtherActor));
	}
}

void ABreakablePlatform::BreakPlatform()
{
	this->State = EBreakablePlatformState::Broken;
	this->MeshAssembly->SetVisibility(false);
	if (this->BreakSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, this->BreakSound, GetActorLocation());
	}

	AActor* AssemblyActor = this->MeshAssembly->GetChildActor();
	if (!IsValid(AssemblyActor))
	{
		return;
	}

	TArray<UStaticMeshComponent*> Meshes;
	AssemblyActor->GetComponents<UStaticMeshComponent>(Meshes);

	for (UStaticMeshComponent* Mesh : Meshes)
	{
		Mesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		Mesh->SetCollisionResponseToChannel(ECC_Monster, ECR_Ignore);
	}
	GetWorldTimerManager().SetTimer(
		this->PlatformRestoreTimer,
		this,
		&ABreakablePlatform::TryRestore,
		this->RestoreTime,
		false
	);
}

void ABreakablePlatform::StopBreakEffect()
{
	// 짧게 보여주고 끝내야 하므로 남은 파티클까지 바로 지운다.
	this->BreakEffect->DeactivateImmediate();
}

void ABreakablePlatform::TryRestore()
{
	if (this->OverlapCount)
	{
		GetWorldTimerManager().SetTimer(
			this->PlatformRestoreTryTimer,
			this,
			&ABreakablePlatform::TryRestore,
			this->RestoreTryTime,
			false
		);
		return ;
	}
	this->RestorePlatform();
}

void ABreakablePlatform::RestorePlatform()
{
	this->State = EBreakablePlatformState::Ready;
	this->MeshAssembly->SetVisibility(true);

	AActor* AssemblyActor = this->MeshAssembly->GetChildActor();
	if (!IsValid(AssemblyActor))
	{
		return;
	}

	TArray<UStaticMeshComponent*> Meshes;
	AssemblyActor->GetComponents<UStaticMeshComponent>(Meshes);

	for (UStaticMeshComponent* Mesh : Meshes)
	{
		Mesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
		Mesh->SetCollisionResponseToChannel(ECC_Monster, ECR_Block);
	}
}

