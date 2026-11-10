#include "Minimap/MinimapGridBuilder.h"
#include "Minimap/MinimapBoundsVolume.h"
#include "Minimap/MinimapData.h"
#include "Minimap/MinimapPaintVolume.h"
#include "Level/TargetCutawayVolume.h"
#include "PuzzelObject/BurnableVine.h"
#include "PuzzelObject/BaseDoor.h"
#include "PuzzelObject/BreakablePlatform.h"
#include "PuzzelObject/FallingObject.h"
#include "PuzzelObject/MovingPlatform.h"
#include "Components/ShapeComponent.h"

#include "Components/BoxComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "CollisionQueryParams.h"
#include "WorldCollision.h"

namespace MinimapGrid
{
	const FName IgnoreTag(TEXT("MinimapIgnore"));

	// 텍스처 한 변의 최대 칸 수
	static constexpr int32 MaxGridSize = 4096;

	bool ShouldSkipActor(const AActor* InActor)
	{
		return !IsValid(InActor) || InActor->IsA<APawn>() || InActor->ActorHasTag(IgnoreTag);
	}

	bool ShouldSkipComponent(const UPrimitiveComponent* InComponent)
	{
		if (!InComponent || InComponent->IsA<UShapeComponent>())
		{
			return true;
		}

		// 자식 액터(MeshAssembly)라면 부모를 따라 올라가며 덩굴인지 본다
		for (const AActor* Actor = InComponent->GetOwner(); Actor; Actor = Actor->GetParentActor())
		{
			// 움직이거나 사라지는 퍼즐 오브젝트는 시작 위치가 땅으로 굳어 버리므로 지도에 그리지 않고 마커로만 보여 준다
			if (ShouldSkipActor(Actor) || Actor->IsA<ABurnableVine>() || Actor->IsA<AMovingPlatform>() || Actor->IsA<ABreakablePlatform>() || Actor->IsA<AFallingObject>())
			{
				return true;
			}
		}
		return false;
	}

	bool ShouldSkipForGrid(const UPrimitiveComponent* InComponent)
	{
		if (ShouldSkipComponent(InComponent))
		{
			return true;
		}
		// 문은 지도에서 통로로 그린다 (마커가 열림·닫힘을 보여 준다). 시야는 닫힌 문이 계속 막는다
		for (const AActor* Actor = InComponent->GetOwner(); Actor; Actor = Actor->GetParentActor())
		{
			if (Actor->IsA<ABaseDoor>())
			{
				return true;
			}
		}
		return false;
	}

	void CollectCutawayTargets(UWorld* World, TArray<const AActor*>& OutActors)
	{
		if (!World)
		{
			return;
		}
		for (TActorIterator<ATargetCutawayVolume> It(World); It; ++It)
		{
			for (AActor* Target : It->GetTargetActors())
			{
				if (IsValid(Target))
				{
					OutActors.Add(Target);
					TArray<AActor*> Children;
					Target->GetAllChildActors(Children, true);
					OutActors.Append(Children);
				}
			}
		}
	}

	bool Build(UWorld* World, const AMinimapBoundsVolume* Bounds, float PlaneY, FMinimapGridResult& Out)
	{
		if (!World || !Bounds)
		{
			return false;
		}
		const FBox Box = Bounds->GetWorldBounds();
		if (!Box.IsValid)
		{
			return false;
		}

		Out = FMinimapGridResult();
		Out.CellSize = FMath::Max(25.f, Bounds->CellSize);
		Out.OriginX = Box.Min.X;
		Out.OriginZ = Box.Min.Z;
		Out.PlaneY = PlaneY;
		Out.Width = FMath::Clamp(FMath::CeilToInt(Box.GetSize().X / Out.CellSize), 1, MaxGridSize);
		Out.Height = FMath::Clamp(FMath::CeilToInt(Box.GetSize().Z / Out.CellSize), 1, MaxGridSize);

		const int32 W = Out.Width;
		const int32 H = Out.Height;
		const int32 CellCount = W * H;
		Out.Kinds.Init(static_cast<uint8>(EMinimapCellKind::Empty), CellCount);

		const auto CellCenter = [&Out](int32 X, int32 Z)
		{
			return FVector(Out.OriginX + (X + 0.5f) * Out.CellSize, Out.PlaneY, Out.OriginZ + (Z + 0.5f) * Out.CellSize);
		};

		// 1. 충돌로 벽 판정
		TArray<const AActor*> Ignored;
		CollectCutawayTargets(World, Ignored);
		FCollisionQueryParams Params(SCENE_QUERY_STAT(MinimapBuildGrid), false);
		Params.AddIgnoredActors(Ignored);
		const FCollisionShape CellShape = FCollisionShape::MakeBox(FVector(Out.CellSize * 0.49f, Bounds->PlaneHalfThickness, Out.CellSize * 0.49f));

		TArray<FOverlapResult> Overlaps;
		for (int32 Z = 0; Z < H; ++Z)
		{
			for (int32 X = 0; X < W; ++X)
			{
				Overlaps.Reset();
				World->OverlapMultiByChannel(Overlaps, CellCenter(X, Z), FQuat::Identity, Bounds->OcclusionChannel, CellShape, Params);
				for (const FOverlapResult& Overlap : Overlaps)
				{
					if (Overlap.bBlockingHit && !ShouldSkipForGrid(Overlap.GetComponent()))
					{
						Out.Kinds[Z * W + X] = static_cast<uint8>(EMinimapCellKind::Solid);
						break;
					}
				}
			}
		}

		// 2. 페인트 볼륨 (Priority 작은 것부터 적용해 큰 것이 이긴다)
		TArray<AMinimapPaintVolume*> Paints;
		for (TActorIterator<AMinimapPaintVolume> It(World); It; ++It)
		{
			if (IsValid(*It) && It->GetBox())
			{
				Paints.Add(*It);
			}
		}
		Paints.StableSort([](const AMinimapPaintVolume& A, const AMinimapPaintVolume& B) { return A.Priority < B.Priority; });

		TArray<uint8> HiddenMask;
		HiddenMask.Init(0, CellCount);
		for (const AMinimapPaintVolume* Paint : Paints)
		{
			const UBoxComponent* PaintBox = Paint->GetBox();
			const FTransform BoxTransform = PaintBox->GetComponentTransform();
			const FVector Extent = PaintBox->GetUnscaledBoxExtent();
			const FBox WorldBox = FBox(-Extent, Extent).TransformBy(BoxTransform);

			const int32 MinX = FMath::Clamp(FMath::FloorToInt((WorldBox.Min.X - Out.OriginX) / Out.CellSize), 0, W - 1);
			const int32 MaxX = FMath::Clamp(FMath::FloorToInt((WorldBox.Max.X - Out.OriginX) / Out.CellSize), 0, W - 1);
			const int32 MinZ = FMath::Clamp(FMath::FloorToInt((WorldBox.Min.Z - Out.OriginZ) / Out.CellSize), 0, H - 1);
			const int32 MaxZ = FMath::Clamp(FMath::FloorToInt((WorldBox.Max.Z - Out.OriginZ) / Out.CellSize), 0, H - 1);

			for (int32 Z = MinZ; Z <= MaxZ; ++Z)
			{
				for (int32 X = MinX; X <= MaxX; ++X)
				{
					// 깊이(Y)는 보지 않는다. 사이드뷰라 플레이 평면에서 떨어진 곳에 놓아도 적용되게 한다
					FVector Center = CellCenter(X, Z);
					Center.Y = BoxTransform.GetLocation().Y;
					const FVector Local = BoxTransform.InverseTransformPosition(Center);
					if (FMath::Abs(Local.X) > Extent.X || FMath::Abs(Local.Z) > Extent.Z)
					{
						continue;
					}

					const int32 Index = Z * W + X;
					switch (Paint->Mode)
					{
					case EMinimapPaintMode::Solid:
						Out.Kinds[Index] = static_cast<uint8>(EMinimapCellKind::Solid);
						HiddenMask[Index] = 0;
						break;
					case EMinimapPaintMode::Empty:
						Out.Kinds[Index] = static_cast<uint8>(EMinimapCellKind::Empty);
						HiddenMask[Index] = 0;
						break;
					case EMinimapPaintMode::Hidden:
						HiddenMask[Index] = 1;
						break;
					case EMinimapPaintMode::Color:
						if (Out.Tints.IsEmpty())
						{
							Out.Tints.Init(FColor(0, 0, 0, 0), CellCount);
						}
						Out.Tints[Index] = Paint->Color;
						break;
					}
				}
			}
		}

		// 3. 빈 칸과 맞닿은 벽은 윤곽
		const auto IsEmptyAt = [&Out, W, H](int32 X, int32 Z)
		{
			return X >= 0 && Z >= 0 && X < W && Z < H && Out.Kinds[Z * W + X] == static_cast<uint8>(EMinimapCellKind::Empty);
		};
		for (int32 Z = 0; Z < H; ++Z)
		{
			for (int32 X = 0; X < W; ++X)
			{
				uint8& Kind = Out.Kinds[Z * W + X];
				if (Kind == static_cast<uint8>(EMinimapCellKind::Solid)
					&& (IsEmptyAt(X - 1, Z) || IsEmptyAt(X + 1, Z) || IsEmptyAt(X, Z - 1) || IsEmptyAt(X, Z + 1)))
				{
					Kind = static_cast<uint8>(EMinimapCellKind::Edge);
				}
			}
		}

		// 4. 숨김
		for (int32 Index = 0; Index < CellCount; ++Index)
		{
			if (HiddenMask[Index])
			{
				Out.Kinds[Index] = static_cast<uint8>(EMinimapCellKind::Hidden);
			}
		}

		return true;
	}
}
