#include "Level/EnemyFloorVolume.h"

#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "Subsystem/EnemyFloorSubsystem.h"

AEnemyFloorVolume::AEnemyFloorVolume()
{
	PrimaryActorTick.bCanEverTick = false;

	// 판정은 위치 계산으로만 하므로 충돌과 오버랩을 모두 끈다.
	FloorBox = CreateDefaultSubobject<UBoxComponent>(TEXT("FloorBox"));
	FloorBox->SetBoxExtent(FVector(1000.0f, 200.0f, 200.0f));
	FloorBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FloorBox->SetGenerateOverlapEvents(false);
	FloorBox->SetCanEverAffectNavigation(false);
	FloorBox->SetHiddenInGame(true);
	FloorBox->ShapeColor = FColor(80, 200, 255);
	RootComponent = FloorBox;
}

bool AEnemyFloorVolume::ContainsLocation(const FVector& Location) const
{
	if (!FloorBox)
	{
		return false;
	}

	// 회전·배율을 포함한 박스 기준 좌표로 바꾼 뒤 배율 전 범위와 비교한다.
	const FVector LocalLocation = FloorBox->GetComponentTransform().InverseTransformPosition(Location);
	const FVector Extent = FloorBox->GetUnscaledBoxExtent();

	return FMath::Abs(LocalLocation.X) <= Extent.X
		&& FMath::Abs(LocalLocation.Y) <= Extent.Y
		&& FMath::Abs(LocalLocation.Z) <= Extent.Z;
}

double AEnemyFloorVolume::GetBoxVolume() const
{
	if (!FloorBox)
	{
		return 0.0;
	}

	const FVector Size = FloorBox->GetScaledBoxExtent() * 2.0f;
	return static_cast<double>(Size.X) * Size.Y * Size.Z;
}

void AEnemyFloorVolume::BeginPlay()
{
	Super::BeginPlay();

	// 기믹 방처럼 숨겼다 다시 보이는 레벨에서도 BeginPlay/EndPlay 짝으로 등록 상태를 맞춘다.
	if (UEnemyFloorSubsystem* FloorSubsystem = GetWorld()->GetSubsystem<UEnemyFloorSubsystem>())
	{
		FloorSubsystem->RegisterVolume(this);
	}
}

void AEnemyFloorVolume::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (UEnemyFloorSubsystem* FloorSubsystem = World->GetSubsystem<UEnemyFloorSubsystem>())
		{
			FloorSubsystem->UnregisterVolume(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}
