

#pragma once

#include "CoreMinimal.h"
#include "Camera/PlayerCameraManager.h"
#include "PlayerCharacterCameraManager.generated.h"

class AHUD;
class UCanvas;

/**
 * 
 */
UCLASS()
class ECLIPSETHENINJA_API APlayerCharacterCameraManager : public APlayerCameraManager
{
	GENERATED_BODY()
	
public:
	/** Overrides the default camera view target calculation */
	virtual void UpdateViewTarget(FTViewTarget& OutVT, float DeltaTime) override;

	// 카메라 모드 변환 (일반, 이동모드)
	void SetCameraExploreMode(bool bEnabled);

	// 캐릭터가 순간이동했을 때 카메라가 튀지 않도록, 이동한 반대만큼 오프셋을 줬다가 TeleportSmoothSpeed로 0까지 줄인다.
	// Delta는 캐릭터가 이동한 양이다.
	void SmoothTeleport(const FVector& Delta);

	// SmoothTeleport 오프셋이 줄어드는 속도
	UPROPERTY(EditAnywhere, Category = "Side Scrolling Camera", meta = (ClampMin = 0, ClampMax = 20))
	float TeleportSmoothSpeed = 4.f;

	/** Use continuous cursor direction and eased strength; disable for independent axis movement. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Side Scrolling Camera", meta = (DisplayName = "Use Smooth Camera Explore"))
	bool bUseSmoothCameraExplore = true;

	/** Draw the mouse dead-zone boundary in screen space, including outside explore mode. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Side Scrolling Camera|Debug")
	bool bDrawCameraExploreBoundary = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Side Scrolling Camera|Debug", meta = (EditCondition = "bDrawCameraExploreBoundary"))
	FLinearColor CameraExploreBoundaryColor = FLinearColor::Yellow;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Side Scrolling Camera|Debug", meta = (EditCondition = "bDrawCameraExploreBoundary", ClampMin = "1.0", ClampMax = "10.0"))
	float CameraExploreBoundaryThickness = 2.0f;

public:

	/** How close we want to stay to the view target */
	UPROPERTY(EditAnywhere, Category = "Side Scrolling Camera", meta = (ClampMin = 0, ClampMax = 10000, Units = "cm"))
	float CurrentZoom = 1000.0f;

	/** How far above the target do we want the camera to focus */
	UPROPERTY(EditAnywhere, Category = "Side Scrolling Camera", meta = (ClampMin = 0, ClampMax = 10000, Units = "cm"))
	float CameraZOffset = 100.0f;

	/** Minimum camera scrolling bounds in world space */
	UPROPERTY(EditAnywhere, Category = "Side Scrolling Camera", meta = (ClampMin = -100000, ClampMax = 100000, Units = "cm"))
	float CameraXMinBounds = -400.0f;

	/** Maximum camera scrolling bounds in world space */
	UPROPERTY(EditAnywhere, Category = "Side Scrolling Camera", meta = (ClampMin = -100000, ClampMax = 100000, Units = "cm"))
	float CameraXMaxBounds = 10000.0f;

	UPROPERTY(EditAnywhere, Category = "Side Scrolling Camera", meta = (ClampMin = 0, ClampMax = 1, Units = "cm"))
	float CameraExploreBoundaryHalfWidth = 0.9f;

	UPROPERTY(EditAnywhere, Category = "Side Scrolling Camera", meta = (ClampMin = 0, ClampMax = 1, Units = "cm"))
	float CameraExploreBoundaryHalfHeight = 0.9f;

	UPROPERTY(EditAnywhere, Category = "Side Scrolling Camera", meta = (ClampMin = 0, ClampMax = 10000, Units = "cm"))
	float CameraExploreMaxDistance = 100.f;

	UPROPERTY(EditAnywhere, Category = "Side Scrolling Camera", meta = (ClampMin = 0, ClampMax = 10))
	float CameraExploreSpeed = 5.f;

	UPROPERTY(EditAnywhere, Category = "Side Scrolling Camera", meta = (ClampMin = 0, ClampMax = 10))
	float CameraReturnSpeed = 2.f;

	// 일반 모드에서 캐릭터를 지연 없이 바로 따라갈지. 끄면 CameraReturnSpeed로 천천히 따라간다.
	UPROPERTY(EditAnywhere, Category = "Side Scrolling Camera")
	bool bFollowInstantlyInNormalMode = true;

	// 1이면 현재 화면의 가로 전체만큼 이동
	UPROPERTY(EditAnywhere, Category = "Side Scrolling Camera", meta = (ClampMin = 0, ClampMax = 1))
	float CameraMoveDistance = .4f;

	UPROPERTY(EditDefaultsOnly, Category = "Side Scrolling Camera")
	bool bOrthographic = false;

	UPROPERTY(EditDefaultsOnly, Category = "Side Scrolling Camera", meta = (ClampMin = 1))
	float OrthoWidth = 1200.0f;

	// 켜면 카메라와 플레이어 사이를 가리는 정적 메시(집 벽·지붕 등)를 숨기고, 더 이상 가리지 않으면 다시 보이게 한다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Side Scrolling Camera|Occlusion")
	bool bHideOccluders = true;

	// 카메라에서 플레이어 쪽으로 훑는 상자의 반 크기. X(좌우)·Z(위아래)를 키우면 더 넓게 숨긴다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Side Scrolling Camera|Occlusion")
	FVector OccluderSweepHalfExtent = FVector(40.0f, 5.0f, 50.0f);

	// 플레이어 액터 위치에서 이만큼 올린 지점(가슴 높이)을 향해 훑는다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Side Scrolling Camera|Occlusion", meta = (Units = "cm"))
	float OccluderTargetZOffset = 40.0f;

	// 플레이어 바로 앞 이 거리에서 훑기를 멈춘다. 플레이어가 서 있는 바닥·뒤쪽 벽은 숨기지 않는다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Side Scrolling Camera|Occlusion", meta = (Units = "cm"))
	float OccluderStopDistance = 60.0f;

	// 이 태그가 붙은 컴포넌트나 액터는 가려도 숨기지 않는다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Side Scrolling Camera|Occlusion")
	FName KeepVisibleTag = TEXT("KeepVisible");

	// 켜면 가림 메시 숨기기를 숨김 구역(HouseCutawayVolume 등) 안에 있을 때만 한다. 그 밖에서는 아무것도 숨기지 않는다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Side Scrolling Camera|Occlusion")
	bool bHideOccludersOnlyInZones = false;

	// 플레이어가 숨김 구역에 들어가고 나올 때 부른다. 구역이 겹쳐도 모두 나갈 때까지 유지된다
	UFUNCTION(BlueprintCallable, Category = "Side Scrolling Camera|Occlusion")
	void EnterOccluderZone();

	UFUNCTION(BlueprintCallable, Category = "Side Scrolling Camera|Occlusion")
	void ExitOccluderZone();


protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void DrawCameraExploreBoundary(AHUD* HUD, UCanvas* Canvas);
	FDelegateHandle CameraBoundaryDrawHandle;

	/** Last cached camera vertical location. The camera only adjusts its height if necessary. */
	float CurrentZ = 0.0f;

	/** First-time update camera setup flag */
	bool bSetup = true;

	bool bIsCameraExploreMode = false;

	// 탐색 모드에서 일반 모드로 돌아온 순간을 알기 위한 이전 프레임 모드
	bool bWasCameraExploreMode = false;

	// 탐색 모드에서 돌아올 때 캐릭터 기준으로 남은 카메라 거리. 0으로 줄어든다.
	FVector ExploreReturnOffset = FVector::ZeroVector;

	// SmoothTeleport로 남은 카메라 오프셋. 0으로 줄어든다.
	FVector TeleportOffset = FVector::ZeroVector;

	// 카메라와 플레이어 사이를 가리는 정적 메시를 찾아 숨기고, 이번 프레임에 가리지 않은 것은 다시 보이게 하는 함수
	void UpdateOccluders(const FVector& CameraLocation, const APawn* TargetPawn);

	// 숨겨 둔 메시를 모두 다시 보이게 하는 함수 (컷신 카메라로 바뀌거나 끝날 때)
	void RestoreOccluders();

	// 지금 숨겨 둔 메시
	TSet<TWeakObjectPtr<UPrimitiveComponent>> HiddenOccluders;

	// 플레이어가 들어가 있는 숨김 구역 수
	int32 OccluderZoneCount = 0;
};

