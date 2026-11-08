// Fill out your copyright notice in the Description page of Project Settings.

#include "Level/TargetCutawayVolume.h"
#include "Character/PlayerCharacterCameraManager.h"

#include "Components/BoxComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"

ATargetCutawayVolume::ATargetCutawayVolume()
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
}

void ATargetCutawayVolume::BeginPlay()
{
	Super::BeginPlay();

	// 벽 붙기 트레이스(WallAttach)가 트리거 상자에 막히지 않게 한다. Trigger 프로필에는 WallAttach 채널이 없어 기본값 Block이 적용된다.
	this->InteriorArea->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Ignore);
	this->InteriorArea->OnComponentBeginOverlap.AddDynamic(this, &ATargetCutawayVolume::OnInteriorBeginOverlap);
	this->InteriorArea->OnComponentEndOverlap.AddDynamic(this, &ATargetCutawayVolume::OnInteriorEndOverlap);
}

void ATargetCutawayVolume::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CloseCutaway();
	Super::EndPlay(EndPlayReason);
}

bool ATargetCutawayVolume::IsPlayerPawn(const AActor* InActor)
{
	// 분신 등 AI 폰은 단면을 열지 않는다
	const APawn* Pawn = Cast<APawn>(InActor);
	return IsValid(Pawn) && Pawn->IsPlayerControlled();
}

void ATargetCutawayVolume::OnInteriorBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (IsPlayerPawn(OtherActor))
	{
		OpenCutaway();
	}
}

void ATargetCutawayVolume::OnInteriorEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	// 플레이어의 다른 컴포넌트가 아직 겹쳐 있으면 닫지 않는다
	if (IsPlayerPawn(OtherActor) && !this->InteriorArea->IsOverlappingActor(OtherActor))
	{
		CloseCutaway();
	}
}

void ATargetCutawayVolume::OpenCutaway()
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

	for (AActor* Target : this->TargetActors)
	{
		if (!IsValid(Target))
		{
			continue;
		}
		// 퍼즐 오브젝트는 메시를 MeshAssembly 자식 액터에 두므로 자식 액터의 메시도 함께 찾는다
		TInlineComponentArray<UStaticMeshComponent*> Meshes(Target, true);
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

void ATargetCutawayVolume::CloseCutaway()
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
