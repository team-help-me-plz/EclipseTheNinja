


#include "Widgets/AbilityCooldownContainerWidget.h"
#include "Widgets/AbilityCooldownWidget.h"
#include "Widgets/ElementalFireCooldownWidget.h"

void	UAbilityCooldownContainerWidget::InitWidget(APawn* InPawn)
{
	if (!InPawn)
	{
		return ;
	}
	if (this->CloneTechCooldown)
	{
		this->CloneTechCooldown->InitWidget(InPawn);
	}
	if (this->KunaiCooldown)
	{
		this->KunaiCooldown->InitWidget(InPawn);
	}
	if (this->ElementalFireCooldown)
	{
		this->ElementalFireCooldown->InitWidget(InPawn);
	}
}
