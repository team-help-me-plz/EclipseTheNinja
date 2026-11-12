// Fill out your copyright notice in the Description page of Project Settings.

#include "Level/TargetCutawayVolume.h"
#include "Character/PlayerCharacterCameraManager.h"
#include "Interface/Interactor.h"
#include "Minimap/MinimapSubsystem.h"
#include "Save/GameSaveSubsystem.h"
#include "Util/EclipseCollisionChannels.h"

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

	// 벽 붙기 트레이스(WallAttach)가 트리거 상자에 막히지 않게 하고, 몬스터가 영역 안에 갇히지 않게 한다.
	// Trigger 프로필에는 두 채널이 없어 기본값 Block이 적용된다.
	// 레벨에서 상자를 더 붙인 경우(디테일 패널 복제 등)에도 막지 않도록 이 액터의 모든 충돌 컴포넌트에 적용한다.
	TInlineComponentArray<UPrimitiveComponent*> Primitives(this);
	for (UPrimitiveComponent* Primitive : Primitives)
	{
		Primitive->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Ignore);
		Primitive->SetCollisionResponseToChannel(ECC_Monster, ECR_Ignore);
	}

	// 미니맵: 들어가 본 적이 없으면 InteriorArea가 시야를 막는 마스크가 된다. 세이브에 남은 발견 여부를 먼저 읽는다
	if (const UGameSaveSubsystem* Save = UGameSaveSubsystem::Get(this))
	{
		FSavedActorState SavedState;
		if (Save->TakeActorState(this, SavedState))
		{
			this->bMinimapDiscovered = SavedState.bIsUsed;
		}
	}
	if (UMinimapSubsystem* Minimap = UMinimapSubsystem::Get(this))
	{
		Minimap->RegisterMaskRegion(this, this->bMinimapDiscovered);
	}

	this->InteriorArea->OnComponentBeginOverlap.AddDynamic(this, &ATargetCutawayVolume::OnInteriorBeginOverlap);
	this->InteriorArea->OnComponentEndOverlap.AddDynamic(this, &ATargetCutawayVolume::OnInteriorEndOverlap);
}

void ATargetCutawayVolume::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CloseCutaway();
	if (UMinimapSubsystem* Minimap = UMinimapSubsystem::Get(this))
	{
		Minimap->UnregisterMaskRegion(this);
	}
	Super::EndPlay(EndPlayReason);
}

bool ATargetCutawayVolume::IsCutawayViewer(const AActor* InActor)
{
	// 플레이어와 분신(IInteractor)이 단면을 연다. 분신으로 건물 안을 미리 볼 수 있다.
	// 빙의 여부(IsPlayerControlled)로 판정하면 리스폰 스폰 직후(빙의 전)의 첫 겹침과
	// 사망 시(빙의 해제 후)의 겹침 종료를 놓치므로 인터페이스로 판정한다
	return IsValid(InActor) && !InActor->IsActorBeingDestroyed() && InActor->Implements<UInteractor>();
}

bool ATargetCutawayVolume::HasViewerInside() const
{
	TArray<AActor*> OverlappingActors;
	this->InteriorArea->GetOverlappingActors(OverlappingActors);
	for (const AActor* Actor : OverlappingActors)
	{
		if (IsCutawayViewer(Actor))
		{
			return true;
		}
	}
	return false;
}

void ATargetCutawayVolume::OnInteriorBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (IsCutawayViewer(OtherActor))
	{
		OpenCutaway();
	}
}

void ATargetCutawayVolume::OnInteriorEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	// 파괴 중인 액터도 나가는 것으로 처리해야 하므로 여기서는 인터페이스만 확인한다.
	// 플레이어나 분신 중 하나라도(다른 컴포넌트 포함) 아직 안에 있으면 닫지 않는다
	if (IsValid(OtherActor) && OtherActor->Implements<UInteractor>() && !HasViewerInside())
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

	// 처음 들어왔을 때만 미니맵에 알리고 세이브에 남긴다. 이후로 이 영역은 일반 시야 규칙만 따른다
	if (!this->bMinimapDiscovered)
	{
		this->bMinimapDiscovered = true;
		if (UMinimapSubsystem* Minimap = UMinimapSubsystem::Get(this))
		{
			Minimap->DiscoverMaskRegion(this);
		}
		if (UGameSaveSubsystem* Save = UGameSaveSubsystem::Get(this))
		{
			FSavedActorState State;
			State.bIsUsed = true;
			Save->RecordActorState(this, State);
		}
	}

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
