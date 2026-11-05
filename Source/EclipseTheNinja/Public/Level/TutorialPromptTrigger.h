// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TutorialPromptTrigger.generated.h"

class UBoxComponent;

/**
 * 튜토리얼 안내 트리거 액터.
 *  - PromptTrigger : 플레이어가 닿으면 Messages를 UIMessageSubsystem으로 순서대로 띄운다
 *  - 문구는 레벨에 배치한 인스턴스의 디테일 패널에서 구역마다 입력
 *  - bShowOnce를 끄면 구역에 다시 들어올 때마다 문구를 띄운다
 */
UCLASS()
class ECLIPSETHENINJA_API ATutorialPromptTrigger : public AActor
{
    GENERATED_BODY()

public:
    ATutorialPromptTrigger();

    // Messages를 플레이어 화면에 띄우는 함수
    UFUNCTION(BlueprintCallable, Category = "Tutorial")
    void ShowPrompt(APlayerController* PlayerController);

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
    TObjectPtr<USceneComponent> SceneRoot = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UBoxComponent> PromptTrigger = nullptr;

    // 구역에 들어왔을 때 띄울 안내 문구. 위에서부터 순서대로 표시된다
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tutorial", meta = (MultiLine = "true"))
    TArray<FText> Messages;

    // 문구 하나가 화면에 남아 있는 시간(초). 기본 메시지보다 길게 두어 조작을 읽을 시간을 준다
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tutorial", meta = (ClampMin = "0.5"))
    float MessageHoldDuration = 4.0f;

    // 켜면 한 번 띄운 뒤에는 다시 들어와도 띄우지 않는다
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tutorial")
    bool bShowOnce = true;

private:
    bool bHasShown = false;

    // 컷신이 끝날 때까지 문구 표시를 미루는 타이머
    FTimerHandle DeferredPromptTimer;

};
