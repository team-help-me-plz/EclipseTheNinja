#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"

#include "EclipseLocalToolsSubsystem.generated.h"

/**
 * 에디터가 켜질 때 UEclipseLocalToolset을 ToolsetRegistry에 등록하고, 꺼질 때 해제한다.
 * C++ 툴셋은 자동으로 찾아지지 않으므로 엔진 툴셋(SlateInspectorToolset)처럼 서브시스템에서 직접 등록한다.
 */
UCLASS()
class UEclipseLocalToolsSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	// 툴셋을 등록해 MCP 툴 목록에 나타나게 한다
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// 등록한 툴셋을 해제한다
	virtual void Deinitialize() override;
};
