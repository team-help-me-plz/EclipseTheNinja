#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "BossPatternRow.generated.h"

class UGameplayAbility;

/**
 * 보스 공격 패턴 가중치 테이블의 한 행.
 * 보스가 공격을 시작할 때 지금 페이즈의 가중치(PhaseWeights)에 비례한 확률로 한 행을 고르고 AbilityClass를 실행한다.
 * 행이 쿨타임(Cooldown) 중이거나, 지금 페이즈의 가중치가 0이거나, 대상이 어빌리티의 사용 거리(MinUseDistance~MaxUseDistance) 밖이면
 * 그 행은 후보에서 빠진다.
 */
USTRUCT(BlueprintType)
struct ECLIPSETHENINJA_API FBossPatternRow : public FTableRowBase
{
	GENERATED_BODY()

	/** 로그와 디버그에 표시할 패턴 이름. 기획 시트의 패턴 이름을 그대로 쓴다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern")
	FText DisplayName;

	/**
	 * 페이즈별 선택 가중치. [0]이 1페이즈, [1]이 2페이즈다. 페이즈는 에너지 링크를 파훼할 때마다 1씩 오른다.
	 * 0 이하인 페이즈에서는 고르지 않는다. 배열보다 높은 페이즈는 마지막 값을 쓰고, 배열이 비어 있으면 고르지 않는다.
	 * 예: {0, 10, 20}이면 1페이즈에는 쓰지 않고, 2페이즈부터 쓰며, 3페이즈 이후에는 두 배로 자주 쓴다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern", meta = (ClampMin = "0.0"))
	TArray<float> PhaseWeights = { 1.0f };

	/** 이 패턴이 실행할 어빌리티. 보스가 처음 고를 때 부여한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern")
	TSubclassOf<UGameplayAbility> AbilityClass;

	/**
	 * 패턴 쿨타임(초). 패턴을 시작할 때부터 세며, 그동안 이 행은 후보에서 빠진다.
	 * 같은 어빌리티를 쓰는 행이라도 쿨타임은 행마다 따로 센다. 0이면 쿨타임 없이 가중치로만 고른다.
	 * 기본 공격(근접 발도)은 0으로 두어 다른 패턴이 모두 쿨타임이어도 쓸 패턴이 남게 한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern", meta = (ClampMin = "0.0", Units = "s"))
	float Cooldown = 0.0f;

	/** Phase(1부터)에서 쓸 가중치. 배열보다 높은 페이즈는 마지막 값을 쓴다. */
	float GetWeightForPhase(int32 Phase) const
	{
		if (PhaseWeights.IsEmpty())
		{
			return 0.0f;
		}

		const int32 Index = FMath::Clamp(Phase - 1, 0, PhaseWeights.Num() - 1);
		return PhaseWeights[Index];
	}
};
