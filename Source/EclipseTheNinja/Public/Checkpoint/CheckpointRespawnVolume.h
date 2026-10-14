// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CheckpointRespawnVolume.generated.h"

class UBoxComponent;

/**
 * 테스트용 리스폰 발동 볼륨
 * 플레이어가 닿으면 마지막 체크포인트로 리스폰(롤백)시키는 볼륨. (낙사 구역, 테스트용 리셋 발판 등)
 */
UCLASS()
class ECLIPSETHENINJA_API ACheckpointRespawnVolume : public AActor
{
    GENERATED_BODY()

public:
    ACheckpointRespawnVolume();

protected:
    virtual void BeginPlay() override;

    UFUNCTION()
    void OnTriggerBeginOverlap(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex,
        bool bFromSweep,
        const FHitResult& SweepResult);

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UBoxComponent> Trigger = nullptr;

};
