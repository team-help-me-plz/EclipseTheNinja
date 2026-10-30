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

    // Lvl_Week_2_Prototype_1 (STAGE 4 - 외곽 성곽)
    W2P1_MoatWall,              // 해자 + 분신 도약으로 외벽 오르기 + 성벽 위 원거리 경비
    W2P1_Courtyard,             // 공중 암살 + 겹친 레버 선택 (낙하물 / 안쪽 문)
    W2P1_SluiceGate,            // 배수로 발판을 분신이 눌러 여는 문 + 화살 함정 회랑
    W2P1_Storehouse,            // 넓은 가시밭 : 분신을 건너편에 보내 위치 교환
    W2P1_WatchPost,             // 발코니 원거리 경비 (하단 암살) + 출구 레버

    // Lvl_Week_2_Prototype_2 (STAGE 5 - 천수각)
    W2P2_Lift,                  // 분신이 발판을 누르는 동안만 올라가는 승강기
    W2P2_Gallery,               // 엄폐물 회랑 + 원거리 경비 하단 암살
    W2P2_Armory,                // 레버·분신 버튼으로 낙하물을 떨어뜨려 가시밭에 디딤돌 만들기
    W2P2_Roof,                  // 벽 점프로 지붕 오르기 + 호위 무사 암살 + 최종 관문

    // Lvl_Week_2_Prototype_3 (STAGE 4 고밀도판 - 외곽 성곽), Lvl_Week_2_Prototype_5 (같은 구조의 밤 판)
    W2P3_MoatGate,              // 부서지는 판자 + 높은 벽(분신 도약) + 발판 문 + 낙하물 버튼
    W2P3_InnerYard,             // 긴 가시밭 위치 교환 + 발판 유지 승강기 + 화살 함정 + 발코니 하단 암살

    // Lvl_Week_2_Prototype_4 (STAGE 5 고밀도판 - 천수각), Lvl_Week_2_Prototype_6 (같은 구조의 밤 판)
    W2P4_Armory,                // 가시 속 버튼으로 움직이는 플랫폼 + 하단 암살 + 벽 점프 수직 통로
    W2P4_UpperKeep,             // 가시 너머 플랫폼에 분신 -> 발판 -> 도착 직전 교환 + 레버 낙하물 + 최종 관문

    // Lvl_Week_3_Prototype_1 (외성 - 덩굴 성벽)
    W3P1_VineGate,              // 덩굴 벽 태우기 + 앉아서 지나는 통로 + 덩굴에 감긴 레버 문
    W3P1_Moat,                  // 모서리 매달리기·벽 슬라이딩으로 조용히 해자 진입 + 가시 너머 벽타기
    W3P1_Rampart,               // 성벽 위 은신구역 + 질풍참 공중 돌진 + 덩굴 문

    // Lvl_Week_3_Prototype_2 (천수각 내부)
    W3P2_Hall,                  // 순찰 회랑 은신구역 + 앉기 통로 + 분신 발판 문
    W3P2_Shaft,                 // 벽타기·벽 점프 수직 통로 + 매달리기·벽 슬라이딩 하강 암살 + 덩굴 문
    W3P2_Keep,                  // 부서지는 판자 + 질풍참 + 덩굴 레버 + 최종 관문

    // Lvl_BossEnemyTest
    BossTest_Arena,             // 보스 몬스터 테스트 구간
};
