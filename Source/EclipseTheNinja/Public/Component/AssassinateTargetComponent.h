

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Component/AssassinationTargetingComponent.h"
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
	 * 아래 바닥에 선 암살자가 적이 선 발판 모서리를 손 짚고 올라와 암살할 수 있는지 확인하고, 그 계획을 돌려준다.
	 * 판정은 올라갈 높이(적 발판 높이 - 암살자 발 높이)와 벽에 붙어 있는지, 경로가 막히지 않는지만 본다.
	 * 적이 모서리에서 얼마나 떨어져 있는지는 따지지 않고, 가까우면 피격자를 앞으로 밀어 잡는 거리를 맞춘다.
	 */
	struct FBelowPlan
	{
		FVector WallLocation = FVector::ZeroVector;   // 벽 바로 앞 아래 바닥(올라가기 시작 위치)
		FVector ClimbLocation = FVector::ZeroVector;  // 모서리 안쪽에 올라선 위치
		FVector GrabLocation = FVector::ZeroVector;   // 목을 잡는 위치(올라선 위치보다 적에게 가깝거나 같다)
		float VictimForwardPush = 0.0f;               // 잡는 거리가 모자라 피격자를 앞으로 밀 거리(cm)
	};
	bool FindBelowPlan(const ACharacter* Assassin, FBelowPlan& OutPlan) const;

	bool IsBelowApproachClear(const ACharacter* Assassin) const
	{
		FBelowPlan Plan;
		return FindBelowPlan(Assassin, Plan);
	}

	/** 지정한 두 위치 사이를 암살자의 캡슐로 막힘 없이 이동할 수 있는지 확인한다. 이 적은 무시한다. */
	bool IsPathClear(const ACharacter* Assassin, const FVector& Start, const FVector& End) const;

	/**
	 * 지금 위치의 암살자에게 이 적을 어떤 방식으로 암살할 수 있는지 돌려준다(평지 > 공중 > 아래 순서로 확인).
	 * 적의 경계 상태와 암살자의 이동 상태는 보지 않는다. 타겟 선정과 테스트 맵 검증이 같은 판정을 쓴다.
	 * @param bCheckZone true면 암살 영역(박스) 안에 있는지도 확인한다. 게임에서는 영역 진입 이벤트로 이미 걸러진다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Assassinate")
	EAssassinationType EvaluateAssassinationType(ACharacter* Assassin, bool bCheckZone) const;

	/** 암살자 캡슐이 암살 영역(박스)과 겹치는지 위치로 확인한다. 겹침 이벤트가 없는 에디터 월드 검증용이다. */
	bool IsAssassinInZone(const ACharacter* Assassin) const;

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

	/**
	 * 암살자 캡슐 중심이 적 캡슐 중심보다 최소 이만큼 높아야 공중 암살이 가능하다.
	 * GroundHeightTolerance와 같게 두면 평지 암살과 공중 암살 사이에 암살이 안 되는 높이 구간이 생기지 않는다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Assassinate|Aerial", meta = (ClampMin = "0.0", Units = "cm"))
	float AerialMinHeight = 45.0f;

	/** 암살자 캡슐 중심이 적 캡슐 중심보다 이보다 높으면 공중 암살을 할 수 없다. 애니메이션은 약 400cm 낙하 기준이다. */
	UPROPERTY(EditDefaultsOnly, Category = "Assassinate|Aerial", meta = (ClampMin = "0.0", Units = "cm"))
	float AerialMaxHeight = 400.0f;

	/**
	 * 공중 암살에서 암살자가 착지할 위치. 적이 바라보는 방향 기준(X 앞, Z 위)이다.
	 * 칼날이 쓰러진 피격자 몸통을 지나도록 애니메이션에서 구한 값이다. 피격자 원점은 암살자 원점보다 160cm 앞이고,
	 * 착지 순간 암살자 루트는 약 120cm 지점에 있으므로 피격자 40cm 뒤에 착지한다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Assassinate|Aerial")
	FVector AerialLandingOffset = FVector(-40.0f, 0.0f, 0.0f);

	/** 아래 암살로 손 짚고 올라갈 수 있는 높이(적이 선 발판 높이 - 암살자 발 높이). 애니메이션은 약 200cm 기준이다. */
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

	/** 올라온 뒤 서는 위치, 모서리에서 얼마나 떨어져 있는지. */
	UPROPERTY(EditDefaultsOnly, Category = "Assassinate|Below", meta = (ClampMin = "0.0", Units = "cm"))
	float BelowClimbInset = 40.0f;

	/** 목을 잡는 순간 암살자와 적의 중심 간 거리. 애니메이션 기준 약 60cm다. */
	UPROPERTY(EditDefaultsOnly, Category = "Assassinate|Below", meta = (ClampMin = "0.0", Units = "cm"))
	float BelowGrabDistance = 60.0f;

	TWeakObjectPtr<UAssassinationTargetingComponent> CurrentTargetedComp;
	bool bAssassinationInProgress = false;
	bool bPlayerInZone = false;


};
