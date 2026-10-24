#include "Component/AssassinationDropComponent.h"
#include "Character/Enemy.h"
#include "Checkpoint/CheckpointSubsystem.h"
#include "ObjectPool/ObjectPoolSubsystem.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"

UAssassinationDropComponent::UAssassinationDropComponent()
{
	// 적의 사망 알림으로만 동작하므로 틱이 필요 없다.
	PrimaryComponentTick.bCanEverTick = false;
}

void UAssassinationDropComponent::BeginPlay()
{
	Super::BeginPlay();

	AEnemy* Enemy = Cast<AEnemy>(GetOwner());
	if (!Enemy)
	{
		UE_LOG(LogTemp, Warning, TEXT("[UAssassinationDropComponent::BeginPlay()] %s 는 AEnemy가 아니라 동작하지 않습니다."),
			*GetNameSafe(GetOwner()));
		return;
	}

	Enemy->OnDied.AddUObject(this, &UAssassinationDropComponent::HandleEnemyDied);
	Enemy->OnDeathFinished.AddUObject(this, &UAssassinationDropComponent::HandleEnemyDeathFinished);

	if (UCheckpointSubsystem* CheckpointSubsystem = GetWorld()->GetSubsystem<UCheckpointSubsystem>())
	{
		CheckpointSubsystem->OnRespawned.AddUniqueDynamic(this, &UAssassinationDropComponent::HandleCheckpointRespawned);
	}
}

void UAssassinationDropComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AEnemy* Enemy = Cast<AEnemy>(GetOwner()))
	{
		Enemy->OnDied.RemoveAll(this);
		Enemy->OnDeathFinished.RemoveAll(this);
	}

	if (UCheckpointSubsystem* CheckpointSubsystem = GetWorld()->GetSubsystem<UCheckpointSubsystem>())
	{
		CheckpointSubsystem->OnRespawned.RemoveDynamic(this, &UAssassinationDropComponent::HandleCheckpointRespawned);
	}

	bPendingDrop = false;

	Super::EndPlay(EndPlayReason);
}

void UAssassinationDropComponent::HandleEnemyDied(AEnemy* Enemy, bool bByAssassination)
{
	// 사망할 때마다 덮어쓰므로, 리셋/풀 반환으로 사라지기 전에 되살아난 경우에도 이전 값이 남지 않는다.
	bPendingDrop = bByAssassination && DropActorClass != nullptr;
}

void UAssassinationDropComponent::HandleEnemyDeathFinished(AEnemy* Enemy)
{
	if (!bPendingDrop || !IsValid(Enemy))
	{
		return;
	}

	bPendingDrop = false;

	// 암살 연출이 메시를 옮겨 둘 수 있으므로 캡슐이 아닌 메시 위치(시체 위치)를 기준으로 한다.
	SpawnDropActor(Enemy->GetMesh()->GetComponentLocation() + SpawnOffset);
}

void UAssassinationDropComponent::HandleCheckpointRespawned(ECheckpointId CheckpointId)
{
	bPendingDrop = false;
}

void UAssassinationDropComponent::SpawnDropActor(const FVector& SpawnLocation)
{
	UWorld* World = GetWorld();
	const FTransform SpawnTransform(FRotator::ZeroRotator, SpawnLocation);

	// 풀이 등록되지 않은 클래스면 서브시스템이 일반 스폰으로 처리한다.
	AActor* DropActor = nullptr;
	if (UObjectPoolSubsystem* PoolSubsystem = World->GetSubsystem<UObjectPoolSubsystem>())
	{
		DropActor = PoolSubsystem->SpawnActor(DropActorClass, SpawnTransform, GetOwner());
	}
	else
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = GetOwner();
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		DropActor = World->SpawnActor<AActor>(DropActorClass, SpawnTransform, SpawnParams);
	}

	if (!DropActor)
	{
		return;
	}

	// 체크포인트 리스폰 시 맵에 남은 드롭 액터를 풀에 반환한다.
	if (UCheckpointSubsystem* CheckpointSubsystem = World->GetSubsystem<UCheckpointSubsystem>())
	{
		CheckpointSubsystem->RegisterRuntimeSpawnedActor(DropActor);
	}
}
