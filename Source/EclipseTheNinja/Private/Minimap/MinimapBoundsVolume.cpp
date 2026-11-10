#include "Minimap/MinimapBoundsVolume.h"
#include "Minimap/MinimapSubsystem.h"
#include "Minimap/MinimapGridBuilder.h"
#include "Minimap/MinimapData.h"
#include "EclipseTheNinja.h"

#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "HAL/IConsoleManager.h"
#include "Engine/OverlapResult.h"
#include "Components/PrimitiveComponent.h"
#include "CollisionQueryParams.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#endif

AMinimapBoundsVolume::AMinimapBoundsVolume()
{
	PrimaryActorTick.bCanEverTick = false;

	this->Bounds = CreateDefaultSubobject<UBoxComponent>(TEXT("Bounds"));
	SetRootComponent(this->Bounds);
	this->Bounds->SetBoxExtent(FVector(5000.f, 200.f, 2000.f));
	this->Bounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	this->Bounds->SetGenerateOverlapEvents(false);
	this->Bounds->SetCanEverAffectNavigation(false);
	this->Bounds->ShapeColor = FColor(80, 200, 255);
	this->Bounds->SetLineThickness(4.f);
}

FBox AMinimapBoundsVolume::GetWorldBounds() const
{
	return this->Bounds ? this->Bounds->Bounds.GetBox() : FBox(ForceInit);
}

void AMinimapBoundsVolume::BeginPlay()
{
	Super::BeginPlay();

	if (UMinimapSubsystem* Minimap = UMinimapSubsystem::Get(this))
	{
		Minimap->RegisterBounds(this);
	}
}

void AMinimapBoundsVolume::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UMinimapSubsystem* Minimap = UMinimapSubsystem::Get(this))
	{
		Minimap->UnregisterBounds(this);
	}
	Super::EndPlay(EndPlayReason);
}

void AMinimapBoundsVolume::BakeMinimap()
{
#if WITH_EDITOR
	UWorld* World = GetWorld();
	if (!World || World->IsGameWorld())
	{
		UE_LOG(LogEclipseTheNinja, Warning, TEXT("[Minimap] 굽기는 에디터 레벨에서만 할 수 있습니다."));
		return;
	}

	// 플레이 평면: 플레이어 시작 지점의 Y. 없으면 상자 중심의 Y
	float PlaneY = GetWorldBounds().GetCenter().Y;
	for (TActorIterator<APlayerStart> It(World); It; ++It)
	{
		PlaneY = It->GetActorLocation().Y;
		break;
	}

	const double StartTime = FPlatformTime::Seconds();
	FMinimapGridResult Result;
	if (!MinimapGrid::Build(World, this, PlaneY, Result))
	{
		UE_LOG(LogEclipseTheNinja, Warning, TEXT("[Minimap] 격자를 만들지 못했습니다."));
		return;
	}

	UMinimapData* Data = this->BakedData;
	if (!Data)
	{
		// 레벨 이름으로 에셋을 찾고, 없으면 새로 만든다
		const FString LevelName = FPackageName::GetShortName(World->GetOutermost()->GetName());
		const FString AssetName = TEXT("MD_") + LevelName;
		const FString PackageName = TEXT("/Game/Data/Minimap/") + AssetName;

		Data = LoadObject<UMinimapData>(nullptr, *(PackageName + TEXT(".") + AssetName), nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!Data)
		{
			UPackage* Package = CreatePackage(*PackageName);
			Data = NewObject<UMinimapData>(Package, *AssetName, RF_Public | RF_Standalone | RF_Transactional);
			FAssetRegistryModule::AssetCreated(Data);
		}
	}

	Data->Modify();
	Data->GridWidth = Result.Width;
	Data->GridHeight = Result.Height;
	Data->CellSize = Result.CellSize;
	Data->OriginX = Result.OriginX;
	Data->OriginZ = Result.OriginZ;
	Data->PlaneY = Result.PlaneY;
	Data->CellKinds = MoveTemp(Result.Kinds);
	Data->CellTints = MoveTemp(Result.Tints);
	Data->MarkPackageDirty();

	// 에셋은 바로 저장한다 (새로 만든 에셋도 디스크에 생기도록)
	UPackage* Package = Data->GetOutermost();
	const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	const bool bSaved = UPackage::SavePackage(Package, Data, *Filename, SaveArgs);

	if (this->BakedData != Data)
	{
		Modify();
		this->BakedData = Data;
	}

	UE_LOG(LogEclipseTheNinja, Log, TEXT("[Minimap] 굽기 완료: %s 격자 %dx%d (칸 %.0fuu, 평면 Y %.0f) %.1fms, 저장 %s"),
		*Data->GetPathName(), Data->GridWidth, Data->GridHeight, Data->CellSize, Data->PlaneY,
		(FPlatformTime::Seconds() - StartTime) * 1000.0, bSaved ? TEXT("성공") : TEXT("실패"));
#endif
}

#if WITH_EDITOR
// 에디터 레벨의 모든 MinimapBoundsVolume을 굽는다. 디테일 패널 버튼과 같다
static FAutoConsoleCommand GMinimapBakeCommand(
	TEXT("Eclipse.Minimap.Bake"),
	TEXT("에디터 레벨의 MinimapBoundsVolume으로 미니맵을 굽는다."),
	FConsoleCommandDelegate::CreateLambda([]()
	{
		UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
		if (!World)
		{
			return;
		}
		int32 Count = 0;
		for (TActorIterator<AMinimapBoundsVolume> It(World); It; ++It)
		{
			It->BakeMinimap();
			++Count;
		}
		if (Count == 0)
		{
			UE_LOG(LogEclipseTheNinja, Warning, TEXT("[Minimap] 레벨에 MinimapBoundsVolume이 없습니다."));
		}
	}));
#endif

#if WITH_EDITOR
// 진단: 미니맵에서 벽 칸을 만드는 액터·컴포넌트를 칸 수 순으로 출력한다. 보이지 않는 충돌(알 수 없는 땅) 찾기용
static FAutoConsoleCommand GMinimapReportSolidCommand(
	TEXT("Eclipse.Minimap.ReportSolid"),
	TEXT("미니맵 벽 칸을 만드는 액터를 칸 수 순으로 출력한다."),
	FConsoleCommandDelegate::CreateLambda([]()
	{
		UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
		AMinimapBoundsVolume* Bounds = nullptr;
		for (TActorIterator<AMinimapBoundsVolume> It(World); It; ++It)
		{
			Bounds = *It;
			break;
		}
		if (!World || !Bounds)
		{
			UE_LOG(LogEclipseTheNinja, Warning, TEXT("[MinimapReport] MinimapBoundsVolume이 없습니다."));
			return;
		}

		float PlaneY = Bounds->GetWorldBounds().GetCenter().Y;
		for (TActorIterator<APlayerStart> It(World); It; ++It)
		{
			PlaneY = It->GetActorLocation().Y;
			break;
		}

		const FBox Box = Bounds->GetWorldBounds();
		const float Cell = FMath::Max(25.f, Bounds->CellSize);
		const int32 W = FMath::CeilToInt(Box.GetSize().X / Cell);
		const int32 H = FMath::CeilToInt(Box.GetSize().Z / Cell);

		TArray<const AActor*> Ignored;
		MinimapGrid::CollectCutawayTargets(World, Ignored);
		FCollisionQueryParams Params(SCENE_QUERY_STAT(MinimapReport), false);
		Params.AddIgnoredActors(Ignored);
		const FCollisionShape Shape = FCollisionShape::MakeBox(FVector(Cell * 0.49f, Bounds->PlaneHalfThickness, Cell * 0.49f));

		struct FStat { int32 Count = 0; FBox2D Area = FBox2D(ForceInit); FString Info; };
		TMap<const UPrimitiveComponent*, FStat> Stats;
		TArray<FOverlapResult> Overlaps;
		for (int32 Z = 0; Z < H; ++Z)
		{
			for (int32 X = 0; X < W; ++X)
			{
				const FVector Center(Box.Min.X + (X + 0.5f) * Cell, PlaneY, Box.Min.Z + (Z + 0.5f) * Cell);
				Overlaps.Reset();
				World->OverlapMultiByChannel(Overlaps, Center, FQuat::Identity, Bounds->OcclusionChannel, Shape, Params);
				for (const FOverlapResult& Overlap : Overlaps)
				{
					const UPrimitiveComponent* Comp = Overlap.GetComponent();
					if (!Overlap.bBlockingHit || !Comp || MinimapGrid::ShouldSkipForGrid(Comp))
					{
						continue;
					}
					FStat& Stat = Stats.FindOrAdd(Comp);
					++Stat.Count;
					Stat.Area += FVector2D(Center.X, Center.Z);
					if (Stat.Info.IsEmpty())
					{
						const AActor* Owner = Comp->GetOwner();
						const bool bVisible = Comp->IsVisible() && !Comp->bHiddenInGame;
						Stat.Info = FString::Printf(TEXT("%s (%s) / %s [%s] 프로필=%s %s"),
							*Owner->GetActorLabel(), *Owner->GetClass()->GetName(), *Comp->GetName(), *Comp->GetClass()->GetName(),
							*Comp->GetCollisionProfileName().ToString(), bVisible ? TEXT("") : TEXT("<<게임에서 안 보임>>"));
					}
				}
			}
		}

		Stats.ValueSort([](const FStat& A, const FStat& B) { return A.Count > B.Count; });
		UE_LOG(LogEclipseTheNinja, Log, TEXT("[MinimapReport] 벽 칸을 만드는 컴포넌트 %d개 (평면 Y %.0f)"), Stats.Num(), PlaneY);
		for (const TPair<const UPrimitiveComponent*, FStat>& Pair : Stats)
		{
			UE_LOG(LogEclipseTheNinja, Log, TEXT("[MinimapReport] %4d칸 X %.0f~%.0f Z %.0f~%.0f : %s"),
				Pair.Value.Count, Pair.Value.Area.Min.X, Pair.Value.Area.Max.X, Pair.Value.Area.Min.Y, Pair.Value.Area.Max.Y, *Pair.Value.Info);
		}
	}));
#endif
