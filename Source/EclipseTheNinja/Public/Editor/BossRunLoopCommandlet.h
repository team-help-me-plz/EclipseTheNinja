#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "BossRunLoopCommandlet.generated.h"

/**
 * 끝부분이 잘린 달리기 클립으로 끊김 없이 반복되는 달리기 애니메이션을 만드는 에디터 전용 커맨드릿.
 *
 * 원본 클립은 오른발 착지 직전에 끝나 첫 프레임(오른발 접지)과 이어지지 않는다.
 * 대칭 달리기에서 왼발 착지 구간을 좌우 반전하면 오른발 착지 구간이 되므로, 하체는 그 반전 포즈로 빠진 프레임을 채운다.
 * - 이어 붙일 지점(Shift): 마지막 원본 키와 가장 닮은 반전 포즈. 그 앞 몇 프레임은 섞는다.
 * - 루프를 닫을 지점(Close): 첫 키와 가장 닮은 반전 포즈. 남은 차이는 이어 붙인 구간에 나눠 보정한다.
 * 칼을 든 상체는 반전하면 손이 바뀌므로 원본 흐름을 유지하다 끝에서 첫 프레임으로 보간한다.
 *
 * 실행 (에디터를 끈 상태):
 *   UnrealEditor-Cmd.exe <프로젝트>.uproject -run=BossRunLoop [-Source=<애님 경로>] [-Dest=<저장 경로>] [-Shift=N] [-Close=N] [-Analyze]
 *   -Analyze: 저장하지 않고 프레임별 발 높이와 이어 붙이기 후보만 출력한다.
 *   -Shift / -Close: 자동으로 고른 지점 대신 사용할 프레임 값.
 */
UCLASS()
class ECLIPSETHENINJA_API UBossRunLoopCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UBossRunLoopCommandlet();

	virtual int32 Main(const FString& Params) override;
};
