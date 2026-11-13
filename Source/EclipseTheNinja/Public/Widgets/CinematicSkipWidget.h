#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CinematicSkipWidget.generated.h"

class UImage;
class UInputAction;
class UInputMappingContext;
class UEnhancedInputComponent;
class UMaterialInstanceDynamic;
struct FInputActionInstance;

/**
 * 컷신 건너뛰기 안내 위젯(WBP_CinematicSkip의 부모). 건너뛰기 입력·시간·표시를 모두 이 위젯이 맡는다.
 *  - SetSkipAvailable(true)를 받은 동안만 동작한다. 그 밖에는 화면에 뜨지 않고 키 입력도 받지 않는다 (ESC는 일시정지 메뉴가 받는다)
 *  - 건너뛸 수 있는 동안 아무 키나 한 번 누르면 뜨고, 건너뛸 수 없게 될 때까지 계속 떠 있다
 *  - SkipAction 키(ESC)를 누르고 있으면 테두리가 차고, SkipHoldTime초가 차면 컨트롤러에 건너뛰기를 요청한다
 *    중간에 떼면 테두리만 비운다
 *  - 건너뛸 수 있는 동안에는 ESC가 일시정지 메뉴를 열지 않게 막는다
 */
UCLASS()
class ECLIPSETHENINJA_API UCinematicSkipWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UCinematicSkipWidget(const FObjectInitializer& ObjectInitializer);

    // 건너뛸 수 있는 컷신이 시작·끝날 때 부른다. 켜면 입력을 받기 시작하고, 끄면 입력을 놓고 화면에서 내린다
    UFUNCTION(BlueprintCallable, Category = "Cinematic Skip")
    void SetSkipAvailable(bool bAvailable);

    UFUNCTION(BlueprintPure, Category = "Cinematic Skip")
    bool IsSkipAvailable() const { return this->bSkipAvailable; }

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    // 진행 테두리를 그리는 이미지. 브러시는 M_UI_CinematicSkipBorder
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UImage> BorderImage;

    // 건너뛰기 입력 액션과 키를 정하는 매핑 컨텍스트 (기본 IA_SkipCinematic / IMC_SkipCinematic: ESC·게임패드 메뉴 버튼)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cinematic Skip|Input")
    TObjectPtr<UInputAction> SkipAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cinematic Skip|Input")
    TObjectPtr<UInputMappingContext> SkipMappingContext;

    // 게임플레이 IMC의 같은 키(일시정지)보다 먼저 처리되도록 넣는 우선순위
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cinematic Skip|Input")
    int32 SkipMappingPriority = 1000;

    // 이 시간(초) 동안 계속 누르면 건너뛴다
    // WBP_CinematicSkip의 클래스 디폴트나, WBP_HUD에 배치한 CinematicSkipWidget 디테일에서 정한다
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cinematic Skip|Timing", meta = (ClampMin = "0.05", Units = "s"))
    float SkipHoldTime = 0.5f;

    // 테두리 머티리얼의 진행률(0~1) 파라미터 이름
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cinematic Skip")
    FName ProgressParameterName = TEXT("Progress");

private:
    void BindSkipInput();
    void UnbindSkipInput();

    // 건너뛰기 키를 누르고 있는 동안 매 프레임 호출된다
    void HandleSkipHeld(const FInputActionInstance& Instance);

    // 건너뛰기 키를 SkipHoldTime 전에 뗐을 때
    void HandleSkipReleased();

    // 아무 키나 눌렀을 때 (건너뛰기 키 포함)
    void HandleAnyKey();

    void ShowPrompt();

    void HidePrompt();

    void SetProgress(float Progress);

    bool bSkipAvailable = false;
    bool bHolding = false;
    bool bSkipRequested = false;

    // 건너뛸 수 있는 동안 컨트롤러 입력 스택에 올리는 이 위젯 전용 입력 컴포넌트
    UPROPERTY(Transient)
    TObjectPtr<UEnhancedInputComponent> SkipInputComponent;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> ProgressMaterial;
};
