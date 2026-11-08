// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ClimbLadder.generated.h"

class UBoxComponent;
class UInstancedStaticMeshComponent;
class UStaticMesh;

/**
 * 벽타기 가능한 벽을 표시하는 사다리.
 *  - 액터 위치는 벽면 아래쪽 끝(플레이 라인 위)이고, 액터의 +X가 벽 안쪽을 향하게 놓는다
 *  - 측면 카메라에서 사다리 모양으로 보이도록 사다리 면을 벽과 직각(XZ 평면)으로 세우고, 플레이어 뒤쪽(VisualYOffset)에 그린다
 *  - bProvidesClimbSurface를 켜면 벽면에 벽타기(WallClimb) 충돌 상자를 함께 만들어, 사다리를 놓은 벽은 타고 오를 수 있게 한다
 *  - 레일과 가로대는 인스턴스 메시로 만들고 충돌은 끈다
 */
UCLASS()
class ECLIPSETHENINJA_API AClimbLadder : public AActor
{
    GENERATED_BODY()

public:
    AClimbLadder();

    virtual void OnConstruction(const FTransform& Transform) override;

protected:
    // 레일·가로대 인스턴스를 다시 만드는 함수
    void RebuildLadder();

    // 메시를 Length 길이의 막대로 놓는 트랜스폼. 메시는 Z축 방향 막대라고 가정한다
    FTransform MakePoleTransform(const UStaticMesh* Mesh, const FVector& Start, const FRotator& Rotation, float Length, float Thickness) const;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USceneComponent> SceneRoot = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UInstancedStaticMeshComponent> Rails = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UInstancedStaticMeshComponent> Rungs = nullptr;

    // 벽타기 판정용 상자. 벽면에서 벽 안쪽으로 ClimbSurfaceDepth만큼 들어간다
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UBoxComponent> ClimbSurface = nullptr;

    // 사다리 높이 (벽면 아래 끝부터)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ladder", meta = (ClampMin = "50.0", Units = "cm"))
    float Height = 500.0f;

    // 벽에서 튀어나온 사다리 폭 (벽면 ~ 바깥 레일)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ladder", meta = (ClampMin = "10.0", Units = "cm"))
    float Width = 45.0f;

    // 벽면과 안쪽 레일 사이 간격
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ladder", meta = (ClampMin = "0.0", Units = "cm"))
    float WallGap = 6.0f;

    // 가로대 간격
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ladder", meta = (ClampMin = "10.0", Units = "cm"))
    float RungSpacing = 32.0f;

    // 레일이 사다리 위 끝보다 더 올라가는 길이 (잡고 올라서는 손잡이)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ladder", meta = (ClampMin = "0.0", Units = "cm"))
    float RailTopExtension = 30.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ladder", meta = (ClampMin = "0.1", Units = "cm"))
    float RailThickness = 9.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ladder", meta = (ClampMin = "0.1", Units = "cm"))
    float RungThickness = 6.0f;

    // 사다리를 그릴 Y 오프셋. 음수면 카메라에서 멀어져 플레이어 뒤에 보인다
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ladder", meta = (Units = "cm"))
    float VisualYOffset = -45.0f;

    // 레일·가로대에 쓸 막대 메시 (Z축 방향 막대)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ladder")
    TObjectPtr<UStaticMesh> RailMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ladder")
    TObjectPtr<UStaticMesh> RungMesh = nullptr;

    // 켜면 벽면에 벽타기 충돌 상자를 만든다. 이미 벽타기 벽이 있는 곳에 표시만 할 때는 꺼도 된다
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ladder|Climb")
    bool bProvidesClimbSurface = true;

    // 벽타기 상자의 벽 안쪽 두께
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ladder|Climb", meta = (ClampMin = "1.0", Units = "cm", EditCondition = "bProvidesClimbSurface"))
    float ClimbSurfaceDepth = 20.0f;

    // 벽타기 상자의 Y 반폭 (플레이 라인 앞뒤)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ladder|Climb", meta = (ClampMin = "1.0", Units = "cm", EditCondition = "bProvidesClimbSurface"))
    float ClimbSurfaceHalfWidth = 100.0f;
};
