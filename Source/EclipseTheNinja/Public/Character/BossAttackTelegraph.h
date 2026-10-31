#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BossAttackTelegraph.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMeshComponent;

/**
 * 보스 패턴 피격 범위 예고에 쓰는 사각형 하나.
 * 보스 액터 기준 좌표(X 정면, Z 위)로 놓이며, 위치와 크기 모두 보스 액터 스케일이 곱해진다(FBossAttackHitArea와 같은 규칙).
 * 횡스크롤 화면에서 보이도록 X-Z 평면에 그린다.
 */
struct FBossTelegraphArea
{
	/** 사각형 중심. 보스 액터 기준(cm). */
	FVector Center = FVector::ZeroVector;

	/** 사각형의 절반 크기(cm). X는 앞뒤 길이, Y는 높이의 절반. */
	FVector2D HalfSize = FVector2D(100.0f, 100.0f);

	/** 보스 정면 기준으로 사각형을 위아래로 기울이는 각도(도). 양수면 앞쪽이 위로 올라간다. */
	float PitchDegrees = 0.0f;
};

/**
 * 보스 패턴을 시전하는 순간 피격 범위를 화면에 보여 주는 반투명 사각형.
 * 패턴 어빌리티가 스폰해 Show로 범위를 정하고, 타격이 끝나면 Hide로 서서히 사라지게 한다.
 * 깊이 테스트를 끄는 머티리얼(M_BossAttackTelegraph)을 써서 지형이나 캐릭터에 가려지지 않는다.
 */
UCLASS()
class ECLIPSETHENINJA_API ABossAttackTelegraph : public AActor
{
	GENERATED_BODY()

public:
	ABossAttackTelegraph();

	virtual void Tick(float DeltaSeconds) override;

	/**
	 * 보스 기준 Area 위치에 범위를 표시한다.
	 * bFollowBoss면 보스에 붙어 함께 움직이고, 아니면 지금 보스 위치에 고정된다.
	 */
	void Show(AActor* Boss, const FBossTelegraphArea& Area, bool bFollowBoss);

	/** 서서히 사라진 뒤 스스로 제거된다. */
	void Hide();

protected:
	virtual void BeginPlay() override;

	// 지금 투명도 배율(0~1)을 머티리얼에 반영한다.
	void ApplyAlpha();

	UPROPERTY(VisibleAnywhere, Category = "Telegraph")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** 범위 사각형 머티리얼. Color, Opacity, EdgeOpacity, BorderPixels 파라미터를 사용한다. */
	UPROPERTY(EditDefaultsOnly, Category = "Telegraph")
	TObjectPtr<UMaterialInterface> TelegraphMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "Telegraph")
	FLinearColor Color = FLinearColor(1.0f, 0.08f, 0.02f, 1.0f);

	/** 사각형 안쪽의 불투명도. */
	UPROPERTY(EditDefaultsOnly, Category = "Telegraph", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FillOpacity = 0.25f;

	/** 사각형 테두리의 불투명도. */
	UPROPERTY(EditDefaultsOnly, Category = "Telegraph", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float EdgeOpacity = 0.85f;

	/** 나타나는 데 걸리는 시간(초). */
	UPROPERTY(EditDefaultsOnly, Category = "Telegraph", meta = (ClampMin = "0.0", Units = "s"))
	float FadeInTime = 0.1f;

	/** 사라지는 데 걸리는 시간(초). */
	UPROPERTY(EditDefaultsOnly, Category = "Telegraph", meta = (ClampMin = "0.0", Units = "s"))
	float FadeOutTime = 0.15f;

	/** Hide가 불리지 않아도 이 시간(초)이 지나면 제거된다. 패턴이 비정상 종료될 때의 안전장치다. */
	UPROPERTY(EditDefaultsOnly, Category = "Telegraph", meta = (ClampMin = "0.0", Units = "s"))
	float MaxLifeTime = 10.0f;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> TelegraphMID;

	float Alpha = 0.0f;
	bool bHiding = false;
};
