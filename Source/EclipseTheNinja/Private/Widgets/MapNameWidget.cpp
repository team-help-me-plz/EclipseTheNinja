#include "Widgets/MapNameWidget.h"

#include "Animation/WidgetAnimation.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "Subsystem/MapNameSubsystem.h"

void UMapNameWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 표시 요청이 오기 전까지는 숨겨 둔다 (보류된 요청이 있으면 아래 Flush에서 바로 보이게 된다)
	SetVisibility(ESlateVisibility::Collapsed);

	UWorld* World = GetWorld();
	if (UMapNameSubsystem* Subsystem = World ? World->GetSubsystem<UMapNameSubsystem>() : nullptr)
	{
		Subsystem->OnShowMapName.AddUniqueDynamic(this, &UMapNameWidget::HandleShowMapName);
		Subsystem->FlushPendingMapName();
	}
}

void UMapNameWidget::NativeDestruct()
{
	UWorld* World = GetWorld();
	if (UMapNameSubsystem* Subsystem = World ? World->GetSubsystem<UMapNameSubsystem>() : nullptr)
	{
		Subsystem->OnShowMapName.RemoveDynamic(this, &UMapNameWidget::HandleShowMapName);
	}

	Super::NativeDestruct();
}

void UMapNameWidget::HandleShowMapName(const FText& InMapName)
{
	if (this->MapNameText)
	{
		this->MapNameText->SetText(InMapName);
	}

	// 입력을 막지 않도록 히트 테스트 없이 보이게 한다
	SetVisibility(ESlateVisibility::HitTestInvisible);

	if (this->MapNameAmin)
	{
		PlayAnimation(this->MapNameAmin);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[UMapNameWidget::HandleShowMapName()] MapNameAmin 애니메이션이 없어 숨기지 않고 계속 표시"));
	}

	BP_OnMapNameShown(InMapName);
}

void UMapNameWidget::OnAnimationFinished_Implementation(const UWidgetAnimation* Animation)
{
	Super::OnAnimationFinished_Implementation(Animation);

	if (Animation == this->MapNameAmin)
	{
		SetVisibility(ESlateVisibility::Collapsed);
	}
}
