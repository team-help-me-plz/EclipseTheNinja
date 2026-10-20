#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "AbilityTask_MontageRootOffset.generated.h"

class ACharacter;
class UAnimMontage;

/**
 * 루트를 캡슐에 고정(Root Lock)한 몽타주에서, 캡슐이 따라갈 수 없는 루트 이동을 메시 위치로 보여준다.
 * 횡스크롤이라 캡슐은 화면 깊이 방향으로 움직이지 못하고, 이동이 꺼진 캐릭터는 아예 움직이지 않는다.
 * 매 틱 몽타주 재생 위치까지의 루트 이동량을 구해 메시의 상대 위치를 그만큼 옮긴다.
 */
UCLASS()
class ECLIPSETHENINJA_API UAbilityTask_MontageRootOffset : public UAbilityTask
{
	GENERATED_BODY()

public:
	UAbilityTask_MontageRootOffset();

	/**
	 * @param Character        메시를 옮길 캐릭터
	 * @param Montage          루트 이동량을 읽을 몽타주. Character에서 재생 중이어야 한다.
	 * @param bSideOnly        true면 캐릭터 옆 방향(화면 깊이) 이동만 옮긴다. 앞뒤는 캡슐이 따라가는 경우에 쓴다.
	 * @param bKeepOffsetOnEnd true면 몽타주나 태스크가 끝나도 메시를 그 자리에 둔다(쓰러진 피격자).
	 *                         false면 몽타주가 섞여 빠지는 동안 원래 자리로 되돌린다.
	 */
	static UAbilityTask_MontageRootOffset* FollowMontageRoot(UGameplayAbility* OwningAbility, ACharacter* Character,
		UAnimMontage* Montage, bool bSideOnly, bool bKeepOffsetOnEnd);

	virtual void Activate() override;
	virtual void TickTask(float DeltaTime) override;

protected:
	virtual void OnDestroy(bool bInOwnerFinished) override;

private:
	// 현재 몽타주 재생 위치까지의 루트 이동을 메시 상대 위치 오프셋으로 바꾼다.
	FVector CalculateOffset() const;
	void ApplyOffset(const FVector& Offset);

	TWeakObjectPtr<ACharacter> Character;

	UPROPERTY()
	TObjectPtr<UAnimMontage> Montage;

	FVector BaseRelativeLocation = FVector::ZeroVector;
	FVector CurrentOffset = FVector::ZeroVector;
	FVector ReturnStartOffset = FVector::ZeroVector;
	float ReturnDuration = 0.25f;
	float ReturnElapsed = 0.0f;
	bool bSideOnly = true;
	bool bKeepOffsetOnEnd = false;
	bool bReturning = false;
};
