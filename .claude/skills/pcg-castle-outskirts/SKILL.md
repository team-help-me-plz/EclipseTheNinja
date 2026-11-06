---
name: pcg-castle-outskirts
description: EclipseTheNinja의 성 외곽 마을 PCG 그래프(PCG_TownRow, PCG_CastleWall, PCG_Forest, PCG_FarSilhouette, PCG_Foreground, PCG_CastleKeep)로 사이드뷰 배경 레벨을 구성·수정·확인한다. "PCG로 마을 거리/성 외곽 배경 만들어", "마을 집 줄 늘려", "숲 밀도 바꿔", "망루 줄여", "배경 레이어 구성" 같은 요청에 쓴다. 레이아웃 JSON 하나로 액터 배치·파라미터·스플라인을 적용하고 게임 카메라 캡처로 확인한다.
---

# 성 외곽 마을 PCG 레벨 구성

사람용 상세 가이드: `Docs/PCG/CastleOutskirts_PCG_Guide.md`(파라미터 전체 표, 굽기, 문제 해결). 이 스킬은 **AI가 에디터를 직접 조작해** 같은 일을 하는 절차다.

## 0. 전제
- 언리얼 에디터가 켜져 있고 MCP(localhost:8000)가 응답해야 한다: `powershell -ExecutionPolicy Bypass -File .claude/tools/editor-mcp/editor.ps1 -Action status`
- 에디터 조작 출력이 길어지므로, 에디터 작업용 서브에이전트(예: 개인 설정의 `ue-editor-operator`)가 있으면 이 스킬 경로를 알려 주고 맡기는 것이 좋다. 없으면 직접 해도 된다.
- 콘솔 명령 실행은 `Plugins/EclipseLocalTools`(에디터 전용 플러그인)가 제공한다. 에디터에서 이 플러그인이 켜져 있어야 한다(처음 받은 사람은 프로젝트를 한 번 빌드).
- 모든 실행은 `scripts/run.ps1` 하나로 한다. 내부적으로 `py scripts/pcg_build.py`를 콘솔로 돌리고 결과 로그를 출력한다.

```powershell
$S = ".claude/skills/pcg-castle-outskirts"
powershell -ExecutionPolicy Bypass -File "$S/scripts/run.ps1" -Mode inspect              # 현재 PCG 액터 덤프
powershell -ExecutionPolicy Bypass -File "$S/scripts/run.ps1" -Mode apply -Layout <json>  # 레이아웃 적용 + 전체 재생성
powershell -ExecutionPolicy Bypass -File "$S/scripts/run.ps1" -Mode regen -Labels PCG_TownRow_Front
powershell -ExecutionPolicy Bypass -File "$S/scripts/run.ps1" -Mode count               # 액터별 생성 인스턴스 수
powershell -ExecutionPolicy Bypass -File "$S/scripts/run.ps1" -Mode shots -Shots "Preview_GameCam_A=a.png,Preview_GameCam_B=b.png" -OutDir Saved/PCGReview/Skill
powershell -ExecutionPolicy Bypass -File "$S/scripts/run.ps1" -Mode save                 # 비 WP 레벨 저장
```

## 1. 작업 흐름
1. **레벨 준비**
   - 새 레벨이면 예시 레벨 `/Game/Maps/PCG/Lvl_PCG_CastleOutskirts`를 복제해서 연다. 바닥·라이트·안개·카메라가 같이 온다. MCP 호출(`.claude/tools/editor-mcp/call.ps1 -Toolset <툴셋> -Tool <이름> -ArgsJson <json>`):
     1. `editor_toolset.toolsets.asset.AssetTools` / `duplicate` `{"path": "<원본>", "new_path": "<새 경로>"}`
     2. `editor_toolset.toolsets.asset.AssetTools` / `save_assets` (새 경로)
     3. `editor_toolset.toolsets.scene.SceneTools` / `load_level` `{"level_path": "<새 경로>"}`. 현재 레벨에 미저장 변경이 있으면 거부된다 → 사용자에게 저장 여부를 묻는다.
     - 인자 이름이 다르면 `call.ps1 -Tool describe_toolset -ArgsJson '{"toolset_name":"<툴셋>"}'`로 확인.
   - **에디터 파이썬 `load_level`/`new_level`은 쓰지 않는다(에디터 크래시 이력).** 레벨 열기는 MCP load_level만.
   - 복제 레벨은 World Partition이 아니어서 `-Mode save`로 저장된다. WP 레벨이면 `.claude/tools/editor-mcp/save_map.ps1`을 쓴다.
2. **현재 상태 읽기**: `-Mode inspect`. 라벨·그래프·위치·스케일·덮어쓴 파라미터·스플라인 점·ISM 수가 나온다.
3. **레이아웃 JSON 작성**: `layouts/castle_outskirts_example.json`을 작업 폴더(`%TEMP%/EclipseMcp/`)로 복사해 고친다. 스키마는 2장.
4. **적용**: `-Mode apply -Layout <json>`. 라벨이 같은 액터는 고치고, 없으면 만든다. 모든 PCG를 다시 생성하고, 3초 뒤 `--- 생성 결과` 아래에 액터별 인스턴스 수를 찍는다(생성이 비동기라 따로 셈).
   - 출력에 `!!`가 있으면 실패한 줄이다(파라미터 이름 오타, 값 불일치, 그래프 없음 등).
   - 인스턴스 0인 그래프 액터는 위치·크기·파라미터를 의심한다(단, `PCG_CastleKeep`은 열린 성벽에서 0이 정상). 큰 레벨이라 아직 생성 중일 수 있으면 `-Mode count`로 다시 센다.
5. **눈으로 확인**: `-Mode shots`로 게임 카메라 시점을 찍고 Read로 이미지를 본다. 카메라마다 따로 찍고 파일을 기다린다. 에디터 창이 앞에 있어야 렌더된다(스크립트가 앞으로 가져오지만 다른 창이 막으면 MISSING → 사용자에게 에디터를 띄워 달라고 한다).
   - 확인할 것: 1열이 2열·성벽을 다 가리지 않는지, 전경이 화면 가운데를 막지 않는지, 원경 천수각이 보이는지, 지붕 계열이 블럭 안에서 섞이지 않는지.
6. **저장**: 사용자 확인 후 `-Mode save`. 파라미터 덮어쓰기는 레벨에 저장된다. **그래프 에셋 기본값은 바꾸지 않는다**(다른 레벨까지 바뀜).

## 2. 레이아웃 JSON 스키마
```jsonc
{
  "remove": ["라벨", ...],                 // 지울 액터(선택)
  "ground": { "rows": [ {"y": 200, "x_from": -4200, "x_to": 24600, "scale": 1} ] },  // 선택. 있으면 Ground_* 전부 지우고 16x16 흙 타일(1600×scale 간격)로 다시 깜
  "volumes": [ {                           // PCG Volume
    "label": "PCG_Forest_Mid", "folder": "PCG/Midground",
    "graph": "PCG_Forest",                 // 생략하면 그래프 없는 표지 볼륨(태그용)
    "tags": ["PCG_Exclusion"],             // 선택
    "center": [x, y, z], "half": [hx, hy, hz],   // 반폭(cm). 스케일 = half/100
    "params": { "Seed": 3, "CellSize": [650,650,100], "ClearCastleInterior": false }
  } ],
  "splines": [ {                           // BP_PCG_SplineSource
    "label": "PCG_TownRow_Front", "folder": "PCG/Town", "graph": "PCG_TownRow",
    "location": [0, 0, 0],                 // 성벽은 Z = 400 × WallHeightScale (기본 600)
    "points": [[x, y], ...],               // 액터 로컬. 반드시 +X 방향으로
    "linear": false, "closed": false,
    "tags": [], "params": { ... }
  } ],
  "cameras": [ {"label": "Preview_GameCam_A", "x": 3000} ],  // (x, 1000, 190), yaw -90, FOV 65
  "moves": [ {"label": "Preview_PlayerProxy_0", "location": [5000, 0, 90]} ]  // PCG가 아닌 액터 옮기기(선택)
}
```
- `ground`는 라벨이 `Ground_`로 시작하는 액터만 지우고 다시 만든다(예시 레벨의 바닥 라벨 규칙).
- 길이를 바꾸면 PCG 외 액터(`Preview_PlayerProxy_0~6` 플레이어 크기 비교용 실린더, `PCG_CastleKeep_Volume`)도 `moves`/`volumes`로 같이 옮긴다.
- 파라미터 이름·타입은 `pcg_build.py`의 `PARAMS` 표가 기준이다. 없는 이름이면 `!! unknown param`과 함께 쓸 수 있는 이름 목록이 찍힌다. 그래프에 파라미터를 추가하면 이 표에도 추가한다.
- 넣은 값은 다시 읽어 비교한다. 다르면 `!! param ... want ... got ...`.
- 스케일류 vector 파라미터(`MapleScaleMin` 등)는 균일하게 `[s, s, s]`.
- `params`에 적은 것만 덮어쓴다. 덮어쓰기를 지우는(기본값 복귀) 기능은 없다 → 기본값을 직접 적는다.

## 3. 사이드뷰 레이어 규칙 (반드시 지킬 것)
카메라는 **−Y를 본다**. 플레이 면은 **Y = 0**, 카메라는 Y +1000, 높이 190, FOV 65. 플레이 면에서 화면 가로 폭 ≈ 1270cm.

| 레이어 | Y 범위 | 그래프 | 핵심 값 |
|---|---|---|---|
| 전경 | +300 ~ +680 | PCG_Foreground (볼륨 center Y 490, half Y 190) | `MaxCount` 3~5. 시야를 가리므로 아주 드물게 |
| 플레이 면 | −300 ~ +300 | 표지 볼륨 태그 `PCG_Exclusion` (half Y 300) | 아무것도 두지 않는다 |
| 근경 1열 | ≈ −900 | PCG_TownRow | `TwoStoryRatio` 0, `HouseOverlap` 60 → 낮고 촘촘 |
| 근경 2열 | ≈ −2200 | PCG_TownRow | 1열과 **다른 Seed**, `RowOffsetX` 1000~2000, `TwoStoryRatio` 0.9, 골목 넓게 |
| 마을 표지 | −3400 ~ −400 | 표지 볼륨 태그 `PCG_Village` | 숲이 마을을 피함 |
| 중경 성벽 | −5000 ~ −6700 | PCG_CastleWall (태그 `PCG_CastleWall`, Z 600, linear) | 기본 선은 뒤(−6200 등)에 두고, 망루를 세울 곳만 앞(−5000 등)으로 튀어나온 사각 구간을 만든다. 튀어나온 구간의 **앞쪽 두 모서리**(+X로 진행하며 카메라 쪽에서 다시 뒤로 꺾이는 볼록 모서리)에 코너 석축·망루 후보가 생긴다. 망루 수는 `YaguraRatio`(0~1), `YaguraMinSpacing`(기본 6000cm)로 줄인다 |
| 중경 숲 | −7200 ~ −10800 | PCG_Forest | `ClearCastleInterior` **false**(열린 성벽), `GridExtents` > 볼륨 반폭 |
| 원경 숲 | −11000 ~ −21000 | PCG_Forest | 격자 넓게(1300), 나무 크게(4~5.5) |
| 원경 실루엣 | −18000 ~ −38000 | PCG_FarSilhouette (center Z 2500) | `TenshuCount` 1~2. `TenshuZone`(vector, 볼륨 중심 기준 반폭, 기본 [9000,6000,0])의 X를 플레이 구간 반폭 정도로 |

- **X 범위**: 모든 레이어를 플레이 구간 X보다 넓게. 뒤 레이어일수록 화면에 넓게 보이므로 더 넓게(원경은 플레이 길이 + 양쪽 1~2만).
- 스플라인은 **+X(왼→오)** 로 그린다. 반대면 TownRow `FacingSide` −1, CastleWall `FlipOutward` true.
- `PCG_Exclusion`을 읽는 그래프는 **PCG_Forest뿐**이다. 나머지는 Y 위치로 플레이 면을 피해야 한다.
- 생성물은 콜리전·내비 없음(배경 전용). 게임플레이 지형은 따로 만든다.

## 4. 요청 → 조절 대응표
| 요청 | 조절 |
|---|---|
| 다른 배치 | 해당 액터 `Seed` |
| 길이를 늘림/줄임 | 스플라인 끝점 X, 볼륨 center X·half X, 숲 `GridExtents`, 카메라 x, `ground.rows` x_to |
| 집이 더 빽빽/듬성 | `HouseOverlap`↑/↓, `AlleyMin/Max`↓/↑, `BlockHousesMin/Max` |
| 2층 많게 | **뒷줄(2열)만** `TwoStoryRatio`↑ (블럭 단위). 앞줄(1열)은 0 유지 — 앞줄에 2층이 서면 뒤 레이어가 전부 가려진다(검증에서 확인됨) |
| 나무지붕 집이 튐 | `WoodRoofBlockRatio`↓ (0~0.5) |
| 두 줄이 같은 X에 반복 | 줄마다 다른 `Seed`, 뒷줄 `RowOffsetX` |
| 소품·가로등 | `PropDensity`, `AlleyPropDensity`, `AlleyFenceRatio`, `LampSpacing` |
| 망루 많음 | `YaguraRatio`↓, `YaguraMinSpacing`↑, 스플라인 꺾임 줄이기 |
| 성벽 높이 | `WallHeightScale` + 액터 Z = 400 × 값 |
| 숲 밀도 | `CellSize`↑ 또는 `NoiseThreshold`↑ = 성김. `MapleRatio` 1 = 대나무 없음 |
| 원경 | `TenshuCount`, `YaguraCount`, `RoofClusterCount`, `TreeCellSize`, `TreeNoiseThreshold` |
| 전경 | `MaxCount`, `Density`, `TreeRatio`, `MinSpacing` |
| 집 메시 교체 | `PCG_TownRow`의 `HouseSet`(에셋 경로 문자열)에 집 세트 데이터 에셋(`EclipseTownHouseSet`)을 지정. 기본 `/Game/PCG/Data/DA_TownHouses_Default.DA_TownHouses_Default`. 새 세트는 이 에셋을 복제해 항목(Mesh, Roof 기와/나무, Stories 1/2, Weight, WidthOverride)을 바꿔 만든다. 집 폭은 메시 바운드에서 자동 계산 |
| 성벽·숲 등 메시 교체 | 그 외 `...Mesh` 파라미터(소프트 오브젝트)는 이 스크립트로 못 넣는다 → 사용자가 디테일 패널에서 지정하도록 안내 |

## 5. 함정
- 파라미터를 바꾼 뒤 결과가 안 바뀌면 `pcg.FlushCache` 후 강제 생성이 필요하다(`apply`/`regen`이 처리).
- 스플라인을 파이썬으로 고칠 때 `spline_has_been_edited`를 켜지 않으면 BP 기본 스플라인(2점)으로 돌아간다(스크립트가 처리).
- **그래프 자체를 수정할 일이 생기면**(노드 추가 등) 이 스킬 범위 밖이다. PCG_TownRow의 집·골목 나누기는 C++ 노드 `Eclipse Town Row`(`Source/EclipseTheNinja/Private/PCG/PCGEclipseTownRow.cpp`)가 한다. 규칙을 바꾸려면 C++ 수정과 빌드가 필요하다.
- 알려진 문제: PCG_Foreground의 담장 기둥 스포너가 연결되지 않아 나오지 않는다. PCG_CastleKeep은 닫힌 성벽용.
- 단풍나무는 나나이트가 켜져 있다. 렉이 생기면 원경 나무 수(`TreeCellSize`·`CellSize`↑)부터 줄인다.
- 상태 표시줄 "N개 저장되지 않음" 버튼은 절대 누르지 않는다(모달로 MCP가 멈춤).

## 6. 보고
적용 후 사용자에게: 바꾼 액터·파라미터 목록, 액터별 인스턴스 수, 캡처 이미지(SendUserFile), 저장 여부.
