

#pragma once

#include "CoreMinimal.h"
#include "Components/BoxComponent.h"
#include "InteractBoxComponent.generated.h"

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

private:
	// 현재 Overlap된 상호작용 가능 액터 목록
	TArray<TWeakObjectPtr<AActor>>	OverlappedInteractActor;
	// 상호작용 키를 눌렀을 때 상호작용할 액터
	TWeakObjectPtr<AActor>			TargetActor = nullptr;
	// 선택 목록에는 넣지 않고 테두리만 강조 중인 액터 (발판 등). Show/Hide를 한 번씩만 호출하기 위해 기록한다
	TSet<TWeakObjectPtr<AActor>>	HighlightOnlyActors;
};
