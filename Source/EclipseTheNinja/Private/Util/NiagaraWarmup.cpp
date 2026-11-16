#include "Util/NiagaraWarmup.h"

#include "Engine/World.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "TimerManager.h"

namespace
{
	// 프리웜용 재생 크기. 실제 게임에서 이 크기로 재생하는 일은 없으므로 프리웜 컴포넌트를 구분하는 데도 쓴다
	const FVector WarmupScale(0.001f);

	// 이만큼 그린 뒤 끈다. 시스템 초기화와 첫 렌더링이 끝나기에 충분한 시간
	constexpr float WarmupDuration = 0.2f;
}

void EclipseVFX::WarmupNiagaraSystem(const UObject* WorldContextObject, UNiagaraSystem* System, const FVector& Location)
{
	if (!WorldContextObject || !System) return;

	UWorld* World = WorldContextObject->GetWorld();
	if (!World || !World->IsGameWorld() || World->GetNetMode() == NM_DedicatedServer) return;

	// 화면에 보이지 않을 만큼 작게 재생한다. 거리 컬링에 걸려 재생이 생략되지 않도록 사전 컬링 검사는 끈다.
	UNiagaraComponent* Component = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		WorldContextObject, System, Location, FRotator::ZeroRotator, WarmupScale,
		true, true, ENCPoolMethod::AutoRelease, false);
	if (!Component) return;

	// 루프 이펙트는 스스로 끝나지 않으므로 잠시 뒤 끈다. 끝나면 AutoRelease로 풀에 돌아간다.
	// 한 번 터지는 이펙트는 그 전에 끝나 풀로 돌아가고 실제 재생에 다시 쓰일 수 있으므로,
	// 아직 프리웜 크기로 켜져 있는 경우에만 끈다.
	TWeakObjectPtr<UNiagaraComponent> WeakComponent(Component);
	FTimerHandle Handle;
	World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([WeakComponent]()
	{
		UNiagaraComponent* Warmed = WeakComponent.Get();
		if (Warmed && Warmed->IsActive() && Warmed->GetComponentScale().Equals(WarmupScale))
		{
			Warmed->DeactivateImmediate();
		}
	}), WarmupDuration, false);
}
