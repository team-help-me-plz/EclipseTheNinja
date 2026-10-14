

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

	// 1이면 현재 화면의 가로 전체만큼 이동
	UPROPERTY(EditAnywhere, Category = "Side Scrolling Camera", meta = (ClampMin = 0, ClampMax = 1))
	float CameraMoveDistance = .4f;


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
};

