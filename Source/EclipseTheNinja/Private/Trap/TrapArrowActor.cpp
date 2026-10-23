


#include "Trap/TrapArrowActor.h"
#include "Interface/Interactor.h"
#include "Character/BaseCharacter.h"
#include "ObjectPool/ObjectPoolSubsystem.h"
#include "GAS/StatAttributeSet.h"
#include "GAS/Effects/GEBasicAttackDamage.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "AbilitySystemComponent.h"

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
	// 풀에 반환된 뒤에도 같은 스윕의 남은 오버랩(메시 본별 중복)이 들어올 수 있다
	if (this->bHasHit)
	{
		return;
	}
	if (OtherActor && OtherActor->Implements<UInteractor>())
	{
		ABaseCharacter* InCharacter = Cast<ABaseCharacter>(OtherActor);
		if (InCharacter)
		{
			// 캡슐 등 다른 컴포넌트와 먼저 겹쳐도 화살이 사라지지 않도록 메시와 겹칠 때만 처리
			if (OtherComp != InCharacter->GetMesh())
			{
				return ;
			}
			UAbilitySystemComponent* TargetASC = InCharacter->GetAbilitySystemComponent();
			// 플레이어면 데미지 주기
			if (TargetASC && TargetASC->GetSet<UStatAttributeSet>()
				&& TargetASC->GetNumericAttribute(UStatAttributeSet::GetHealthAttribute()) > 0.f)
			{
				FGameplayEffectContextHandle	Context = TargetASC->MakeEffectContext();
				Context.AddInstigator(this, this);

				FGameplayEffectSpecHandle	Spec = TargetASC->MakeOutgoingSpec(UGEBasicAttackDamage::StaticClass(), 1.f, Context);
				if (Spec.IsValid())
				{
					Spec.Data->SetSetByCallerMagnitude(FName(TEXT("Damage")), this->ArrowDamage);
					TargetASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
				}
			}
			IInteractor::Execute_ReturnToSafePoint(OtherActor);
		}
	}
	this->bHasHit = true;
	UObjectPoolSubsystem::Release(this);
}

void ATrapArrowActor::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	this->SphereCollision->OnComponentBeginOverlap.AddDynamic(this, &ATrapArrowActor::OnArrowBeginOverlap);
}

void ATrapArrowActor::LifeSpanExpired()
{
	UObjectPoolSubsystem::Release(this);
}

// Called every frame
void ATrapArrowActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void ATrapArrowActor::SetLaunch(float InVelocity, float InDamage)
{
	this->ProjectileMovement->MaxSpeed = InVelocity;
	this->ProjectileMovement->SetVelocityInLocalSpace(FVector(InVelocity, 0.f, 0.f));
	this->ArrowDamage = InDamage;

	SetLifeSpan(Lifetime);
}

void ATrapArrowActor::OnSpawnFromPool_Implementation()
{
	// 충돌로 시뮬레이션이 멈추면 UpdatedComponent가 해제되므로 다시 지정 (속도는 SetLaunch에서 설정)
	this->ProjectileMovement->SetUpdatedComponent(this->SphereCollision);
	this->bHasHit = false;
}

void ATrapArrowActor::OnReturnToPool_Implementation()
{
	this->ProjectileMovement->StopMovementImmediately();
}
