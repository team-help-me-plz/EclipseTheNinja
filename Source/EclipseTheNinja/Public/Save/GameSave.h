#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Checkpoint/CheckpointId.h"
#include "GAS/AbilityInfo.h"
#include "GameSave.generated.h"

/**
 * 현재 맵에 배치된 퍼즐 액터 하나의 저장 상태.
 * 액터마다 쓰는 의미가 다르다.
 *  - 문: bIsOn = 열림
 *  - 스위치·압력 발판: bIsUsed = 눌림
 *  - 레버: bIsOn = On 자세, bIsUsed = 당김
 *  - 덩굴: bIsUsed = 다 탐
 *  - 낙하물: bIsUsed = 착지, Location = 착지한 몸체의 상대 위치
 *  - 이동 발판: bIsOn = 움직임 켜짐, Progress = 경로 진행도
 */
USTRUCT()
struct FSavedActorState
{
	GENERATED_BODY()

	/** 켜짐 상태. 문은 열림, 레버는 On 자세, 이동 발판은 움직임. */
	UPROPERTY(SaveGame)
	bool bIsOn = false;

	/** 한 번 쓰고 나면 되돌아가지 않는 상태. 눌린 스위치, 다 탄 덩굴, 떨어진 낙하물. */
	UPROPERTY(SaveGame)
	bool bIsUsed = false;

	/** 액터 안에서 움직인 컴포넌트의 상대 위치. 낙하물의 착지 위치. */
	UPROPERTY(SaveGame)
	FVector Location = FVector::ZeroVector;

	/** 경로 진행도. 이동 발판이 멈춘 위치. */
	UPROPERTY(SaveGame)
	float Progress = 0.f;
};

/** 맵 하나에 배치된 퍼즐 액터들의 저장 상태. */
USTRUCT()
struct FSavedLevelState
{
	GENERATED_BODY()

	/** 키는 액터 이름. */
	UPROPERTY(SaveGame)
	TMap<FName, FSavedActorState> ActorStates;

	/** 미니맵에서 밝힌 칸. 한 칸이 1비트다. */
	UPROPERTY(SaveGame)
	TArray<uint8> MinimapRevealed;

	/** MinimapRevealed를 만든 격자의 범위·칸 크기 해시. 다르면 예전 기록을 버린다. */
	UPROPERTY(SaveGame)
	int32 MinimapGridHash = 0;
};

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

	/**
	 * 앞 맵의 출구가 지정한 도착 지점. Level을 열면 이 체크포인트의 "위치"로만 옮기고 비운다.
	 * 체크포인트를 활성화하지 않으므로 현재 체크포인트·사망 시 리스폰 지점은 그 맵의 원래 규칙(시작 체크포인트 등)을 따른다.
	 */
	UPROPERTY(SaveGame)
	ECheckpointId ArrivalCheckpoint = ECheckpointId::None;

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

	/** 맵별 퍼즐 액터 상태. 키는 맵 패키지 이름. 다음 맵으로 넘어가도 지우지 않아 이전 맵에 다시 들어가면 그대로 복원한다. */
	UPROPERTY(SaveGame)
	TMap<FName, FSavedLevelState> LevelStates;
};
