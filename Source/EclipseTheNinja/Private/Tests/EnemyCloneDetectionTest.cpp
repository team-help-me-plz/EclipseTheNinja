#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Character/Enemy.h"
#include "Character/PlayerCharacter.h"
#include "Clone/CloneCharacter.h"
#include "Component/EnemyDetectionComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/PlayerController.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BlackboardData.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "AbilitySystemComponent.h"
#include "GAS/StatAttributeSet.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyCloneDetectionTest,
    "Eclipse.AI.PlayerAndCloneDetection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEnemyCloneDetectionTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestNotNull(TEXT("Test world"), World)) return false;
    FActorSpawnParameters Spawn;
    Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AEnemy* Enemy = World->SpawnActor<AEnemy>(FVector::ZeroVector, FRotator::ZeroRotator, Spawn);
    APlayerCharacter* Player = World->SpawnActor<APlayerCharacter>(FVector(200.f, 0.f, 0.f), FRotator::ZeroRotator, Spawn);
    ACloneCharacter* Clone = World->SpawnActor<ACloneCharacter>(FVector(2000.f, 0.f, 0.f), FRotator::ZeroRotator, Spawn);
    APawn* Bystander = World->SpawnActor<APawn>(FVector(100.f, 0.f, 0.f), FRotator::ZeroRotator, Spawn);
    APlayerController* PC = World->SpawnActor<APlayerController>();
    PC->Possess(Player);
    UEnemyDetectionComponent* Detection = Enemy->FindComponentByClass<UEnemyDetectionComponent>();
    Detection->AwarenessWidgetClass = nullptr;
    Enemy->DispatchBeginPlay();

    AAIController* AI = World->SpawnActor<AAIController>();
    AI->Possess(Enemy);
    UBlackboardData* Data = NewObject<UBlackboardData>(AI);
    FBlackboardEntry Entry;
    Entry.EntryName = TEXT("TargetActor");
    Entry.KeyType = NewObject<UBlackboardKeyType_Object>(Data);
    Data->Keys.Add(Entry);
    UBlackboardComponent* Blackboard = nullptr;
    AI->UseBlackboard(Data, Blackboard);

    auto Detect = [&]() { Detection->TickComponent(0.11f, LEVELTICK_All, nullptr); };
    auto Target = [&]() { return Blackboard->GetValueAsObject(TEXT("TargetActor")); };
    TestTrue(TEXT("Player is an AI target"), Enemy->IsValidAITarget(Player));
    TestTrue(TEXT("Unpossessed clone is an AI target"), Enemy->IsValidAITarget(Clone));
    TestFalse(TEXT("Unrelated pawn is not a target"), Enemy->IsValidAITarget(Bystander));
    Detect();
    TestEqual(TEXT("Player in front enters combat"), Detection->AlertLevel, EEnemyAlertLevel::Combat);
    TestTrue(TEXT("Blackboard selects player"), Target() == Player);

    Player->SetActorLocation(FVector(2000.f, 0.f, 0.f));
    Clone->SetActorLocation(FVector(200.f, 0.f, 0.f));
    Detection->ResetDetection();
    Detect();
    TestEqual(TEXT("Clone in front enters combat identically"), Detection->AlertLevel, EEnemyAlertLevel::Combat);
    TestTrue(TEXT("Blackboard selects clone"), Target() == Clone);
    TestTrue(TEXT("Clone can be attacked"), Enemy->IsWithinAttackRange(Clone) == Enemy->IsWithinAttackRange(Clone));

    Player->SetActorLocation(FVector(100.f, 0.f, 0.f));
    Detect();
    TestTrue(TEXT("Closer visible player replaces clone"), Target() == Player);
    Player->SetActorLocation(FVector(300.f, 0.f, 0.f));
    Detect();
    TestTrue(TEXT("Closer visible clone replaces player"), Target() == Clone);

    Player->SetActorLocation(FVector(2000.f, 0.f, 0.f));
    Clone->SetActorLocation(FVector(-400.f, 0.f, 0.f));
    Detection->ResetDetection();
    Detect();
    const float CloneGauge = Detection->AlertGauge;
    TestTrue(TEXT("Clone behind fills awareness without immediate combat"), CloneGauge > 0.f && Detection->AlertLevel == EEnemyAlertLevel::Normal);
    Clone->SetActorLocation(FVector(2000.f, 0.f, 0.f));
    Player->SetActorLocation(FVector(-400.f, 0.f, 0.f));
    Detection->ResetDetection();
    Detect();
    TestEqual(TEXT("Identical rear awareness rate"), Detection->AlertGauge, CloneGauge);

    Player->SetActorLocation(FVector(2000.f, 0.f, 0.f));
    Clone->SetActorLocation(FVector(200.f, 0.f, 0.f));
    AActor* Wall = World->SpawnActor<AActor>();
    UBoxComponent* Box = NewObject<UBoxComponent>(Wall);
    Wall->SetRootComponent(Box);
    Box->SetBoxExtent(FVector(20.f, 500.f, 500.f));
    Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Box->SetCollisionResponseToAllChannels(ECR_Block);
    Box->RegisterComponent();
    Wall->SetActorLocation(FVector(100.f, 0.f, 0.f));
    Detection->ResetDetection();
    Detect();
    TestFalse(TEXT("Wall occludes clone"), Detection->bHasLineOfSight);
    Wall->Destroy();
    Detect();
    TestTrue(TEXT("Clone is reacquired without wall"), Target() == Clone);
    Clone->Destroy();
    Detect();
    TestFalse(TEXT("Expired clone loses sight"), Detection->bHasLineOfSight);
    TestNull(TEXT("Expired clone clears combat target"), Target());

    World->DestroyWorld(false);
    return true;
}

#endif
