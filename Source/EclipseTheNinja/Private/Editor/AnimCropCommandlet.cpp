#include "Editor/AnimCropCommandlet.h"

#if WITH_EDITOR
#include "Animation/AnimSequence.h"
#include "Animation/AnimSequenceHelpers.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "Animation/Skeleton.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogAnimCrop, Log, All);

UAnimCropCommandlet::UAnimCropCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

#if WITH_EDITOR
namespace AnimCrop
{
	// 프레임별 컴포넌트 공간 본 위치를 출력한다. 칼을 휘두르는 구간은 손 본의 이동 속도가 크게 튄다.
	void PrintBoneSpeeds(const UAnimSequence* Sequence, const TArray<FString>& BoneNames)
	{
		const FReferenceSkeleton& RefSkeleton = Sequence->GetSkeleton()->GetReferenceSkeleton();
		const IAnimationDataModel* Model = Sequence->GetDataModel();
		const int32 NumBones = RefSkeleton.GetNum();
		const int32 NumKeys = Model->GetNumberOfKeys();

		// 트랙이 없는 본은 레퍼런스 포즈를 쓴다.
		TArray<TArray<FTransform>> Local;
		Local.SetNum(NumKeys);
		for (int32 Key = 0; Key < NumKeys; ++Key)
		{
			Local[Key] = RefSkeleton.GetRefBonePose();
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
				Local[Key][BoneIndex] = Track[Key];
			}
		}

		TArray<int32> BoneIndices;
		for (const FString& Name : BoneNames)
		{
			const int32 BoneIndex = RefSkeleton.FindBoneIndex(FName(*Name));
			if (BoneIndex == INDEX_NONE)
			{
				UE_LOG(LogAnimCrop, Warning, TEXT("본을 찾지 못했습니다: %s"), *Name);
				continue;
			}

			BoneIndices.Add(BoneIndex);
		}

		// 부모부터 순서대로 로컬 포즈를 컴포넌트 공간으로 바꿔 대상 본 위치만 모은다.
		TArray<TArray<FVector>> Positions;
		Positions.SetNum(NumKeys);
		for (int32 Key = 0; Key < NumKeys; ++Key)
		{
			TArray<FTransform> Component;
			Component.SetNum(NumBones);
			for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
			{
				const int32 ParentIndex = RefSkeleton.GetParentIndex(BoneIndex);
				Component[BoneIndex] = ParentIndex == INDEX_NONE
					? Local[Key][BoneIndex]
					: Local[Key][BoneIndex] * Component[ParentIndex];
			}

			for (const int32 BoneIndex : BoneIndices)
			{
				Positions[Key].Add(Component[BoneIndex].GetTranslation());
			}
		}

		const double FrameRate = Model->GetFrameRate().AsDecimal();
		UE_LOG(LogAnimCrop, Display, TEXT("키 %d개, %.2f fps, 길이 %.3f초"), NumKeys, FrameRate, Model->GetPlayLength());

		for (int32 Key = 0; Key < NumKeys; ++Key)
		{
			FString Line = FString::Printf(TEXT("  프레임 %3d (%.3f초)"), Key, Key / FrameRate);
			for (int32 Index = 0; Index < BoneIndices.Num(); ++Index)
			{
				const float Speed = Key > 0
					? FVector::Dist(Positions[Key][Index], Positions[Key - 1][Index])
					: 0.0f;
				const FVector& P = Positions[Key][Index];
				Line += FString::Printf(TEXT("  %s 속도 %6.2f 위치(%7.1f,%7.1f,%7.1f)"),
					*RefSkeleton.GetBoneName(BoneIndices[Index]).ToString(),
					Speed,
					P.X,
					P.Y,
					P.Z);
			}

			UE_LOG(LogAnimCrop, Display, TEXT("%s"), *Line);
		}
	}
}
#endif

int32 UAnimCropCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
	FString SourcePath;
	FString DestPackageName;
	FString BonesParam = TEXT("hand_r");
	int32 StartFrame = 0;
	int32 EndFrame = INDEX_NONE;
	FParse::Value(*Params, TEXT("Source="), SourcePath);
	FParse::Value(*Params, TEXT("Dest="), DestPackageName);
	FParse::Value(*Params, TEXT("Bones="), BonesParam);
	FParse::Value(*Params, TEXT("Start="), StartFrame);
	FParse::Value(*Params, TEXT("End="), EndFrame);
	const bool bAnalyzeOnly = FParse::Param(*Params, TEXT("Analyze"));

	UAnimSequence* Source = LoadObject<UAnimSequence>(nullptr, *SourcePath);
	if (!Source || !Source->GetSkeleton() || !Source->GetDataModel())
	{
		UE_LOG(LogAnimCrop, Error, TEXT("원본 애니메이션을 불러오지 못했습니다: %s"), *SourcePath);
		return 1;
	}

	if (bAnalyzeOnly)
	{
		TArray<FString> BoneNames;
		BonesParam.ParseIntoArray(BoneNames, TEXT(","));
		AnimCrop::PrintBoneSpeeds(Source, BoneNames);
		return 0;
	}

	// 남길 구간 [StartFrame, EndFrame]을 확인한다. 마지막 키 번호는 프레임 수와 같다.
	const int32 LastFrame = Source->GetDataModel()->GetNumberOfFrames();
	if (EndFrame == INDEX_NONE)
	{
		EndFrame = LastFrame;
	}

	if (StartFrame < 0 || EndFrame > LastFrame || StartFrame >= EndFrame)
	{
		UE_LOG(LogAnimCrop, Error, TEXT("잘라낼 구간이 잘못되었습니다. Start %d, End %d (마지막 프레임 %d)"), StartFrame, EndFrame, LastFrame);
		return 1;
	}

	if (DestPackageName.IsEmpty())
	{
		UE_LOG(LogAnimCrop, Error, TEXT("-Dest=<저장 패키지>를 지정해야 합니다."));
		return 1;
	}

	const FString DestAssetName = FPackageName::GetLongPackageAssetName(DestPackageName);
	if (FPackageName::DoesPackageExist(DestPackageName))
	{
		UE_LOG(LogAnimCrop, Error, TEXT("이미 있는 에셋입니다. 다른 이름을 쓰거나 지운 뒤 실행하세요: %s"), *DestPackageName);
		return 1;
	}

	UPackage* DestPackage = CreatePackage(*DestPackageName);
	UAnimSequence* Dest = DuplicateObject<UAnimSequence>(Source, DestPackage, FName(*DestAssetName));
	Dest->SetFlags(RF_Public | RF_Standalone);

	// 뒤쪽부터 지워야 앞쪽 프레임 번호가 바뀌지 않는다. TRange는 [시작, 끝) 구간이다.
	if (EndFrame < LastFrame)
	{
		UE::Anim::AnimationData::Trim(Dest, TRange<FFrameNumber>(FFrameNumber(EndFrame + 1), FFrameNumber(LastFrame + 1)));
	}

	if (StartFrame > 0)
	{
		UE::Anim::AnimationData::Trim(Dest, TRange<FFrameNumber>(FFrameNumber(0), FFrameNumber(StartFrame)));
	}

	Dest->MarkPackageDirty();

	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	SaveArgs.SaveFlags = SAVE_NoError;
	const FString FileName = FPackageName::LongPackageNameToFilename(DestPackageName, FPackageName::GetAssetPackageExtension());
	if (!UPackage::SavePackage(DestPackage, Dest, *FileName, SaveArgs))
	{
		UE_LOG(LogAnimCrop, Error, TEXT("저장 실패: %s"), *FileName);
		return 1;
	}

	UE_LOG(LogAnimCrop, Display, TEXT("저장 완료: %s (원본 %d~%d 프레임, 길이 %.3f초)"),
		*DestPackageName,
		StartFrame,
		EndFrame,
		Dest->GetPlayLength());
	return 0;
#else
	return 1;
#endif
}
