


#include "PuzzelObject/FallingObject.h"
#include "Component/CuttableComponent.h"
#include "Checkpoint/CheckpointBoundComponent.h"
#include "Character/BaseCharacter.h"
#include "GAS/StatAttributeSet.h"
#include "GAS/Effects/GEBasicAttackDamage.h"

#include "Components/BoxComponent.h"
#include "AbilitySystemComponent.h"

// Sets default values
AFallingObject::AFallingObject()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	//PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bCanEverTick = true;

	this->RootScene = CreateDefaultSubobject<USceneComponent>(TEXT("RootScene"));
	SetRootComponent(this->RootScene);

	this->RopeComponent = CreateDefaultSubobject<UCuttableComponent>(TEXT("RopeComponent"));
	this->RopeComponent->SetupAttachment(GetRootComponent());

	this->FallingBody = CreateDefaultSubobject<UBoxComponent>(TEXT("FallingBody"));
	this->FallingBody->SetupAttachment(GetRootComponent());
	FallingBody->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	FallingBody->SetCollisionResponseToAllChannels(ECR_Ignore);
	FallingBody->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	FallingBody->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);

	this->FallingMeshAssembly = CreateDefaultSubobject<UChildActorComponent>(TEXT("FallingMeshAssembly"));
	this->FallingMeshAssembly->SetupAttachment(this->FallingBody);

	this->FallingIndicator = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FallingIndicator"));
	this->FallingIndicator->SetupAttachment(GetRootComponent());
	this->FallingIndicator->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	this->FallingIndicator->SetCastShadow(false);
	this->FallingIndicator->SetVisibility(false);

	this->CheckpointBoundComponent = CreateDefaultSubobject<UCheckpointBoundComponent>(TEXT("CheckpointBoundComponent"));
}

// Called when the game starts or when spawned
void AFallingObject::BeginPlay()
{
	Super::BeginPlay();
	
	if (AActor* ChildActor = this->FallingMeshAssembly->GetChildActor())
	{
		this->FallingBody->IgnoreActorWhenMoving(ChildActor, true);
		// 외형은 충돌을 갖지 않는다. 충돌과 판정은 FallingBody가 담당한다
		// (외형 메시가 Pawn을 Block하면 떨어지며 캐릭터를 파고들어 끼이게 된다)
		ChildActor->SetActorEnableCollision(false);
	}
	this->FallingBody->OnComponentBeginOverlap.AddDynamic(this, &AFallingObject::OnFallingBodyBeginOverlap);
	this->FallingBody->OnComponentEndOverlap.AddDynamic(this, &AFallingObject::OnFallingBodyEndOverlap);
	this->InitialBodyRelativeLocation = this->FallingBody->GetRelativeLocation();
	this->RopeComponent->OnRopeCut.AddUObject(this, &AFallingObject::ObjectDrop);

}

// Called every frame
void AFallingObject::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (this->bIsFalling)
	{
		this->FallSpeed = FMath::Min(this->FallSpeed + this->GravityAccel * DeltaTime, this->MaxFallSpeed);
		float	Remaining = this->FallingBody->GetComponentLocation().Z - this->FallingLocation.Z;
		float	Step = FMath::Clamp(FallSpeed * DeltaTime, 0.f, Remaining);
		FHitResult	ObjectHit;
		this->FallingBody->AddWorldOffset(FVector(0.f, 0.f, -Step), true, &ObjectHit);
		// 캐릭터에 붙은 컴포넌트에 막혔다면 착지로 보지 않고, 그 캐릭터를 무시한 채 계속 떨어진다
		if (ObjectHit.bBlockingHit && this->IgnorePawnWhileFalling(ObjectHit.GetActor()))
		{
			return ;
		}
		if (ObjectHit.bBlockingHit || Remaining - Step <= KINDA_SMALL_NUMBER)
		{
			this->ObjectLand();
		}
	}
}

void AFallingObject::ToggleActivate_Implementation()
{
	if (this->bIsFalling || this->bIsLanded)
	{
		return ;
	}
	this->RopeComponent->CutRope();
}

void AFallingObject::ShowActivationIndicator_Implementation()
{
	if (this->bIsFalling || this->bIsLanded)
	{
		return ;
	}

	this->FindFallingLocation();
	
	const FVector	ObjectStart = this->FallingBody->GetComponentLocation();
	const FVector	ObjectExtent = this->FallingBody->GetScaledBoxExtent();
	const float		ObjectHeight = ObjectStart.Z - FallingLocation.Z;
	if (ObjectHeight <= KINDA_SMALL_NUMBER)
	{
		return ;
	}
	const FVector Center(ObjectStart.X, ObjectStart.Y, (ObjectStart.Z + this->FallingLocation.Z) * 0.5f - ObjectExtent.Z);

	this->FallingIndicator->SetWorldLocationAndRotation(Center, FallingBody->GetComponentQuat());
	this->FallingIndicator->SetWorldScale3D(FVector(ObjectExtent.X * 2.f, ObjectExtent.Y * 2.f, ObjectHeight) / 100.f);  // Cube = 100
	this->FallingIndicator->SetVisibility(true);
}

void AFallingObject::HideActivationIndicator_Implementation()
{
	this->FallingIndicator->SetVisibility(false);
}

void AFallingObject::ResetToInitial_Implementation()
{
	//FallingIndicator->SetVisibility(false);
	this->bIsFalling = false;
	this->bIsLanded = false;
	this->bIsPendingBlock = false;
	this->FallSpeed = 0.f;
	this->HitActors.Reset();
	this->ClearIgnoredPawns();
	this->FallingBody->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	this->FallingBody->SetRelativeLocation(this->InitialBodyRelativeLocation);
	this->RopeComponent->RestoreRope();
}

void AFallingObject::OnAfterReset_Implementation()
{
}

void AFallingObject::OnFallingBodyBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!this->bIsFalling)
	{
		return ;
	}
	this->ApplyFallingDamage(OtherActor, SweepResult);
}

void AFallingObject::OnFallingBodyEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (this->bIsPendingBlock)
	{
		this->TryBlockPawn();
	}
}

void AFallingObject::ApplyFallingDamage(AActor* InActor, const FHitResult& InHit)
{
	ABaseCharacter*	TargetActor = Cast<ABaseCharacter>(InActor);
	if (!TargetActor)
	{
		return ;
	}
	// 캡슐과 메시가 모두 겹칠 수 있으므로 한 번 낙하에 한 번만 데미지를 준다
	if (this->HitActors.Contains(TargetActor))
	{
		return ;
	}
	UAbilitySystemComponent*	TargetASC = TargetActor->GetAbilitySystemComponent();
	// 어트리뷰트가 없거나 이미 죽은 대상은 생략
	if (!TargetASC || !TargetASC->GetSet<UStatAttributeSet>()
		|| TargetASC->GetNumericAttribute(UStatAttributeSet::GetHealthAttribute()) <= 0.f)
	{
		return ;
	}
	this->HitActors.Add(TargetActor);

	FGameplayEffectContextHandle	Context = TargetASC->MakeEffectContext();
	Context.AddInstigator(this, this);
	Context.AddHitResult(InHit);

	FGameplayEffectSpecHandle	Spec = TargetASC->MakeOutgoingSpec(UGEBasicAttackDamage::StaticClass(), 1.f, Context);
	if (Spec.IsValid())
	{
		Spec.Data->SetSetByCallerMagnitude(FName(TEXT("Damage")), FallingDamage);
		TargetASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	}
}

void AFallingObject::FindFallingLocation()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return ;
	}
	// Ray 시작지점과 끝지점. MaxTraceDistance를 조정해 찾을 땅의 길이를 조절할 수 있다.
	const FVector	RayOrigin = this->FallingBody->GetComponentLocation();
	const FVector	RayEnd = RayOrigin - FVector(0.f, 0.f, this->MaxTraceDistance);
	// 스케일이 반영된 박스 크기 및 회전
	const FCollisionShape	ObjectShape = FCollisionShape::MakeBox(FallingBody->GetScaledBoxExtent());
	const FQuat				ObjectRot = FallingBody->GetComponentQuat();

	// 자기 자신과 외형은 무시
	FCollisionQueryParams	QueryParams(
		SCENE_QUERY_STAT(FindFallingLocation), 
		false,
		this
	);
	if (this->FallingMeshAssembly->GetChildActor())
	{
		QueryParams.AddIgnoredActor(this->FallingMeshAssembly->GetChildActor());
	}
	// 바닥 혹은 벽만 대상으로 찾는다 (Pawn은 제외)
	// 오브젝트 타입만으로 찾으면 적의 AssassinateBoundary 같은 판정용 박스(WorldDynamic, Pawn만 Overlap)도 잡히므로,
	// 실제로 이 물체를 막는(WorldDynamic 채널을 Block하는) 대상만 찾는다
	FCollisionResponseParams	ResponseParams(ECR_Ignore);
	ResponseParams.CollisionResponse.SetResponse(ECC_WorldStatic, ECR_Block);
	ResponseParams.CollisionResponse.SetResponse(ECC_WorldDynamic, ECR_Block);

	// 캐릭터에 붙은 컴포넌트가 걸리면 캐릭터 머리 위가 착지 지점이 되므로, 그 캐릭터를 무시하고 다시 찾는다
	constexpr int32	MaxRetry = 8;
	FHitResult	ObjectHit;
	bool		bIsObjectHit = false;
	for (int32 Retry = 0; Retry < MaxRetry; ++Retry)
	{
		bIsObjectHit = World->SweepSingleByChannel(
			ObjectHit, RayOrigin, RayEnd, ObjectRot, ECC_WorldDynamic, ObjectShape, QueryParams, ResponseParams
		);
		APawn*	HitPawn = bIsObjectHit ? Cast<APawn>(ObjectHit.GetActor()) : nullptr;
		if (!HitPawn)
		{
			break ;
		}
		QueryParams.AddIgnoredActor(HitPawn);
		bIsObjectHit = false;
	}
	this->FallingLocation = (bIsObjectHit && !ObjectHit.bStartPenetrating) ? ObjectHit.Location : RayEnd;
}

void AFallingObject::ObjectDrop()
{
	if (this->bIsFalling || this->bIsLanded)
	{
		return ;
	}
	this->FindFallingLocation();
	this->FallingIndicator->SetVisibility(false);
	this->bIsFalling = true;
	this->FallSpeed = this->InitFallSpeed;
	this->HitActors.Reset();
	//SetActorTickEnabled(true);
	// 매달려 있을 때는 Pawn을 Ignore하므로, 이미 겹쳐 있던 대상도 첫 이동 시 BeginOverlap이 발생한다
	FallingBody->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
}

void AFallingObject::ObjectLand()
{
	if (!this->bIsFalling)
	{
		return ;
	}
	this->bIsFalling = false;
	this->bIsLanded = true;
	this->FallSpeed = 0.f;
	//SetActorTickEnabled(false);
	this->ClearIgnoredPawns();
	this->TryBlockPawn();
}

bool AFallingObject::IgnorePawnWhileFalling(AActor* InActor)
{
	APawn*	HitPawn = Cast<APawn>(InActor);
	if (!HitPawn)
	{
		return false;
	}
	this->FallingBody->IgnoreActorWhenMoving(HitPawn, true);
	this->IgnoredPawns.Add(HitPawn);
	return true;
}

void AFallingObject::ClearIgnoredPawns()
{
	for (const TWeakObjectPtr<APawn>& IgnoredPawn : this->IgnoredPawns)
	{
		if (IgnoredPawn.IsValid())
		{
			this->FallingBody->IgnoreActorWhenMoving(IgnoredPawn.Get(), false);
		}
	}
	this->IgnoredPawns.Reset();
}

void AFallingObject::TryBlockPawn()
{
	// 살아남은 적이나 분신과 겹친 채 Block으로 바꾸면 밀려나며 바닥을 뚫으므로, 모두 빠져나간 뒤 전환한다
	// (죽은 적은 충돌이 꺼져 있어 여기에 잡히지 않는다)
	TArray<AActor*>	OverlappingPawns;
	this->FallingBody->GetOverlappingActors(OverlappingPawns, APawn::StaticClass());
	if (OverlappingPawns.Num() > 0)
	{
		this->bIsPendingBlock = true;
		return ;
	}
	this->bIsPendingBlock = false;
	this->FallingBody->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
}

