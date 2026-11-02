#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AnimNotifyState_SwordTrail.generated.h"

class UMaterialInterface;
class UNiagaraComponent;
class UNiagaraSystem;

/**
 * 노티파이 구간 동안 칼에 트레일을 붙이는 노티파이 스테이트. 몽타주에서 휘두르는 구간에만 배치한다.
 * 칼은 캐릭터 메시와 별도의 메시 컴포넌트이므로, 소유 액터의 메시 중 SocketName 소켓을 가진 메시를 찾아 붙인다.
 * 구간이 끝나면 새 리본 생성만 멈추고, 남은 리본은 수명대로 사라진다.
 */
UCLASS(meta = (DisplayName = "Sword Trail"))
class ECLIPSETHENINJA_API UAnimNotifyState_SwordTrail : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	UAnimNotifyState_SwordTrail();

	virtual void NotifyBegin(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		float TotalDuration,
		const FAnimNotifyEventReference& EventReference
	) override;

	virtual void NotifyEnd(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference
	) override;

	virtual FString GetNotifyName_Implementation() const override;

protected:
	/** 칼에 붙여 재생할 트레일 이펙트 */
	UPROPERTY(EditAnywhere, Category = "Trail")
	TObjectPtr<UNiagaraSystem> TrailSystem = nullptr;

	/** 트레일 리본 머티리얼. 비워 두면 이펙트에 지정된 기본 머티리얼을 쓴다. */
	UPROPERTY(EditAnywhere, Category = "Trail")
	TObjectPtr<UMaterialInterface> TrailMaterial = nullptr;

	/** 칼에 불이 붙어 있을 때 쓸 트레일 이펙트. 비워 두면 TrailSystem을 그대로 쓴다. */
	UPROPERTY(EditAnywhere, Category = "Trail|Fire")
	TObjectPtr<UNiagaraSystem> FireTrailSystem = nullptr;

	/** 칼에 불이 붙어 있을 때 쓸 트레일 머티리얼. 비워 두면 TrailMaterial을 그대로 쓴다. */
	UPROPERTY(EditAnywhere, Category = "Trail|Fire")
	TObjectPtr<UMaterialInterface> FireTrailMaterial = nullptr;

	/**
	 * 트레일을 붙일 칼 메시 소켓. 칼날 중앙에 두고 소켓의 Z축이 칼날 방향을 향해야 한다.
	 * (트레일 리본이 소켓 Z축 방향으로 펼쳐진다)
	 */
	UPROPERTY(EditAnywhere, Category = "Trail")
	FName SocketName = TEXT("b_TrailSocket");

	/** 트레일 리본 폭 (칼날 길이에 맞춘다) */
	UPROPERTY(EditAnywhere, Category = "Trail", meta = (ClampMin = "0.0", Units = "cm"))
	float TrailWidth = 60.0f;

	/**
	 * 트레일 중심을 소켓에서 칼끝 쪽(소켓 Z축)으로 옮기는 거리.
	 * 폭을 칼날보다 넓히면 리본이 손(회전 중심)을 넘어가 접히면서 선이 생기므로, 넓힌 만큼의 절반을 밀어 준다.
	 */
	UPROPERTY(EditAnywhere, Category = "Trail", meta = (Units = "cm"))
	float TrailOffset = 0.0f;

private:
	// 소유 액터에 불속성 태그가 붙어 있는지 확인한다.
	bool IsWeaponOnFire(const USkeletalMeshComponent* MeshComp) const;

	// 소유 액터의 메시 중 SocketName 소켓을 가진 메시를 찾는다.
	USceneComponent* FindAttachComponent(USkeletalMeshComponent* MeshComp) const;

	// 노티파이 객체는 몽타주 하나를 여러 메시가 함께 쓰므로 메시마다 붙인 트레일을 따로 둔다.
	TMap<TWeakObjectPtr<USkeletalMeshComponent>, TWeakObjectPtr<UNiagaraComponent>> ActiveTrails;
};
