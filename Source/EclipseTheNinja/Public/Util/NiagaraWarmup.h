#pragma once

#include "CoreMinimal.h"

class UNiagaraSystem;

namespace EclipseVFX
{
	/**
	 * 나이아가라 이펙트를 보이지 않을 만큼 작게 한 번 재생해, 처음 쓸 때 드는 비용(컴포넌트 생성, 시스템 초기화, 첫 렌더링)을 미리 치른다.
	 *  - 풀링(AutoRelease)으로 재생하므로 끝난 컴포넌트가 월드의 풀에 남는다. 실제 재생도 AutoRelease로 하면 그 컴포넌트를 다시 쓴다
	 *  - 한 번 터지고 끝나는 이펙트 전용이다. 계속 도는(루프) 이펙트에는 쓰지 않는다
	 *  - 게임 월드가 아니거나 데디케이티드 서버면 아무것도 하지 않는다
	 */
	ECLIPSETHENINJA_API void WarmupNiagaraSystem(const UObject* WorldContextObject, UNiagaraSystem* System, const FVector& Location);
}
