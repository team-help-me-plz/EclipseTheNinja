#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"

#include "EclipseLocalToolset.generated.h"

/**
 * EclipseTheNinja 로컬 전용 MCP 툴셋. 기본 에디터 MCP에 없는 콘솔 명령 실행, Live Coding 컴파일, 미저장 에셋 목록을 제공한다.
 * UEclipseLocalToolsSubsystem이 에디터 시작 때 ToolsetRegistry에 등록해 MCP 툴로 노출한다.
 */
UCLASS(BlueprintType)
class UEclipseLocalToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	/**
	 * Runs a console command in the editor world (or the PIE world while playing) and returns what it printed.
	 * Commands that quit the editor (quit, exit) are refused.
	 * @param Command  Console command line, e.g. "stat unit" or "LiveCoding.Compile".
	 */
	UFUNCTION(meta = (AICallable), Category = "EclipseLocalToolset")
	static FString ExecuteConsoleCommand(const FString& Command);

	/**
	 * Compiles changed C++ .cpp files with Live Coding and waits until it finishes.
	 * Header, UPROPERTY, UFUNCTION or constructor changes still need a full build with the editor closed.
	 * Returns one of: Success, NoChanges, Failure, NotStarted, CompileStillActive, Cancelled, Unavailable.
	 * On Failure, read the LogLiveCoding category with LogsToolset for the compiler errors.
	 */
	UFUNCTION(meta = (AICallable), Category = "EclipseLocalToolset")
	static FString LiveCodingCompile();

	/** Returns whether Live Coding is available, started and compiling right now. */
	UFUNCTION(meta = (AICallable), Category = "EclipseLocalToolset")
	static FString GetLiveCodingStatus();

	/** Returns the names of packages (assets and levels, including World Partition external actors) with unsaved changes. */
	UFUNCTION(meta = (AICallable), Category = "EclipseLocalToolset")
	static TArray<FString> GetDirtyPackages();
};
