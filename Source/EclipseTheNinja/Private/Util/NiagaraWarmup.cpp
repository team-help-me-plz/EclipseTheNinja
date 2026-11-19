#include "Util/NiagaraWarmup.h"

#include "Engine/World.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

void EclipseVFX::WarmupNiagaraSystem(const UObject* WorldContextObject, UNiagaraSystem* System, const FVector& Location)
{
	if (!WorldContextObject || !System) return;

	const UWorld* World = WorldContextObject->GetWorld();
	if (!World || !World->IsGameWorld() || World->GetNetMode() == NM_DedicatedServer) return;

	// 화면에 보이지 않을 만큼 작게 재생한다. 거리 컬링에 걸려 재생이 생략되지 않도록 사전 컬링 검사는 끈다.
	const FVector WarmupScale(0.001f);
	UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		WorldContextObject, System, Location, FRotator::ZeroRotator, WarmupScale,
		true, true, ENCPoolMethod::AutoRelease, false);
}
