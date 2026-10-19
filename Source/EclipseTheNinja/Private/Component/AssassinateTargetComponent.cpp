


#include "Component/AssassinateTargetComponent.h"
#include "Components/BoxComponent.h"
#include "Character/PlayerCharacter.h"
#include "Character/Enemy.h"
#include "Component/EnemyDetectionComponent.h"
#include "Component/AssassinationTargetingComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

// Sets default values for this component's properties
UAssassinateTargetComponent::UAssassinateTargetComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;

	// ...
	AssassinateBoundary = CreateDefaultSubobject<UBoxComponent>(TEXT("AssassinateBoundary"));
	AssassinateBoundary->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	AssassinateBoundary->SetCollisionResponseToAllChannels(ECR_Ignore);
	AssassinateBoundary->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	AssassinateBoundary->SetGenerateOverlapEvents(true);
}


//TODO 암살가능 여부 확인 함수
bool UAssassinateTargetComponent::CanAssassinate()
{
	AEnemy* Enemy = Cast<AEnemy>(GetOwner());
	if (!IsValid(Enemy)) return false;
	if (Enemy->IsDead()) return false;
	if (bAssassinationInProgress) return false;
	UEnemyDetectionComponent* EDC = Enemy->GetEneymDetectionComponent();
	if (!EDC || EDC->GetCurrentAlertLevel() == EEnemyAlertLevel::Combat) return false;

	return true;
}

void UAssassinateTargetComponent::SetTargetHighlighted(bool bInValue)
{
	if (!IsValid(OutlineMesh)) return;

	OutlineMesh->SetVisibility(bInValue);
}


// Called when the game starts
void UAssassinateTargetComponent::BeginPlay()
{
	Super::BeginPlay();
	InitializeOutlineMesh();

	// ...

	AssassinateBoundary->OnComponentBeginOverlap.AddDynamic(
		this, &UAssassinateTargetComponent::OnAssassinationBeginOverlapped);

	AssassinateBoundary->OnComponentEndOverlap.AddDynamic(
		this, &UAssassinateTargetComponent::OnAssassinationBoxEndOverlapped);

	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor)) return;

	USceneComponent* OwnerRoot = OwnerActor->GetRootComponent();
	if (!IsValid(OwnerRoot)) return;

	// ActorComponent 내부에서 만든 박스는 직접 부착하고 월드에 등록한다.
	AssassinateBoundary->AttachToComponent(OwnerRoot, FAttachmentTransformRules::KeepRelativeTransform);
	AssassinateBoundary->SetRelativeLocation(BoundaryOffset);
	AssassinateBoundary->SetBoxExtent(BoundaryExtent);
	if (!AssassinateBoundary->IsRegistered())
	{
		AssassinateBoundary->RegisterComponent();
	}

	// 이미 영역 안에서 시작한 플레이어도 후보로 등록한다.
	AssassinateBoundary->UpdateOverlaps();
	TArray<UPrimitiveComponent*> OverlappingComponents;
	AssassinateBoundary->GetOverlappingComponents(OverlappingComponents);
	for (UPrimitiveComponent* OverlappingComponent : OverlappingComponents)
	{
		APlayerCharacter* Player = Cast<APlayerCharacter>(OverlappingComponent->GetOwner());
		if (!IsValid(Player)) continue;
		if (OverlappingComponent != Player->GetCapsuleComponent()) continue;

		AddTarget(Player->GetAssassinationTargetingComponent());
	}
}


void UAssassinateTargetComponent::InitializeOutlineMesh()
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!IsValid(Character)) return;

	USkeletalMeshComponent* SourceMesh = Character->GetMesh();
	if (!IsValid(SourceMesh)) return;
	if (!IsValid(SourceMesh->GetSkeletalMeshAsset())) return;

	UMaterialInterface* Material = OutlineMaterial.LoadSynchronous();
	if (!IsValid(Material))
	{
		UE_LOG(LogTemp, Warning, TEXT("Assassination outline material missing: %s"), *GetNameSafe(Character));
		return;
	}

	OutlineMaterialInstance = UMaterialInstanceDynamic::Create(Material, this);
	if (!IsValid(OutlineMaterialInstance)) return;

	OutlineMaterialInstance->SetScalarParameterValue(TEXT("OutlineThickness"), OutlineThickness);
	OutlineMaterialInstance->SetVectorParameterValue(TEXT("OutlineColor"), OutlineColor);

	// 원본의 포즈를 공유하므로 별도 AnimBP와 포즈 갱신이 필요 없다.
	OutlineMesh = NewObject<USkeletalMeshComponent>(Character, TEXT("AssassinationOutline"));
	OutlineMesh->SetupAttachment(SourceMesh);
	OutlineMesh->SetSkeletalMeshAsset(SourceMesh->GetSkeletalMeshAsset());
	OutlineMesh->SetLeaderPoseComponent(SourceMesh);
	OutlineMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	OutlineMesh->SetGenerateOverlapEvents(false);
	OutlineMesh->SetCastShadow(false);
	OutlineMesh->SetCanEverAffectNavigation(false);
	OutlineMesh->SetVisibility(false);
	OutlineMesh->SetBoundsScale(1.2f);

	for (int32 MaterialIndex = 0; MaterialIndex < SourceMesh->GetNumMaterials(); ++MaterialIndex)
	{
		OutlineMesh->SetMaterial(MaterialIndex, OutlineMaterialInstance);
	}

	OutlineMesh->RegisterComponent();
}


void UAssassinateTargetComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	AssassinateBoundary->OnComponentBeginOverlap.RemoveDynamic(
		this, &UAssassinateTargetComponent::OnAssassinationBeginOverlapped);
	AssassinateBoundary->OnComponentEndOverlap.RemoveDynamic(
		this, &UAssassinateTargetComponent::OnAssassinationBoxEndOverlapped);
	AssassinateBoundary->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	AssassinateBoundary->UnregisterComponent();

	SetTargetHighlighted(false);
	if (CurrentTargetedComp.IsValid())
	{
		RemoveTarget(CurrentTargetedComp.Get());
	}

	if (IsValid(OutlineMesh))
	{
		OutlineMesh->DestroyComponent();
	}

	Super::EndPlay(EndPlayReason);
}


// Called every frame
void UAssassinateTargetComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

void UAssassinateTargetComponent::OnAssassinationBeginOverlapped(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(OtherActor))
	{
		if (OtherComp != PlayerCharacter->GetCapsuleComponent()) return;

		// 0. 플레이어 캐릭터의 AssassinateTargetingComponent를 가져온다
		UAssassinationTargetingComponent* ATC = PlayerCharacter->GetAssassinationTargetingComponent();
		if (!ATC) return;

		if (AEnemy* Enemy = Cast<AEnemy>(GetOwner()))
		{
			AddTarget(ATC);
		}

	}
}

void UAssassinateTargetComponent::OnAssassinationBoxEndOverlapped(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(OtherActor))
	{
		if (OtherComp != PlayerCharacter->GetCapsuleComponent()) return;

		// 0. 플레이어 캐릭터의 AssassinateTargetingComponent를 가져온다
		UAssassinationTargetingComponent* ATC = PlayerCharacter->GetAssassinationTargetingComponent();
		if (!ATC) return;

		// 1. 해당 컴포넌트에 이 캐릭터를 등록 해제한다.
		RemoveTarget(ATC);
	}
}

void UAssassinateTargetComponent::AddTarget(UAssassinationTargetingComponent* ATC)
{
	if (!ATC) return;

	ATC->AddTargetCandidate(this);
	CurrentTargetedComp = ATC;
}

void UAssassinateTargetComponent::RemoveTarget(UAssassinationTargetingComponent* ATC)
{
	if (!ATC) return;
	if (!CurrentTargetedComp.IsValid() || !CurrentTargetedComp.Get() || ATC != CurrentTargetedComp) return;

	ATC->RemoveTargetCandidate(this);
	CurrentTargetedComp = nullptr;
}
