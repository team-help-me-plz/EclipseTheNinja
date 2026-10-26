#include "Subsystem/NoiseSubsystem.h"
#include "Noise/NoiseRingActor.h"
#include "Noise/NoiseSettings.h"
#include "ObjectPool/ObjectPoolSubsystem.h"
#include "Clone/CloneCharacter.h"

#include "Engine/DataTable.h"

void UNoiseSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const UNoiseSettings* Settings = GetDefault<UNoiseSettings>();
	this->NoiseTable = Settings->NoiseTable.LoadSynchronous();
	this->NoiseRingClass = Settings->NoiseRingClass.LoadSynchronous();

	if (!this->NoiseTable)
	{
		UE_LOG(
			LogTemp, 
			Warning, 
			TEXT("[UNoiseSubsystem::Initialize] Project Settings > Game > Noise에 NoiseTable이 지정되지 않았습니다")
		);
	}
	if (!this->NoiseRingClass)
	{
		UE_LOG(
			LogTemp, 
			Warning, 
			TEXT("[UNoiseSubsystem::Initialize] Project Settings > Game > Noise에 NoiseRingClass가 지정되지 않았습니다")
		);
	}
}

void UNoiseSubsystem::MakeSomeNoise(const UObject* InObject, ENoiseType InType, const FVector& InLocation, AActor* InInstigator, float InScale)
{
	UWorld*				TargetWorld = InObject ? InObject->GetWorld() : nullptr;
	UNoiseSubsystem*	TargetSubSystem = TargetWorld ? TargetWorld->GetSubsystem<UNoiseSubsystem>() : nullptr;
	if (!TargetSubSystem)
	{
		return ;
	}
	TargetSubSystem->ReportNoise(InType, InLocation, InInstigator, InScale);
}

void UNoiseSubsystem::ReportNoise(ENoiseType InType, const FVector& InLocation, AActor* InInstigator, float InScale)
{
	if (!IsPlayerNoise(InInstigator))
	{
		return ;
	}
	if (!this->NoiseTable)
	{
		return ;
	}

	const FNoiseTableRow* TargetRow = this->NoiseTable->FindRow<FNoiseTableRow>(FNoiseTableRow::GetRowName(InType), TEXT("UNoiseSubsystem"));
	if (!TargetRow)
	{
		UE_LOG(LogTemp, Warning, TEXT("[UNoiseSubsystem::ReportNoise] DT_Noise에 %s 행이 없습니다"), *UEnum::GetValueAsString(InType));
		return ;
	}

	// 링 크기와 판정이 어긋나지 않도록 배율까지 곱한 최종 값을 쓴다.
	const float Radius = TargetRow->Radius * InScale;
	const float Strength = TargetRow->Strength * InScale;
	if (Radius <= 0.f)
	{
		return ;
	}

	// TODO: 반경 안의 적에게 NotifyNoise

	this->SpawnNoiseRing(InLocation, Radius, Strength);
	this->OnNoiseReported.Broadcast(InLocation, Radius, Strength);
}

void UNoiseSubsystem::SpawnNoiseRing(const FVector& InLocation, const float InRadius, const float InStrength)
{
	UObjectPoolSubsystem* Pool = GetWorld()->GetSubsystem<UObjectPoolSubsystem>();
	if (!Pool || !this->NoiseRingClass)
	{
		return ;
	}

	// 풀이 등록되지 않았으면 일반 스폰되고, 끝날 때 Release에서 파괴된다.
	if (ANoiseRingActor* NoiseRing = Pool->SpawnActor<ANoiseRingActor>(this->NoiseRingClass, FTransform(InLocation)))
	{
		NoiseRing->Play(InLocation, InRadius, InStrength);
	}
}

bool UNoiseSubsystem::IsPlayerNoise(const AActor* InInstigator)
{
	const APawn* Pawn = Cast<APawn>(InInstigator);
	return (IsValid(Pawn) && !Pawn->IsActorBeingDestroyed()
		&& (Pawn->IsPlayerControlled() || Pawn->IsA<ACloneCharacter>()));
}
