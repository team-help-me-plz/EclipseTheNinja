// Fill out your copyright notice in the Description page of Project Settings.

#include "Level/HouseCutawayVolume.h"
#include "Character/PlayerCharacterCameraManager.h"

#include "Components/BoxComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"
#include "Util/EclipseCollisionChannels.h"

AHouseCutawayVolume::AHouseCutawayVolume()
{
	PrimaryActorTick.bCanEverTick = false;

	this->SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(this->SceneRoot);

	this->InteriorArea = CreateDefaultSubobject<UBoxComponent>(TEXT("InteriorArea"));
	this->InteriorArea->SetupAttachment(this->SceneRoot);
	this->InteriorArea->SetBoxExtent(FVector(500.0f, 300.0f, 150.0f));
	this->InteriorArea->SetRelativeLocation(FVector(0.0f, 0.0f, 150.0f));
	this->InteriorArea->SetCollisionProfileName(TEXT("Trigger"));
	this->InteriorArea->ShapeColor = FColor::Cyan;

	// 숨길 벽을 고르는 범위일 뿐이라 충돌은 쓰지 않는다
	this->CutArea = CreateDefaultSubobject<UBoxComponent>(TEXT("CutArea"));
	this->CutArea->SetupAttachment(this->SceneRoot);
	this->CutArea->SetBoxExtent(FVector(500.0f, 30.0f, 200.0f));
	this->CutArea->SetRelativeLocation(FVector(0.0f, 600.0f, 200.0f));
	this->CutArea->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	this->CutArea->SetGenerateOverlapEvents(false);
	this->CutArea->ShapeColor = FColor::Orange;
}

void AHouseCutawayVolume::BeginPlay()
{
	Super::BeginPlay();

	// 벽 붙기 트레이스(WallAttach)가 트리거 상자에 막히지 않게 한다. Trigger 프로필에는 WallAttach 채널이 없어 기본값 Block이 적용된다.
	this->InteriorArea->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Ignore);
	// 몬스터가 영역 안에 갇히지 않게 한다. Trigger 프로필에는 Monster 채널이 없어 기본값 Block이 적용된다.
	this->InteriorArea->SetCollisionResponseToChannel(ECC_Monster, ECR_Ignore);
	this->InteriorArea->OnComponentBeginOverlap.AddDynamic(this, &AHouseCutawayVolume::OnInteriorBeginOverlap);
	this->InteriorArea->OnComponentEndOverlap.AddDynamic(this, &AHouseCutawayVolume::OnInteriorEndOverlap);
}

void AHouseCutawayVolume::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CloseCutaway();
	Super::EndPlay(EndPlayReason);
}

bool AHouseCutawayVolume::IsPlayerPawn(const AActor* InActor)
{
	// 분신 등 AI 폰은 단면을 열지 않는다
	const APawn* Pawn = Cast<APawn>(InActor);
	return IsValid(Pawn) && Pawn->IsPlayerControlled();
}

void AHouseCutawayVolume::OnInteriorBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (IsPlayerPawn(OtherActor))
	{
		OpenCutaway();
	}
}

void AHouseCutawayVolume::OnInteriorEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	// 플레이어의 다른 컴포넌트가 아직 겹쳐 있으면 닫지 않는다
	if (IsPlayerPawn(OtherActor) && !this->InteriorArea->IsOverlappingActor(OtherActor))
	{
		CloseCutaway();
	}
}

void AHouseCutawayVolume::OpenCutaway()
{
	if (this->bIsOpen || !GetWorld())
	{
		return;
	}
	this->bIsOpen = true;

	if (this->bAllowOccluderHiding && !this->bEnteredOccluderZone)
	{
		if (APlayerCharacterCameraManager* CameraManager = Cast<APlayerCharacterCameraManager>(UGameplayStatics::GetPlayerCameraManager(this, 0)))
		{
			CameraManager->EnterOccluderZone();
			this->bEnteredOccluderZone = true;
		}
	}

	// 레벨 인스턴스는 늦게 스트리밍될 수 있으므로 들어올 때마다 다시 찾는다
	const FTransform CutTransform = this->CutArea->GetComponentTransform();
	const FVector CutExtent = this->CutArea->GetUnscaledBoxExtent();
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (*It == this)
		{
			continue;
		}
		TInlineComponentArray<UStaticMeshComponent*> Meshes(*It);
		for (UStaticMeshComponent* Mesh : Meshes)
		{
			if (!IsValid(Mesh) || !IsValid(Mesh->GetStaticMesh()) || Mesh->bHiddenInGame || !Mesh->IsVisible())
			{
				continue;
			}
			if (!this->MeshNameFilter.IsEmpty() && !Mesh->GetStaticMesh()->GetName().Contains(this->MeshNameFilter))
			{
				continue;
			}
			const FVector LocalCenter = CutTransform.InverseTransformPosition(Mesh->Bounds.Origin);
			if (FMath::Abs(LocalCenter.X) > CutExtent.X || FMath::Abs(LocalCenter.Y) > CutExtent.Y || FMath::Abs(LocalCenter.Z) > CutExtent.Z)
			{
				continue;
			}

			FHiddenMesh& Hidden = this->HiddenMeshes.AddDefaulted_GetRef();
			Hidden.Mesh = Mesh;
			Hidden.bSavedCastHiddenShadow = Mesh->bCastHiddenShadow;
			if (this->bKeepShadows)
			{
				Mesh->SetCastHiddenShadow(true);
			}
			// 카메라 매니저의 가림 메시 숨김은 Visibility를 쓰므로 서로 겹치지 않게 HiddenInGame으로 숨긴다
			Mesh->SetHiddenInGame(true);
		}
	}
}

void AHouseCutawayVolume::CloseCutaway()
{
	for (const FHiddenMesh& Hidden : this->HiddenMeshes)
	{
		if (UStaticMeshComponent* Mesh = Hidden.Mesh.Get())
		{
			Mesh->SetHiddenInGame(false);
			Mesh->SetCastHiddenShadow(Hidden.bSavedCastHiddenShadow);
		}
	}
	this->HiddenMeshes.Reset();
	this->bIsOpen = false;

	if (this->bEnteredOccluderZone)
	{
		if (APlayerCharacterCameraManager* CameraManager = Cast<APlayerCharacterCameraManager>(UGameplayStatics::GetPlayerCameraManager(this, 0)))
		{
			CameraManager->ExitOccluderZone();
		}
		this->bEnteredOccluderZone = false;
	}
}
