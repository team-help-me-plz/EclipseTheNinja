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
#include "Minimap/MinimapMarkerSettings.h"
#include "Minimap/MinimapMarkerComponent.h"
#include "ImageCore.h"
#include "ImageUtils.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
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

#if WITH_EDITOR
namespace MinimapExport
{
	// 숨김 칸(게임에서는 밝혀져도 안 그리는 칸)을 내보낸 그림에서 구분하는 색
	static const FColor HiddenColor(120, 64, 150, 255);
	// 칸 색 뒤에 깔리는 배경 (위젯 배경과 비슷한 어두운 색)
	static const FColor BackgroundColor(14, 16, 22, 255);

	/** 픽셀 단위로 도형을 칠하는 단순한 캔버스. 알파는 배경 위에 섞는다. */
	struct FCanvas
	{
		int32 Width = 0;
		int32 Height = 0;
		TArray<FColor> Pixels;

		FCanvas(int32 InWidth, int32 InHeight) : Width(InWidth), Height(InHeight)
		{
			Pixels.Init(BackgroundColor, Width * Height);
		}

		void Blend(int32 X, int32 Y, const FColor& Src)
		{
			if (X < 0 || Y < 0 || X >= Width || Y >= Height || Src.A == 0)
			{
				return;
			}
			FColor& Dst = Pixels[Y * Width + X];
			const float A = Src.A / 255.f;
			Dst.R = static_cast<uint8>(FMath::RoundToInt(FMath::Lerp(static_cast<float>(Dst.R), static_cast<float>(Src.R), A)));
			Dst.G = static_cast<uint8>(FMath::RoundToInt(FMath::Lerp(static_cast<float>(Dst.G), static_cast<float>(Src.G), A)));
			Dst.B = static_cast<uint8>(FMath::RoundToInt(FMath::Lerp(static_cast<float>(Dst.B), static_cast<float>(Src.B), A)));
			Dst.A = 255;
		}

		void FillRect(double X0, double Y0, double X1, double Y1, const FColor& Color)
		{
			const int32 MinX = FMath::Max(0, FMath::RoundToInt(FMath::Min(X0, X1)));
			const int32 MaxX = FMath::Min(Width, FMath::RoundToInt(FMath::Max(X0, X1)));
			const int32 MinY = FMath::Max(0, FMath::RoundToInt(FMath::Min(Y0, Y1)));
			const int32 MaxY = FMath::Min(Height, FMath::RoundToInt(FMath::Max(Y0, Y1)));
			for (int32 Y = MinY; Y < MaxY; ++Y)
			{
				for (int32 X = MinX; X < MaxX; ++X)
				{
					Blend(X, Y, Color);
				}
			}
		}

		// Shape: 0 사각형, 1 마름모, 2 원. Radius는 중심에서 변(마름모는 꼭짓점, 원은 둘레)까지
		void FillShape(const FVector2D& Center, double Radius, int32 Shape, const FColor& Color)
		{
			const int32 MinX = FMath::FloorToInt(Center.X - Radius);
			const int32 MaxX = FMath::CeilToInt(Center.X + Radius);
			const int32 MinY = FMath::FloorToInt(Center.Y - Radius);
			const int32 MaxY = FMath::CeilToInt(Center.Y + Radius);
			for (int32 Y = MinY; Y <= MaxY; ++Y)
			{
				for (int32 X = MinX; X <= MaxX; ++X)
				{
					const double DX = FMath::Abs(X + 0.5 - Center.X);
					const double DY = FMath::Abs(Y + 0.5 - Center.Y);
					const bool bInside = Shape == 1 ? (DX + DY <= Radius)
						: Shape == 2 ? (DX * DX + DY * DY <= Radius * Radius)
						: (DX <= Radius && DY <= Radius);
					if (bInside)
					{
						Blend(X, Y, Color);
					}
				}
			}
		}

		void Line(const FVector2D& A, const FVector2D& B, double Thickness, const FColor& Color)
		{
			// 같은 픽셀을 여러 번 섞으면 진해지므로 한 번만 칠한다
			TSet<int32> Painted;
			const double Length = FVector2D::Distance(A, B);
			const int32 Steps = FMath::Max(1, FMath::CeilToInt(Length * 2.0));
			const double Radius = Thickness * 0.5;
			for (int32 Step = 0; Step <= Steps; ++Step)
			{
				const FVector2D P = FMath::Lerp(A, B, static_cast<double>(Step) / Steps);
				for (int32 Y = FMath::FloorToInt(P.Y - Radius); Y <= FMath::CeilToInt(P.Y + Radius); ++Y)
				{
					for (int32 X = FMath::FloorToInt(P.X - Radius); X <= FMath::CeilToInt(P.X + Radius); ++X)
					{
						const double DX = X + 0.5 - P.X;
						const double DY = Y + 0.5 - P.Y;
						if (DX * DX + DY * DY <= Radius * Radius && X >= 0 && Y >= 0 && X < Width && Y < Height)
						{
							bool bAlready = false;
							Painted.Add(Y * Width + X, &bAlready);
							if (!bAlready)
							{
								Blend(X, Y, Color);
							}
						}
					}
				}
			}
		}
	};

	// 레벨의 마커 액터 하나
	struct FMarker
	{
		const AActor* Actor = nullptr;
		FMinimapMarkerStyle Style;
	};

	// UMinimapSubsystem::CollectMarkers와 같은 규칙으로 마커를 고른다. 에디터 월드라 레벨 인스턴스 안의 액터도 들어온다
	static void CollectMarkers(UWorld* World, TArray<FMarker>& OutMarkers)
	{
		const UMinimapMarkerSettings* Settings = GetDefault<UMinimapMarkerSettings>();
		TArray<TPair<UClass*, const FMinimapMarkerStyle*>> Rules;
		for (const FMinimapMarkerRule& Rule : Settings->Rules)
		{
			if (UClass* RuleClass = Rule.ActorClass.LoadSynchronous())
			{
				Rules.Emplace(RuleClass, &Rule.Style);
			}
		}

		for (TActorIterator<AActor> It(World); It; ++It)
		{
			const AActor* Actor = *It;
			if (!IsValid(Actor))
			{
				continue;
			}
			const FMinimapMarkerStyle* Style = nullptr;
			if (const UMinimapMarkerComponent* MarkerComp = Actor->FindComponentByClass<UMinimapMarkerComponent>())
			{
				if (MarkerComp->bHideMarker)
				{
					continue;
				}
				if (MarkerComp->bOverrideStyle)
				{
					Style = &MarkerComp->Style;
				}
			}
			if (!Style)
			{
				for (const TPair<UClass*, const FMinimapMarkerStyle*>& Rule : Rules)
				{
					if (Actor->IsA(Rule.Key))
					{
						Style = Rule.Value;
						break;
					}
				}
			}
			if (Style)
			{
				OutMarkers.Add({ Actor, *Style });
			}
		}
	}

	static bool Export(UWorld* World, const AMinimapBoundsVolume* Bounds, int32 Scale, FString& OutPath)
	{
		const UMinimapData* Data = Bounds->BakedData;
		if (!Data || !Data->IsValidGrid())
		{
			UE_LOG(LogEclipseTheNinja, Warning, TEXT("[Minimap] %s: 구운 데이터가 없습니다. 먼저 Eclipse.Minimap.Bake를 실행하세요."), *Bounds->GetActorLabel());
			return false;
		}

		const int32 GW = Data->GridWidth;
		const int32 GH = Data->GridHeight;
		FCanvas Canvas(GW * Scale, GH * Scale);

		// 1. 칸: 모두 밝혀진 상태로 칠한다 (UMinimapSubsystem::WritePixel과 같은 색 규칙, 숨김 칸만 따로 표시)
		for (int32 Z = 0; Z < GH; ++Z)
		{
			for (int32 X = 0; X < GW; ++X)
			{
				const int32 Index = Z * GW + X;
				const uint8 Kind = Data->CellKinds[Index];
				FColor Color = Bounds->EmptyColor;
				if (Kind == static_cast<uint8>(EMinimapCellKind::Solid))
				{
					Color = Bounds->SolidColor;
				}
				else if (Kind == static_cast<uint8>(EMinimapCellKind::Edge))
				{
					Color = Bounds->EdgeColor;
				}
				else if (Kind == static_cast<uint8>(EMinimapCellKind::Hidden))
				{
					Color = HiddenColor;
				}
				if (Data->CellTints.IsValidIndex(Index) && Data->CellTints[Index].A > 0 && Kind != static_cast<uint8>(EMinimapCellKind::Hidden))
				{
					Color = Data->CellTints[Index];
				}
				// 텍스처와 같이 위쪽 행이 높은 Z
				const int32 Row = GH - 1 - Z;
				Canvas.FillRect(X * Scale, Row * Scale, (X + 1) * Scale, (Row + 1) * Scale, Color);
			}
		}

		// 2. 마커: 위젯(UMinimapWidget::DrawObjectMarkers)과 같은 모양. 마커 크기는 칸당 8px일 때 화면 크기와 같게 맞춘다
		const double SizeScale = Scale / 8.0;
		const auto ToPixel = [&](const FVector& Location)
		{
			return FVector2D((Location.X - Data->OriginX) / Data->CellSize * Scale,
				(GH - (Location.Z - Data->OriginZ) / Data->CellSize) * Scale);
		};
		const auto ToFColor = [](const FLinearColor& Color) { return Color.ToFColor(true); };
		const FColor Outline(0, 0, 0, 255);

		TArray<FMarker> Markers;
		CollectMarkers(World, Markers);

		// 경로(움직이는 발판)를 먼저 그려 마커 아래에 깔리게 한다
		for (const FMarker& Marker : Markers)
		{
			if (Marker.Style.PathProperty.IsNone())
			{
				continue;
			}
			const FArrayProperty* ArrayProp = FindFProperty<FArrayProperty>(Marker.Actor->GetClass(), Marker.Style.PathProperty);
			const FObjectPropertyBase* Inner = ArrayProp ? CastField<FObjectPropertyBase>(ArrayProp->Inner) : nullptr;
			if (!Inner)
			{
				continue;
			}
			TArray<FVector2D> Points;
			FScriptArrayHelper Helper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(Marker.Actor));
			for (int32 Index = 0; Index < Helper.Num(); ++Index)
			{
				if (const AActor* Point = Cast<AActor>(Inner->GetObjectPropertyValue(Helper.GetRawPtr(Index))))
				{
					Points.Add(ToPixel(Point->GetActorLocation()));
				}
			}
			const bool bPathOnly = Marker.Style.Shape == EMinimapMarkerShape::PathOnly;
			FLinearColor PathColor = Marker.Style.Color;
			PathColor.A *= bPathOnly ? 0.95f : 0.45f;
			const double Thickness = (bPathOnly ? 3.5 : 2.0) * SizeScale;
			for (int32 Index = 1; Index < Points.Num(); ++Index)
			{
				Canvas.Line(Points[Index - 1], Points[Index], Thickness, ToFColor(PathColor));
			}
			if (bPathOnly)
			{
				for (const FVector2D& Point : Points)
				{
					Canvas.FillShape(Point, (Thickness + 4.0 * SizeScale) * 0.5, 2, ToFColor(PathColor));
				}
			}
		}

		for (const FMarker& Marker : Markers)
		{
			const FMinimapMarkerStyle& Style = Marker.Style;
			if (Style.Shape == EMinimapMarkerShape::PathOnly)
			{
				continue;
			}

			// 문: 닫힌 상태의 충돌 범위를 막대로
			if (Style.Shape == EMinimapMarkerShape::ActorBounds)
			{
				const FBox Box = Marker.Actor->GetComponentsBoundingBox(false, true);
				if (!Box.IsValid)
				{
					continue;
				}
				FVector2D Min = ToPixel(FVector(Box.Min.X, 0.0, Box.Max.Z));
				FVector2D Max = ToPixel(FVector(Box.Max.X, 0.0, Box.Min.Z));
				const FVector2D MinSize(FMath::Max(3.0, Style.Size * 0.4) * SizeScale);
				const FVector2D Grow = (MinSize - (Max - Min)).ComponentMax(FVector2D::ZeroVector) * 0.5;
				Min -= Grow;
				Max += Grow;
				const double Pad = 1.5 * SizeScale;
				Canvas.FillRect(Min.X - Pad, Min.Y - Pad, Max.X + Pad, Max.Y + Pad, Outline);
				Canvas.FillRect(Min.X, Min.Y, Max.X, Max.Y, ToFColor(Style.Color));
				continue;
			}

			// 아이콘은 그리지 않고 도형으로 대신한다
			const int32 Shape = Style.Shape == EMinimapMarkerShape::Diamond ? 1 : Style.Shape == EMinimapMarkerShape::Circle ? 2 : 0;
			// 위젯의 마름모는 정사각형을 0.75배로 줄여 45도 돌린 것 (중심~꼭짓점 = 변 * 0.75 / √2 * 2 / 2)
			const double ShapeFactor = Shape == 1 ? 0.75 * UE_DOUBLE_SQRT_2 : 1.0;
			const double Radius = Style.Size * 0.5 * SizeScale * ShapeFactor;
			const double OutlineRadius = (Style.Size + 3.0) * 0.5 * SizeScale * ShapeFactor;
			const FVector2D Center = ToPixel(Marker.Actor->GetActorLocation());
			Canvas.FillShape(Center, OutlineRadius, Shape, Outline);
			Canvas.FillShape(Center, Radius, Shape, ToFColor(Style.Color));
		}

		const FString Dir = FPaths::ProjectSavedDir() / TEXT("Minimap");
		IFileManager::Get().MakeDirectory(*Dir, true);
		OutPath = FPaths::ConvertRelativePathToFull(Dir / (Data->GetName() + TEXT(".png")));
		const FImageView Image(Canvas.Pixels.GetData(), Canvas.Width, Canvas.Height, EGammaSpace::sRGB);
		if (!FImageUtils::SaveImageAutoFormat(*OutPath, Image))
		{
			UE_LOG(LogEclipseTheNinja, Warning, TEXT("[Minimap] PNG 저장 실패: %s"), *OutPath);
			return false;
		}
		UE_LOG(LogEclipseTheNinja, Log, TEXT("[Minimap] 전체 지도 내보내기: %s (%dx%d px, 칸당 %dpx, 마커 %d개)"),
			*OutPath, Canvas.Width, Canvas.Height, Scale, Markers.Num());
		return true;
	}
}

// 구운 미니맵을 모든 칸이 밝혀진 모습으로(마커 포함) Saved/Minimap/<데이터 이름>.png에 저장한다
static FAutoConsoleCommand GMinimapExportPngCommand(
	TEXT("Eclipse.Minimap.ExportPng"),
	TEXT("구운 미니맵 전체(모든 칸 밝힘 + 마커)를 Saved/Minimap/<데이터 이름>.png로 저장한다. 인자: 칸당 픽셀 (기본 8, 2~32)"),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
		if (!World)
		{
			return;
		}
		const int32 Scale = FMath::Clamp(Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 8, 2, 32);
		int32 Count = 0;
		for (TActorIterator<AMinimapBoundsVolume> It(World); It; ++It)
		{
			FString Path;
			if (MinimapExport::Export(World, *It, Scale, Path))
			{
				++Count;
			}
		}
		if (Count == 0)
		{
			UE_LOG(LogEclipseTheNinja, Warning, TEXT("[Minimap] 내보낸 미니맵이 없습니다. MinimapBoundsVolume과 구운 데이터를 확인하세요."));
		}
	}));
#endif
