#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "AnimCropCommandlet.generated.h"

/**
 * 애니메이션 시퀀스의 일부 프레임 구간만 잘라 새 시퀀스로 저장하는 에디터 전용 커맨드릿.
 * 원본은 건드리지 않고 복제본에서 앞뒤 프레임을 지운다. 노티파이·커브도 엔진 Trim 규칙대로 함께 정리된다.
 *
 * 실행 (에디터를 끈 상태):
 *   UnrealEditor-Cmd.exe <프로젝트>.uproject -run=AnimCrop -Source=<애님 경로> [-Dest=<저장 패키지>] [-Start=N] [-End=N] [-Bones=a,b] [-Analyze]
 *   -Start / -End: 남길 첫 프레임과 마지막 프레임(포함). 생략하면 처음 / 끝.
 *   -Analyze: 저장하지 않고 프레임별로 -Bones 본(기본 hand_r)의 이동 속도를 출력한다. 휘두르는 구간을 찾을 때 쓴다.
 */
UCLASS()
class ECLIPSETHENINJA_API UAnimCropCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UAnimCropCommandlet();

	virtual int32 Main(const FString& Params) override;
};
