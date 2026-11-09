#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Character/PlayerCharacter.h"
#include "Character/Enemy.h"
#include "Character/BossEnemy.h"
#include "AbilitySystemComponent.h"
#include "Clone/CloneCharacter.h"
#include "Component/AssassinateTargetComponent.h"
#include "Component/AllySummonComponent.h"
#include "Component/CuttableComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GAS/StatAttributeSet.h"
#include "GAS/Tags/EclipseGameplayTags.h"
#include "PuzzelObject/MovingPlatform.h"
#include "Projectile/KunaiProjectile.h"
#include "TimerManager.h"
#include "Trap/DamageTrap.h"
#include "UObject/UnrealType.h"

namespace GameplayPolishTests
{
    struct FTestWorld
    {
        UWorld* World = nullptr;

        FTestWorld()
        {
            UWorld::InitializationValues Values;
            Values.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
            World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
        }

        ~FTestWorld()
        {
            World->DestroyWorld(false);
        }
    };

    class FFinishCloneTrapTest : public IAutomationLatentCommand
    {
    public:
        FFinishCloneTrapTest(TSharedPtr<FTestWorld> InFixture, ACloneCharacter* InClone, FAutomationTestBase* InTest)
            : Fixture(InFixture), Clone(InClone), Test(InTest) {}

        virtual bool Update() override
        {
            Fixture->World->GetTimerManager().Tick(0.016f);
            bool bCleanedUp = !Clone.IsValid();
            if (Clone.IsValid()) bCleanedUp = Clone->IsActorBeingDestroyed();
            Test->TestTrue(TEXT("중복 오버랩 후 다음 프레임에 분신 정리"), bCleanedUp);
            return true;
        }

    private:
        TSharedPtr<FTestWorld> Fixture;
        TWeakObjectPtr<ACloneCharacter> Clone;
        FAutomationTestBase* Test;
    };

    class FFinishKunaiFlightTest : public IAutomationLatentCommand
    {
    public:
        FFinishKunaiFlightTest(TSharedPtr<FTestWorld> InFixture, AKunaiProjectile* InKunai, FAutomationTestBase* InTest)
            : Fixture(InFixture), Kunai(InKunai), Test(InTest) {}

        virtual bool Update() override
        {
            Fixture->World->GetTimerManager().Tick(0.016f);
            if (!Test->TestTrue(TEXT("줄 절단 후 표창 유지"), Kunai.IsValid())) return true;

            UProjectileMovementComponent* Movement = Kunai->FindComponentByClass<UProjectileMovementComponent>();
            if (!Test->TestNotNull(TEXT("표창 이동 컴포넌트"), Movement)) return true;
            Test->TestTrue(TEXT("줄 절단 후 비행 재개"), Movement->IsActive());
            const float StartZ = Kunai->GetActorLocation().Z;
            Movement->TickComponent(0.016f, LEVELTICK_All, nullptr);
            Test->TestTrue(TEXT("절단한 줄을 지나 표창 이동"), Kunai->GetActorLocation().Z > StartZ);
            return true;
        }

    private:
        TSharedPtr<FTestWorld> Fixture;
        TWeakObjectPtr<AKunaiProjectile> Kunai;
        FAutomationTestBase* Test;
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCloneTrapPolishTest, "Eclipse.Polish.CloneTrap",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCloneTrapPolishTest::RunTest(const FString& Parameters)
{
    TSharedPtr<GameplayPolishTests::FTestWorld> Fixture = MakeShared<GameplayPolishTests::FTestWorld>();
    Fixture->World->InitializeActorsForPlay(FURL());
    ACloneCharacter* Clone = Fixture->World->SpawnActor<ACloneCharacter>();
    if (!TestNotNull(TEXT("분신 생성"), Clone)) return false;

    // 미설정 안전 지점이 들어와도 크래시 없이 무시해야 한다.
    Clone->RegisterSafePoint_Implementation(nullptr);

    ADamageTrap* Trap = Fixture->World->SpawnActor<ADamageTrap>();
    if (!TestNotNull(TEXT("함정 생성"), Trap)) return false;

    UBoxComponent* Box = Trap->FindComponentByClass<UBoxComponent>();
    if (!TestNotNull(TEXT("함정 충돌 박스"), Box)) return false;
    if (!TestTrue(TEXT("함정 오버랩 콜백 연결"), Box->OnComponentBeginOverlap.IsBound())) return false;

    Box->OnComponentBeginOverlap.Broadcast(Box, Clone, Clone->GetCapsuleComponent(), 0, false, FHitResult());
    Box->OnComponentBeginOverlap.Broadcast(Box, Clone, Clone->GetMesh(), 0, false, FHitResult());
    TestFalse(TEXT("오버랩 처리 중에는 분신을 파괴하지 않음"), Clone->IsActorBeingDestroyed());

    Fixture->World->GetTimerManager().Tick(0.016f);
    ADD_LATENT_AUTOMATION_COMMAND(GameplayPolishTests::FFinishCloneTrapTest(Fixture, Clone, this));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKunaiRopePolishTest, "Eclipse.Polish.KunaiMuzzleRopeHit",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKunaiRopePolishTest::RunTest(const FString& Parameters)
{
    TSharedPtr<GameplayPolishTests::FTestWorld> Fixture = MakeShared<GameplayPolishTests::FTestWorld>();
    APlayerCharacter* Player = Fixture->World->SpawnActor<APlayerCharacter>();
    if (!TestNotNull(TEXT("플레이어 생성"), Player)) return false;

    AActor* RopeActor = Fixture->World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("줄 액터 생성"), RopeActor)) return false;

    UCuttableComponent* Rope = NewObject<UCuttableComponent>(RopeActor);
    RopeActor->SetRootComponent(Rope);
    Rope->RegisterComponent();
    int32 CutCount = 0;
    const FDelegateHandle CutHandle = Rope->OnRopeCut.AddLambda([&CutCount]() { ++CutCount; });

    AKunaiProjectile* Kunai = Fixture->World->SpawnActor<AKunaiProjectile>();
    if (!TestNotNull(TEXT("표창 생성"), Kunai)) return false;

    Kunai->SetOwner(Player);
    Kunai->SetInstigator(Player);
    Kunai->InitializeKunai(nullptr, FGameplayEffectSpecHandle(), FVector::UpVector, 1000.0f);
    const FHitResult Hit(RopeActor, Rope, FVector::ZeroVector, FVector::DownVector);
    Kunai->HandleLaunchHit(Hit);
    Kunai->HandleLaunchHit(Hit);
    TestEqual(TEXT("발사 전 검사로 맞은 줄도 한 번만 절단"), CutCount, 1);
    Rope->OnRopeCut.Remove(CutHandle);
    Fixture->World->GetTimerManager().Tick(0.016f);
    ADD_LATENT_AUTOMATION_COMMAND(GameplayPolishTests::FFinishKunaiFlightTest(Fixture, Kunai, this));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAllySummonPolishTest, "Eclipse.Polish.AllySummonClearance",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAllySummonPolishTest::RunTest(const FString& Parameters)
{
    GameplayPolishTests::FTestWorld Fixture;
    AActor* Floor = Fixture.World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("소환 바닥 생성"), Floor)) return false;

    UBoxComponent* FloorBox = NewObject<UBoxComponent>(Floor);
    FloorBox->SetBoxExtent(FVector(1000, 250, 20));
    FloorBox->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    Floor->SetRootComponent(FloorBox);
    FloorBox->RegisterComponent();
    Floor->SetActorLocation(FVector(0, 0, -20));

    AEnemy* Owner = Fixture.World->SpawnActor<AEnemy>(FVector(0, 0, 100), FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("소환자 생성"), Owner)) return false;

    UAllySummonComponent* Summon = NewObject<UAllySummonComponent>(Owner);
    Summon->RegisterComponent();
    // BeginPlay를 실행하지 않는 격리 월드에서 소환 설정만 주입한다.
    FindFProperty<FObjectProperty>(Summon->GetClass(), TEXT("OwnerEnemy"))->SetObjectPropertyValue_InContainer(Summon, Owner);
    FindFProperty<FClassProperty>(Summon->GetClass(), TEXT("SummonClass"))->SetObjectPropertyValue_InContainer(Summon, AEnemy::StaticClass());
    FindFProperty<FIntProperty>(Summon->GetClass(), TEXT("SummonCount"))->SetPropertyValue_InContainer(Summon, 1);

    AddExpectedError(TEXT("EnemyClimbEntranceComponent"), EAutomationExpectedErrorFlags::Contains, 1);
    TestEqual(TEXT("바닥과 공간이 충분하면 소환"), Summon->SummonAllies(nullptr), 1);

    AActor* Wall = Fixture.World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("소환 공간 장애물 생성"), Wall)) return false;

    UBoxComponent* WallBox = NewObject<UBoxComponent>(Wall);
    WallBox->SetBoxExtent(FVector(400, 250, 100));
    WallBox->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    Wall->SetRootComponent(WallBox);
    WallBox->RegisterComponent();
    Wall->SetActorLocation(FVector(0, 0, 100));

    AddExpectedError(TEXT("동료를 세울 바닥"), EAutomationExpectedErrorFlags::Contains, 2);
    Summon->ResetSummon();
    TestEqual(TEXT("양쪽 공간이 막히면 소환하지 않음"), Summon->SummonAllies(nullptr), 0);

    Wall->Destroy();
    Floor->Destroy();
    Summon->ResetSummon();
    TestEqual(TEXT("지면이 없으면 소환하지 않음"), Summon->SummonAllies(nullptr), 0);
    return true;
}

#endif
