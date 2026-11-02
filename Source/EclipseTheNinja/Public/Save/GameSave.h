#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Checkpoint/CheckpointId.h"
#include "GAS/AbilityInfo.h"
#include "GameSave.generated.h"

/**
 * 게임을 종료한 뒤에도 유지할 진행 데이터.
 * 저장 슬롯에는 값만 보관하고, 액터 이동과 능력 부여는 UGameSaveSubsystem이 처리한다.
 */
UCLASS()
class ECLIPSETHENINJA_API UGameSave : public USaveGame
{
	GENERATED_BODY()

public:
	/** 저장 형식 버전. 읽기 시 지원하는 버전인지 확인한다. */
	UPROPERTY(SaveGame)
	int32 Version = 1;

	/** 이어하기로 열 맵. 스테이지 클리어 시 다음 맵으로 갱신한다. */
	UPROPERTY(SaveGame)
	TSoftObjectPtr<UWorld> Level;

	/** 저장된 맵에서 마지막으로 활성화한 체크포인트. */
	UPROPERTY(SaveGame)
	ECheckpointId Checkpoint = ECheckpointId::None;

	/** 체크포인트의 재시작 위치와 회전. 종료 직전의 플레이어 위치는 아니다. */
	UPROPERTY(SaveGame)
	FTransform RespawnTransform;

	/** 새 게임이나 다음 스테이지처럼 저장된 재시작 위치가 없는 경우를 구분한다. */
	UPROPERTY(SaveGame)
	bool bHasRespawnTransform = false;

	/** 마지막으로 저장한 살아 있는 플레이어의 현재 체력. */
	UPROPERTY(SaveGame)
	float Health = 0.f;

	/** 초기 저장에는 체력이 없으므로 기본 스탯을 사용할지 구분한다. */
	UPROPERTY(SaveGame)
	bool bHasHealth = false;

	/** 클리어한 맵의 패키지 이름. 같은 맵은 중복 기록하지 않는다. */
	UPROPERTY(SaveGame)
	TArray<FName> ClearedLevels;

	/** 해금한 능력의 입력 ID. 실제 능력 클래스는 플레이어 Blueprint에서 지정한다. */
	UPROPERTY(SaveGame)
	TArray<EAbilityInputID> UnlockedAbilities;

	/** 종료형 최종 스테이지를 클리어했는지 여부. */
	UPROPERTY(SaveGame)
	bool bGameCompleted = false;
};
