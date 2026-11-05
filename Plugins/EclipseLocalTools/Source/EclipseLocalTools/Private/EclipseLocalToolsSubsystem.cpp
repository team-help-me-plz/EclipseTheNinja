#include "EclipseLocalToolsSubsystem.h"

#include "EclipseLocalToolset.h"
#include "ToolsetRegistry/UToolsetRegistry.h"

void UEclipseLocalToolsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UToolsetRegistry::RegisterToolsetClass(UEclipseLocalToolset::StaticClass());
}

void UEclipseLocalToolsSubsystem::Deinitialize()
{
	UToolsetRegistry::UnregisterToolsetClass(UEclipseLocalToolset::StaticClass());

	Super::Deinitialize();
}
