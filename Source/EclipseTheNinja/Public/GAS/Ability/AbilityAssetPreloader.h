#pragma once

#include "CoreMinimal.h"
#include "Engine/StreamableManager.h"
#include "UObject/SoftObjectPath.h"

/**
 * 어빌리티가 소프트 참조로 든 에셋(몽타주, VFX 등)을 미리 불러 두는 헬퍼.
 *  - OnGiveAbility에서 Request로 비동기 로드를 요청하고, OnRemoveAbility에서 Cancel로 놓는다
 *  - 핸들을 들고 있는 동안 에셋이 메모리에 남으므로, 시전할 때의 LoadSynchronous가 바로 끝난다
 *  - 다 불러오기 전에 시전하면 기존처럼 그 자리에서 동기 로드된다 (사용 쪽 코드는 바꾸지 않아도 된다)
 */
struct ECLIPSETHENINJA_API FAbilityAssetPreloader
{
	// 비어 있지 않은 경로만 모아 비동기 로드를 요청한다. 이전 요청이 있으면 놓고 다시 요청한다
	// OnLoaded : 모두 불러온 뒤 호출된다 (이미 메모리에 있으면 요청 즉시 호출될 수 있다)
	void Request(const TArray<FSoftObjectPath>& InPaths, FStreamableDelegate OnLoaded = FStreamableDelegate());

	// 요청을 취소하고 잡아 둔 에셋을 놓는다
	void Cancel();

	// 요청한 에셋을 모두 불러왔는지
	bool IsLoaded() const { return this->Handle.IsValid() && this->Handle->HasLoadCompleted(); }

private:
	TSharedPtr<FStreamableHandle> Handle;
};
