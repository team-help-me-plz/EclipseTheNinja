#include "EclipseLocalToolset.h"

#include "Editor.h"
#include "Misc/StringOutputDevice.h"
#include "UObject/Package.h"
#include "UObject/UObjectIterator.h"

#if WITH_LIVE_CODING
#include "ILiveCodingModule.h"
#endif

namespace
{
#if WITH_LIVE_CODING
	// Live Coding 모듈. 에디터 설정에서 꺼져 있거나 이 빌드에 없으면 nullptr
	ILiveCodingModule* GetLiveCoding()
	{
		return FModuleManager::GetModulePtr<ILiveCodingModule>(LIVE_CODING_MODULE_NAME);
	}

	FString ToString(ELiveCodingCompileResult Result)
	{
		switch (Result)
		{
		case ELiveCodingCompileResult::Success:
			return TEXT("Success");
		case ELiveCodingCompileResult::NoChanges:
			return TEXT("NoChanges");
		case ELiveCodingCompileResult::InProgress:
			return TEXT("InProgress");
		case ELiveCodingCompileResult::CompileStillActive:
			return TEXT("CompileStillActive");
		case ELiveCodingCompileResult::NotStarted:
			return TEXT("NotStarted");
		case ELiveCodingCompileResult::Failure:
			return TEXT("Failure");
		case ELiveCodingCompileResult::Cancelled:
			return TEXT("Cancelled");
		default:
			return TEXT("Unknown");
		}
	}
#endif
}

FString UEclipseLocalToolset::ExecuteConsoleCommand(const FString& Command)
{
	if (!GEditor)
	{
		return TEXT("handled=false\nGEditor가 없음");
	}

	// 에디터를 끄는 명령은 막는다. 저장 확인 없이 꺼지면 작업을 잃을 수 있다
	const FString	Trimmed = Command.TrimStartAndEnd();
	FString			FirstWord;
	FString			Rest;

	if (!Trimmed.Split(TEXT(" "), &FirstWord, &Rest))
	{
		FirstWord = Trimmed;
	}

	if (FirstWord.Equals(TEXT("quit"), ESearchCase::IgnoreCase) || FirstWord.Equals(TEXT("exit"), ESearchCase::IgnoreCase))
	{
		return TEXT("handled=false\n에디터 종료 명령은 실행하지 않음");
	}

	// PIE 중이면 PIE 월드, 아니면 에디터 월드에서 실행한다
	UWorld* World = GEditor->PlayWorld
		? GEditor->PlayWorld.Get()
		: GEditor->GetEditorWorldContext().World();

	FStringOutputDevice Output;
	const bool bHandled = GEditor->Exec(World, *Command, Output);

	return FString::Printf(TEXT("handled=%s\n%s"), bHandled ? TEXT("true") : TEXT("false"), *Output);
}

FString UEclipseLocalToolset::LiveCodingCompile()
{
#if WITH_LIVE_CODING
	ILiveCodingModule* LiveCoding = GetLiveCoding();

	if (!LiveCoding)
	{
		return TEXT("Unavailable");
	}

	// 끝날 때까지 게임 스레드에서 기다린다. 결과를 바로 돌려받기 위해서다
	ELiveCodingCompileResult Result = ELiveCodingCompileResult::Failure;
	LiveCoding->Compile(ELiveCodingCompileFlags::WaitForCompletion, &Result);

	return ToString(Result);
#else
	return TEXT("Unavailable");
#endif
}

FString UEclipseLocalToolset::GetLiveCodingStatus()
{
#if WITH_LIVE_CODING
	const ILiveCodingModule* LiveCoding = GetLiveCoding();

	if (!LiveCoding)
	{
		return TEXT("available=false");
	}

	return FString::Printf(
		TEXT("available=true enabledByDefault=%s started=%s compiling=%s"),
		LiveCoding->IsEnabledByDefault() ? TEXT("true") : TEXT("false"),
		LiveCoding->HasStarted() ? TEXT("true") : TEXT("false"),
		LiveCoding->IsCompiling() ? TEXT("true") : TEXT("false")
	);
#else
	return TEXT("available=false");
#endif
}

TArray<FString> UEclipseLocalToolset::GetDirtyPackages()
{
	TArray<FString> Names;

	// 메모리에 있는 모든 패키지를 본다. WP 외부 액터 패키지도 따로 잡힌다
	for (TObjectIterator<UPackage> It; It; ++It)
	{
		const UPackage* Package = *It;

		if (!Package->IsDirty() || Package == GetTransientPackage() || Package->HasAnyPackageFlags(PKG_CompiledIn))
		{
			continue;
		}

		// 저장 대상이 아닌 임시·스크립트·PIE 복제 패키지는 뺀다
		const FString	Name = Package->GetName();
		const bool		bTemporary = Name.StartsWith(TEXT("/Temp/"))
			|| Name.StartsWith(TEXT("/Script/"))
			|| Name.StartsWith(TEXT("/Engine/Transient"))
			|| Name.Contains(TEXT("UEDPIE_"));

		if (bTemporary)
		{
			continue;
		}

		Names.Add(Name);
	}

	Names.Sort();
	return Names;
}
