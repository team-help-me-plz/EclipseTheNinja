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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FParryPlatformPolishTest, "Eclipse.Polish.ParryMovingPlatform",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FParryPlatformPolishTest::RunTest(const FString& Parameters)
{
    GameplayPolishTests::FTestWorld Fixture;
    AActor* Platform = Fixture.World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("발판 생성"), Platform)) return false;

    UBoxComponent* Box = NewObject<UBoxComponent>(Platform);
    Box->SetMobility(EComponentMobility::Movable);
    Box->SetBoxExtent(FVector(200, 200, 10));
    Box->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    Platform->SetRootComponent(Box);
    Box->RegisterComponent();

    APlayerCharacter* Player = Fixture.World->SpawnActor<APlayerCharacter>(FVector(0, 0, 102), FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("플레이어 생성"), Player)) return false;

    UCharacterMovementComponent* Movement = Player->GetCharacterMovement();
    Movement->bRunPhysicsWithNoController = true;
    Movement->SetMovementMode(MOVE_Walking);
    FMovementBaseInterfaceData BaseData = MovementBaseUtility::GetMovementBaseDataFromPhysicsOwner(Box);
    Player->SetBase(&BaseData);
    Movement->SaveBaseLocation();
    const float StartZ = Player->GetActorLocation().Z;

    Player->SetParryMovementLocked(true);
    TestTrue(TEXT("패링 중 걷기 모드 유지"), Movement->IsMovingOnGround());
    TestTrue(TEXT("패링 중 발판 연결 유지"), Player->GetMovementBaseObject() == Box);

    Platform->SetActorLocation(FVector(0, 0, 20));
    Movement->TickComponent(0.016f, LEVELTICK_All, nullptr);
    TestTrue(TEXT("패링 중에도 발판 상승을 따라 이동"), Player->GetActorLocation().Z > StartZ + 15.0f);

    Player->SetParryMovementLocked(false);
    TestTrue(TEXT("패링 해제 후 걷기 유지"), Movement->IsMovingOnGround());
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMovingPlatformReverseTest, "Eclipse.Polish.MovingPlatformReversesAtCharacter",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMovingPlatformReverseTest::RunTest(const FString& Parameters)
{
    GameplayPolishTests::FTestWorld Fixture;
    AMovingPlatform* Platform = Fixture.World->SpawnActor<AMovingPlatform>();
    if (!TestNotNull(TEXT("이동 발판 생성"), Platform)) return false;

    UBoxComponent* Box = NewObject<UBoxComponent>(Platform);
    Box->SetupAttachment(Platform->GetRootComponent());
    Box->SetBoxExtent(FVector(100, 100, 10));
    Box->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    Box->RegisterComponent();

    ATargetPoint* Lower = Fixture.World->SpawnActor<ATargetPoint>();
    ATargetPoint* Upper = Fixture.World->SpawnActor<ATargetPoint>(FVector(0, 0, 200), FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("아래 경로 지점"), Lower)) return false;
    if (!TestNotNull(TEXT("위 경로 지점"), Upper)) return false;

    FArrayProperty* RouteProperty = FindFProperty<FArrayProperty>(Platform->GetClass(), TEXT("TargetPoint"));
    FScriptArrayHelper Route(RouteProperty, RouteProperty->ContainerPtrToValuePtr<void>(Platform));
    Route.AddValues(2);
    FObjectProperty* RouteEntry = CastFieldChecked<FObjectProperty>(RouteProperty->Inner);
    RouteEntry->SetObjectPropertyValue(Route.GetRawPtr(0), Lower);
    RouteEntry->SetObjectPropertyValue(Route.GetRawPtr(1), Upper);
    FindFProperty<FFloatProperty>(Platform->GetClass(), TEXT("TargetPointTravelTime"))->SetPropertyValue_InContainer(Platform, 2.0f);
    Platform->DispatchBeginPlay();

    Platform->Tick(1.8f);
    Platform->Tick(0.4f);
    const double BeforeCollisionZ = Platform->GetActorLocation().Z;

    AEnemy* Below = Fixture.World->SpawnActor<AEnemy>();
    if (!TestNotNull(TEXT("발판 아래 적 생성"), Below)) return false;
    Below->SetActorLocation(FVector(0, 0, 150.0f - Below->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));
    Platform->Tick(0.4f);
    TestEqual(TEXT("아래 캐릭터를 누르는 이동은 적용하지 않음"), Platform->GetActorLocation().Z, BeforeCollisionZ, 0.01);
    Platform->Tick(0.1f);
    TestTrue(TEXT("캐릭터와 충돌한 다음 위로 반전"), Platform->GetActorLocation().Z > BeforeCollisionZ);

    Below->SetActorLocation(FVector(1000, 0, 0));
    APlayerCharacter* Rider = Fixture.World->SpawnActor<APlayerCharacter>();
    if (!TestNotNull(TEXT("탑승 플레이어 생성"), Rider)) return false;
    Rider->SetActorLocation(Platform->GetActorLocation() + FVector(0, 0, 102));
    FMovementBaseInterfaceData BaseData = MovementBaseUtility::GetMovementBaseDataFromPhysicsOwner(Box);
    Rider->SetBase(&BaseData);
    Platform->Tick(0.2f);
    const double BeforeDescentZ = Platform->GetActorLocation().Z;
    Platform->Tick(0.2f);
    TestTrue(TEXT("위에 탄 캐릭터는 정상 하강을 막지 않음"), Platform->GetActorLocation().Z < BeforeDescentZ);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBossAssassinationEnergyLinkTest, "Eclipse.Polish.BossDefersEnergyLinkUntilAssassinationEnds",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBossAssassinationEnergyLinkTest::RunTest(const FString& Parameters)
{
    GameplayPolishTests::FTestWorld Fixture;
    Fixture.World->InitializeActorsForPlay(FURL());
    ABossEnemy* Boss = Fixture.World->SpawnActor<ABossEnemy>();
    if (!TestNotNull(TEXT("보스 생성"), Boss)) return false;
    Boss->DispatchBeginPlay();

    UAbilitySystemComponent* ASC = Boss->GetAbilitySystemComponent();
    if (!TestNotNull(TEXT("보스 ASC"), ASC)) return false;
    UAssassinateTargetComponent* Target = Boss->FindComponentByClass<UAssassinateTargetComponent>();
    if (!TestNotNull(TEXT("암살 대상 상태"), Target)) return false;

    ASC->SetNumericAttributeBase(UStatAttributeSet::GetMaxHealthAttribute(), 100.0f);
    ASC->SetNumericAttributeBase(UStatAttributeSet::GetHealthAttribute(), 75.0f);
    Target->SetAssassinationInProgress(true);
    Boss->ReceiveAssassination(nullptr);

    TestEqual(TEXT("암살 피해를 첫 임계 체력에서 멈춤"), Boss->GetStatAttribute()->GetHealth(), 70.0f);
    TestFalse(TEXT("암살 중 링크 비활성"), Boss->IsEnergyLinkActive());
    TestFalse(TEXT("암살 중 쉴드 태그 미적용"), ASC->HasMatchingGameplayTag(EclipseGameplayTags::State_Invincible));
    TestTrue(TEXT("활성화 대기 중 새 패턴 차단"), Boss->IsCinematicLocked());
    TestEqual(TEXT("암살 중 링크 제한시간 미시작"), Boss->GetEnergyLinkTimeRemaining(), -1.0f);
    static_cast<AActor*>(Boss)->Tick(0.016f);
    TestFalse(TEXT("암살 상태가 유지되면 다음 프레임에도 링크 대기"), Boss->IsEnergyLinkActive());

    ASC->SetNumericAttributeBase(UStatAttributeSet::GetHealthAttribute(), 0.0f);
    TestEqual(TEXT("대기 중 추가 피해도 예약한 임계 체력 유지"), Boss->GetStatAttribute()->GetHealth(), 70.0f);
    TestFalse(TEXT("대기 중 큰 피해로 사망하지 않음"), Boss->IsDead());

    Target->SetAssassinationInProgress(false);
    static_cast<AActor*>(Boss)->Tick(0.016f);
    TestTrue(TEXT("암살 종료 후 링크 활성화"), Boss->IsEnergyLinkActive());
    TestTrue(TEXT("암살 종료 후 쉴드 태그 적용"), ASC->HasMatchingGameplayTag(EclipseGameplayTags::State_Invincible));
    TestTrue(TEXT("링크 활성화 시점부터 제한시간 시작"), Boss->GetEnergyLinkTimeRemaining() > 0.0f);

    ABossEnemy* NormalBoss = Fixture.World->SpawnActor<ABossEnemy>(FVector(1000, 0, 0), FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("일반 피해 비교용 보스"), NormalBoss)) return false;
    NormalBoss->DispatchBeginPlay();
    UAbilitySystemComponent* NormalASC = NormalBoss->GetAbilitySystemComponent();
    NormalASC->SetNumericAttributeBase(UStatAttributeSet::GetMaxHealthAttribute(), 100.0f);
    NormalASC->SetNumericAttributeBase(UStatAttributeSet::GetHealthAttribute(), 75.0f);
    NormalASC->SetNumericAttributeBase(UStatAttributeSet::GetHealthAttribute(), 50.0f);
    TestTrue(TEXT("일반 피해의 링크 활성화는 즉시 유지"), NormalBoss->IsEnergyLinkActive());
    TestEqual(TEXT("일반 피해도 임계 체력으로 보정"), NormalBoss->GetStatAttribute()->GetHealth(), 70.0f);
    return true;
}

#endif
