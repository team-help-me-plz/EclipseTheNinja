


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
#include "Components/StaticMeshComponent.h"
#include "Misc/App.h"
#include "Materials/MaterialInstanceDynamic.h"

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

	RestoreOccluders();

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
		if (bOrthographic)
		{
			OutVT.POV.ProjectionMode = ECameraProjectionMode::Orthographic;
			OutVT.POV.OrthoWidth = OrthoWidth;   // 화면 가로에 보이는 월드 폭(cm)
		}

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

		// 암살 클로즈업 같은 연출은 일반 추적 위치 위에 섞는다. 카메라 셰이크는 그 뒤에 적용해 클로즈업 중에도 흔들린다.
		UpdateCloseUp(OutVT.POV, CurrentActorLocation);

		// Super를 부르지 않으므로 카메라 모디파이어(카메라 셰이크, 임팩트 프레임 흔들림 등)를 직접 적용한다.
		ApplyCameraModifiers(DeltaTime, OutVT.POV);

		// ApplyCameraModifiers가 포스트 프로세스 블렌드 목록을 비우므로 그 뒤에 넣는다.
		ApplyCloseUpBlur(OutVT.POV, CurrentActorLocation);

		UpdateOccluders(OutVT.POV.Location, TargetPawn);
	}
	else
	{
		// 시퀀서 카메라 컷처럼 폰이 아닌 액터가 뷰 타깃이면 그 액터의 카메라를 그대로 쓴다
		Super::UpdateViewTarget(OutVT, DeltaTime);
		RestoreOccluders();
	}
}

void APlayerCharacterCameraManager::UpdateOccluders(const FVector& CameraLocation, const APawn* TargetPawn)
{
	if (!bHideOccluders || !IsValid(TargetPawn) || !GetWorld() || (bHideOccludersOnlyInZones && OccluderZoneCount <= 0))
	{
		RestoreOccluders();
		return;
	}

	const FVector Target = TargetPawn->GetActorLocation() + FVector(0.0f, 0.0f, OccluderTargetZOffset);
	const FVector ToTarget = Target - CameraLocation;
	const float Distance = ToTarget.Size();
	if (Distance <= OccluderStopDistance + 1.0f)
	{
		RestoreOccluders();
		return;
	}
	const FVector SweepEnd = CameraLocation + ToTarget / Distance * (Distance - OccluderStopDistance);

	// 오브젝트 타입 쿼리는 막힘 없이 경로 위의 모든 정적 메시를 돌려준다
	TArray<FHitResult> Hits;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CameraOccluders), false, TargetPawn);
	GetWorld()->SweepMultiByObjectType(Hits, CameraLocation, SweepEnd, FQuat::Identity,
		FCollisionObjectQueryParams(ECC_WorldStatic), FCollisionShape::MakeBox(OccluderSweepHalfExtent), Params);

	TSet<TWeakObjectPtr<UPrimitiveComponent>> Occluders;
	for (const FHitResult& Hit : Hits)
	{
		UStaticMeshComponent* Mesh = Cast<UStaticMeshComponent>(Hit.GetComponent());
		if (!IsValid(Mesh)) continue;
		if (Mesh->ComponentHasTag(KeepVisibleTag)) continue;
		if (const AActor* MeshOwner = Mesh->GetOwner(); IsValid(MeshOwner) && MeshOwner->ActorHasTag(KeepVisibleTag)) continue;
		// 원래 보이지 않는 충돌용 메시(지붕 발판, 오르기 상자 등)는 건드리지 않는다
		if (!Mesh->IsVisible() && !HiddenOccluders.Contains(Mesh)) continue;
		Occluders.Add(Mesh);
	}

	for (const TWeakObjectPtr<UPrimitiveComponent>& Hidden : HiddenOccluders)
	{
		if (Hidden.IsValid() && !Occluders.Contains(Hidden))
		{
			Hidden->SetVisibility(true);
		}
	}
	for (const TWeakObjectPtr<UPrimitiveComponent>& Occluder : Occluders)
	{
		if (Occluder.IsValid() && !HiddenOccluders.Contains(Occluder))
		{
			Occluder->SetVisibility(false);
		}
	}
	HiddenOccluders = MoveTemp(Occluders);
}

void APlayerCharacterCameraManager::EnterOccluderZone()
{
	++OccluderZoneCount;
}

void APlayerCharacterCameraManager::ExitOccluderZone()
{
	OccluderZoneCount = FMath::Max(0, OccluderZoneCount - 1);
}

void APlayerCharacterCameraManager::RestoreOccluders()
{
	for (const TWeakObjectPtr<UPrimitiveComponent>& Hidden : HiddenOccluders)
	{
		if (Hidden.IsValid())
		{
			Hidden->SetVisibility(true);
		}
	}
	HiddenOccluders.Reset();
}

void APlayerCharacterCameraManager::SetCameraExploreMode(bool IsAiming)
{
	bIsCameraExploreMode = IsAiming;
}

void APlayerCharacterCameraManager::SmoothTeleport(const FVector& Delta)
{
	TeleportOffset -= Delta;
}

void APlayerCharacterCameraManager::StartCloseUp(AActor* FocusActor)
{
	if (!IsValid(FocusActor)) return;

	CloseUpFocus = FocusActor;
	CloseUpFocusLocation = FocusActor->GetActorLocation();
	bCloseUpActive = true;
}

void APlayerCharacterCameraManager::StopCloseUp()
{
	bCloseUpActive = false;
	bCloseUpBlurActive = false;
}

void APlayerCharacterCameraManager::UpdateCloseUp(FMinimalViewInfo& POV, const FVector& PawnLocation)
{
	// 암살 히트스톱·슬로모션 중에도 같은 빠르기로 움직이도록 게임 속도 배율이 빠진 실제 프레임 시간을 쓴다.
	const float RealDeltaTime = static_cast<float>(FApp::GetDeltaTime());
	if (bCloseUpActive)
	{
		CloseUpAlpha = CloseUpBlendInTime > 0.0f ? FMath::Min(1.0f, CloseUpAlpha + RealDeltaTime / CloseUpBlendInTime) : 1.0f;
	}
	else
	{
		CloseUpAlpha = CloseUpBlendOutTime > 0.0f ? FMath::Max(0.0f, CloseUpAlpha - RealDeltaTime / CloseUpBlendOutTime) : 0.0f;
	}

	if (CloseUpAlpha <= 0.0f)
	{
		CloseUpFocus.Reset();
		return;
	}

	if (const AActor* Focus = CloseUpFocus.Get())
	{
		CloseUpFocusLocation = Focus->GetActorLocation();
	}

	// 플레이어와 대상 사이를 초점으로, 같은 이동 평면에서 CloseUpZoom만큼 떨어진 곳에 카메라를 둔다.
	const FVector FocusPoint = FMath::Lerp(PawnLocation, CloseUpFocusLocation, CloseUpFocusWeight);
	const FVector CloseUpLocation(FocusPoint.X, PawnLocation.Y + CloseUpZoom, FocusPoint.Z + CloseUpZOffset);

	const float Blend = FMath::InterpEaseInOut(0.0f, 1.0f, CloseUpAlpha, 2.0f);
	POV.Location = FMath::Lerp(POV.Location, CloseUpLocation, Blend);
	if (POV.ProjectionMode == ECameraProjectionMode::Orthographic)
	{
		POV.OrthoWidth = FMath::Lerp(POV.OrthoWidth, POV.OrthoWidth * CloseUpOrthoWidthScale, Blend);
	}
}

void APlayerCharacterCameraManager::SetCloseUpBlur(bool bEnable)
{
	bCloseUpBlurActive = bEnable;
}

void APlayerCharacterCameraManager::ApplyCloseUpBlur(const FMinimalViewInfo& POV, const FVector& PawnLocation)
{
	const float RealDeltaTime = static_cast<float>(FApp::GetDeltaTime());
	const float Target = bCloseUpBlurActive ? 1.0f : 0.0f;
	CloseUpBlurAlpha = CloseUpBlurBlendTime > 0.0f
		? FMath::FInterpConstantTo(CloseUpBlurAlpha, Target, RealDeltaTime, 1.0f / CloseUpBlurBlendTime)
		: Target;

	if (CloseUpBlurAlpha <= 0.0f) return;

	if (CloseUpBlurMode == ECloseUpBlurMode::DepthOfField)
	{
		// 피사계 심도는 원근 투영에서만 동작한다.
		if (POV.ProjectionMode != ECameraProjectionMode::Perspective) return;

		// 캐릭터들이 선 이동 평면을 가운데 두고 앞뒤 CloseUpBlurFocalRegion만큼만 선명하게 남긴다.
		// 사이드뷰라 배경·전경은 이 구간 밖에 있어 흐려지고, 같은 평면의 플레이어와 대상은 선명하다.
		const float PlaneDistance = FMath::Abs(POV.Location.Y - PawnLocation.Y);
		const float HalfRegion = CloseUpBlurFocalRegion * 0.5f;

		FPostProcessSettings Settings;
		Settings.bOverride_DepthOfFieldFocalDistance = true;
		Settings.DepthOfFieldFocalDistance = FMath::Max(1.0f, PlaneDistance - HalfRegion);
		Settings.bOverride_DepthOfFieldFocalRegion = true;
		Settings.DepthOfFieldFocalRegion = CloseUpBlurFocalRegion;
		Settings.bOverride_DepthOfFieldMinFstop = true;
		Settings.DepthOfFieldMinFstop = 0.0f;
		Settings.bOverride_DepthOfFieldFstop = true;
		Settings.DepthOfFieldFstop = CloseUpBlurFstop;
		AddCachedPPBlend(Settings, FMath::InterpEaseInOut(0.0f, 1.0f, CloseUpBlurAlpha, 2.0f));
		return;
	}

	if (!IsValid(CloseUpBlurMID))
	{
		UMaterialInterface* Material = CloseUpBlurMaterial.LoadSynchronous();
		if (!IsValid(Material)) return;
		CloseUpBlurMID = UMaterialInstanceDynamic::Create(Material, this);
	}

	// 선명한 원의 중심은 플레이어와 대상 사이 초점이다. 카메라가 아직 다가가는 중이어도 초점을 따라간다.
	const FVector FocusPoint = FMath::Lerp(PawnLocation, CloseUpFocusLocation, CloseUpFocusWeight);
	FVector2D CenterUV(0.5, 0.5);
	ProjectToViewportUV(POV, FocusPoint, CenterUV);

	CloseUpBlurMID->SetVectorParameterValue(TEXT("Center"), FLinearColor(CenterUV.X, CenterUV.Y, 0.0f, 0.0f));
	CloseUpBlurMID->SetScalarParameterValue(TEXT("Radius"), CloseUpBlurRadius);
	CloseUpBlurMID->SetScalarParameterValue(TEXT("Falloff"), CloseUpBlurFalloff);
	CloseUpBlurMID->SetScalarParameterValue(TEXT("BlurStep"), CloseUpBlurStep);
	// 머티리얼 블렌더블은 가중치로 섞이지 않으므로 켜고 끄는 정도는 Strength로 준다.
	CloseUpBlurMID->SetScalarParameterValue(TEXT("Strength"), FMath::InterpEaseInOut(0.0f, 1.0f, CloseUpBlurAlpha, 2.0f));

	FPostProcessSettings Settings;
	Settings.WeightedBlendables.Array.Add(FWeightedBlendable(1.0f, CloseUpBlurMID));
	AddCachedPPBlend(Settings, 1.0f);
}

bool APlayerCharacterCameraManager::ProjectToViewportUV(const FMinimalViewInfo& POV, const FVector& WorldLocation, FVector2D& OutUV) const
{
	// 카메라 공간: X 앞, Y 오른쪽, Z 위
	const FVector Local = POV.Rotation.UnrotateVector(WorldLocation - POV.Location);

	int32 Width = 0;
	int32 Height = 0;
	if (PCOwner)
	{
		PCOwner->GetViewportSize(Width, Height);
	}
	const float Aspect = (Width > 0 && Height > 0) ? static_cast<float>(Width) / Height : 16.0f / 9.0f;

	// 가로 화면 반폭에 해당하는 거리 (원근은 깊이에 비례, 직교는 OrthoWidth 고정)
	float HalfWidth = 0.0f;
	if (POV.ProjectionMode == ECameraProjectionMode::Orthographic)
	{
		HalfWidth = POV.OrthoWidth * 0.5f;
	}
	else
	{
		if (Local.X <= 1.0f) return false;
		HalfWidth = Local.X * FMath::Tan(FMath::DegreesToRadians(POV.FOV * 0.5f));
	}
	if (HalfWidth <= KINDA_SMALL_NUMBER) return false;

	const float HalfHeight = HalfWidth / Aspect;
	OutUV.X = 0.5f + 0.5f * Local.Y / HalfWidth;
	OutUV.Y = 0.5f - 0.5f * Local.Z / HalfHeight;
	return true;
}
