

#pragma once

#include "CoreMinimal.h"
#include "Components/BoxComponent.h"
#include "InteractBoxComponent.generated.h"

class UAnimMontage;

/**
 * 
 */
UCLASS()
class ECLIPSETHENINJA_API UInteractBoxComponent : public UBoxComponent
{
	GENERATED_BODY()
	
public:
	virtual void BeginPlay() override;

	void	OnScrollInput(const int32 Direction);

	void	TryInteract();

	// 상호작용 성공 시 상호작용 액터가 호출. 입력을 막고 InInteractable 쪽으로 몸을 돌린 뒤 재생하며, 몽타주가 끝나면 입력을 돌려준다
	// InMontage가 nullptr이면 기본 몽타주를 재생
	UFUNCTION(BlueprintCallable)
	void	PlayInteractMontage(AActor* InInteractable, UAnimMontage* InMontage);

	// 상호작용 몽타주 재생 중이라 입력이 막혀 있는가
	bool	IsPlayingInteractMontage() const { return (this->bIsPlayingInteractMontage); }

protected:
	UFUNCTION()
	void	OnInteractBoxBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void	OnInteractBoxEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex);

	// TargetActor를 바꾸는 함수
	void	ChangeTargetActor(AActor* InActor);

	// 다음 TargetActor를 선택하는 함수
	AActor* FindNextActor(const int32 InIndex);

	// 겹친 액터가 MeshAssembly 같은 자식 액터라면 Interactable을 구현한 부모 액터를 찾아 반환
	AActor*	ResolveInteractableActor(AActor* InActor) const;

	// 상호작용 액터가 전용 몽타주를 넘기지 않았을 때 재생하는 기본 몽타주
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interact|Montage")
	TObjectPtr<UAnimMontage> InteractMontage = nullptr;

	// 몽타주가 끝나거나 끊기면 입력을 돌려준다. 다음 재생에 밀려 끝난 이전 몽타주(InPlayId가 다름)는 무시한다
	void	OnInteractMontageEnded(UAnimMontage* InMontage, bool bInterrupted, int32 InPlayId);
	void	SetOwnerInputLocked(bool bInLocked);

private:
	bool	bIsPlayingInteractMontage = false;
	// 재생할 때마다 증가. 끝난 몽타주가 마지막으로 재생한 것인지 구분한다
	int32	InteractMontagePlayId = 0;

	// 현재 Overlap된 상호작용 가능 액터 목록
	TArray<TWeakObjectPtr<AActor>>	OverlappedInteractActor;
	// 상호작용 키를 눌렀을 때 상호작용할 액터
	TWeakObjectPtr<AActor>			TargetActor = nullptr;
	// 선택 목록에는 넣지 않고 테두리만 강조 중인 액터 (발판 등). Show/Hide를 한 번씩만 호출하기 위해 기록한다
	TSet<TWeakObjectPtr<AActor>>	HighlightOnlyActors;
};
