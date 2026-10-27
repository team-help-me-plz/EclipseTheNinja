
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/CoreStyle.h"
#include "MainMenuWidget.generated.h"

class UButton;
class UImage;
class UTextBlock;
class UVerticalBox;
class USettingsWidget;

/**
 * 메인 화면 위젯 (초안)
 * - WBP 없이 C++ 클래스만으로도 기본 레이아웃을 구성한다.
 * - 이 클래스를 부모로 하는 WBP에 같은 이름의 위젯을 배치하면 C++ 레이아웃 대신 WBP 레이아웃을 사용한다.
 */
UCLASS()
class ECLIPSETHENINJA_API UMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual bool	Initialize() override;

protected:
	virtual void	NativeConstruct() override;

	UFUNCTION()
	void	HandleStartClicked();

	UFUNCTION()
	void	HandleContinueClicked();

	UFUNCTION()
	void	HandleSettingsClicked();

	UFUNCTION()
	void	HandleQuitClicked();

	UFUNCTION()
	void	HandleSettingsClosed();

	// 게임 시작 시 이동할 레벨 (에디터에서 지정)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MainMenu|Level")
	TSoftObjectPtr<UWorld>	StartLevel = nullptr;

	// 설정 창 클래스 (비워두면 USettingsWidget 기본 레이아웃 사용)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MainMenu|Settings")
	TSubclassOf<USettingsWidget>	SettingsWidgetClass = nullptr;

	// 배경 텍스처 (비워두면 단색 배경 + 일식 링)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MainMenu|Style")
	TObjectPtr<UTexture2D>	BackgroundTexture = nullptr;

	// 배경 텍스처 위에 덮는 어두운 막의 불투명도 (버튼 가독성용)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MainMenu|Style", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float	BackgroundDimOpacity = 0.35f;

	// 배경 이미지에 타이틀이 그려져 있으면 끈다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MainMenu|Layout")
	bool	bShowTitle = true;

	// 타이틀 상단 중앙 위치 (화면 비율 0~1)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MainMenu|Layout")
	FVector2D	TitleAnchor = FVector2D(0.5f, 0.13f);

	// 버튼 목록 상단 중앙 위치 (화면 비율 0~1)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MainMenu|Layout")
	FVector2D	MenuAnchor = FVector2D(0.5f, 0.45f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MainMenu|Style")
	FText	TitleText = FText::FromString(TEXT("ECLIPSE"));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MainMenu|Style")
	FText	SubtitleText = FText::FromString(TEXT("THE NINJA"));

	// 타이틀 폰트 (세리프 폰트 에셋을 지정하면 키 아트 느낌에 가까워짐)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MainMenu|Style")
	FSlateFontInfo	TitleFont = FCoreStyle::GetDefaultFontStyle("Bold", 96);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MainMenu|Style")
	FSlateFontInfo	SubtitleFont = FCoreStyle::GetDefaultFontStyle("Regular", 30);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MainMenu|Style")
	FLinearColor	BackgroundColor = FLinearColor(0.006f, 0.007f, 0.011f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MainMenu|Style")
	FLinearColor	TitleColor = FLinearColor(0.72f, 0.70f, 0.66f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MainMenu|Style")
	FLinearColor	AccentColor = FLinearColor(0.6f, 0.015f, 0.025f, 1.f);

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage>		BackgroundImage = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage>		BackgroundDimImage = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock>	TitleTextBlock = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock>	SubtitleTextBlock = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton>		StartButton = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton>		ContinueButton = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton>		SettingsButton = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton>		QuitButton = nullptr;

	// 타이틀 + 부제를 감싸는 컨테이너 (bShowTitle로 표시 여부 제어)
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UWidget>		TitleContainer = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<USettingsWidget>	SettingsWidget = nullptr;

private:
	void		BuildDefaultLayout();
	UButton*	MakeMenuButton(UVerticalBox* InParent, const FText& InLabel);
};
