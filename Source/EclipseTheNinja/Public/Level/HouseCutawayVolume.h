// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HouseCutawayVolume.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

/**
 * 집 단면 볼륨. 플레이어가 집 안(InteriorArea)에 들어와 있는 동안 카메라 쪽 벽을 숨긴다.
 *  - InteriorArea : 플레이어가 겹쳐 있는 동안 단면을 연다
 *  - CutArea      : 바운즈 중심이 이 상자 안에 있는 정적 메시를 숨긴다. 레벨 인스턴스 안의 메시도 대상이다
 *  - MeshNameFilter를 채우면 메시 에셋 이름에 그 글자가 들어간 것(예: "Wall")만 숨겨, 기둥·지붕은 남긴다
 *  - 숨긴 벽도 그림자는 계속 드리워 실내 조명이 바뀌지 않는다
 */
UCLASS()
class ECLIPSETHENINJA_API AHouseCutawayVolume : public AActor
{
	GENERATED_BODY()

public:
	AHouseCutawayVolume();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void OnInteriorBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnInteriorEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	// CutArea 안의 벽을 숨긴다
	void OpenCutaway();

	// 숨긴 벽을 되돌린다
	void CloseCutaway();

	static bool IsPlayerPawn(const AActor* InActor);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> InteriorArea = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> CutArea = nullptr;

	// 비워 두면 CutArea 안의 정적 메시를 모두 숨긴다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cutaway")
	FString MeshNameFilter = TEXT("Wall");

	// 숨긴 벽이 그림자를 계속 드리우게 할지 여부
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cutaway")
	bool bKeepShadows = true;

	// 켜면 안에 있는 동안 카메라 매니저의 가림 메시 숨기기를 허용한다 (bHideOccludersOnlyInZones인 카메라용).
	// 벽 이름 필터와 상관없이 카메라를 가리는 것을 모두 숨기므로, 벽을 다 날려야 하는 집(튜토리얼 퍼즐 하우스)에서만 켠다
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
};
