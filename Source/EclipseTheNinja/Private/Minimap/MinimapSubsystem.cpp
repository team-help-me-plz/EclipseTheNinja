#include "Minimap/MinimapSubsystem.h"
#include "Minimap/MinimapBoundsVolume.h"
#include "Level/TargetCutawayVolume.h"
#include "Save/GameSaveSubsystem.h"
#include "EclipseTheNinja.h"

#include "Components/BoxComponent.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "CollisionQueryParams.h"
#include "WorldCollision.h"
#include "Minimap/MinimapData.h"
#include "Minimap/MinimapGridBuilder.h"
#include "Minimap/MinimapMarkerComponent.h"
#include "Minimap/MinimapMarkerSettings.h"
#include "Components/PrimitiveComponent.h"
#include "Components/MeshComponent.h"
#include "EngineUtils.h"

namespace MinimapPrivate
{
	// 트레이스가 폰 등을 맞혔을 때 그걸 빼고 다시 쏘는 최대 횟수
	static constexpr int32 MaxTraceRetries = 4;

	// 밝힌 칸을 세이브 진행 데이터에 옮기는 간격(초)
	static constexpr double RecordInterval = 2.0;

	static constexpr uint8 Empty = static_cast<uint8>(EMinimapCellKind::Empty);
	static constexpr uint8 Solid = static_cast<uint8>(EMinimapCellKind::Solid);
	static constexpr uint8 Edge = static_cast<uint8>(EMinimapCellKind::Edge);
	static constexpr uint8 Hidden = static_cast<uint8>(EMinimapCellKind::Hidden);
}

UMinimapSubsystem* UMinimapSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UMinimapSubsystem>() : nullptr;
}

void UMinimapSubsystem::Deinitialize()
{
	// 맵을 떠날 때 마지막으로 밝힌 칸까지 진행 데이터에 남긴다 (파일 기록은 다음 저장 때 함께 된다)
	if (this->bSaveDirty)
	{
		RecordReveal();
	}
	Super::Deinitialize();
}

// ---------------------------------------------------------------------------
// 등록

void UMinimapSubsystem::RegisterBounds(AMinimapBoundsVolume* InBounds)
{
	if (this->BoundsVolume.IsValid() && this->BoundsVolume.Get() != InBounds)
	{
		UE_LOG(LogEclipseTheNinja, Warning, TEXT("[Minimap] MinimapBoundsVolume이 여러 개입니다. %s는 무시합니다."), *GetNameSafe(InBounds));
		return;
	}
	this->BoundsVolume = InBounds;
}

void UMinimapSubsystem::UnregisterBounds(AMinimapBoundsVolume* InBounds)
{
	if (this->BoundsVolume.Get() == InBounds)
	{
		this->BoundsVolume.Reset();
	}
}

int32 UMinimapSubsystem::FindRegion(const ATargetCutawayVolume* InVolume) const
{
	return this->MaskRegions.IndexOfByPredicate([InVolume](const FMaskRegion& Region)
	{
		return Region.Volume.Get() == InVolume;
	});
}

void UMinimapSubsystem::RegisterMaskRegion(ATargetCutawayVolume* InVolume, bool bDiscovered)
{
	if (!IsValid(InVolume) || !IsValid(InVolume->GetInteriorArea()))
	{
		return;
	}

	int32 Index = FindRegion(InVolume);
	if (Index == INDEX_NONE)
	{
		Index = this->MaskRegions.AddDefaulted();
	}

	FMaskRegion& Region = this->MaskRegions[Index];
	const UBoxComponent* Box = InVolume->GetInteriorArea();
	Region.Volume = InVolume;
	Region.BoxTransform = Box->GetComponentTransform();
	Region.BoxExtent = Box->GetUnscaledBoxExtent();
	Region.bDiscovered = Region.bDiscovered || bDiscovered;

	// 격자를 만든 뒤에 등록된 영역(스트리밍 등)은 그 영역만 칸에 채운다
	if (this->bGridReady)
	{
		BuildMaskIndex(Index);
	}
}

void UMinimapSubsystem::UnregisterMaskRegion(ATargetCutawayVolume* InVolume)
{
	const int32 Index = FindRegion(InVolume);
	if (Index == INDEX_NONE)
	{
		return;
	}
	this->MaskRegions.RemoveAt(Index);
	if (this->bGridReady)
	{
		RebuildAllMaskIndices();
	}
}

void UMinimapSubsystem::DiscoverMaskRegion(ATargetCutawayVolume* InVolume)
{
	// 볼륨의 BeginPlay 중(시작부터 플레이어가 안에 있을 때) 불릴 수 있으므로 없으면 등록부터 한다
	int32 Index = FindRegion(InVolume);
	if (Index == INDEX_NONE)
	{
		RegisterMaskRegion(InVolume, true);
		return;
	}
	this->MaskRegions[Index].bDiscovered = true;
}

// ---------------------------------------------------------------------------
// 격자

bool UMinimapSubsystem::ShouldSkipActor(const AActor* InActor)
{
	// 적·분신·플레이어는 지형이 아니다
	return MinimapGrid::ShouldSkipActor(InActor);
}

void UMinimapSubsystem::CollectIgnoredActors(TArray<const AActor*>& OutActors) const
{
	// 컷어웨이 대상(외벽)은 마스크 영역이 대신 막으므로 벽 판정과 트레이스에서 항상 뺀다
	for (const FMaskRegion& Region : this->MaskRegions)
	{
		if (const ATargetCutawayVolume* Volume = Region.Volume.Get())
		{
			for (AActor* Target : Volume->GetTargetActors())
			{
				if (IsValid(Target))
				{
					OutActors.Add(Target);
					// 퍼즐 오브젝트처럼 자식 액터에 메시를 둔 경우도 함께 뺀다
					TArray<AActor*> Children;
					Target->GetAllChildActors(Children, true);
					OutActors.Append(Children);
				}
			}
		}
	}
}

bool UMinimapSubsystem::BuildGrid(float InPlaneY)
{
	const AMinimapBoundsVolume* Bounds = this->BoundsVolume.Get();
	UWorld* World = GetWorld();
	if (!Bounds || !World)
	{
		return false;
	}

	const double StartTime = FPlatformTime::Seconds();
	this->OcclusionChannel = Bounds->OcclusionChannel;
	this->EmptyColor = Bounds->EmptyColor;
	this->SolidColor = Bounds->SolidColor;
	this->EdgeColor = Bounds->EdgeColor;

	// 구운 데이터가 있으면 그대로 쓰고, 없으면 지금 레벨로 만든다 (굽기와 같은 코드)
	const UMinimapData* Baked = Bounds->BakedData;
	const bool bUseBaked = Baked && Baked->IsValidGrid();
	if (bUseBaked)
	{
		this->GridWidth = Baked->GridWidth;
		this->GridHeight = Baked->GridHeight;
		this->CellSize = Baked->CellSize;
		this->OriginX = Baked->OriginX;
		this->OriginZ = Baked->OriginZ;
		this->PlaneY = Baked->PlaneY;
		this->CellKinds = Baked->CellKinds;
		this->CellTints = Baked->CellTints;
	}
	else
	{
		FMinimapGridResult Result;
		if (!MinimapGrid::Build(World, Bounds, InPlaneY, Result))
		{
			return false;
		}
		this->GridWidth = Result.Width;
		this->GridHeight = Result.Height;
		this->CellSize = Result.CellSize;
		this->OriginX = Result.OriginX;
		this->OriginZ = Result.OriginZ;
		this->PlaneY = Result.PlaneY;
		this->CellKinds = MoveTemp(Result.Kinds);
		this->CellTints = MoveTemp(Result.Tints);
	}

	const int32 CellCount = this->GridWidth * this->GridHeight;
	this->Revealed.Init(0, CellCount);
	this->Pixels.Init(FColor(0, 0, 0, 0), CellCount);

	this->bGridReady = true;
	RebuildAllMaskIndices();

	this->Texture = UTexture2D::CreateTransient(this->GridWidth, this->GridHeight, PF_B8G8R8A8, TEXT("MinimapTexture"));
	if (this->Texture)
	{
		this->Texture->Filter = TF_Nearest;
		this->Texture->SRGB = true;
		this->Texture->CompressionSettings = TC_VectorDisplacementmap;
		this->Texture->AddressX = TA_Clamp;
		this->Texture->AddressY = TA_Clamp;
		this->Texture->UpdateResource();
	}

	LoadSavedReveal();
	CollectMarkers();
	UpdateMarkerDiscovery();
	this->bTextureDirty = true;
	FlushTexture();

	UE_LOG(LogEclipseTheNinja, Log, TEXT("[Minimap] 격자 %dx%d (칸 %.0fuu, 평면 Y %.0f) %s %.1fms"),
		this->GridWidth, this->GridHeight, this->CellSize, this->PlaneY,
		bUseBaked ? TEXT("구운 데이터 사용") : TEXT("런타임 생성"), (FPlatformTime::Seconds() - StartTime) * 1000.0);
	return true;
}

void UMinimapSubsystem::RebuildAllMaskIndices()
{
	this->CellMaskIndex.Init(INDEX_NONE, this->GridWidth * this->GridHeight);
	for (int32 Index = 0; Index < this->MaskRegions.Num(); ++Index)
	{
		BuildMaskIndex(Index);
	}
}

void UMinimapSubsystem::BuildMaskIndex(int32 RegionIndex)
{
	if (!this->MaskRegions.IsValidIndex(RegionIndex) || this->CellMaskIndex.Num() != this->GridWidth * this->GridHeight)
	{
		return;
	}

	const FMaskRegion& Region = this->MaskRegions[RegionIndex];

	// 박스의 월드 경계 안의 칸만 훑고, 칸 중심을 박스 로컬로 바꿔 회전된 박스도 정확히 판정한다
	const FBox WorldBox = FBox(-Region.BoxExtent, Region.BoxExtent).TransformBy(Region.BoxTransform);
	const int32 MinX = FMath::Clamp(FMath::FloorToInt((WorldBox.Min.X - this->OriginX) / this->CellSize), 0, this->GridWidth - 1);
	const int32 MaxX = FMath::Clamp(FMath::FloorToInt((WorldBox.Max.X - this->OriginX) / this->CellSize), 0, this->GridWidth - 1);
	const int32 MinZ = FMath::Clamp(FMath::FloorToInt((WorldBox.Min.Z - this->OriginZ) / this->CellSize), 0, this->GridHeight - 1);
	const int32 MaxZ = FMath::Clamp(FMath::FloorToInt((WorldBox.Max.Z - this->OriginZ) / this->CellSize), 0, this->GridHeight - 1);

	for (int32 Z = MinZ; Z <= MaxZ; ++Z)
	{
		for (int32 X = MinX; X <= MaxX; ++X)
		{
			const FVector Local = Region.BoxTransform.InverseTransformPosition(CellCenter(X, Z));
			if (FMath::Abs(Local.X) <= Region.BoxExtent.X && FMath::Abs(Local.Y) <= Region.BoxExtent.Y && FMath::Abs(Local.Z) <= Region.BoxExtent.Z)
			{
				// 영역이 겹치면 먼저 등록된 영역을 쓴다 (볼륨끼리 겹치지 않게 배치할 것)
				int32& Slot = this->CellMaskIndex[CellIndex(X, Z)];
				if (Slot == INDEX_NONE)
				{
					Slot = RegionIndex;
				}
			}
		}
	}
}

FVector UMinimapSubsystem::CellCenter(int32 X, int32 Z) const
{
	return FVector(this->OriginX + (X + 0.5f) * this->CellSize, this->PlaneY, this->OriginZ + (Z + 0.5f) * this->CellSize);
}

bool UMinimapSubsystem::WorldToCell(const FVector& WorldLocation, int32& OutX, int32& OutZ) const
{
	OutX = FMath::FloorToInt((WorldLocation.X - this->OriginX) / this->CellSize);
	OutZ = FMath::FloorToInt((WorldLocation.Z - this->OriginZ) / this->CellSize);
	return OutX >= 0 && OutZ >= 0 && OutX < this->GridWidth && OutZ < this->GridHeight;
}

FVector2D UMinimapSubsystem::WorldToTexturePixel(const FVector& WorldLocation) const
{
	const double PX = (WorldLocation.X - this->OriginX) / this->CellSize;
	const double PZ = (WorldLocation.Z - this->OriginZ) / this->CellSize;
	return FVector2D(PX, this->GridHeight - PZ);
}

bool UMinimapSubsystem::IsCellMasked(int32 Index) const
{
	const int32 RegionIndex = this->CellMaskIndex.IsValidIndex(Index) ? this->CellMaskIndex[Index] : INDEX_NONE;
	return RegionIndex != INDEX_NONE && !this->MaskRegions[RegionIndex].bDiscovered;
}

// ---------------------------------------------------------------------------
// 시야

void UMinimapSubsystem::RevealSight(const FVector& Eye, float Radius, int32 RayCount, const AActor* Viewer)
{
	RevealSightInternal(Eye, Radius, nullptr, RayCount, Viewer);
}

void UMinimapSubsystem::RevealSightInView(const FVector& Eye, const FVector2D& ViewHalfExtent, int32 RayCount, const AActor* Viewer)
{
	RevealSightInternal(Eye, 0.f, &ViewHalfExtent, RayCount, Viewer);
}

void UMinimapSubsystem::RevealSightInternal(const FVector& Eye, float Radius, const FVector2D* ViewHalfExtent, int32 RayCount, const AActor* Viewer)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	if (!this->bGridReady && !BuildGrid(Eye.Y))
	{
		return;
	}

	// 플레이 평면 위에서만 쏜다
	const FVector Origin(Eye.X, this->PlaneY, Eye.Z);

	int32 EyeX = 0;
	int32 EyeZ = 0;
	if (!WorldToCell(Origin, EyeX, EyeZ) || IsCellMasked(CellIndex(EyeX, EyeZ)))
	{
		// 범위 밖이거나, 아직 발견 처리 전에 마스크 안에 서 있는 순간이다
		return;
	}
	bool bChanged = RevealCell(CellIndex(EyeX, EyeZ));

	TArray<const AActor*> Ignored;
	CollectIgnoredActors(Ignored);
	if (Viewer)
	{
		Ignored.Add(Viewer);
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MinimapRevealSight), false);
	Params.AddIgnoredActors(Ignored);

	const int32 Rays = FMath::Max(8, RayCount);
	const float Step = this->CellSize * 0.5f;

	// 선 하나로 쏘면 양문처럼 플레이 평면에서 두 메시가 맞닿은 틈을 지나간다. 벽 판정과 같은 Y 두께의 얇은 상자로 쓸어서 막히게 한다
	const AMinimapBoundsVolume* Bounds = this->BoundsVolume.Get();
	const float PlaneHalfThickness = Bounds ? Bounds->PlaneHalfThickness : 15.f;
	const FCollisionShape RayShape = FCollisionShape::MakeBox(FVector(1.f, PlaneHalfThickness, 1.f));

	for (int32 RayIndex = 0; RayIndex < Rays; ++RayIndex)
	{
		const float Angle = (2.f * PI * RayIndex) / Rays;
		const FVector Dir(FMath::Cos(Angle), 0.f, FMath::Sin(Angle));

		// 화면 사각형이면 그 방향으로 사각형 가장자리까지의 거리만큼만 쏜다
		float RayLength = Radius;
		if (ViewHalfExtent)
		{
			const float ToSideX = FMath::Abs(Dir.X) > KINDA_SMALL_NUMBER ? ViewHalfExtent->X / FMath::Abs(Dir.X) : TNumericLimits<float>::Max();
			const float ToSideZ = FMath::Abs(Dir.Z) > KINDA_SMALL_NUMBER ? ViewHalfExtent->Y / FMath::Abs(Dir.Z) : TNumericLimits<float>::Max();
			RayLength = FMath::Min(ToSideX, ToSideZ);
		}
		if (RayLength <= KINDA_SMALL_NUMBER)
		{
			continue;
		}
		const FVector End = Origin + Dir * RayLength;

		// 폰이나 MinimapIgnore 액터에 맞으면 그걸 빼고 다시 쏜다
		FHitResult Hit;
		bool bHit = false;
		FCollisionQueryParams RayParams = Params;
		for (int32 Retry = 0; Retry < MinimapPrivate::MaxTraceRetries; ++Retry)
		{
			bHit = World->SweepSingleByChannel(Hit, Origin, End, FQuat::Identity, this->OcclusionChannel, RayShape, RayParams);
			if (!bHit || !MinimapGrid::ShouldSkipComponent(Hit.GetComponent()))
			{
				break;
			}
			// 레버 상호작용 상자 등은 그 컴포넌트만, 폰 등은 액터째 뺀다
			if (ShouldSkipActor(Hit.GetActor())) { RayParams.AddIgnoredActor(Hit.GetActor()); }
			else { RayParams.AddIgnoredComponent(Hit.GetComponent()); }
			bHit = false;
		}

		const float Distance = bHit ? Hit.Distance : RayLength;
		bool bStopped = false;
		for (float T = Step; T <= Distance; T += Step)
		{
			int32 CX = 0;
			int32 CZ = 0;
			if (!WorldToCell(Origin + Dir * T, CX, CZ))
			{
				bStopped = true;
				break;
			}
			const int32 Index = CellIndex(CX, CZ);
			// 아직 들어가 보지 않은 건물은 벽처럼 레이를 멈춘다 (안도, 너머도 밝히지 않는다)
			if (IsCellMasked(Index))
			{
				bStopped = true;
				break;
			}
			bChanged |= RevealCell(Index);
		}

		// 레이를 막은 벽 칸까지 밝혀야 윤곽이 보인다
		// 얇은 벽(칸보다 얇은 벽)은 맞은 점에서 조금만 더 가도 벽 너머 칸이 된다. 맞은 점 바로 안쪽부터 짧게 훑어
		// 처음 만나는 벽 칸만 밝히고, 벽 칸이 없으면(판정 칸이 비어 있으면) 맞은 점의 칸만 밝힌다
		if (bHit && !bStopped)
		{
			int32 RevealIndex = INDEX_NONE;
			for (const float Depth : { 1.f, this->CellSize * 0.25f, this->CellSize * 0.5f })
			{
				int32 CX = 0;
				int32 CZ = 0;
				if (!WorldToCell(Hit.ImpactPoint + Dir * Depth, CX, CZ))
				{
					break;
				}
				const int32 Index = CellIndex(CX, CZ);
				if (RevealIndex == INDEX_NONE)
				{
					RevealIndex = Index;
				}
				if (this->CellKinds[Index] != MinimapPrivate::Empty)
				{
					RevealIndex = Index;
					break;
				}
			}
			if (RevealIndex != INDEX_NONE && !IsCellMasked(RevealIndex))
			{
				bChanged |= RevealCell(RevealIndex);
			}
		}
	}

	if (bChanged)
	{
		this->bSaveDirty = true;
		UpdateMarkerDiscovery();
		FlushTexture();
	}

	const double Now = World->GetTimeSeconds();
	if (this->bSaveDirty && Now - this->LastRecordTime >= MinimapPrivate::RecordInterval)
	{
		RecordReveal();
	}
}

bool UMinimapSubsystem::RevealCell(int32 Index)
{
	if (!this->Revealed.IsValidIndex(Index) || this->Revealed[Index])
	{
		return false;
	}
	this->Revealed[Index] = 1;
	WritePixel(Index);

	// 보이는 빈 칸에 맞닿은 벽도 보인다. 비스듬한 레이가 바닥·벽을 띄엄띄엄 맞혀 윤곽이 점선이 되는 것을 막는다
	if (this->CellKinds[Index] == MinimapPrivate::Empty)
	{
		const int32 X = Index % this->GridWidth;
		const int32 Z = Index / this->GridWidth;
		for (int32 DZ = -1; DZ <= 1; ++DZ)
		{
			for (int32 DX = -1; DX <= 1; ++DX)
			{
				const int32 NX = X + DX;
				const int32 NZ = Z + DZ;
				if ((DX == 0 && DZ == 0) || NX < 0 || NZ < 0 || NX >= this->GridWidth || NZ >= this->GridHeight)
				{
					continue;
				}
				const int32 Neighbor = CellIndex(NX, NZ);
				if (this->CellKinds[Neighbor] != MinimapPrivate::Empty && !this->Revealed[Neighbor] && !IsCellMasked(Neighbor))
				{
					this->Revealed[Neighbor] = 1;
					WritePixel(Neighbor);
				}
			}
		}
	}
	return true;
}

void UMinimapSubsystem::WritePixel(int32 Index)
{
	const int32 X = Index % this->GridWidth;
	const int32 Z = Index / this->GridWidth;
	// 텍스처는 위쪽 행이 높은 Z다
	const int32 PixelIndex = (this->GridHeight - 1 - Z) * this->GridWidth + X;

	FColor Color(0, 0, 0, 0);
	// 숨김 칸은 밝혀져도 그리지 않는다
	if (this->Revealed[Index] && this->CellKinds[Index] != MinimapPrivate::Hidden)
	{
		switch (this->CellKinds[Index])
		{
		case MinimapPrivate::Solid: Color = this->SolidColor; break;
		case MinimapPrivate::Edge:  Color = this->EdgeColor;  break;
		default:                    Color = this->EmptyColor; break;
		}
		// 페인트 볼륨 Color로 칠한 칸
		if (this->CellTints.IsValidIndex(Index) && this->CellTints[Index].A > 0)
		{
			Color = this->CellTints[Index];
		}
	}
	this->Pixels[PixelIndex] = Color;
	this->bTextureDirty = true;
}

void UMinimapSubsystem::FlushTexture()
{
	if (!this->bTextureDirty || !this->Texture || this->Pixels.IsEmpty())
	{
		return;
	}
	this->bTextureDirty = false;

	// 렌더 스레드가 나중에 읽으므로 복사본을 넘기고 다 쓰면 지운다
	const int32 Bytes = this->Pixels.Num() * sizeof(FColor);
	uint8* Data = static_cast<uint8*>(FMemory::Malloc(Bytes));
	FMemory::Memcpy(Data, this->Pixels.GetData(), Bytes);
	FUpdateTextureRegion2D* Region = new FUpdateTextureRegion2D(0, 0, 0, 0, this->GridWidth, this->GridHeight);

	this->Texture->UpdateTextureRegions(0, 1, Region, this->GridWidth * sizeof(FColor), sizeof(FColor), Data,
		[](uint8* InData, const FUpdateTextureRegion2D* InRegions)
		{
			FMemory::Free(InData);
			delete InRegions;
		});
}

// ---------------------------------------------------------------------------
// 저장

int32 UMinimapSubsystem::ComputeGridHash() const
{
	// 범위나 칸 크기가 바뀌면 예전 기록을 버린다
	uint32 Hash = GetTypeHash(this->GridWidth);
	Hash = HashCombine(Hash, GetTypeHash(this->GridHeight));
	Hash = HashCombine(Hash, GetTypeHash(FMath::RoundToInt(this->OriginX)));
	Hash = HashCombine(Hash, GetTypeHash(FMath::RoundToInt(this->OriginZ)));
	Hash = HashCombine(Hash, GetTypeHash(FMath::RoundToInt(this->CellSize)));
	return static_cast<int32>(Hash);
}

void UMinimapSubsystem::LoadSavedReveal()
{
	UGameSaveSubsystem* Save = UGameSaveSubsystem::Get(this);
	TArray<uint8> Bits;
	if (!Save || !Save->TakeMinimapReveal(this, ComputeGridHash(), Bits))
	{
		return;
	}

	const int32 CellCount = this->GridWidth * this->GridHeight;
	if (Bits.Num() != (CellCount + 7) / 8)
	{
		return;
	}

	for (int32 Index = 0; Index < CellCount; ++Index)
	{
		if (Bits[Index >> 3] & (1 << (Index & 7)))
		{
			this->Revealed[Index] = 1;
			WritePixel(Index);
		}
	}
}

void UMinimapSubsystem::FlushReveal()
{
	if (this->bSaveDirty)
	{
		RecordReveal();
	}
}

void UMinimapSubsystem::RecordReveal()
{
	if (!this->bGridReady)
	{
		return;
	}

	UGameSaveSubsystem* Save = UGameSaveSubsystem::Get(this);
	if (!Save)
	{
		return;
	}

	const int32 CellCount = this->GridWidth * this->GridHeight;
	TArray<uint8> Bits;
	Bits.Init(0, (CellCount + 7) / 8);
	for (int32 Index = 0; Index < CellCount; ++Index)
	{
		if (this->Revealed[Index])
		{
			Bits[Index >> 3] |= (1 << (Index & 7));
		}
	}

	Save->RecordMinimapReveal(this, ComputeGridHash(), Bits);
	this->bSaveDirty = false;
	if (const UWorld* World = GetWorld())
	{
		this->LastRecordTime = World->GetTimeSeconds();
	}
}

// ---------------------------------------------------------------------------
// 마커

void UMinimapSubsystem::CollectMarkers()
{
	this->Markers.Reset();
	this->LoadedIcons.Reset();

	UWorld* World = GetWorld();
	const UMinimapMarkerSettings* Settings = GetDefault<UMinimapMarkerSettings>();
	if (!World || !Settings)
	{
		return;
	}

	// 규칙의 클래스를 한 번만 불러 둔다
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
		AActor* Actor = *It;
		if (!IsValid(Actor))
		{
			continue;
		}

		// 컴포넌트가 있으면 그 설정이 규칙보다 우선한다
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
		if (!Style)
		{
			continue;
		}

		FMarkerEntry& Entry = this->Markers.AddDefaulted_GetRef();
		Entry.Actor = Actor;
		Entry.Style = *Style;
		if (UTexture2D* Icon = Style->Icon.LoadSynchronous())
		{
			this->LoadedIcons.AddUnique(Icon);
			Entry.Icon = Icon;
		}
		// 상태·경로는 이름으로 찾은 UPROPERTY를 읽는다 (퍼즐 클래스를 고치지 않아도 되게)
		if (!Style->StateProperty.IsNone())
		{
			Entry.StateProperty = FindFProperty<FBoolProperty>(Actor->GetClass(), Style->StateProperty);
		}
		if (!Style->PathProperty.IsNone())
		{
			const FArrayProperty* ArrayProp = FindFProperty<FArrayProperty>(Actor->GetClass(), Style->PathProperty);
			if (ArrayProp && ArrayProp->Inner->IsA<FObjectPropertyBase>())
			{
				Entry.PathProperty = ArrayProp;
			}
		}
		// 문처럼 범위로 그리는 마커는 지금(닫힌 상태)의 충돌 범위를 기억해 둔다. 열리며 메시가 돌아가도 막대 자리는 그대로다
		if (Style->Shape == EMinimapMarkerShape::ActorBounds)
		{
			Entry.Bounds = Actor->GetComponentsBoundingBox(false, true);
		}
	}

	UE_LOG(LogEclipseTheNinja, Log, TEXT("[Minimap] 마커 %d개 등록"), this->Markers.Num());
}

bool UMinimapSubsystem::IsLocationVisibleOnMap(const FVector& WorldLocation) const
{
	int32 X = 0;
	int32 Z = 0;
	if (!WorldToCell(WorldLocation, X, Z))
	{
		return false;
	}
	const int32 Index = CellIndex(X, Z);
	// 숨김 칸(비밀 통로 등) 안의 마커는 보이지 않는다
	return this->Revealed[Index] && this->CellKinds[Index] != MinimapPrivate::Hidden;
}

bool UMinimapSubsystem::IsBoxVisibleOnMap(const FBox& WorldBox) const
{
	const int32 MinX = FMath::Clamp(FMath::FloorToInt((WorldBox.Min.X - this->OriginX) / this->CellSize), 0, this->GridWidth - 1);
	const int32 MaxX = FMath::Clamp(FMath::FloorToInt((WorldBox.Max.X - this->OriginX) / this->CellSize), 0, this->GridWidth - 1);
	const int32 MinZ = FMath::Clamp(FMath::FloorToInt((WorldBox.Min.Z - this->OriginZ) / this->CellSize), 0, this->GridHeight - 1);
	const int32 MaxZ = FMath::Clamp(FMath::FloorToInt((WorldBox.Max.Z - this->OriginZ) / this->CellSize), 0, this->GridHeight - 1);
	for (int32 Z = MinZ; Z <= MaxZ; ++Z)
	{
		for (int32 X = MinX; X <= MaxX; ++X)
		{
			const int32 Index = CellIndex(X, Z);
			if (this->Revealed[Index] && this->CellKinds[Index] != MinimapPrivate::Hidden)
			{
				return true;
			}
		}
	}
	return false;
}

void UMinimapSubsystem::UpdateMarkerDiscovery()
{
	for (FMarkerEntry& Entry : this->Markers)
	{
		if (Entry.bDiscovered)
		{
			continue;
		}
		const AActor* Actor = Entry.Actor.Get();
		if (!Actor)
		{
			continue;
		}
		// 문처럼 범위로 그리는 마커는 범위의 어느 칸이든 보이면 발견한다.
		// 닫힌 문은 시야를 막아서 문의 앞면 칸만 밝혀지므로 중심 칸으로 판정하면 영영 발견되지 않는다
		const bool bVisible = Entry.Bounds.IsValid ? IsBoxVisibleOnMap(Entry.Bounds) : IsLocationVisibleOnMap(Actor->GetActorLocation());
		if (bVisible)
		{
			Entry.bDiscovered = true;
		}
	}
}

void UMinimapSubsystem::GetMarkerViews(TArray<FMinimapMarkerView>& OutViews) const
{
	OutViews.Reset();
	for (const FMarkerEntry& Entry : this->Markers)
	{
		const AActor* Actor = Entry.Actor.Get();
		if (!Entry.bDiscovered || !IsValid(Actor) || Actor->IsHidden())
		{
			continue;
		}

		FMinimapMarkerView& View = OutViews.AddDefaulted_GetRef();
		View.Pixel = WorldToTexturePixel(Actor->GetActorLocation());
		View.Style = &Entry.Style;
		View.Icon = Entry.Icon.Get();
		View.Color = Entry.Style.Color;

		if (Entry.StateProperty && Entry.StateProperty->GetPropertyValue_InContainer(Actor))
		{
			View.Color = Entry.Style.StateColor;
			View.bStateOn = true;
		}

		if (Entry.Bounds.IsValid)
		{
			// 텍스처는 위쪽이 높은 Z라서 왼쪽 위 = (최소 X, 최대 Z)
			View.BoundsMin = WorldToTexturePixel(FVector(Entry.Bounds.Min.X, 0.f, Entry.Bounds.Max.Z));
			View.BoundsMax = WorldToTexturePixel(FVector(Entry.Bounds.Max.X, 0.f, Entry.Bounds.Min.Z));
		}

		// 지금 보이는 메시들의 범위. 흐리게 그릴지, 땅으로 그릴지 정할 때 쓴다
		const bool bWantGround = View.bStateOn && Entry.Style.bDrawAsGroundWhenStateOn;
		if (Entry.Style.bDimWhenMeshHidden || bWantGround)
		{
			FBox VisibleBounds(ForceInit);
			TArray<AActor*> Owners;
			Owners.Add(const_cast<AActor*>(Actor));
			Actor->GetAllChildActors(Owners, true);
			for (const AActor* Owner : Owners)
			{
				TInlineComponentArray<UPrimitiveComponent*> Prims(Owner);
				for (const UPrimitiveComponent* Prim : Prims)
				{
					if (Prim && Prim->IsA<UMeshComponent>() && Prim->IsVisible() && !Prim->bHiddenInGame)
					{
						VisibleBounds += Prim->Bounds.GetBox();
					}
				}
			}

			// 보이는 메시가 없으면(부서진 발판 등) 흐리게
			View.bDimmed = Entry.Style.bDimWhenMeshHidden && !VisibleBounds.IsValid;

			// 착지한 낙하물처럼 땅이 된 오브젝트는 그 자리를 땅으로 그린다
			if (bWantGround && VisibleBounds.IsValid)
			{
				View.bAsGround = true;
				View.BoundsMin = WorldToTexturePixel(FVector(VisibleBounds.Min.X, 0.f, VisibleBounds.Max.Z));
				View.BoundsMax = WorldToTexturePixel(FVector(VisibleBounds.Max.X, 0.f, VisibleBounds.Min.Z));
			}
		}

		if (Entry.PathProperty)
		{
			FScriptArrayHelper Helper(Entry.PathProperty, Entry.PathProperty->ContainerPtrToValuePtr<void>(Actor));
			const FObjectPropertyBase* Inner = CastField<FObjectPropertyBase>(Entry.PathProperty->Inner);
			for (int32 Index = 0; Index < Helper.Num(); ++Index)
			{
				if (const AActor* Point = Cast<AActor>(Inner->GetObjectPropertyValue(Helper.GetRawPtr(Index))))
				{
					View.Path.Add(WorldToTexturePixel(Point->GetActorLocation()));
				}
			}
		}
	}
}
