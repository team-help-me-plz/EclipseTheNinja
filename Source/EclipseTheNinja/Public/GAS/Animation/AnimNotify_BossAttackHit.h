#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_BossAttackHit.generated.h"

/**
 * 보스 근접 패턴 타격 1회의 판정 영역.
 * 보스 액터 기준(X 정면, Y 깊이, Z 위)으로 놓이는 상자이며, 보스가 바라보는 방향을 따라 돈다.
 */
USTRUCT(BlueprintType)
struct ECLIPSETHENINJA_API FBossAttackHitArea
{
	GENERATED_BODY()

	/** 상자 중심 위치. 보스 액터 기준 X는 정면, Z는 위쪽(cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hit", meta = (Units = "cm"))
	FVector Offset = FVector(150.0f, 0.0f, 0.0f);

	/** 상자의 절반 크기(cm). X는 앞뒤 길이, Y는 깊이, Z는 높이의 절반. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hit", meta = (ClampMin = "1.0", Units = "cm"))
	FVector HalfExtent = FVector(130.0f, 60.0f, 90.0f);

	/** 어빌리티의 Damage에 곱하는 배율. 2타째만 강하게 하는 식으로 쓴다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hit", meta = (ClampMin = "0.0"))
	float DamageScale = 1.0f;
};

/**
 * 보스 근접 패턴 몽타주의 타격 프레임에 놓는 노티파이.
 * 노티파이마다 판정 영역(FBossAttackHitArea)을 따로 정할 수 있어, 다단히트 패턴의 타격마다 다른 범위를 쓸 수 있다.
 * UGABossPatternBase 계열 어빌리티는 이 노티파이로만 타격 시점을 받는다.
 * 판정 영역은 UGABossPatternAttack이 사용하며, 검기처럼 영역을 쓰지 않는 패턴에서는 발사 시점으로만 쓰인다.
 */
UCLASS(meta = (DisplayName = "Boss Attack Hit"))
class ECLIPSETHENINJA_API UAnimNotify_BossAttackHit : public UAnimNotify
{
	GENERATED_BODY()

public:
	UAnimNotify_BossAttackHit();

	virtual void Notify(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference
	) override;

	virtual FString GetNotifyName_Implementation() const override;

	const FBossAttackHitArea& GetHitArea() const { return HitArea; }

protected:
	UPROPERTY(EditAnywhere, Category = "Hit", meta = (ShowOnlyInnerProperties))
	FBossAttackHitArea HitArea;
};
