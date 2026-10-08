


#include "Widgets/OverheadWidget.h"
#include "Widgets/StatBarWidget.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "GAS/StatAttributeSet.h"

void UOverheadWidget::InitWidget(APawn* InPawn)
{
	if (!InPawn) return;

	// 어떤 경로에서 이미 ASC가 설정 되었다면
	if (ASC.IsValid())
	{
		UnbindASC();
	}

	IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(InPawn);
	if (!ASI) return;

	UAbilitySystemComponent* AbilitySystemComp = ASI->GetAbilitySystemComponent();
	if (!AbilitySystemComp) return;
	ASC = AbilitySystemComp;

	// 어트리뷰트 델리게이트에 바인딩
	FOnGameplayAttributeValueChange& HealthChange = ASC->GetGameplayAttributeValueChangeDelegate(UStatAttributeSet::GetHealthAttribute());
	HealthChange.AddUObject(this, &UOverheadWidget::UpdateHealth);

	FOnGameplayAttributeValueChange& MaxHealthChange = ASC->GetGameplayAttributeValueChangeDelegate(UStatAttributeSet::GetMaxHealthAttribute());
	MaxHealthChange.AddUObject(this, &UOverheadWidget::UpdateMaxHealth);


	// UI 초기값 세팅
	// 체력 세팅
	bool bFound = false;
	const float HealthTempCurrent = ASC->GetGameplayAttributeValue(UStatAttributeSet::GetHealthAttribute(), bFound);
	float CurrentHealth = bFound ? HealthTempCurrent : 0.0f;	// 못찾았으면 0

	bFound = false;
	const float HealthTempMax = ASC->GetGameplayAttributeValue(UStatAttributeSet::GetMaxHealthAttribute(), bFound);
	float MaxHealth = bFound ? HealthTempMax : 100.0f;	// 못찾았으면 100


	UpdateHealth(CurrentHealth, MaxHealth);
}

void UOverheadWidget::UpdateHealth(float CurrentVal, float MaxVal)
{
	if (HealthBar)
	{
		HealthBar->UpdateValue(CurrentVal, MaxVal);
	}
}

void UOverheadWidget::NativeDestruct()
{
	UnbindASC();
	Super::NativeDestruct();
}

void UOverheadWidget::UpdateHealth(const FOnAttributeChangeData & InData)
{
	// 최대 체력은 뽑아서 사용
	float MaxHealth = 100.f;
	bool bFound = false;
	const float TempMax = ASC->GetGameplayAttributeValue(UStatAttributeSet::GetMaxHealthAttribute(), bFound);
	if (bFound) MaxHealth = TempMax;

	UpdateHealth(InData.NewValue, MaxHealth);
}

void UOverheadWidget::UpdateMaxHealth(const FOnAttributeChangeData & InData)
{
	// 현재 체력은 뽑아서 사용
	float CurrenHealth = .0f;
	bool bFound = false;
	const float TempCurrent = ASC->GetGameplayAttributeValue(UStatAttributeSet::GetHealthAttribute(), bFound);
	if (bFound) CurrenHealth = TempCurrent;

	UpdateHealth(CurrenHealth, InData.NewValue);
}

void UOverheadWidget::UnbindASC()
{
	UAbilitySystemComponent* CurrASC = ASC.Get();
	if (!CurrASC) return;

	// 델리게이트 해제
	CurrASC->GetGameplayAttributeValueChangeDelegate(UStatAttributeSet::GetHealthAttribute()).RemoveAll(this);
	CurrASC->GetGameplayAttributeValueChangeDelegate(UStatAttributeSet::GetMaxHealthAttribute()).RemoveAll(this);

	// ASC nullptr로 밀어주기
	ASC.Reset();
}
