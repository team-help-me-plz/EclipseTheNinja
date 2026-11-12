// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TargetCutawayVolume.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

/**
 * 지정한 액터를 숨기는 단면 볼륨. 플레이어나 분신(IInteractor)이 InteriorArea에 들어와 있는 동안 TargetActors를 숨긴다.
 *  - 숨기는 방식은 AHouseCutawayVolume과 같다 (HiddenInGame, 그림자 유지)
 *  - 대상 액터와 그 자식 액터(ChildActorComponent)의 정적 메시를 모두 숨긴다
 *  - MeshNameFilter를 채우면 메시 에셋 이름에 그 글자가 들어간 것만 숨긴다
 */
UCLASS()
class ECLIPSETHENINJA_API ATargetCutawayVolume : public AActor
{
	GENERATED_BODY()

public:
	ATargetCutawayVolume();

	UBoxComponent* GetInteriorArea() const { return InteriorArea; }
	const TArray<TObjectPtr<AActor>>& GetTargetActors() const { return TargetActors; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void OnInteriorBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnInteriorEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	// TargetActors의 메시를 숨긴다
	void OpenCutaway();

	// 숨긴 메시를 되돌린다
	void CloseCutaway();

	// 단면을 여는 대상인지 (플레이어·분신 등 IInteractor 구현 액터)
	static bool IsCutawayViewer(const AActor* InActor);

	// 단면을 여는 대상이 아직 InteriorArea 안에 있는지
	bool HasViewerInside() const;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> InteriorArea = nullptr;

	// 플레이어가 안에 있는 동안 숨길 액터
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Cutaway")
	TArray<TObjectPtr<AActor>> TargetActors;

	// 비워 두면 대상 액터의 정적 메시를 모두 숨긴다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cutaway")
	FString MeshNameFilter;

	// 숨긴 메시가 그림자를 계속 드리우게 할지 여부
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cutaway")
	bool bKeepShadows = true;

	// 켜면 안에 있는 동안 카메라 매니저의 가림 메시 숨기기를 허용한다 (bHideOccludersOnlyInZones인 카메라용)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cutaway")
	bool bAllowOccluderHiding = false;

private:
	// 카메라 매니저에 숨김 구역 진입을 알린 상태인지
	bool bEnteredOccluderZone = false;

	struct FHiddenMesh
	{
		TWeakObjectPtr<UStaticMeshComponent> Mesh;
		bool bSavedCastHiddenShadow = false;
	};

	TArray<FHiddenMesh> HiddenMeshes;

	bool bIsOpen = false;

	// 플레이어가 한 번이라도 들어왔는지. 들어오기 전까지 미니맵 시야가 InteriorArea를 넘지 못한다. 세이브에 bIsUsed로 남는다.
	bool bMinimapDiscovered = false;
};
