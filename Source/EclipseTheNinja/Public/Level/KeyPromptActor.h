// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InputCoreTypes.h"
#include "Widgets/InputGlyphSet.h"
#include "Widgets/KeyPromptWidget.h"
#include "KeyPromptActor.generated.h"

class UArrowComponent;
class UInputAction;
class UInputGlyphSubsystem;
class UMaterialInterface;
class UTexture2D;
class UWidgetComponent;

/**
 * 레벨에 직접 놓는 튜토리얼 키 안내 액터.
 *  - 키보드 / Xbox / PlayStation 키 이미지(DA_InputGlyphs)를 현재 입력 장치에 맞춰 보여 준다
 *  - InputAction을 지정하면 키 설정에 맞는 키를 자동으로 찾고, 지정하지 않으면 KeyboardKey / GamepadKey를 쓴다
 *  - GamepadImage / PlayStationImage를 지정하면 게임패드일 때 이미지 세트 대신 그 이미지를 쓴다(스틱 방향 안내 등)
 *  - CustomText를 쓰거나 키 이미지가 없으면 바탕 없이 글자만 그린다
 *  - 밝기·불투명도를 낮추고 플레이어가 가까이 올 때만 떠오르게 해 주변 장면에 섞이게 한다
 */
UCLASS()
class ECLIPSETHENINJA_API AKeyPromptActor : public AActor
{
    GENERATED_BODY()

public:
    AKeyPromptActor();

    /** 키보드로 보여 줄 키를 바꾸고 바로 반영한다 */
    UFUNCTION(BlueprintCallable, Category = "Key Prompt")
    void SetKeyboardKey(const FKey& InKey);

    /** 현재 입력 장치와 설정(키, 스타일, 밝기)을 위젯에 다시 적용한다 */
    UFUNCTION(BlueprintCallable, Category = "Key Prompt")
    void RefreshPrompt();

protected:
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void Tick(float DeltaSeconds) override;

    /** 지금 보여 줄 입력 장치(플레이 중엔 마지막 입력 장치, 에디터에선 PreviewDevice) */
    EInputGlyphDevice GetDisplayDevice() const;

    /** 장치에 맞는 키를 구한다(InputAction 우선, 없으면 KeyboardKey / GamepadKey) */
    FKey ResolveKey(bool bGamepad) const;

    /** 플레이어와의 거리로 목표 불투명도(0~1)를 구한다 */
    float CalcDistanceAlpha() const;

    /** 밝기와 불투명도를 위젯 컴포넌트 틴트에 적용한다 */
    void ApplyTint(float Alpha);

    /** 사이드뷰 카메라 쪽으로 키 안내 앞면을 돌린다(Yaw만) */
    void FaceCamera();

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USceneComponent> SceneRoot = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UWidgetComponent> KeyWidget = nullptr;

#if WITH_EDITORONLY_DATA
    /** 키 안내 앞면 방향(에디터 전용 표시) */
    UPROPERTY(VisibleAnywhere, Category = "Components")
    TObjectPtr<UArrowComponent> FacingArrow = nullptr;
#endif

    /** 지정하면 키 설정·입력 장치에 맞는 키를 자동으로 찾는다(선택). 못 찾으면 아래 키를 쓴다 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Key Prompt")
    TObjectPtr<UInputAction> InputAction = nullptr;

    /** 키보드·마우스로 플레이할 때 보여 줄 키 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Key Prompt")
    FKey KeyboardKey = EKeys::W;

    /** 게임패드로 플레이할 때 보여 줄 버튼. 비우면 게임패드에서도 키보드 키를 보여 준다 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Key Prompt")
    FKey GamepadKey;

    /**
     * 게임패드로 플레이할 때 키 이미지 세트 대신 보여 줄 이미지(선택).
     * 스틱 방향처럼 이 안내에만 다른 그림을 쓰고 싶을 때 지정한다. 지정하면 GamepadKey·InputAction보다 우선한다
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Key Prompt")
    TObjectPtr<UTexture2D> GamepadImage = nullptr;

    /** PlayStation 패드일 때만 따로 쓸 이미지(선택). 비우면 GamepadImage를 쓴다 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Key Prompt")
    TObjectPtr<UTexture2D> PlayStationImage = nullptr;

    /** 키보드일 때 이미지 대신 보여 줄 글자(선택, 바탕 없는 흰 글자). 게임패드일 때는 버튼 이미지가 우선한다 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Key Prompt")
    FText CustomText;

    /** 에디터 뷰포트에서 미리 볼 입력 장치 */
    UPROPERTY(EditAnywhere, Category = "Key Prompt")
    EInputGlyphDevice PreviewDevice = EInputGlyphDevice::KeyboardMouse;

    /** 생김새(이미지 크기, 글자 폰트·색) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Key Prompt", meta = (ShowOnlyInnerProperties))
    FKeyPromptStyle Style;

    /** 키 이미지 세트 */
    UPROPERTY(EditAnywhere, Category = "Key Prompt", AdvancedDisplay)
    TSoftObjectPtr<UInputGlyphSet> GlyphSet = TSoftObjectPtr<UInputGlyphSet>(FSoftObjectPath(TEXT("/Game/UI/InputGlyphs/DA_InputGlyphs.DA_InputGlyphs")));

    /**
     * 위젯을 몇 배 해상도로 그릴지. 크게 그린 뒤 그만큼 줄여 표시해 글자·이미지가 선명해진다.
     * 키울수록 선명하지만 렌더 타깃 메모리가 늘어난다
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Key Prompt", AdvancedDisplay, meta = (ClampMin = "1.0", ClampMax = "4.0"))
    float RenderScale = 2.0f;

    /**
     * 위젯을 그릴 재질. 비우면 엔진 기본 위젯 재질을 쓴다.
     * 기본 재질은 속도(velocity)를 기록하지 않아 카메라가 움직일 때 TSR·모션 블러로 번져 보이므로, Output Velocity를 켠 재질을 지정한다
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Key Prompt", AdvancedDisplay)
    TObjectPtr<UMaterialInterface> WidgetMaterial = nullptr;

    /** 전체 밝기. 어두운 밤 장면에서는 1보다 낮춰 하얀 키가 튀지 않게 한다 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Key Prompt|Blend", meta = (ClampMin = "0.0", ClampMax = "2.0"))
    float Brightness = 0.8f;

    /** 최대 불투명도 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Key Prompt|Blend", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float Opacity = 0.9f;

    /** 켜면 플레이 중 앞면을 카메라 쪽으로 돌린다. 끄면 배치한 회전 그대로 둔다(벽·바닥에 붙일 때) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Key Prompt|Blend")
    bool bFaceCamera = true;

    /** 플레이어가 이 거리(cm) 안에 들어오면 완전히 보인다. 0이면 거리와 상관없이 항상 보인다 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Key Prompt|Blend", meta = (ClampMin = "0.0", Units = "cm"))
    float ShowDistance = 0.0f;

    /** ShowDistance 밖에서 완전히 사라지기까지의 거리(cm) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Key Prompt|Blend", meta = (ClampMin = "1.0", Units = "cm", EditCondition = "ShowDistance > 0"))
    float FadeDistance = 200.0f;

    /** 나타나고 사라지는 속도. 클수록 빠르다 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Key Prompt|Blend", meta = (ClampMin = "0.1", EditCondition = "ShowDistance > 0"))
    float FadeSpeed = 4.0f;

    /** 위아래로 떠다니는 폭(cm). 0이면 움직이지 않는다 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Key Prompt|Blend", meta = (ClampMin = "0.0", Units = "cm"))
    float BobHeight = 0.0f;

    /** 떠다니는 주기(초) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Key Prompt|Blend", meta = (ClampMin = "0.1", Units = "s", EditCondition = "BobHeight > 0"))
    float BobPeriod = 2.0f;

private:
    /** 입력 장치가 바뀌면 키 이미지를 다시 고른다 */
    void HandleDeviceChanged();

    /** 키 설정이 바뀌면(키 재설정, 매핑 컨텍스트 교체) 키 이미지를 다시 고른다 */
    UFUNCTION()
    void HandleMappingsRebuilt();

    UPROPERTY(Transient)
    TObjectPtr<UInputGlyphSet> LoadedGlyphSet = nullptr;

    TWeakObjectPtr<UInputGlyphSubsystem> DeviceSubsystem;

    FDelegateHandle DeviceChangedHandle;

    float CurrentAlpha = 1.0f;

    float BobTime = 0.0f;

    FVector BaseWidgetLocation = FVector::ZeroVector;
};
