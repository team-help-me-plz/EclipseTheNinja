


#include "Trap/TrapArrowActor.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameFramework/Character.h"
#include "Interface/Interactor.h"
#include "ObjectPool/ObjectPoolSubsystem.h"

// Sets default values
ATrapArrowActor::ATrapArrowActor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	this->SphereCollision = CreateDefaultSubobject<USphereComponent>(TEXT("SphereCollision"));
	this->SphereCollision->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Overlap);
	//this->SphereCollision->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Overlap);
	//this->SphereCollision->SetCollisionResponseToChannel(ECollisionChannel::ECC_WorldStatic, ECollisionResponse::ECR_Overlap);
	//this->SphereCollision->SetCollisionResponseToChannel(ECollisionChannel::ECC_WorldDynamic, ECollisionResponse::ECR_Overlap);
	this->SphereCollision->IgnoreActorWhenMoving(this->GetOwner(), true);
	//this->SphereCollision->;
	//this->SphereCollision->;
	SetRootComponent(this->SphereCollision);

	this->Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	this->Mesh->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
	this->Mesh->SetupAttachment(GetRootComponent());

	this->ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	this->ProjectileMovement->SetUpdatedComponent(this->SphereCollision);
	this->ProjectileMovement->ProjectileGravityScale = 0.f;
	this->ProjectileMovement->bRotationFollowsVelocity = true;
	this->ProjectileMovement->bShouldBounce = false;

}

// Called when the game starts or when spawned
void ATrapArrowActor::BeginPlay()
{
	Super::BeginPlay();
	
	//PostInitializeComponents();
}

void ATrapArrowActor::OnArrowBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// 플레이어 혹은 분신이면 데미지 주기
	if (OtherActor && OtherActor->Implements<UInteractor>())
	{
		ACharacter* InCharacter = Cast<ACharacter>(OtherActor);
		if (InCharacter)
		{
			// 캡슐 등 다른 컴포넌트와 먼저 겹쳐도 화살이 사라지지 않도록 메시와 겹칠 때만 처리
			if (OtherComp != InCharacter->GetMesh())
			{
				return ;
			}
			IInteractor::Execute_ReturnToSafePoint(OtherActor);
		}
	}
	UObjectPoolSubsystem::Release(this);
}

void ATrapArrowActor::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	this->SphereCollision->OnComponentBeginOverlap.AddDynamic(this, &ATrapArrowActor::OnArrowBeginOverlap);
}

// Called every frame
void ATrapArrowActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void ATrapArrowActor::SetLaunch(float InVelocity)
{
	this->ProjectileMovement->MaxSpeed = InVelocity;
	this->ProjectileMovement->SetVelocityInLocalSpace(FVector(InVelocity, 0.f, 0.f));
}

void ATrapArrowActor::OnSpawnFromPool_Implementation()
{
	// 충돌로 시뮬레이션이 멈추면 UpdatedComponent가 해제되므로 다시 지정 (속도는 SetLaunch에서 설정)
	this->ProjectileMovement->SetUpdatedComponent(this->SphereCollision);
}

void ATrapArrowActor::OnReturnToPool_Implementation()
{
	this->ProjectileMovement->StopMovementImmediately();
}
