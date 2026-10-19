


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

	// 암살 가능한 바운더리
	AssassinateBoundary = CreateDefaultSubobject<UBoxComponent>(TEXT("AssassinateBoundary"));
	// 초기값만 지정하고, 이후에는 BP에서 직접 편집한 박스 값을 사용한다.
	// 등 뒤의 아래쪽 바닥(아래 암살)부터 머리 위 발판(공중 암살)까지 덮는다.
	AssassinateBoundary->SetRelativeLocation(FVector(-100.0f, 0.0f, 100.0f));
	AssassinateBoundary->SetBoxExtent(FVector(90.0f, 60.0f, 390.0f));
	AssassinateBoundary->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	AssassinateBoundary->SetCollisionResponseToAllChannels(ECR_Ignore);
	AssassinateBoundary->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	AssassinateBoundary->SetGenerateOverlapEvents(true);
}

void UAssassinateTargetComponent::OnRegister()
{
	Super::OnRegister();
	RegisterBoundary();
}

void UAssassinateTargetComponent::RegisterBoundary()
{
	if (IsTemplate()) return;
	if (!IsValid(AssassinateBoundary)) return;

	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor)) return;

	USceneComponent* OwnerRoot = OwnerActor->GetRootComponent();
	if (!IsValid(OwnerRoot)) return;

	// 상대 위치·회전·스케일과 크기를 덮어쓰지 않는다.
	if (!AssassinateBoundary->GetAttachParent())
	{
		AssassinateBoundary->AttachToComponent(OwnerRoot, FAttachmentTransformRules::KeepRelativeTransform);
	}

	AssassinateBoundary->SetHiddenInGame(false);
	AssassinateBoundary->SetVisibility(true);

	if (!AssassinateBoundary->IsRegistered())
	{
		AssassinateBoundary->RegisterComponent();
	}
}


//암살가능 여부 확인 함수
bool UAssassinateTargetComponent::CanAssassinate()
{
	AEnemy* Enemy = Cast<AEnemy>(GetOwner());
	if (!IsValid(Enemy)) return false;
	if (Enemy->IsDead()) return false;
	// 이 컴포넌트를 대상으로 이미 암살이 진행되고 있으면 암살 불가
	if (bAssassinationInProgress) return false;

	// 이 컴포넌트를 소유한 적의 경계상태가 전투 상태이면 암살 불가
	UEnemyDetectionComponent* EDC = Enemy->GetEneymDetectionComponent();
	if (!EDC || EDC->GetCurrentAlertLevel() == EEnemyAlertLevel::Combat) return false;

	return true;
}

void UAssassinateTargetComponent::SetTargetHighlighted(bool bInValue)
{
	if (!IsValid(OutlineMesh)) return;

	OutlineMesh->SetVisibility(bInValue);
}

FVector UAssassinateTargetComponent::GetApproachLocation(const ACharacter* Assassin) const
{
	// 암살대상이 유효한지 확인
	const ACharacter* Victim = Cast<ACharacter>(GetOwner());
	if (!IsValid(Victim) || !IsValid(Assassin)) return FVector::ZeroVector;

	// 암살자의 캡슐 반지름과 암살대상의 캡슐 반지름, 그리고 여유 공간을 더해서 캡슐이 겹치지 않는 거리를 만든다
	const float CapsuleSeparation = Assassin->GetCapsuleComponent()->GetScaledCapsuleRadius()
		+ Victim->GetCapsuleComponent()->GetScaledCapsuleRadius() + 5.0f;

	// 암살 몽타주 재생을 시작할 위치 까지의 거리
	const float DistanceBehind = FMath::Max(BehindDistance, CapsuleSeparation);

	// 암살 대상에서 위에서 구한 거리 만큼 뒤쪽에서 암살 애니메이션 재생
	FVector ApproachLocation = Victim->GetActorLocation() - Victim->GetActorForwardVector() * DistanceBehind;
	// 횡스크롤이기 때문에 Y,Z값은 암살자와 일치시키기
	ApproachLocation.Y = Assassin->GetActorLocation().Y;
	ApproachLocation.Z = Assassin->GetActorLocation().Z;
	return ApproachLocation;
}

bool UAssassinateTargetComponent::IsApproachClear(const ACharacter* Assassin) const
{
	const AActor* OwnerActor = GetOwner();
	if (!IsValid(Assassin) || !IsValid(OwnerActor)) return false;

	// 평지 암살이 가능한지 확인한다.
	if (FMath::Abs(Assassin->GetActorLocation().Z - OwnerActor->GetActorLocation().Z) > GroundHeightTolerance) return false;

	return IsPathClear(Assassin, Assassin->GetActorLocation(), GetApproachLocation(Assassin));
}

FVector UAssassinateTargetComponent::GetAerialLandingLocation(const ACharacter* Assassin) const
{
	const ACharacter* Victim = Cast<ACharacter>(GetOwner());
	if (!IsValid(Victim) || !IsValid(Assassin)) return FVector::ZeroVector;

	// 암살 몽타주가 끝났을 때 암살자가 서 있을 위치 암살대상의 뒤쪽 (AerialLandingOffset.x)가 음수
	FVector Landing = Victim->GetActorLocation() + Victim->GetActorQuat().RotateVector(AerialLandingOffset);
	// 사이드뷰의 이동 평면을 유지
	Landing.Y = Assassin->GetActorLocation().Y;
	// 두 캐릭터의 캡슐 바닥 높이를 맞춤 
	Landing.Z = Victim->GetActorLocation().Z - Victim->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()
		+ Assassin->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + AerialLandingOffset.Z;
	return Landing;
}

bool UAssassinateTargetComponent::IsAerialApproachClear(const ACharacter* Assassin) const
{
	const AActor* OwnerActor = GetOwner();
	if (!IsValid(Assassin) || !IsValid(OwnerActor)) return false;
	// 높이차가 공중 암살 기준보다 작으면 암살 진행 불가
	if (Assassin->GetActorLocation().Z - OwnerActor->GetActorLocation().Z < AerialMinHeight) return false;

	// 발판 위에서 대각선으로 확인하면 발판 모서리에 걸리므로, 같은 높이로 착지 위치 위까지 나간 뒤 수직으로 내려가는 경로를 확인한다.
	// 적이 발판 바로 아래에 있으면 수직 경로가 발판에 막혀 탈락한다.
	const FVector Start = Assassin->GetActorLocation();
	const FVector Landing = GetAerialLandingLocation(Assassin);
	const FVector AboveLanding(Landing.X, Landing.Y, Start.Z);

	// 간단히 말하면 착지지점 까지를 수직/수평 성분으로 나눠서 각각 충돌이 없는지 검사
	return IsPathClear(Assassin, Start, AboveLanding) && IsPathClear(Assassin, AboveLanding, Landing);
}

bool UAssassinateTargetComponent::FindBelowClimbLocation(const ACharacter* Assassin, FVector& OutClimbLocation,
	FVector& OutWallLocation) const
{
	const ACharacter* Victim = Cast<ACharacter>(GetOwner());
	if (!IsValid(Victim) || !IsValid(Assassin) || !GetWorld()) return false;

	const FVector VictimLocation = Victim->GetActorLocation();
	const FVector AssassinLocation = Assassin->GetActorLocation();
	const float Height = VictimLocation.Z - AssassinLocation.Z;
	if (Height < BelowMinHeight || Height > BelowMaxHeight) return false;

	const UCapsuleComponent* Capsule = Assassin->GetCapsuleComponent();
	const float VictimHalfHeight = Victim->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const float FloorZ = VictimLocation.Z - VictimHalfHeight;
	const FVector Back = -Victim->GetActorForwardVector().GetSafeNormal2D();

	FCollisionQueryParams Params(SCENE_QUERY_STAT(AssassinationLedge), false, Assassin);
	Params.AddIgnoredActor(Victim);
	FCollisionResponseParams ResponseParams;
	ResponseParams.CollisionResponse = Capsule->GetCollisionResponseToChannels();

	// 적 발밑에서 뒤쪽으로 가며 바닥이 끊기는 곳(발판 모서리)을 찾는다.
	constexpr float SearchStep = 10.0f;
	float EdgeDistance = -1.0f;
	for (float Distance = SearchStep; Distance <= BelowEdgeSearchDistance; Distance += SearchStep)
	{
		const FVector Probe = VictimLocation + Back * Distance;
		const FVector TraceStart(Probe.X, Probe.Y, FloorZ + 10.0f);
		const FVector TraceEnd(Probe.X, Probe.Y, FloorZ - 20.0f);
		FHitResult Hit;
		if (!GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, Capsule->GetCollisionObjectType(), Params, ResponseParams))
		{
			EdgeDistance = Distance - SearchStep;
			break;
		}
	}
	if (EdgeDistance <= 0.0f) return false;

	// 암살자는 모서리 바깥 아래에 있어야 한다. 발판 밑에 들어가 있으면 올라갈 수 없다.
	const FVector ToAssassin = AssassinLocation - VictimLocation;
	const float AssassinDistance = FVector::DotProduct(FVector(ToAssassin.X, ToAssassin.Y, 0.0f), Back);
	if (AssassinDistance < EdgeDistance) return false;

	// 암살자 높이에서 적 쪽으로 캡슐을 밀어 발판 옆면(벽)을 찾는다. 몽타주는 벽 바로 앞에서 시작해야 한다.
	const FCollisionShape Shape = FCollisionShape::MakeCapsule(
		FMath::Max(1.0f, Capsule->GetScaledCapsuleRadius() - 1.0f),
		FMath::Max(1.0f, Capsule->GetScaledCapsuleHalfHeight() - 5.0f));
	const FVector WallSearchEnd = AssassinLocation - Back * (AssassinDistance - EdgeDistance + 30.0f);
	FHitResult WallHit;
	if (!GetWorld()->SweepSingleByChannel(WallHit, AssassinLocation, WallSearchEnd, Capsule->GetComponentQuat(),
		Capsule->GetCollisionObjectType(), Shape, Params, ResponseParams))
	{
		return false;
	}
	if (WallHit.bStartPenetrating) return false;

	// 벽에 바짝 붙어 있어야 한다. 매달리기가 생기면 이 조건을 모서리에 매달린 상태로 바꾼다.
	FVector WallLocation = WallHit.Location + Back * 1.0f;
	WallLocation.Z = AssassinLocation.Z;
	if (FVector::Dist2D(AssassinLocation, WallLocation) > BelowWallTolerance) return false;

	// 모서리 안쪽에 올라선 위치. 적을 잡을 거리보다는 뒤에 있어야 한다.
	const float ClimbDistance = EdgeDistance - BelowClimbInset;
	if (ClimbDistance < BelowGrabDistance) return false;

	FVector ClimbLocation = VictimLocation + Back * ClimbDistance;
	ClimbLocation.Y = AssassinLocation.Y;
	ClimbLocation.Z = FloorZ + Capsule->GetScaledCapsuleHalfHeight();

	// 벽 앞에서 벽을 따라 올라간 뒤 모서리 안쪽으로 넘어가는 경로, 그리고 잡는 위치까지 걸어가는 경로가 막히지 않아야 한다.
	// 벽까지 걸어가는 경로는 위의 벽 찾기 스윕이 확인했다.
	const FVector AboveWall(WallLocation.X, WallLocation.Y, ClimbLocation.Z + 5.0f);
	if (!IsPathClear(Assassin, WallLocation, AboveWall)) return false;
	if (!IsPathClear(Assassin, AboveWall, ClimbLocation)) return false;
	if (!IsPathClear(Assassin, ClimbLocation, GetBelowGrabLocation(Assassin))) return false;

	OutClimbLocation = ClimbLocation;
	OutWallLocation = WallLocation;
	return true;
}

FVector UAssassinateTargetComponent::GetBelowGrabLocation(const ACharacter* Assassin) const
{
	const ACharacter* Victim = Cast<ACharacter>(GetOwner());
	if (!IsValid(Victim) || !IsValid(Assassin)) return FVector::ZeroVector;

	FVector GrabLocation = Victim->GetActorLocation() - Victim->GetActorForwardVector().GetSafeNormal2D() * BelowGrabDistance;
	GrabLocation.Y = Assassin->GetActorLocation().Y;
	GrabLocation.Z = Victim->GetActorLocation().Z - Victim->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()
		+ Assassin->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	return GrabLocation;
}

bool UAssassinateTargetComponent::IsPathClear(const ACharacter* Assassin, const FVector& Start, const FVector& End) const
{
	//Start 부터 End까지 암살자가 실제로 도달할 수 있는지를 반환한다
	//Start 부터 End까지 캡슐을 스윕해서 블록되는게 있는지 확인한다.

	if (!IsValid(Assassin) || !GetWorld()) return false;

	const UCapsuleComponent* Capsule = Assassin->GetCapsuleComponent();
	if (!IsValid(Capsule)) return false;

	// 실제 접근과 같은 캡슐·충돌 설정으로 스윕한다. 바닥에 닿은 부분이 걸리지 않도록 조금 줄인다.
	const FCollisionShape Shape = FCollisionShape::MakeCapsule(
		FMath::Max(1.0f, Capsule->GetScaledCapsuleRadius() - 1.0f),
		FMath::Max(1.0f, Capsule->GetScaledCapsuleHalfHeight() - 5.0f));

	FCollisionQueryParams Params(SCENE_QUERY_STAT(AssassinationApproach), false, Assassin);
	Params.AddIgnoredActor(GetOwner());

	FCollisionResponseParams ResponseParams;
	ResponseParams.CollisionResponse = Capsule->GetCollisionResponseToChannels();

	return !GetWorld()->SweepTestByChannel(Start, End, Capsule->GetComponentQuat(),
		Capsule->GetCollisionObjectType(), Shape, Params, ResponseParams);
}


// Called when the game starts
void UAssassinateTargetComponent::BeginPlay()
{
	Super::BeginPlay();
	InitializeOutlineMesh();

	// ...

	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor)) return;

	USceneComponent* OwnerRoot = OwnerActor->GetRootComponent();
	if (!IsValid(OwnerRoot)) return;

	AssassinateBoundary->OnComponentBeginOverlap.AddDynamic(
		this, &UAssassinateTargetComponent::OnAssassinationBeginOverlapped);
	AssassinateBoundary->OnComponentEndOverlap.AddDynamic(
		this, &UAssassinateTargetComponent::OnAssassinationBoxEndOverlapped);

	RegisterBoundary();

	// 이미 영역 안에서 시작한 플레이어도 후보로 등록한다.
	AssassinateBoundary->UpdateOverlaps();
	TArray<UPrimitiveComponent*> OverlappingComponents;
	AssassinateBoundary->GetOverlappingComponents(OverlappingComponents);

	for (UPrimitiveComponent* OverlappingComponent : OverlappingComponents)
	{
		APlayerCharacter* Player = Cast<APlayerCharacter>(OverlappingComponent->GetOwner());
		if (!IsValid(Player)) continue;
		if (OverlappingComponent != Player->GetCapsuleComponent()) continue;

		UpdateZoneState(Player, true);
	}
}

void UAssassinateTargetComponent::UpdateZoneState(APlayerCharacter* Player, bool bInside)
{
	if (!IsValid(Player)) return;

	bPlayerInZone = bInside;

	UAssassinationTargetingComponent* ATC = Player->GetAssassinationTargetingComponent();
	if (!ATC) return;

	if (bInside)
	{
		AddTarget(ATC);
	}
	else
	{
		RemoveTarget(ATC);
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
	//시작할 때 바인딩 했던 함수 해제
	AssassinateBoundary->OnComponentBeginOverlap.RemoveDynamic(
		this, &UAssassinateTargetComponent::OnAssassinationBeginOverlapped);
	AssassinateBoundary->OnComponentEndOverlap.RemoveDynamic(
		this, &UAssassinateTargetComponent::OnAssassinationBoxEndOverlapped);

	// 암살 가능 바운더리 삭제
	AssassinateBoundary->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	AssassinateBoundary->UnregisterComponent();

	// 플레이어 표시 변수 초기화 및 테두리 지우기
	bPlayerInZone = false;
	SetTargetHighlighted(false);

	// CurrentTarget에서 나를 지우기
	if (CurrentTargetedComp.IsValid())
	{
		RemoveTarget(CurrentTargetedComp.Get());
	}

	// 아웃라인 메시 없애기
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

		// 0. 플레이어 캐릭터의 후보 목록에 이 적을 등록한다.
		UpdateZoneState(PlayerCharacter, true);
	}
}

void UAssassinateTargetComponent::OnAssassinationBoxEndOverlapped(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(OtherActor))
	{
		if (OtherComp != PlayerCharacter->GetCapsuleComponent()) return;

		// 0. 해당 컴포넌트에 이 캐릭터를 등록 해제한다.
		UpdateZoneState(PlayerCharacter, false);
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
