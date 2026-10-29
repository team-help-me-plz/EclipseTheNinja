


#include "Character/PlayerCharacterCameraManager.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "Engine/HitResult.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "Engine/Canvas.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"

void APlayerCharacterCameraManager::BeginPlay()
{
	Super::BeginPlay();

#if !UE_BUILD_SHIPPING
	if (PCOwner && PCOwner->IsLocalPlayerController())
	{
		CameraBoundaryDrawHandle = AHUD::OnHUDPostRender.AddUObject(
			this, &APlayerCharacterCameraManager::DrawCameraExploreBoundary);
	}
#endif
}

void APlayerCharacterCameraManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (CameraBoundaryDrawHandle.IsValid())
	{
		AHUD::OnHUDPostRender.Remove(CameraBoundaryDrawHandle);
		CameraBoundaryDrawHandle.Reset();
	}

	Super::EndPlay(EndPlayReason);
}

void APlayerCharacterCameraManager::DrawCameraExploreBoundary(AHUD* HUD, UCanvas* Canvas)
{
	// The HUD delegate is global: only draw into this camera owner's viewport.
	if (!bDrawCameraExploreBoundary || !Canvas || !HUD || !PCOwner
		|| HUD->GetOwningPlayerController() != PCOwner)
	{
		return;
	}

	int32 Width = 0;
	int32 Height = 0;
	PCOwner->GetViewportSize(Width, Height);
	if (Width <= 0 || Height <= 0)
	{
		return;
	}

	// Use the same viewport-pixel coordinates and thresholds as the mouse calculation.
	const float Left = (1.f - CameraExploreBoundaryHalfWidth) * Width;
	const float Right = CameraExploreBoundaryHalfWidth * Width;
	const float Top = (1.f - CameraExploreBoundaryHalfHeight) * Height;
	const float Bottom = CameraExploreBoundaryHalfHeight * Height;
	const float Thickness = FMath::Max(1.f, CameraExploreBoundaryThickness);

	Canvas->K2_DrawLine(FVector2D(Left, Top), FVector2D(Right, Top), Thickness, CameraExploreBoundaryColor);
	Canvas->K2_DrawLine(FVector2D(Right, Top), FVector2D(Right, Bottom), Thickness, CameraExploreBoundaryColor);
	Canvas->K2_DrawLine(FVector2D(Right, Bottom), FVector2D(Left, Bottom), Thickness, CameraExploreBoundaryColor);
	Canvas->K2_DrawLine(FVector2D(Left, Bottom), FVector2D(Left, Top), Thickness, CameraExploreBoundaryColor);
}

void APlayerCharacterCameraManager::UpdateViewTarget(FTViewTarget& OutVT, float DeltaTime)
{
	//	Super::UpdateViewTarget(OutVT, DeltaTime);

		//0. 기본 캐릭터 추적 위치 계산
		// ensure the view target is a pawn
	APawn* TargetPawn = Cast<APawn>(OutVT.Target);
	if (IsValid(TargetPawn))
	{
		// set the view target FOV and rotation
		OutVT.POV.Rotation = FRotator(0.0f, -90.0f, 0.0f);
		OutVT.POV.FOV = 65.0f;

		// cache the current location
		FVector CurrentActorLocation = OutVT.Target->GetActorLocation();

		// 앉으면 캡슐이 줄어 액터 위치가 내려가므로, 선 자세 기준 높이로 보정해 카메라를 고정한다.
		if (const ACharacter* TargetCharacter = Cast<ACharacter>(TargetPawn))
		{
			CurrentActorLocation.Z += TargetCharacter->GetDefaultHalfHeight()
				- TargetCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		}

		FVector2D MoveOffset = FVector2D::ZeroVector;

		// calculate the "zoom distance" - in reality the distance we want to keep to the target
		float CurrentY = CurrentZoom + CurrentActorLocation.Y;

		// do first-time setup
		// 첫 프레임에 카메라가 플레이어 위치에 있도록 하기 위해서
		if (bSetup)
		{
			// lower the setup flag
			bSetup = false;

			// initialize the camera viewpoint and return
			OutVT.POV.Location.X = CurrentActorLocation.X;
			OutVT.POV.Location.Y = CurrentY;
			OutVT.POV.Location.Z = CurrentActorLocation.Z + CameraZOffset;

			// save the current camera height
			CurrentZ = OutVT.POV.Location.Z;

			// skip the rest of the calculations
			return;
		}


		//1. 탐색모드인 경우
		if (bIsCameraExploreMode)
		{
			float MouseX;
			float MouseY;
			int Width;
			int Height;

			if (PCOwner->GetMousePosition(MouseX, MouseY))
			{
				PCOwner->GetViewportSize(Width, Height);
				if (Width <= 0 || Height <= 0) return;

				//1.2 커서 위치를 뷰포트 크기로 정규화
				const FVector2D NormalizedMouse(MouseX / Width, MouseY / Height);

				//1.3 화면비를 통해 모든 해상도에서 동일한 비율로 이동하도록 설정
				const float AspectRatio = static_cast<float>(Width) / Height;
				
				// 탄젠트 값을 위한 깊이 
				const float CameraDepth = FMath::Abs(GetCameraLocation().Y - CurrentActorLocation.Y);

				// 전체 FOV의 40프로에 해당하는 길이를 이동시킴
				const float HalfWidth = CameraDepth * FMath::Tan(FMath::DegreesToRadians(OutVT.POV.FOV * CameraMoveDistance));
				// 높이는 비율에 맞춰서 계산
				const float HalfHeight = HalfWidth / AspectRatio;


				//1.3 수직 값이 가상의 박스 외부에 있으면 가장자리까지의 비율로 수직 이동비율 계산
				float VirticalDiffRatio = 0.f;
				if (NormalizedMouse.Y > CameraExploreBoundaryHalfHeight || NormalizedMouse.Y < 1 - CameraExploreBoundaryHalfHeight)
				{
					float MaxDiff = 1 - CameraExploreBoundaryHalfHeight;
					float Diff = NormalizedMouse.Y > CameraExploreBoundaryHalfHeight ? NormalizedMouse.Y - CameraExploreBoundaryHalfHeight : NormalizedMouse.Y - (1 - CameraExploreBoundaryHalfHeight);
					VirticalDiffRatio = Diff / MaxDiff;
				}

				//1.4 수평 값이 가상의 박스 외부에 있으면 가장자리까지의 비율로 수평 이동비율 계산
				float HorizeDiffRatio = 0.f;
				if (NormalizedMouse.X > CameraExploreBoundaryHalfWidth || NormalizedMouse.X < 1 - CameraExploreBoundaryHalfWidth)
				{
					float MaxDiff = 1 - CameraExploreBoundaryHalfWidth;
					float Diff = NormalizedMouse.X > CameraExploreBoundaryHalfWidth ? NormalizedMouse.X - CameraExploreBoundaryHalfWidth : NormalizedMouse.X - (1 - CameraExploreBoundaryHalfWidth);
					HorizeDiffRatio = Diff / MaxDiff;
				}

				FVector2D DiffRatioVector(HorizeDiffRatio, -VirticalDiffRatio);

				// 경계를 넘어가면 부드럽게 이동할지
				if (bUseSmoothCameraExplore)
				{
					// The rectangle controls activation and strength; direction follows the cursor.
					const float Strength = FMath::Clamp(
						FMath::Max(FMath::Abs(HorizeDiffRatio), FMath::Abs(VirticalDiffRatio)), 0.f, 1.f);
					const float SmoothStrength = Strength * Strength * (3.f - 2.f * Strength);
					const FVector2D CursorDirection(
						NormalizedMouse.X - 0.5f, 0.5f - NormalizedMouse.Y);
					DiffRatioVector = CursorDirection.GetSafeNormal() * SmoothStrength;
				}
				// 여기서는 동서남북 + 대각선 이동, 대각선은 비교적 자유로움
				else if (DiffRatioVector.SizeSquared() > 1.f)
				{
					DiffRatioVector.Normalize();
				}

				MoveOffset.X = DiffRatioVector.X * HalfWidth;
				MoveOffset.Y = DiffRatioVector.Y * HalfHeight;
			}
		}

		// 2. 기본 모드인 경우
		else if (!bIsCameraExploreMode)
		{
			//2-1 offset을 0으로 보간
			MoveOffset = FMath::Vector2DInterpTo(MoveOffset, FVector2D::ZeroVector, DeltaTime, CameraExploreSpeed);
		}

		// blend towards the new camera location and update the output

		float FinalX = FMath::Clamp(CurrentActorLocation.X + MoveOffset.X, CameraXMinBounds, CameraXMaxBounds);
		FVector TargetCameraLocation(FinalX, CurrentY, CurrentActorLocation.Z + CameraZOffset + MoveOffset.Y);

		// 순간이동 보정은 캐릭터 기준 오프셋이라 캐릭터가 움직이는 중에도 늦게 따라오지 않는다
		TeleportOffset = FMath::VInterpTo(TeleportOffset, FVector::ZeroVector, DeltaTime, TeleportSmoothSpeed);
		if (TeleportOffset.SizeSquared() < 0.01f)
		{
			TeleportOffset = FVector::ZeroVector;
		}
		TargetCameraLocation += TeleportOffset;

		if (bIsCameraExploreMode || !bFollowInstantlyInNormalMode)
		{
			OutVT.POV.Location = FMath::VInterpTo(GetCameraLocation(), TargetCameraLocation, DeltaTime, CameraReturnSpeed);
		}
		else
		{
			// 일반 모드는 캐릭터에 바로 붙는다. 탐색 모드에서 막 돌아왔을 때만 벌어진 거리를 부드럽게 줄인다.
			// 캐릭터 기준 오프셋을 줄이므로 캐릭터가 움직이는 중에도 늦게 따라오지 않는다.
			if (bWasCameraExploreMode)
			{
				ExploreReturnOffset = GetCameraLocation() - TargetCameraLocation;
			}
			ExploreReturnOffset = FMath::VInterpTo(ExploreReturnOffset, FVector::ZeroVector, DeltaTime, CameraReturnSpeed);
			if (ExploreReturnOffset.SizeSquared() < 1.0f)
			{
				ExploreReturnOffset = FVector::ZeroVector;
			}
			OutVT.POV.Location = TargetCameraLocation + ExploreReturnOffset;
		}
		bWasCameraExploreMode = bIsCameraExploreMode;
	}
}

void APlayerCharacterCameraManager::SetCameraExploreMode(bool IsAiming)
{
	bIsCameraExploreMode = IsAiming;
}

void APlayerCharacterCameraManager::SmoothTeleport(const FVector& Delta)
{
	TeleportOffset -= Delta;
}
