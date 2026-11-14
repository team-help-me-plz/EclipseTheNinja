#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Fonts/SlateFontInfo.h"
#include "KeyPromptWidget.generated.h"

class UImage;
class UOverlay;
class USizeBox;
class UTextBlock;
class UTexture2D;

/** 레벨에 놓는 키 안내의 생김새 */
USTRUCT(BlueprintType)
struct FKeyPromptStyle
{
	GENERATED_BODY()

	/** 키 이미지 높이(cm). 너비는 이미지 비율을 따른다 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Key Prompt|Image", meta = (ClampMin = "8.0", Units = "cm"))
	float ImageHeight = 64.0f;

	/** 키 이미지 색 보정(흰색이면 원본 그대로) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Key Prompt|Image")
	FLinearColor ImageTint = FLinearColor::White;

	/** 글자 폰트와 크기(크기는 cm 기준). 폰트 에셋을 비우면 기본 폰트(Roboto)에 크기·굵기만 적용한다 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Key Prompt|Text")
	FSlateFontInfo Font;

	/** 글자 색 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Key Prompt|Text")
	FLinearColor TextColor = FLinearColor::White;

	FKeyPromptStyle();
};

/**
 * 키 안내 위젯.
 *  - 키 이미지가 있으면 이미지만 그린다(바탕 없이, 이미지의 투명 영역은 그대로 비친다)
 *  - 직접 쓴 문구나 이미지가 없는 키는 바탕 없이 글자만 그린다
 *  - 위젯 트리를 코드로 만들어 위젯 블루프린트 없이 AKeyPromptActor의 위젯 컴포넌트에 바로 쓴다
 */
UCLASS()
class ECLIPSETHENINJA_API UKeyPromptWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual bool Initialize() override;

	/** 키 이미지를 보여 준다. 높이는 스타일의 ImageHeight, 너비는 이미지 비율을 따른다 */
	UFUNCTION(BlueprintCallable, Category = "Key Prompt")
	void ShowImage(UTexture2D* InTexture);

	/** 바탕 없이 글자만 보여 준다 */
	UFUNCTION(BlueprintCallable, Category = "Key Prompt")
	void ShowLabel(const FText& InLabel);

	/**
	 * 생김새를 적용한다.
	 * RenderScale배 큰 해상도로 그려 두고 액터가 위젯 컴포넌트를 그만큼 줄여 표시해 선명도를 높인다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Key Prompt")
	void ApplyStyle(const FKeyPromptStyle& InStyle, float InRenderScale = 1.0f);

private:
	/** 현재 이미지와 스타일로 이미지 상자 크기를 맞춘다 */
	void UpdateImageSize();

	UPROPERTY(Transient)
	TObjectPtr<UOverlay> RootOverlay;

	UPROPERTY(Transient)
	TObjectPtr<USizeBox> ImageSize;

	UPROPERTY(Transient)
	TObjectPtr<UImage> KeyImage;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> KeyText;

	/** 폰트 에셋을 비워 둔 스타일에 쓸 텍스트 블록 기본 폰트 */
	UPROPERTY(Transient)
	TObjectPtr<const UObject> DefaultFontObject;

	FKeyPromptStyle CurrentStyle;

	float RenderScale = 1.0f;
};
