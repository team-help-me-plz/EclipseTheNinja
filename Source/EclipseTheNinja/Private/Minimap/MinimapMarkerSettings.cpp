#include "Minimap/MinimapMarkerSettings.h"

#include "Checkpoint/Checkpoint.h"
#include "Item/AbilityUnlockPickup.h"
#include "Item/HealPickup.h"
#include "PuzzelObject/BaseDoor.h"
#include "PuzzelObject/BreakablePlatform.h"
#include "PuzzelObject/BurnableVine.h"
#include "PuzzelObject/FallingObject.h"
#include "PuzzelObject/InteractSwitch.h"
#include "PuzzelObject/LeverSwitch.h"
#include "PuzzelObject/MovingPlatform.h"
#include "PuzzelObject/PressureSwitch.h"
#include "PuzzelObject/VentPassage.h"
#include "Trap/ArrowTrapActor.h"

#include "HAL/IConsoleManager.h"

namespace
{
	FMinimapMarkerRule MakeRule(UClass* InClass, EMinimapMarkerShape InShape, const FLinearColor& InColor, float InSize)
	{
		FMinimapMarkerRule Rule;
		Rule.ActorClass = InClass;
		Rule.Style.Shape = InShape;
		Rule.Style.Color = InColor;
		Rule.Style.Size = InSize;
		return Rule;
	}

	// 기본 규칙. 생성자와 Eclipse.Minimap.ResetMarkerRules가 같이 쓴다
	void BuildDefaultRules(TArray<FMinimapMarkerRule>& OutRules)
	{
		const FLinearColor Yellow(1.f, 0.82f, 0.2f);
		const FLinearColor Orange(1.f, 0.5f, 0.15f);
		const FLinearColor Red(0.95f, 0.22f, 0.18f);
		const FLinearColor Cyan(0.3f, 0.85f, 1.f);
		const FLinearColor Brown(0.72f, 0.48f, 0.28f);
		const FLinearColor Green(0.35f, 0.9f, 0.4f);
		const FLinearColor Purple(0.7f, 0.45f, 1.f);
		const FLinearColor Pink(1.f, 0.45f, 0.65f);

		OutRules.Reset();
		OutRules.Add(MakeRule(ALeverSwitch::StaticClass(), EMinimapMarkerShape::Diamond, Yellow, 10.f));
		OutRules.Add(MakeRule(APressureSwitch::StaticClass(), EMinimapMarkerShape::Square, Yellow, 7.f));
		OutRules.Add(MakeRule(AInteractSwitch::StaticClass(), EMinimapMarkerShape::Diamond, Yellow, 9.f));

		// 문: 통로를 막는 막대. 열리면 초록 테두리
		FMinimapMarkerRule& Door = OutRules.Add_GetRef(MakeRule(ABaseDoor::StaticClass(), EMinimapMarkerShape::ActorBounds, Brown, 9.f));
		Door.Style.StateProperty = TEXT("bIsOpen");
		Door.Style.StateColor = Green;
		Door.Style.bDimWhenMeshHidden = false;

		// 움직이는 발판: 이동 경로만
		FMinimapMarkerRule& Moving = OutRules.Add_GetRef(MakeRule(AMovingPlatform::StaticClass(), EMinimapMarkerShape::PathOnly, Cyan, 9.f));
		Moving.Style.PathProperty = TEXT("TargetPoint");
		Moving.Style.bDimWhenMeshHidden = false;

		OutRules.Add(MakeRule(ABreakablePlatform::StaticClass(), EMinimapMarkerShape::Square, Orange, 8.f));
		// 덩굴은 지도에서 벽으로 그리지 않고 마커로 보여 준다. 다 타면 회색
		FMinimapMarkerRule& Vine = OutRules.Add_GetRef(MakeRule(ABurnableVine::StaticClass(), EMinimapMarkerShape::Square, FLinearColor(0.35f, 0.7f, 0.25f), 9.f));
		Vine.Style.StateProperty = TEXT("bIsBurned");
		Vine.Style.StateColor = FLinearColor(0.4f, 0.4f, 0.4f, 0.6f);
		Vine.Style.bDimWhenMeshHidden = false;

		// 낙하물: 떨어지기 전에는 빨간 마름모, 착지하면 그 자리를 땅으로
		FMinimapMarkerRule& Falling = OutRules.Add_GetRef(MakeRule(AFallingObject::StaticClass(), EMinimapMarkerShape::Diamond, Red, 9.f));
		Falling.Style.StateProperty = TEXT("bIsLanded");
		Falling.Style.bDrawAsGroundWhenStateOn = true;
		Falling.Style.bDimWhenMeshHidden = false;
		OutRules.Add(MakeRule(AArrowTrapActor::StaticClass(), EMinimapMarkerShape::Diamond, Red, 8.f));
		OutRules.Add(MakeRule(ACheckpoint::StaticClass(), EMinimapMarkerShape::Circle, Green, 10.f));
		OutRules.Add(MakeRule(AVentPassage::StaticClass(), EMinimapMarkerShape::Circle, Purple, 9.f));
		OutRules.Add(MakeRule(AAbilityUnlockPickup::StaticClass(), EMinimapMarkerShape::Circle, Yellow, 10.f));
		OutRules.Add(MakeRule(AHealPickup::StaticClass(), EMinimapMarkerShape::Circle, Pink, 8.f));
	}
}

UMinimapMarkerSettings::UMinimapMarkerSettings()
{
	// 프로젝트 설정에서 바꾸면 DefaultGame.ini에 저장되어 이 기본값보다 우선한다
	BuildDefaultRules(this->Rules);
}

// 마커 규칙을 코드의 기본값으로 되돌린다 (메모리에서만. 저장하려면 프로젝트 설정에서 Set as Default)
static FAutoConsoleCommand GMinimapResetMarkerRulesCommand(
	TEXT("Eclipse.Minimap.ResetMarkerRules"),
	TEXT("미니맵 마커 규칙을 코드 기본값으로 되돌린다."),
	FConsoleCommandDelegate::CreateLambda([]()
	{
		BuildDefaultRules(GetMutableDefault<UMinimapMarkerSettings>()->Rules);
	}));
