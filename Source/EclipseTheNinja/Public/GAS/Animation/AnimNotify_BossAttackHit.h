#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_BossAttackHit.generated.h"

class ACharacter;

/**
 * 보스 근접 패턴 타격 1회의 판정 영역.
 * 보스 액터 기준(X 정면, Y 깊이, Z 위)으로 놓이는 상자이며, 보스가 바라보는 방향을 따라 돈다.
 * 위치와 크기 모두 보스 액터 스케일이 곱해진다. 스케일 1.25인 보스는 상자도 1.25배로 커진다.
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

#if WITH_EDITOR
	// 애니메이션·몽타주 에디터 미리보기에 판정 상자를 그린다.
	// 재생 위치가 이 노티파이 직후 PreviewHighlightTime 안이면 굵게, 아니면 옅게 그린다.
	virtual void DrawInEditor(
		FPrimitiveDrawInterface* PDI,
		USkeletalMeshComponent* MeshComp,
		const UAnimSequenceBase* Animation,
		const FAnimNotifyEvent& NotifyEvent
	) const override;
#endif

	const FBossAttackHitArea& GetHitArea() const { return HitArea; }

protected:
	UPROPERTY(EditAnywhere, Category = "Hit", meta = (ShowOnlyInnerProperties))
	FBossAttackHitArea HitArea;

#if WITH_EDITORONLY_DATA
	/**
	 * 미리보기에서 판정 상자를 놓을 기준 캐릭터 BP.
	 * 판정 상자는 보스 액터 기준이라, 이 BP의 메시 위치·회전으로 미리보기 메시 기준 위치를 계산한다.
	 * 뷰포트의 Show > Notifications(노티파이 시각화)가 켜져 있어야 보인다.
	 */
	UPROPERTY(EditAnywhere, Category = "Hit|Preview")
	TSoftClassPtr<ACharacter> PreviewCharacterClass;

	/** 노티파이 시점부터 이 시간(초) 동안은 판정 상자를 굵게 그린다. */
	UPROPERTY(EditAnywhere, Category = "Hit|Preview", meta = (ClampMin = "0.0", Units = "s"))
	float PreviewHighlightTime = 0.2f;
#endif
};
