


#include "Component/AssassinationTargetingComponent.h"
#include "Character/Enemy.h"
#include "Component/AssassinateTargetComponent.h"
#include "TimerManager.h"
#include "Character/PlayerCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"

// Sets default values for this component's properties
UAssassinationTargetingComponent::UAssassinationTargetingComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;

	// ...
}


// Called when the game starts
void UAssassinationTargetingComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...

}


void UAssassinationTargetingComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorld()->GetTimerManager().ClearTimer(SearchCandidateTimer);
	if (IsValid(CurrentTarget))
	{
		CurrentTarget->SetTargetHighlighted(false);
	}

	CurrentTarget = nullptr;
	TargetCandidate.Empty();
	Super::EndPlay(EndPlayReason);
}

// Called every frame
void UAssassinationTargetingComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

void UAssassinationTargetingComponent::SelectCandidate()
{
	APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(GetOwner());
	if (!PlayerCharacter || !PlayerCharacter->GetCharacterMovement()->IsMovingOnGround())
	{
		if (CurrentTarget)
		{
			CurrentTarget->SetTargetHighlighted(false);
			CurrentTarget = nullptr;
		}
		return;
	}
	if (PlayerCharacter->IsAssassinating()) return;

	UAssassinateTargetComponent* MinDistEnemy = nullptr;
	for (auto& Enemy : TargetCandidate)
	{
		if (!IsValid(Enemy)) continue;
		if (!Enemy->CanAssassinate()) continue;
		// 등 뒤 위치까지 벽에 막혀 있으면 실행해도 취소되므로 테두리도 표시하지 않는다.
		if (!Enemy->IsApproachClear(PlayerCharacter)) continue;
		if (!MinDistEnemy)
		{
			MinDistEnemy = Enemy;
		}
		else
		{
			AActor* OwnerActor = GetOwner();
			if (!IsValid(OwnerActor)) return;
			FVector PlayerLocation = OwnerActor->GetActorLocation();

			AActor* EnemyOwnerActor = Enemy->GetOwner();
			if (!IsValid(EnemyOwnerActor)) return;
			FVector EnemyLocation = EnemyOwnerActor->GetActorLocation();

			AActor* MinDistOwnerActor = MinDistEnemy->GetOwner();
			if (!IsValid(MinDistOwnerActor)) return;
			FVector MinDistEnemyLocation = MinDistOwnerActor->GetActorLocation();

			const float DistFromNewOne = (PlayerLocation - EnemyLocation).SquaredLength();
			const float DistFromMinDistOne = (PlayerLocation - MinDistEnemyLocation).SquaredLength();

			if (DistFromNewOne < DistFromMinDistOne)
			{
				MinDistEnemy = Enemy;
			}
		}
	}

	// 선정 가능한 후보가 없으면 nullptr로 갱신해 기존 테두리도 해제한다.
	if (CurrentTarget == MinDistEnemy) return;

	if (IsValid(CurrentTarget))
	{
		// 기존 타겟의 테두리 제거
		CurrentTarget->SetTargetHighlighted(false);
	}

	CurrentTarget = MinDistEnemy;

	if (IsValid(CurrentTarget))
	{
		// 새로운 타겟의 테두리 생성
		CurrentTarget->SetTargetHighlighted(true);
	}
}

void UAssassinationTargetingComponent::AddTargetCandidate(UAssassinateTargetComponent* Enemy)
{
	TargetCandidate.Add(Enemy);
	SelectCandidate();
	if (GetWorld()->GetTimerManager().IsTimerActive(SearchCandidateTimer)) return;

	GetWorld()->GetTimerManager().SetTimer(
		SearchCandidateTimer,
		this,
		&UAssassinationTargetingComponent::SelectCandidate,
		0.1f,
		true
	);
}

void UAssassinationTargetingComponent::RemoveTargetCandidate(UAssassinateTargetComponent* Enemy)
{
	if (TargetCandidate.Contains(Enemy))
	{
		TargetCandidate.Remove(Enemy);

		// 기존 대상의 표시를 해제한 뒤 남은 후보 중에서 다시 선정한다.
		SelectCandidate();
	}

	if(TargetCandidate.Num() <= 0)
		GetWorld()->GetTimerManager().ClearTimer(SearchCandidateTimer);
}
