// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CheckpointId.generated.h"

/**
 * 체크포인트 식별자.
 *  - 레벨이 선형이므로 선언 순서 = 진행 순서. (값이 클수록 이후에 위치한 체크포인트)
 *  - 중간에 끼워 넣는 것은 괜찮지만, 디스크 저장을 도입한 뒤에는 기존 값의 순서를 바꾸면 안 된다.
 */
UENUM(BlueprintType)
enum class ECheckpointId : uint8
{
    None = 0                UMETA(Hidden),

    // Lvl_CheckpointTest
    CheckpointTest_Start,       // 압력 발판 + 슬라이딩 문 + 밀 수 있는 상자
    CheckpointTest_Trap,        // 화살 함정 + 낙사 구덩이
    CheckpointTest_Clone,       // 분신으로 발판을 누르고 있어야 열리는 문
    CheckpointTest_Final,       // 상호작용 스위치 + 힌지 문 + 상자

    // Lvl_CheckpointTest_Enemy
    CheckpointEnemy_Start,      // 첫 번째 몬스터 구간
    CheckpointEnemy_Second,     // 두 번째 몬스터 구간

    // Lvl_Prototype
    Proto_S1_Tutorial,          // 이동/점프 + 상자를 밀어 압력 발판 위에 올리면 열리는 미닫이문
    Proto_S1_GuardPost,         // 지붕 아래를 순찰하는 경비병 (잠입 or 전투)
    Proto_S1_CloneGate,         // 분신으로 발판을 누르고 있어야 열리는 여닫이문
    Proto_S1_SpikeBridge,       // 가시 구덩이 + 분신 상호작용 스위치 + 이동 다리
    Proto_S2_TrapGauntlet,      // 화살 함정 회랑 + 무너지는 판자
    Proto_S2_Courtyard,         // 경비 2명 마당 + 가시밭 위치 교체 + 마지막 관문
};
