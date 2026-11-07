# 캐릭터 사운드 에셋

요청 목록의 26개 사운드와 참조 의존성 23개를 추가했다. 기존 Cue 참조를 보존하도록 원본 `/Game/` 패키지 경로를 유지했다.

## 사용처와 경로

| 사용처 | 에셋 경로 |
| --- | --- |
| 검 공격 1타 | `/Game/MEGA_FULL_BUNDLE/COMBAT_FIGHTING_SOUNDS/CUES/Whooshes/whoosh_short_2_Cue` |
| 검 공격 2타 | `/Game/MEGA_FULL_BUNDLE/COMBAT_FIGHTING_SOUNDS/CUES/Whooshes/whoosh_short_3_Cue` |
| 검 공격 3타 | `/Game/ActionSoundsPack/Combat/cue/Stab_5-2_wav_Cue` |
| 불 공격 1타 | `/Game/ActionSoundsPack/Environment/cue/Fire_Whoosh_2-1_wav_Cue` |
| 불 공격 2타 | `/Game/ActionSoundsPack/Environment/cue/Fire_Whoosh_2-2_wav_Cue` |
| 불 공격 3타 | `/Game/ActionSoundsPack/Environment/cue/Fire_Whoosh_1-3_wav_Cue` |
| 얼음 공격 1타 | `/Game/MEGA_FULL_BUNDLE/BASKETBALL_SOCCER_GAME_SOUNDS/CUES/BONUS/net_swish_1_Cue` |
| 얼음 공격 2타 | `/Game/MEGA_FULL_BUNDLE/BASKETBALL_SOCCER_GAME_SOUNDS/CUES/BONUS/net_swish_2_Cue` |
| 얼음 공격 3타 | `/Game/MEGA_FULL_BUNDLE/BASKETBALL_SOCCER_GAME_SOUNDS/CUES/BONUS/net_swish_3_Cue` |
| 표창 투척 | `/Game/ActionSoundsPack/Combat/cue/Whoosh_Metal_2-2_wav_Cue` |
| 벽타기 | `/Game/MEGA_FULL_BUNDLE/INDOOR_FOOTSTEP_SOUNDS/CUES/PARQUET/Parquet_v5_Cue` |
| 벽타기 | `/Game/MEGA_FULL_BUNDLE/INDOOR_FOOTSTEP_SOUNDS/CUES/PARQUET/Parquet_v6_Cue` |
| 벽타기 | `/Game/MEGA_FULL_BUNDLE/INDOOR_FOOTSTEP_SOUNDS/CUES/PARQUET/Parquet_v8_Cue` |
| 벽타기 | `/Game/MEGA_FULL_BUNDLE/INDOOR_FOOTSTEP_SOUNDS/CUES/PARQUET/Parquet_v9_Cue` |
| 분신 소환 | `/Game/MEGA_FULL_BUNDLE/SURVIVAL_ESSENTIALS_SOUNDS/CUES/clothe_item_v9_Cue` |
| 암살 | `/Game/ActionSoundsPack/Combat/wav/Blood___Gore_1-2_wav` |
| 암살 | `/Game/MEGA_FULL_BUNDLE/FANTASY_RPG_SOUNDS/CUES/splash_something_squishy_v3_Cue` |
| 암살 | `/Game/MEGA_FULL_BUNDLE/MEDIEVAL_GAME_SOUNDS/CUES/blood_splash_v2_Cue` |
| 하단 암살 | `/Game/MEGA_FULL_BUNDLE/CARD_TABLE_GAME_SOUNDS/CUES/MONEY-DICE/rolling_dices3_Cue` |
| 점프 | `/Game/MEGA_FULL_BUNDLE/SURVIVAL_ESSENTIALS_SOUNDS/CUES/footstep_leatherboot_interior_jump_v2_Cue` |
| 착지 | `/Game/MEGA_FULL_BUNDLE/280FOOTSTEP_SOUNDS/CUES/FOOTSTEP_ON_WOODEN_FLOOR/footstep_on_wooden_floor_14_Cue` |
| 피격 | `/Game/MEGA_FULL_BUNDLE/FANTASY_RPG_SOUNDS/CUES/body_falling_Cue` |
| 걷기 | `/Game/MEGA_FULL_BUNDLE/280FOOTSTEP_SOUNDS/CUES/FOOTSTEP_ON_WOODEN_FLOOR/footstep_indoor_v2_Cue` |
| 걷기 | `/Game/MEGA_FULL_BUNDLE/HORROR_GAME_SOUNDS/CUES/footstep_indoor_v10_Cue` |
| 질풍참 | `/Game/Audio/SFX/Samurai/samurai_01_Swing_Cut_01` |
| 패링 | `/Game/Audio/SFX/Samurai/samurai_04_Deflecting_01` |

## 가져오기 및 조정

- 원본: `C:\Users\user\Desktop\SFX`.
- 질풍참: `Sound/Samurai/01_Swing_Cut/01_Swing_Cut_01.wav`를 `samurai_01_Swing_Cut_01` SoundWave로 임포트했다.
- 패링: `Sound/Samurai/04_Deflecting/04_Deflecting_01.wav`를 `samurai_04_Deflecting_01` SoundWave로 임포트했다.
- 이름이 중복된 `footstep_indoor_v2_Cue`는 `280FOOTSTEP_SOUNDS`, `body_falling_Cue`는 `FANTASY_RPG_SOUNDS`, `clothe_item_v9_Cue`는 `SURVIVAL_ESSENTIALS_SOUNDS` 팩에서 선택했다.
- 착지 Cue의 Volume Multiplier를 0.75에서 1.5로 높였다(2배, 약 +6 dB). 원본 SoundWave는 변경하지 않았다.
- 얼음 공격은 표의 비고에 지정된 `net_swish_1_Cue`부터 `net_swish_3_Cue`까지 사용한다.
- 이번 변경은 에셋 포함과 착지 볼륨 조정이다. 게임플레이·몽타주·AnimNotify의 재생 연결은 포함하지 않는다.

## 검증

- Unreal Engine 5.8 Python commandlet에서 26개 대상과 23개 의존성 패키지를 모두 로드했다.
- Asset Registry의 hard/soft package 의존성을 재귀 확인했다.
- Samurai WAV 2개의 SoundWave 임포트·저장, 착지 Cue 볼륨 저장을 확인했다.
- 프로젝트 Content로 복사한 파일은 검증된 파일과 SHA-256이 동일하다.
- 실제 게임 플레이 중 음량 밸런스와 재생 타이밍은 아직 청취 검증하지 않았다.
