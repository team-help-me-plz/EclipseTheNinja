#include "Editor/BossRunLoopCommandlet.h"

#if WITH_EDITOR
#include "Animation/AnimSequence.h"
#include "Animation/AnimData/IAnimationDataController.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "Animation/Skeleton.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogBossRunLoop, Log, All);

UBossRunLoopCommandlet::UBossRunLoopCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

#if WITH_EDITOR
namespace BossRunLoop
{
	using FPose = TArray<FTransform>;

	// 부모부터 순서대로 로컬 포즈를 컴포넌트 공간으로 바꾼다. (레퍼런스 스켈레톤은 부모가 항상 앞에 있다)
	void ToComponentSpace(const FReferenceSkeleton& RefSkeleton, const FPose& Local, FPose& OutComponent)
	{
		OutComponent.SetNum(Local.Num());
		for (int32 BoneIndex = 0; BoneIndex < Local.Num(); ++BoneIndex)
		{
			const int32 ParentIndex = RefSkeleton.GetParentIndex(BoneIndex);
			OutComponent[BoneIndex] = ParentIndex == INDEX_NONE
				? Local[BoneIndex]
				: Local[BoneIndex] * OutComponent[ParentIndex];
		}
	}

	// 좌우 축에 대해 반사한 회전. 반사에서 회전축은 의사 벡터라 반사 축 성분만 남고 나머지 부호가 바뀐다.
	FQuat MirrorQuat(const FQuat& Q, EAxis::Type Axis)
	{
		switch (Axis)
		{
		case EAxis::X:
			return FQuat(Q.X, -Q.Y, -Q.Z, Q.W);
		case EAxis::Y:
			return FQuat(-Q.X, Q.Y, -Q.Z, Q.W);
		default:
			return FQuat(-Q.X, -Q.Y, Q.Z, Q.W);
		}
	}

	FVector MirrorVector(FVector V, EAxis::Type Axis)
	{
		V.SetComponentForAxis(Axis, -V.GetComponentForAxis(Axis));
		return V;
	}

	// thigh_l <-> thigh_r 처럼 끝의 _l/_r을 바꾼 이름. 좌우가 없는 본은 그대로 반환한다.
	FName MirrorBoneName(FName BoneName)
	{
		FString Name = BoneName.ToString();
		if (Name.EndsWith(TEXT("_l")))
		{
			return FName(*(Name.LeftChop(2) + TEXT("_r")));
		}

		if (Name.EndsWith(TEXT("_r")))
		{
			return FName(*(Name.LeftChop(2) + TEXT("_l")));
		}

		return BoneName;
	}

	bool IsInSubtree(const FReferenceSkeleton& RefSkeleton, int32 BoneIndex, int32 RootIndex)
	{
		if (RootIndex == INDEX_NONE)
		{
			return false;
		}

		for (int32 Index = BoneIndex; Index != INDEX_NONE; Index = RefSkeleton.GetParentIndex(Index))
		{
			if (Index == RootIndex)
			{
				return true;
			}
		}

		return false;
	}

	float SmoothStep(float Alpha)
	{
		const float A = FMath::Clamp(Alpha, 0.0f, 1.0f);
		return A * A * (3.0f - 2.0f * A);
	}

	FTransform BlendTransform(const FTransform& A, const FTransform& B, float Alpha)
	{
		FTransform Result;
		Result.SetRotation(FQuat::Slerp(A.GetRotation(), B.GetRotation(), Alpha).GetNormalized());
		Result.SetTranslation(FMath::Lerp(A.GetTranslation(), B.GetTranslation(), Alpha));
		Result.SetScale3D(FMath::Lerp(A.GetScale3D(), B.GetScale3D(), Alpha));
		return Result;
	}
}
#endif

int32 UBossRunLoopCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
	using namespace BossRunLoop;

	FString SourcePath = TEXT("/Game/_ExternalAssets/Oni_Samurai/Animation/Anim_Run.Anim_Run");
	FString DestPackageName = TEXT("/Game/Blueprint/Week_3/BossEnemyTest/Anim_Boss_RunLoop");
	FParse::Value(*Params, TEXT("Source="), SourcePath);
	FParse::Value(*Params, TEXT("Dest="), DestPackageName);
	const bool bAnalyzeOnly = FParse::Param(*Params, TEXT("Analyze"));

	UAnimSequence* Source = LoadObject<UAnimSequence>(nullptr, *SourcePath);
	if (!Source || !Source->GetSkeleton() || !Source->GetDataModel())
	{
		UE_LOG(LogBossRunLoop, Error, TEXT("원본 애니메이션을 불러오지 못했습니다: %s"), *SourcePath);
		return 1;
	}

	const FReferenceSkeleton& RefSkeleton = Source->GetSkeleton()->GetReferenceSkeleton();
	const IAnimationDataModel* Model = Source->GetDataModel();
	const int32 NumBones = RefSkeleton.GetNum();
	const int32 NumKeys = Model->GetNumberOfKeys();
	const int32 LastKey = NumKeys - 1;

	// 원본의 프레임별 로컬 포즈. 트랙이 없는 본은 레퍼런스 포즈를 쓴다.
	TArray<FPose> SourceLocal;
	SourceLocal.SetNum(NumKeys);
	for (int32 Key = 0; Key < NumKeys; ++Key)
	{
		SourceLocal[Key] = RefSkeleton.GetRefBonePose();
	}

	for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
	{
		const FName BoneName = RefSkeleton.GetBoneName(BoneIndex);
		if (!Model->IsValidBoneTrackName(BoneName))
		{
			continue;
		}

		TArray<FTransform> Track;
		Model->GetBoneTrackTransforms(BoneName, Track);
		for (int32 Key = 0; Key < NumKeys && Key < Track.Num(); ++Key)
		{
			SourceLocal[Key][BoneIndex] = Track[Key];
		}
	}

	TArray<FPose> SourceComponent;
	SourceComponent.SetNum(NumKeys);
	for (int32 Key = 0; Key < NumKeys; ++Key)
	{
		ToComponentSpace(RefSkeleton, SourceLocal[Key], SourceComponent[Key]);
	}

	FPose RefComponent;
	ToComponentSpace(RefSkeleton, RefSkeleton.GetRefBonePose(), RefComponent);

	// 좌우 축: 레퍼런스 포즈에서 thigh_l과 thigh_r이 부호만 다른 축.
	const int32 ThighL = RefSkeleton.FindBoneIndex(TEXT("thigh_l"));
	const int32 ThighR = RefSkeleton.FindBoneIndex(TEXT("thigh_r"));
	if (ThighL == INDEX_NONE || ThighR == INDEX_NONE)
	{
		UE_LOG(LogBossRunLoop, Error, TEXT("thigh_l / thigh_r 본을 찾지 못했습니다."));
		return 1;
	}

	const FVector ThighOffset = RefComponent[ThighL].GetTranslation() - RefComponent[ThighR].GetTranslation();
	const EAxis::Type MirrorAxis = FMath::Abs(ThighOffset.X) >= FMath::Abs(ThighOffset.Y) ? EAxis::X : EAxis::Y;

	// 발 높이로 왼발 접지 구간을 찾는다. ball 본이 없으면 foot 본을 쓴다.
	const int32 FootL = RefSkeleton.FindBoneIndex(TEXT("ball_l")) != INDEX_NONE
		? RefSkeleton.FindBoneIndex(TEXT("ball_l"))
		: RefSkeleton.FindBoneIndex(TEXT("foot_l"));
	const int32 FootR = RefSkeleton.FindBoneIndex(TEXT("ball_r")) != INDEX_NONE
		? RefSkeleton.FindBoneIndex(TEXT("ball_r"))
		: RefSkeleton.FindBoneIndex(TEXT("foot_r"));
	const int32 UpAxisIndex = 2;

	float MinL = TNumericLimits<float>::Max();
	for (int32 Key = 0; Key < NumKeys; ++Key)
	{
		MinL = FMath::Min(MinL, static_cast<float>(SourceComponent[Key][FootL].GetTranslation()[UpAxisIndex]));
	}

	UE_LOG(LogBossRunLoop, Display, TEXT("본 %d개, 키 %d개, %.2f fps, 좌우 축 %s"),
		NumBones,
		NumKeys,
		Model->GetFrameRate().AsDecimal(),
		MirrorAxis == EAxis::X ? TEXT("X") : TEXT("Y"));

	int32 FirstContact = INDEX_NONE;
	int32 DetectedHalf = INDEX_NONE;
	for (int32 Key = 0; Key < NumKeys; ++Key)
	{
		const float HeightL = SourceComponent[Key][FootL].GetTranslation()[UpAxisIndex];
		const float HeightR = SourceComponent[Key][FootR].GetTranslation()[UpAxisIndex];
		const bool bContactL = HeightL <= MinL + 3.0f;
		UE_LOG(LogBossRunLoop, Display, TEXT("  프레임 %2d  왼발 %7.2f  오른발 %7.2f  %s"),
			Key,
			HeightL,
			HeightR,
			bContactL ? TEXT("왼발 접지") : TEXT(""));

		if (bContactL && FirstContact == INDEX_NONE)
		{
			FirstContact = Key;
		}

		if (!bContactL && FirstContact != INDEX_NONE && DetectedHalf == INDEX_NONE)
		{
			DetectedHalf = Key;
		}
	}

	UE_LOG(LogBossRunLoop, Display, TEXT("왼발 접지 시작 %d, 왼발 이륙 %d"), FirstContact, DetectedHalf);

	// 1배속 지면 속도 추정: 땅에 닿아 있는 발이 앞뒤 축으로 뒤로 밀리는 속도.
	// 제자리 애니메이션이면 이 속도로 움직일 때 발이 미끄러지지 않는다. ABP의 *AnimSpeed 값을 정할 때 쓴다.
	{
		const int32 ForwardAxisIndex = MirrorAxis == EAxis::X ? 1 : 0;
		const double FrameRate = Model->GetFrameRate().AsDecimal();
		float MinR = TNumericLimits<float>::Max();
		for (int32 Key = 0; Key < NumKeys; ++Key)
		{
			MinR = FMath::Min(MinR, static_cast<float>(SourceComponent[Key][FootR].GetTranslation()[UpAxisIndex]));
		}

		double SpeedSum = 0.0;
		int32 SpeedCount = 0;
		for (int32 Key = 0; Key < LastKey; ++Key)
		{
			for (const TPair<int32, float>& Foot : { TPair<int32, float>(FootL, MinL), TPair<int32, float>(FootR, MinR) })
			{
				const FVector Current = SourceComponent[Key][Foot.Key].GetTranslation();
				const FVector Next = SourceComponent[Key + 1][Foot.Key].GetTranslation();
				if (Current[UpAxisIndex] > Foot.Value + 3.0f || Next[UpAxisIndex] > Foot.Value + 3.0f)
				{
					continue;
				}

				SpeedSum += FMath::Abs(Next[ForwardAxisIndex] - Current[ForwardAxisIndex]) * FrameRate;
				++SpeedCount;
			}
		}

		UE_LOG(LogBossRunLoop, Display, TEXT("접지 발의 앞뒤 이동 속도(1배속 지면 속도 추정): %.1f cm/s (표본 %d)"),
			SpeedCount > 0 ? SpeedSum / SpeedCount : 0.0,
			SpeedCount);
	}

	// 하체 포즈 비교: 골반 기준 다리 본 위치. A 포즈와 B 포즈의 좌우 반전이 얼마나 다른지(cm).
	const TArray<FName> LegBoneNames = { TEXT("calf_l"), TEXT("calf_r"), TEXT("foot_l"), TEXT("foot_r"), TEXT("ball_l"), TEXT("ball_r") };
	const int32 PelvisBone = RefSkeleton.FindBoneIndex(TEXT("pelvis"));
	auto LegPosition = [&](int32 Key, int32 BoneIndex)
	{
		return SourceComponent[Key][BoneIndex].GetTranslation() - SourceComponent[Key][PelvisBone].GetTranslation();
	};
	auto MirrorDistance = [&](int32 KeyA, int32 KeyB)
	{
		float Sum = 0.0f;
		for (const FName& LegName : LegBoneNames)
		{
			const int32 Bone = RefSkeleton.FindBoneIndex(LegName);
			const int32 Mirrored = RefSkeleton.FindBoneIndex(MirrorBoneName(LegName));
			if (Bone == INDEX_NONE || Mirrored == INDEX_NONE)
			{
				continue;
			}

			Sum += FVector::Dist(LegPosition(KeyA, Bone), MirrorVector(LegPosition(KeyB, Mirrored), MirrorAxis));
		}

		return Sum;
	};

	// 이어 붙일 지점: 마지막 원본 키와 가장 닮은 반전 포즈 -> Shift. 루프를 닫을 지점: 첫 키와 가장 닮은 반전 포즈 -> Close.
	int32 ShiftFrames = 0;
	int32 CloseKey = 0;
	FParse::Value(*Params, TEXT("Shift="), ShiftFrames);
	FParse::Value(*Params, TEXT("Close="), CloseKey);
	if (ShiftFrames <= 0)
	{
		float Best = TNumericLimits<float>::Max();
		for (int32 Candidate = 1; Candidate <= LastKey; ++Candidate)
		{
			const float Distance = MirrorDistance(LastKey, LastKey - Candidate);
			UE_LOG(LogBossRunLoop, Display, TEXT("  이어 붙이기 후보 Shift=%2d : 마지막 키 vs 반전(%2d) = %6.1f"), Candidate, LastKey - Candidate, Distance);
			if (Distance < Best)
			{
				Best = Distance;
				ShiftFrames = Candidate;
			}
		}
	}

	if (CloseKey <= 0)
	{
		float Best = TNumericLimits<float>::Max();
		for (int32 Candidate = 1; Candidate <= LastKey; ++Candidate)
		{
			// 닫는 지점은 이어 붙인 뒤여야 한다.
			if (Candidate + ShiftFrames <= LastKey)
			{
				continue;
			}

			const float Distance = MirrorDistance(0, Candidate);
			UE_LOG(LogBossRunLoop, Display, TEXT("  루프 닫기 후보 반전(%2d) vs 첫 키 = %6.1f"), Candidate, Distance);
			if (Distance < Best)
			{
				Best = Distance;
				CloseKey = Candidate;
			}
		}
	}

	const int32 CycleFrames = ShiftFrames + CloseKey;
	if (ShiftFrames <= 0 || CloseKey <= 0 || CycleFrames <= LastKey)
	{
		UE_LOG(LogBossRunLoop, Error, TEXT("이어 붙일 지점을 찾지 못했습니다. (Shift %d, Close %d)"), ShiftFrames, CloseKey);
		return 1;
	}

	UE_LOG(LogBossRunLoop, Display, TEXT("새 사이클: %d프레임 (원본 0~%d + 반전(%d~%d)), 이음새 오차 %.1f / 루프 오차 %.1f"),
		CycleFrames,
		LastKey,
		LastKey + 1 - ShiftFrames,
		CloseKey - 1,
		MirrorDistance(LastKey, LastKey - ShiftFrames),
		MirrorDistance(0, CloseKey));

	if (bAnalyzeOnly)
	{
		return 0;
	}

	// 본 분류: 하체(골반, 다리, 발 IK)는 반전으로 채우고, 나머지(상체·칼)는 첫 프레임으로 보간한다.
	const int32 Pelvis = RefSkeleton.FindBoneIndex(TEXT("pelvis"));
	const int32 IkFootRoot = RefSkeleton.FindBoneIndex(TEXT("ik_foot_root"));
	TArray<bool> bLower;
	TArray<bool> bPositionDriven;
	TArray<int32> MirrorSource;
	bLower.SetNum(NumBones);
	bPositionDriven.SetNum(NumBones);
	MirrorSource.SetNum(NumBones);
	for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
	{
		bLower[BoneIndex] = BoneIndex == Pelvis
			|| IsInSubtree(RefSkeleton, BoneIndex, ThighL)
			|| IsInSubtree(RefSkeleton, BoneIndex, ThighR)
			|| IsInSubtree(RefSkeleton, BoneIndex, IkFootRoot);

		// 골반과 발 IK 본은 위치도 반전한다. 나머지 본은 본 길이를 유지하도록 자기 로컬 이동값을 쓴다.
		bPositionDriven[BoneIndex] = BoneIndex == Pelvis || IsInSubtree(RefSkeleton, BoneIndex, IkFootRoot);

		const int32 Mirrored = RefSkeleton.FindBoneIndex(MirrorBoneName(RefSkeleton.GetBoneName(BoneIndex)));
		MirrorSource[BoneIndex] = Mirrored == INDEX_NONE ? BoneIndex : Mirrored;
	}

	// SourceKey 포즈의 좌우 반전을 BoneIndex 본의 컴포넌트 공간 목표로 만든다.
	// 레퍼런스 보정: 반전한 원본 본의 레퍼런스 회전이 대상 본의 레퍼런스 회전과 같아지도록 한다.
	auto MirroredComponent = [&](int32 SourceKey, int32 BoneIndex)
	{
		const int32 SourceBone = MirrorSource[BoneIndex];
		const FTransform& SourceCS = SourceComponent[SourceKey][SourceBone];
		const FQuat RefCorrection = MirrorQuat(RefComponent[SourceBone].GetRotation(), MirrorAxis).Inverse()
			* RefComponent[BoneIndex].GetRotation();

		FTransform Result = SourceCS;
		Result.SetRotation((MirrorQuat(SourceCS.GetRotation(), MirrorAxis) * RefCorrection).GetNormalized());
		Result.SetTranslation(MirrorVector(SourceCS.GetTranslation(), MirrorAxis));
		return Result;
	};

	// 루프를 닫는 반전 포즈와 첫 키의 차이. 이어 붙인 구간에 조금씩 나눠 더해 마지막 프레임이 첫 키와 같아지게 한다.
	TArray<FQuat> CloseRotationDelta;
	TArray<FVector> ClosePositionDelta;
	CloseRotationDelta.SetNum(NumBones);
	ClosePositionDelta.SetNum(NumBones);
	for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
	{
		const FTransform CloseMirror = MirroredComponent(CloseKey, BoneIndex);
		const FTransform& FirstKey = SourceComponent[0][BoneIndex];
		CloseRotationDelta[BoneIndex] = (FirstKey.GetRotation() * CloseMirror.GetRotation().Inverse()).GetNormalized();
		ClosePositionDelta[BoneIndex] = FirstKey.GetTranslation() - CloseMirror.GetTranslation();
	}

	// 이어 붙이는 지점 앞 몇 프레임은 원본과 반전 포즈를 섞는다.
	const int32 SeamBlendFrames = FMath::Clamp(2, 0, LastKey - ShiftFrames);

	TArray<FPose> NewLocal;
	NewLocal.SetNum(CycleFrames + 1);
	for (int32 Frame = 0; Frame < CycleFrames; ++Frame)
	{
		FPose& Local = NewLocal[Frame];
		Local.SetNum(NumBones);
		FPose NewComponent;
		NewComponent.SetNum(NumBones);

		// 하체: 마지막 원본 키 근처에서 반전 포즈로 넘어가고, 그 뒤로는 반전 포즈에 루프 보정을 더한다.
		const int32 MirrorKey = Frame - ShiftFrames;
		const int32 SeamStart = LastKey - SeamBlendFrames;
		const float LowerAlpha = Frame <= SeamStart
			? 0.0f
			: FMath::Min(1.0f, static_cast<float>(Frame - SeamStart) / (SeamBlendFrames + 1));
		const float CloseAlpha = Frame <= LastKey
			? 0.0f
			: static_cast<float>(Frame - LastKey) / (CycleFrames - LastKey);

		// 상체: 마지막 원본 키 이후에는 마지막 키에서 첫 키로 보간한다.
		const float UpperAlpha = Frame <= LastKey
			? 0.0f
			: SmoothStep(static_cast<float>(Frame - LastKey) / (CycleFrames - LastKey));
		const int32 OriginalKey = FMath::Min(Frame, LastKey);

		for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
		{
			FTransform TargetCS;
			FTransform OwnLocal;
			if (bLower[BoneIndex])
			{
				FTransform Mirrored = MirroredComponent(FMath::Clamp(MirrorKey, 0, LastKey), BoneIndex);
				if (CloseAlpha > 0.0f)
				{
					const FQuat Correction = FQuat::Slerp(FQuat::Identity, CloseRotationDelta[BoneIndex], CloseAlpha);
					Mirrored.SetRotation((Correction * Mirrored.GetRotation()).GetNormalized());
					Mirrored.AddToTranslation(ClosePositionDelta[BoneIndex] * CloseAlpha);
				}

				const FTransform OriginalCS = SourceComponent[OriginalKey][BoneIndex];
				TargetCS = LowerAlpha <= 0.0f
					? OriginalCS
					: BlendTransform(OriginalCS, Mirrored, SmoothStep(LowerAlpha));
				OwnLocal = SourceLocal[Frame > LastKey ? FMath::Clamp(MirrorKey, 0, LastKey) : OriginalKey][BoneIndex];
			}
			else
			{
				TargetCS = BlendTransform(SourceComponent[OriginalKey][BoneIndex], SourceComponent[0][BoneIndex], UpperAlpha);
				OwnLocal = BlendTransform(SourceLocal[OriginalKey][BoneIndex], SourceLocal[0][BoneIndex], UpperAlpha);
			}

			const int32 ParentIndex = RefSkeleton.GetParentIndex(BoneIndex);
			if (ParentIndex == INDEX_NONE)
			{
				Local[BoneIndex] = OwnLocal;
				NewComponent[BoneIndex] = OwnLocal;
				continue;
			}

			FTransform BoneLocal = TargetCS.GetRelativeTransform(NewComponent[ParentIndex]);
			if (!bPositionDriven[BoneIndex])
			{
				BoneLocal.SetTranslation(OwnLocal.GetTranslation());
			}

			BoneLocal.SetScale3D(OwnLocal.GetScale3D());
			BoneLocal.NormalizeRotation();

			Local[BoneIndex] = BoneLocal;
			NewComponent[BoneIndex] = BoneLocal * NewComponent[ParentIndex];
		}
	}

	// 마지막 키는 첫 키와 같게 두어 루프 지점에서 그대로 이어지게 한다.
	NewLocal[CycleFrames] = NewLocal[0];

	// 대상 에셋: 이미 있으면 덮어쓰고, 없으면 원본을 복제한다.
	const FString DestAssetName = FPackageName::GetLongPackageAssetName(DestPackageName);
	const FString DestObjectPath = DestPackageName + TEXT(".") + DestAssetName;
	UAnimSequence* Dest = LoadObject<UAnimSequence>(nullptr, *DestObjectPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	UPackage* DestPackage = nullptr;
	if (Dest)
	{
		DestPackage = Dest->GetOutermost();
	}
	else
	{
		DestPackage = CreatePackage(*DestPackageName);
		Dest = DuplicateObject<UAnimSequence>(Source, DestPackage, FName(*DestAssetName));
		Dest->SetFlags(RF_Public | RF_Standalone);
	}

	IAnimationDataController& Controller = Dest->GetController();
	Controller.OpenBracket(FText::FromString(TEXT("Build boss run loop")), false);
	Controller.SetNumberOfFrames(FFrameNumber(CycleFrames), false);

	for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
	{
		const FName BoneName = RefSkeleton.GetBoneName(BoneIndex);
		if (!Dest->GetDataModel()->IsValidBoneTrackName(BoneName))
		{
			Controller.AddBoneCurve(BoneName, false);
		}

		TArray<FVector3f> Positions;
		TArray<FQuat4f> Rotations;
		TArray<FVector3f> Scales;
		for (int32 Key = 0; Key <= CycleFrames; ++Key)
		{
			const FTransform& BoneLocal = NewLocal[Key][BoneIndex];
			Positions.Add(FVector3f(BoneLocal.GetTranslation()));
			Rotations.Add(FQuat4f(BoneLocal.GetRotation()));
			Scales.Add(FVector3f(BoneLocal.GetScale3D()));
		}

		Controller.SetBoneTrackKeys(BoneName, Positions, Rotations, Scales, false);
	}

	Controller.CloseBracket(false);
	Dest->MarkPackageDirty();

	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	SaveArgs.SaveFlags = SAVE_NoError;
	const FString FileName = FPackageName::LongPackageNameToFilename(DestPackageName, FPackageName::GetAssetPackageExtension());
	if (!UPackage::SavePackage(DestPackage, Dest, *FileName, SaveArgs))
	{
		UE_LOG(LogBossRunLoop, Error, TEXT("저장 실패: %s"), *FileName);
		return 1;
	}

	UE_LOG(LogBossRunLoop, Display, TEXT("저장 완료: %s (%d프레임)"), *DestObjectPath, CycleFrames);
	return 0;
#else
	return 1;
#endif
}
