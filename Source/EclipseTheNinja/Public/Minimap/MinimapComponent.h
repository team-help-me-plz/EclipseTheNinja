#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InputCoreTypes.h"
#include "MinimapComponent.generated.h"

class UInputAction;
class UInputMappingContext;
class UEnhancedInputComponent;
class UMinimapWidget;
class APlayerController;
struct FInputActionValue;

/**
 * 플레이어에 붙어 미니맵 시야 밝히기, Tab 입력, 미니맵 위젯 표시를 맡는다.
 *  - RevealInterval마다 눈 위치에서 시야를 밝힌다 (움직이지 않으면 IdleRevealInterval마다)
 *  - MinimapAction/MinimapMappingContext를 비워 두면 FallbackKey(기본 Tab)로 런타임 입력을 만든다
 *  - 입력은 별도 입력 컴포넌트를 컨트롤러에 넣어 받으므로 은신·조준 입력 잠금 중에도 미니맵을 볼 수 있다
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class ECLIPSETHENINJA_API UMinimapComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMinimapComponent();

	UFUNCTION(BlueprintCallable, Category = "Minimap")
	void SetMinimapVisible(bool bVisible);

	UFUNCTION(BlueprintPure, Category = "Minimap")
	bool IsMinimapVisible() const { return bMinimapVisible; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 카메라 화면 크기를 구하지 못할 때만 쓰는 시야 반경(uu). 평소에는 플레이어를 중심으로 화면에 보이는 크기의 사각형까지만 밝힌다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minimap|Reveal", meta = (ClampMin = "100.0"))
	float RevealRadius = 2000.f;

	/** 한 번에 쏘는 레이 수. 반경 끝에서 레이 사이 간격이 칸 크기보다 작아야 빈틈이 없다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minimap|Reveal", meta = (ClampMin = "8"))
	int32 RayCount = 240;

	/** 움직일 때 시야를 갱신하는 간격(초) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minimap|Reveal", meta = (ClampMin = "0.02"))
	float RevealInterval = 0.15f;

	/** 가만히 있어도 문이 열리는 등 변화를 반영하기 위해 시야를 갱신하는 간격(초) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minimap|Reveal", meta = (ClampMin = "0.1"))
	float IdleRevealInterval = 0.6f;

	/** 이만큼 움직이면 바로 시야를 갱신한다(uu) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minimap|Reveal", meta = (ClampMin = "1.0"))
	float MoveThreshold = 40.f;

	/** 비워 두면 FallbackKey로 런타임 입력 액션을 만든다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minimap|Input")
	TObjectPtr<UInputAction> MinimapAction = nullptr;

	/** 비워 두면 MinimapAction을 FallbackKey에 매핑한 런타임 IMC를 만든다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minimap|Input")
	TObjectPtr<UInputMappingContext> MinimapMappingContext = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minimap|Input")
	FKey FallbackKey = EKeys::Tab;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minimap|Input")
	int32 MappingPriority = 1;

	/** 켜면 누르는 동안만 보이고, 끄면 누를 때마다 켜고 끈다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minimap|Input")
	bool bHoldToShow = true;

	/** 표시할 위젯. Blueprint로 상속해 색·크기를 바꿀 수 있다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minimap|Widget")
	TSubclassOf<UMinimapWidget> WidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minimap|Widget")
	int32 WidgetZOrder = 50;

private:
	UFUNCTION()
	void HandleControllerChanged(APawn* Pawn, AController* OldController, AController* NewController);

	void SetupForController(APlayerController* PlayerController);
	void TeardownController();

	void OnMinimapStarted(const FInputActionValue& Value);
	void OnMinimapCompleted(const FInputActionValue& Value);

	void TickReveal();
	FVector GetEyeLocation() const;

	TWeakObjectPtr<APlayerController> BoundController;

	UPROPERTY(Transient)
	TObjectPtr<UEnhancedInputComponent> MinimapInputComponent = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> ActiveAction = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> ActiveContext = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UMinimapWidget> Widget = nullptr;

	FTimerHandle RevealTimer;
	FVector LastRevealLocation = FVector(TNumericLimits<float>::Max());
	double LastRevealTime = -1000.0;
	bool bMinimapVisible = false;
};
