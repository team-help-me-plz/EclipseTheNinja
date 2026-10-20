


#include "Widgets/AbilityCooldownContainerWidget.h"
#include "Widgets/AbilityCooldownWidget.h"

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
}
