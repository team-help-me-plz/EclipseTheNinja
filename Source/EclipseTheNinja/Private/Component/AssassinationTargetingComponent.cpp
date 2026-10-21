


#include "Component/AssassinationTargetingComponent.h"
#include "Character/Enemy.h"
#include "Component/AssassinateTargetComponent.h"
#include "TimerManager.h"
#include "Character/PlayerCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/IConsoleManager.h"


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
	// 후보 선정 타이머 해제
	GetWorld()->GetTimerManager().ClearTimer(SearchCandidateTimer);

	// 현재 후보 변수 초기회
	if (IsValid(CurrentTarget))
	{
		CurrentTarget->SetTargetHighlighted(false);
	}

	CurrentTarget = nullptr;
	CurrentTargetType = EAssassinationType::None;

	// 후보Set 초기화
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
	if (!PlayerCharacter) return;
	if (PlayerCharacter->IsAssassinating()) return;


	// 적과 같은 바닥이면 등 뒤 암살, 적보다 높은 발판 위면 뛰어내리는 공중 암살이다.

	// 서 있거나 벽에 매달렸을 때만 타겟을 선정한다. 점프·낙하 중에는 표시하지 않는다.
	// 매달린 상태에서는 걸어서 다가갈 수 없으므로 공중·아래 암살만 찾는다.
	const UCharacterMovementComponent* Movement = PlayerCharacter->GetCharacterMovement();
	const bool bWallAttached = PlayerCharacter->IsWallAttached();
	EAssassinationType SearchType = EAssassinationType::None;
	if (Movement->IsMovingOnGround() || bWallAttached) SearchType = EAssassinationType::Ground;

	// 서 있는 상태가 아닌 경우
	if (SearchType == EAssassinationType::None)
	{
		if (CurrentTarget)
		{
			CurrentTarget->SetTargetHighlighted(false);
			CurrentTarget = nullptr;
		}
		CurrentTargetType = EAssassinationType::None;
		return;
	}

	UAssassinateTargetComponent* MinDistEnemy = nullptr;
	EAssassinationType MinDistType = EAssassinationType::None;
	// 후보를 순회
	for (auto& Enemy : TargetCandidate)
	{
		// 유효하지 않거나, 암살할 수 없으면 건너 뛴다.
		if (!IsValid(Enemy)) continue;
		if (!Enemy->CanAssassinate())
		{
			continue;
		}
		// 높이로 방식을 정하고, 실행 위치까지 막혀 있으면 실행해도 취소되므로 테두리도 표시하지 않는다.
		// 후보는 이미 암살 영역 안에 들어온 적이므로 영역은 다시 확인하지 않는다.
		const EAssassinationType EnemyType = Enemy->EvaluateAssassinationType(PlayerCharacter, false, !bWallAttached);

		// 평지·공중·아래 모두 아닌 경우 건너 뛴다.
		if (EnemyType == EAssassinationType::None)
		{
			continue;
		}

		if (!MinDistEnemy)
		{
			MinDistEnemy = Enemy;
			MinDistType = EnemyType;
		}
		else
		{
			// 후보 중에서 가장 가까운 후보를 고른다
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
				MinDistType = EnemyType;
			}
		}
	}

	// 같은 적이라도 서 있는 높이가 바뀌면 실행할 암살 방식이 달라지므로 방식은 매번 갱신한다.
	CurrentTargetType = MinDistEnemy ? MinDistType : EAssassinationType::None;

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

	// 후보 추가 후 타이머 안돌고 있으면 타이머 돌리기
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

	// 후보 없으면 타이머 종료하기
	if(TargetCandidate.Num() <= 0)
		GetWorld()->GetTimerManager().ClearTimer(SearchCandidateTimer);
}
