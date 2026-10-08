// Fill out your copyright notice in the Description page of Project Settings.


#include "Framework/PCCloneTech.h"
#include "Ability/GATargetingAbility.h"
#include "ProjComponent/TargetingComponent.h"

#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
//#include "Abilities/GameplayAbility.h"

void APCCloneTech::StartTargeting(UTargetingComponent* InTargetingComponent, UGATargetingAbility* InGameplayAbility)
{
	if (this->bIsTargeting() || !InTargetingComponent || !InGameplayAbility)
		return ;
	this->SavedTargetingComponent = InTargetingComponent;
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		if (this->TargetingMappingContext)
		{
			// 일단 아무런 높은 값으로 설정
			Subsystem->AddMappingContext(this->TargetingMappingContext, 100);
		}
	}
	// 슬로우모션 넣기
	InTargetingComponent->BeginTargeting();
}
