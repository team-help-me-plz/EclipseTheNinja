#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Checkpoint/Checkpoint.h"
#include "Checkpoint/CheckpointSubsystem.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "UObject/UnrealType.h"

namespace CheckpointLampTests
{
    struct FTestWorld
    {
        UWorld* World = nullptr;

        FTestWorld()
        {
            UWorld::InitializationValues Values;
            Values.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
            World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
            World->InitializeActorsForPlay(FURL());
        }

        ~FTestWorld()
        {
            World->DestroyWorld(false);
        }
    };

    ACheckpoint* SpawnCheckpoint(UWorld* World, ECheckpointId Id, float X,
        bool bActivateOnBeginPlay = false, UClass* Class = ACheckpoint::StaticClass())
    {
        ACheckpoint* Checkpoint = World->SpawnActor<ACheckpoint>(Class, FVector(X, 0, 0), FRotator::ZeroRotator);
        if (!Checkpoint) return nullptr;

        FEnumProperty* IdProperty = FindFProperty<FEnumProperty>(Checkpoint->GetClass(), TEXT("CheckpointId"));
        if (!IdProperty) return nullptr;
        IdProperty->GetUnderlyingProperty()->SetIntPropertyValue(
            IdProperty->ContainerPtrToValuePtr<void>(Checkpoint), static_cast<int64>(Id));

        FBoolProperty* AutoActivateProperty = FindFProperty<FBoolProperty>(Checkpoint->GetClass(), TEXT("bActivateOnBeginPlay"));
        if (!AutoActivateProperty) return nullptr;
        AutoActivateProperty->SetPropertyValue_InContainer(Checkpoint, bActivateOnBeginPlay);

        Checkpoint->DispatchBeginPlay();
        return Checkpoint;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCheckpointLampActivationTest, "Eclipse.Checkpoint.LampActivation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCheckpointLampActivationTest::RunTest(const FString& Parameters)
{
    CheckpointLampTests::FTestWorld Fixture;
    UCheckpointSubsystem* Subsystem = Fixture.World->GetSubsystem<UCheckpointSubsystem>();
    if (!TestNotNull(TEXT("체크포인트 서브시스템"), Subsystem)) return false;

    ACheckpoint* First = CheckpointLampTests::SpawnCheckpoint(Fixture.World, ECheckpointId::Tutorial_Start, 0);
    if (!TestNotNull(TEXT("첫 체크포인트"), First)) return false;

    ACheckpoint* Second = CheckpointLampTests::SpawnCheckpoint(Fixture.World, ECheckpointId::Tutorial_Clone, 1000);
    if (!TestNotNull(TEXT("다음 체크포인트"), Second)) return false;

    UPointLightComponent* FirstLight = First->FindComponentByClass<UPointLightComponent>();
    if (!TestNotNull(TEXT("첫 가로등 조명"), FirstLight)) return false;

    UPointLightComponent* SecondLight = Second->FindComponentByClass<UPointLightComponent>();
    if (!TestNotNull(TEXT("다음 가로등 조명"), SecondLight)) return false;

    UStaticMeshComponent* FirstMesh = First->FindComponentByClass<UStaticMeshComponent>();
    if (!TestNotNull(TEXT("가로등 메시"), FirstMesh)) return false;
    if (!TestNotNull(TEXT("소등 재질"), FirstMesh->GetMaterial(0))) return false;
    TestFalse(TEXT("등록 전에는 소등"), FirstLight->IsVisible());
    TestEqual(TEXT("등록 전에는 발광하지 않는 재질"), FirstMesh->GetMaterial(0)->GetName(), FString(TEXT("MI_CheckpointLamp_Off")));

    Subsystem->ActivateCheckpoint(ECheckpointId::Tutorial_Start);
    TestTrue(TEXT("등록한 체크포인트 점등"), First->IsCheckpointActive());
    TestTrue(TEXT("등록한 가로등의 실제 조명 켜짐"), FirstLight->IsVisible());
    TestEqual(TEXT("점등 재질로 전환"), FirstMesh->GetMaterial(0)->GetName(), FString(TEXT("MI_CheckpointLamp_On")));
    TestFalse(TEXT("등록하지 않은 가로등은 소등 유지"), SecondLight->IsVisible());

    Subsystem->ActivateCheckpoint(ECheckpointId::Tutorial_Clone);
    TestFalse(TEXT("새 체크포인트 등록 시 이전 조명 꺼짐"), FirstLight->IsVisible());
    TestEqual(TEXT("이전 체크포인트 발광도 꺼짐"), FirstMesh->GetMaterial(0)->GetName(), FString(TEXT("MI_CheckpointLamp_Off")));
    TestTrue(TEXT("현재 체크포인트 조명만 켜짐"), SecondLight->IsVisible());

    Subsystem->RestoreCheckpoint(ECheckpointId::Tutorial_Start);
    TestTrue(TEXT("저장된 체크포인트 복원 시 점등"), FirstLight->IsVisible());
    TestFalse(TEXT("복원 시 다른 체크포인트 소등"), SecondLight->IsVisible());

    // 액터가 나중에 로드되어도 복원된 현재 체크포인트의 조명과 동기화한다.
    Subsystem->RestoreCheckpoint(ECheckpointId::Tutorial_Combat);
    ACheckpoint* LateCheckpoint = CheckpointLampTests::SpawnCheckpoint(Fixture.World, ECheckpointId::Tutorial_Combat, 2000);
    if (!TestNotNull(TEXT("복원 후 로드된 체크포인트"), LateCheckpoint)) return false;
    TestTrue(TEXT("늦게 등록된 현재 체크포인트도 점등"), LateCheckpoint->IsCheckpointActive());
    TestFalse(TEXT("현재가 아닌 체크포인트 소등 유지"), First->IsCheckpointActive());
    Subsystem->UnregisterCheckpoint(LateCheckpoint);
    TestFalse(TEXT("등록 해제 시 소등"), LateCheckpoint->IsCheckpointActive());

    ACheckpoint* StartCheckpoint = CheckpointLampTests::SpawnCheckpoint(
        Fixture.World, ECheckpointId::Tutorial_Assassinate, 3000, true);
    if (!TestNotNull(TEXT("자동 활성화 체크포인트"), StartCheckpoint)) return false;
    TestTrue(TEXT("레벨 시작 자동 등록도 점등"), StartCheckpoint->IsCheckpointActive());
    Subsystem->RestoreCheckpoint(ECheckpointId::None);
    TestFalse(TEXT("현재 체크포인트가 없으면 모두 소등"), StartCheckpoint->IsCheckpointActive());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCheckpointLampBlueprintTest, "Eclipse.Checkpoint.LampBlueprint",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCheckpointLampBlueprintTest::RunTest(const FString& Parameters)
{
    CheckpointLampTests::FTestWorld Fixture;
    UClass* CheckpointClass = LoadClass<ACheckpoint>(nullptr,
        TEXT("/Game/Blueprint/Checkpoint/BP_Checkpoint.BP_Checkpoint_C"));
    if (!TestNotNull(TEXT("기존 체크포인트 Blueprint"), CheckpointClass)) return false;

    ACheckpoint* Checkpoint = CheckpointLampTests::SpawnCheckpoint(
        Fixture.World, ECheckpointId::Tutorial_Start, 0, false, CheckpointClass);
    if (!TestNotNull(TEXT("기존 Blueprint로 배치한 체크포인트"), Checkpoint)) return false;

    UBoxComponent* Trigger = Checkpoint->FindComponentByClass<UBoxComponent>();
    if (!TestNotNull(TEXT("기존 체크포인트 감지 영역"), Trigger)) return false;
    TestFalse(TEXT("초록색 볼륨은 보이지 않음"), Trigger->IsVisible());
    TestTrue(TEXT("감지 볼륨은 게임에서 숨김"), Trigger->bHiddenInGame);
    TestTrue(TEXT("숨겨도 체크포인트 감지 충돌은 유지"), Trigger->GetCollisionEnabled() != ECollisionEnabled::NoCollision);

    UStaticMeshComponent* Mesh = Checkpoint->FindComponentByClass<UStaticMeshComponent>();
    if (!TestNotNull(TEXT("기존 배치에도 가로등 자동 추가"), Mesh)) return false;
    const UStaticMesh* LampAsset = Mesh->GetStaticMesh();
    if (!TestNotNull(TEXT("지정한 가로등 에셋"), LampAsset)) return false;
    TestEqual(TEXT("요청한 가로등 메시 사용"), LampAsset->GetName(), FString(TEXT("SM_JVK_Lamp_Street_01")));
    TestEqual(TEXT("가로등이 플레이어를 막지 않음"), Mesh->GetCollisionEnabled(), ECollisionEnabled::NoCollision);

    Checkpoint->SetActorScale3D(FVector(2, 3, 4));
    TestTrue(TEXT("감지 영역을 확대해도 가로등 크기는 유지"), Mesh->GetComponentScale().Equals(FVector::OneVector));
    Fixture.World->GetSubsystem<UCheckpointSubsystem>()->ActivateCheckpoint(ECheckpointId::Tutorial_Start);
    TestTrue(TEXT("기존 Blueprint도 등록 시 점등"), Checkpoint->IsCheckpointActive());
    return true;
}

#endif
