#pragma once

#include "CoreMinimal.h"
#include "MinimapMarkerTypes.generated.h"

class AActor;
class UTexture2D;

UENUM(BlueprintType)
enum class EMinimapMarkerShape : uint8
{
	Square,
	Diamond,
	Circle,
	/** 마커는 그리지 않고 PathProperty 경로만 그린다 (움직이는 발판) */
	PathOnly,
	/** 액터의 충돌 범위(XZ)를 막대로 그린다. StateProperty가 true면 테두리만 StateColor로 그린다 (문) */
	ActorBounds,
};

/** 미니맵 마커 모양 */
USTRUCT(BlueprintType)
struct FMinimapMarkerStyle
{
	GENERATED_BODY()

	/** 아이콘이 없을 때 그리는 도형 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Marker")
	EMinimapMarkerShape Shape = EMinimapMarkerShape::Square;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Marker")
	FLinearColor Color = FLinearColor::White;

	/** 지정하면 도형 대신 이 아이콘을 Color로 물들여 그린다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Marker")
	TSoftObjectPtr<UTexture2D> Icon;

	/** 화면 크기(px) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Marker", meta = (ClampMin = "2.0"))
	float Size = 9.f;

	/** 이 이름의 bool UPROPERTY가 true면 StateColor로 그린다 (예: 문 bIsOpen) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Marker|State")
	FName StateProperty;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Marker|State")
	FLinearColor StateColor = FLinearColor(0.35f, 0.9f, 0.4f, 1.f);

	/** 켜면 StateProperty가 true일 때 마커 대신 지금 보이는 메시 범위를 땅(벽 색)으로 그린다 (착지한 낙하물) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Marker|State")
	bool bDrawAsGroundWhenStateOn = false;

	/** 이 이름의 액터 배열 UPROPERTY를 경로로 이어 그린다 (예: 움직이는 발판 TargetPoint) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Marker|State")
	FName PathProperty;

	/** 보이는 메시가 하나도 없으면(부서진 발판 등) 흐리게 그린다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Marker|State")
	bool bDimWhenMeshHidden = true;
};

/** 클래스별 마커 규칙. 위에서부터 처음 맞는 규칙을 쓰므로 자식 클래스를 위에 둔다. */
USTRUCT(BlueprintType)
struct FMinimapMarkerRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Marker")
	TSoftClassPtr<AActor> ActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Marker")
	FMinimapMarkerStyle Style;
};

/** 위젯이 그릴 마커 하나 */
struct FMinimapMarkerView
{
	FVector2D Pixel = FVector2D::ZeroVector;	// 미니맵 텍스처 픽셀 좌표
	const FMinimapMarkerStyle* Style = nullptr;
	UTexture2D* Icon = nullptr;
	FLinearColor Color = FLinearColor::White;
	bool bDimmed = false;
	bool bStateOn = false;						// StateProperty 값 (문 열림 등)
	bool bAsGround = false;						// 땅으로 그린다. BoundsMin/Max 사용
	FVector2D BoundsMin = FVector2D::ZeroVector;	// ActorBounds: 텍스처 픽셀 좌표 (왼쪽 위)
	FVector2D BoundsMax = FVector2D::ZeroVector;	// ActorBounds: 텍스처 픽셀 좌표 (오른쪽 아래)
	TArray<FVector2D> Path;						// 텍스처 픽셀 좌표
};
