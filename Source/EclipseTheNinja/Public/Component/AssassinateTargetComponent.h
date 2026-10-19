

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AssassinateTargetComponent.generated.h"

class UBoxComponent;
class AEnemy;
class ACharacter;
class APlayerCharacter;
class UPrimitiveComponent;
class UAssassinationTargetingComponent;
class USkeletalMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

// if Actor has this component, it can be assassinate
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class ECLIPSETHENINJA_API UAssassinateTargetComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UAssassinateTargetComponent();

	// 현재 이 캐릭터(적)을 암살할 수 있는지
	UFUNCTION(BlueprintCallable, Category = "Assassinate")
	bool CanAssassinate();

	// 현재 이 컴포넌트를 대상으로 암살이 진행중인지 세팅하는 함수
	void SetAssassinationInProgress(bool bInProgress) { bAssassinationInProgress = bInProgress; }

	/** 암살가능 상태를 표시하는 테두리를 켜고끄는 함수*/
	UFUNCTION(BlueprintCallable, Category = "Assassinate|Outline")
	void SetTargetHighlighted(bool bInValue);

	/** 암살 연출을 시작할 등 뒤 위치. 사이드뷰 이동 평면과 암살자의 바닥 높이를 유지한다. */
	FVector GetApproachLocation(const ACharacter* Assassin) const;

	/** 암살자가 적과 같은 바닥 높이에 있고, 등 뒤 위치까지 벽이나 다른 캐릭터에 막히지 않는지 확인한다. (평지 암살용)*/
	bool IsApproachClear(const ACharacter* Assassin) const;

	/** 공중 암살 후 암살자가 내려설 위치. 적이 바라보는 방향 기준 오프셋을 쓰고 높이는 적의 바닥에 맞춘다. */
	FVector GetAerialLandingLocation(const ACharacter* Assassin) const;

	/** 암살자가 적보다 충분히 높은 곳에 있고, 착지 위치 위로 나가 내려가는 경로가 발판·벽에 막히지 않는지 확인한다. */
	bool IsAerialApproachClear(const ACharacter* Assassin) const;

	/**
	 * 아래 바닥에 선 암살자가 적이 선 발판 모서리를 잡고 올라올 수 있는지 확인한다.
	 * 적 뒤쪽으로 발판이 끝나는 모서리와 그 아래 벽을 찾아, 암살자가 벽에 바짝 붙어 있고 올라갈 경로가 막히지 않으면
	 * 모서리 안쪽에 올라선 위치(OutClimbLocation)와 벽 바로 앞 아래 바닥 위치(OutWallLocation)를 돌려준다.
	 */
	bool FindBelowClimbLocation(const ACharacter* Assassin, FVector& OutClimbLocation, FVector& OutWallLocation) const;

	bool IsBelowApproachClear(const ACharacter* Assassin) const
	{
		FVector ClimbLocation;
		FVector WallLocation;
		return FindBelowClimbLocation(Assassin, ClimbLocation, WallLocation);
	}

	/** 아래 암살에서 올라온 뒤 적을 잡는 위치. 적 뒤 BelowGrabDistance만큼 떨어진 발판 위다. */
	FVector GetBelowGrabLocation(const ACharacter* Assassin) const;

	/** 지정한 두 위치 사이를 암살자의 캡슐로 막힘 없이 이동할 수 있는지 확인한다. 이 적은 무시한다. */
	bool IsPathClear(const ACharacter* Assassin, const FVector& Start, const FVector& End) const;

	/** 플레이어가 암살 영역 안에 있는지. 평지·공중은 높이와 이동 상태로 구분한다. */
	bool IsPlayerInZone() const { return bPlayerInZone; }

protected:
	virtual void OnRegister() override;

	// Called when the game starts
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION()
	void OnAssassinationBeginOverlapped(UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void OnAssassinationBoxEndOverlapped(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex);

	UFUNCTION()
	void AddTarget(UAssassinationTargetingComponent* ATC);

	UFUNCTION()
	void RemoveTarget(UAssassinationTargetingComponent* ATC);

protected:
	// 직접 편집한 박스 설정을 유지한 채 부착하고 등록한다.
	void RegisterBoundary();

	// 암살 표시용 테두리 메시 초기화
	void InitializeOutlineMesh();

	// 플레이어가 영역에 들어오면 후보로 등록하고, 벗어나면 해제한다.
	void UpdateZoneState(APlayerCharacter* Player, bool bInside);

	UPROPERTY(EditDefaultsOnly, Category = "Assassinate|Outline")
	TSoftObjectPtr<UMaterialInterface> OutlineMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "Assassinate|Outline", meta = (ClampMin = "0.0", Units = "cm"))
	float OutlineThickness = 1.5f;

	UPROPERTY(EditDefaultsOnly, Category = "Assassinate|Outline")
	FLinearColor OutlineColor = FLinearColor::Red;

	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> OutlineMesh;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> OutlineMaterialInstance;

	/** 위치와 Box Extent를 직접 편집하는 암살 판정 영역. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Assassinate")
	TObjectPtr<UBoxComponent> AssassinateBoundary;

	/** 암살 접근 후 적 뒤에 유지할 중심 간 거리. 캡슐이 겹치지 않는 최소 거리로 보정한다. */
	UPROPERTY(EditDefaultsOnly, Category = "Assassinate|Approach", meta = (ClampMin = "0.0", Units = "cm"))
	float BehindDistance = 150.0f;

	/** 평지 암살로 보는 암살자와 적의 캡슐 중심 높이 차. 영역 안이라도 위쪽 발판에 서 있으면 평지 암살이 아니다. */
	UPROPERTY(EditDefaultsOnly, Category = "Assassinate|Approach", meta = (ClampMin = "0.0", Units = "cm"))
	float GroundHeightTolerance = 45.0f;

	/** 암살자 캡슐 중심이 적 캡슐 중심보다 최소 이만큼 높아야 공중 암살이 가능하다. 짧은 점프에서 발동되지 않게 한다. */
	UPROPERTY(EditDefaultsOnly, Category = "Assassinate|Aerial", meta = (ClampMin = "0.0", Units = "cm"))
	float AerialMinHeight = 120.0f;

	/** 공중 암살 몽타주가 끝났을 때 암살자가 서 있을 위치. 적이 바라보는 방향 기준(X 앞, Z 위)이다. */
	UPROPERTY(EditDefaultsOnly, Category = "Assassinate|Aerial")
	FVector AerialLandingOffset = FVector(-80.0f, 0.0f, 0.0f);

	/** 아래 암살이 가능한 높이 차(적 캡슐 중심 - 암살자 캡슐 중심). 애니메이션은 약 200cm 기준이다. */
	UPROPERTY(EditDefaultsOnly, Category = "Assassinate|Below", meta = (ClampMin = "0.0", Units = "cm"))
	float BelowMinHeight = 120.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Assassinate|Below", meta = (ClampMin = "0.0", Units = "cm"))
	float BelowMaxHeight = 300.0f;

	/** 적 발밑에서 뒤쪽으로 발판 모서리를 찾는 최대 거리. */
	UPROPERTY(EditDefaultsOnly, Category = "Assassinate|Below", meta = (ClampMin = "0.0", Units = "cm"))
	float BelowEdgeSearchDistance = 300.0f;

	/** 암살자가 발판 아래 벽에서 이 거리 안에 바짝 붙어 있어야 올라갈 수 있다. */
	UPROPERTY(EditDefaultsOnly, Category = "Assassinate|Below", meta = (ClampMin = "0.0", Units = "cm"))
	float BelowWallTolerance = 10.0f;

	/** 올라온 뒤 서는 위치가 모서리에서 발판 안쪽으로 들어온 거리. */
	UPROPERTY(EditDefaultsOnly, Category = "Assassinate|Below", meta = (ClampMin = "0.0", Units = "cm"))
	float BelowClimbInset = 40.0f;

	/** 목을 잡는 순간 암살자와 적의 중심 간 거리. 애니메이션 기준 약 60cm다. */
	UPROPERTY(EditDefaultsOnly, Category = "Assassinate|Below", meta = (ClampMin = "0.0", Units = "cm"))
	float BelowGrabDistance = 60.0f;

	TWeakObjectPtr<UAssassinationTargetingComponent> CurrentTargetedComp;
	bool bAssassinationInProgress = false;
	bool bPlayerInZone = false;


};
