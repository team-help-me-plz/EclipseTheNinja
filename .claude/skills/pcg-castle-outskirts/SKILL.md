---
name: pcg-castle-outskirts
description: EclipseTheNinja의 성 외곽 PCG 전용 액터(Eclipse 숲·원경 실루엣·전경·표지·바닥 볼륨, Eclipse 마을 집 줄·성벽)로 사이드뷰 배경 레벨을 구성·수정·확인한다. "PCG로 마을 거리/성 외곽 배경 만들어", "마을 집 줄 늘려", "숲 밀도 바꿔", "망루 줄여", "바닥 깔아", "배경 레이어 구성", "예전 PCG 액터 변환" 같은 요청에 쓴다. 레이아웃 JSON 하나로 액터 배치·크기·속성을 적용하고 게임 카메라 캡처로 확인한다.
---

# 성 외곽 마을 PCG 레벨 구성

사람용 상세 가이드: `Docs/PCG/CastleOutskirts_PCG_Guide.md`(액터별 속성 표, 굽기, 문제 해결). 이 스킬은 **AI가 에디터를 직접 조작해** 같은 일을 하는 절차다.

## 0. 전제
- 언리얼 에디터가 켜져 있고 MCP(localhost:8000)가 응답해야 한다: `powershell -ExecutionPolicy Bypass -File .claude/tools/editor-mcp/editor.ps1 -Action status`
- 에디터 조작 출력이 길어지므로, 에디터 작업용 서브에이전트(예: 개인 설정의 `ue-editor-operator`)가 있으면 이 스킬 경로를 알려 주고 맡기는 것이 좋다. 없으면 직접 해도 된다.
- 콘솔 명령 실행은 `Plugins/EclipseLocalTools`(에디터 전용 플러그인)가 제공한다. 에디터에서 이 플러그인이 켜져 있어야 한다(처음 받은 사람은 프로젝트를 한 번 빌드).
- 모든 실행은 `scripts/run.ps1` 하나로 한다. 내부적으로 `py scripts/pcg_build.py`를 콘솔로 돌리고 결과 로그를 출력한다.

```powershell
$S = ".claude/skills/pcg-castle-outskirts"
powershell -ExecutionPolicy Bypass -File "$S/scripts/run.ps1" -Mode inspect              # 현재 PCG 액터 덤프
powershell -ExecutionPolicy Bypass -File "$S/scripts/run.ps1" -Mode apply -Layout <json>  # 레이아웃 적용 + 전체 재생성
powershell -ExecutionPolicy Bypass -File "$S/scripts/run.ps1" -Mode migrate              # 예전 방식 액터 → 전용 액터
powershell -ExecutionPolicy Bypass -File "$S/scripts/run.ps1" -Mode regen -Labels PCG_TownRow_Front
powershell -ExecutionPolicy Bypass -File "$S/scripts/run.ps1" -Mode count               # 액터별 생성 인스턴스 수
powershell -ExecutionPolicy Bypass -File "$S/scripts/run.ps1" -Mode shots -Shots "Preview_GameCam_A=a.png,Preview_GameCam_B=b.png" -OutDir Saved/PCGReview/Skill
powershell -ExecutionPolicy Bypass -File "$S/scripts/run.ps1" -Mode save                 # 비 WP 레벨 저장
```

## 1. 액터 (C++, `Source/EclipseTheNinja/Public/PCG/`)
레벨에는 아래 전용 액터만 놓는다. 그래프는 놓을 때 자동으로 들어가고, 속성을 바꾸면 그래프 파라미터로 전달된다(그래프 인스턴스 체크박스를 직접 건드리지 않는다).

| 레이아웃 class | C++ 클래스(배치 패널 이름) | 그래프 | 모양 |
|---|---|---|---|
| `Forest` | `AEclipseForestVolume` (Eclipse 숲 볼륨) | PCG_Forest | 볼륨 |
| `FarSilhouette` | `AEclipseFarSilhouetteVolume` | PCG_FarSilhouette | 볼륨 |
| `Foreground` | `AEclipseForegroundVolume` | PCG_Foreground | 볼륨 |
| `Marker` | `AEclipsePCGMarkerVolume` (표지, 그래프 없음) | - | 볼륨. `kind` PlayArea → 태그 PCG_Exclusion, Village → PCG_Village |
| `Ground` | `AEclipseGroundVolume` (야외 바닥) | PCG_Ground (노드 `Eclipse Ground`) | 볼륨. 윗면 = 바닥 높이 |
| `Interior` | `AEclipseInteriorVolume` (실내 방) | PCG_Interior_* | 볼륨 |
| `TownRow` | `AEclipseTownRowActor` | PCG_TownRow | 직선 |
| `CastleWall` | `AEclipseCastleWallActor` | PCG_CastleWall | 직선 + 돌출부 |

- **볼륨 크기는 `Size`(cm)** 로 정한다. 스케일은 항상 1이다(스케일로 늘리면 Size로 옮겨 담긴다). 사람은 뷰포트의 면 핸들(±X, ±Y, 위)을 끌어 바꾼다.
- **선 액터는 액터 위치 = 왼쪽 끝, `Length`만큼 +X로** 뻗는다. 스플라인은 액터가 매번 다시 만들므로 점을 직접 고치지 않는다.
- **성벽**: 액터는 **바닥 높이(Z 0 등)** 에 둔다. 석축 윗면 높이는 400 × `WallHeightScale`로 자동 계산된다. 망루는 돌출부마다 하나씩 선다. 돌출부는 `BastionPlacement` Auto면 `BastionCount`개를 고르게 놓고, Manual이면 `BastionPositions`(시작점에서의 거리)에 놓는다. 크기는 `BastionWidth`·`BastionDepth`로 정한다. 이웃 돌출부는 (폭 + 200)cm 넘게 떨어져야 한다. 가깝거나 선 밖에 걸치면 그 돌출부는 건너뛴다. 태그 `PCG_CastleWall`은 자동으로 붙는다.
- 볼륨 크기에서 계산되는 그래프 값(숲 `GridExtents`, 원경·전경 `Extents`, 성벽 `YaguraMinSpacing` 등)은 액터가 자동으로 넣는다. 레이아웃에 적지 않는다.
- 각 속성의 의미·범위는 헤더 주석(= 디테일 패널 툴팁)이 기준이다: `EclipseOutskirtsActors.h`, `EclipseGroundVolume.h`, `EclipseInteriorTypes.h`.

## 2. 작업 흐름
1. **레벨 준비**
   - 새 레벨이면 예시 레벨 `/Game/Maps/PCG/Lvl_PCG_CastleOutskirts`를 복제해서 연다. 바닥·라이트·안개·카메라가 같이 온다. MCP 호출(`.claude/tools/editor-mcp/call.ps1 -Toolset <툴셋> -Tool <이름> -ArgsJson <json>`):
     1. `editor_toolset.toolsets.asset.AssetTools` / `duplicate` `{"path": "<원본>", "new_path": "<새 경로>"}`
     2. `editor_toolset.toolsets.asset.AssetTools` / `save_assets` (새 경로)
     3. `editor_toolset.toolsets.scene.SceneTools` / `load_level` `{"level_path": "<새 경로>"}`. 현재 레벨에 미저장 변경이 있으면 거부된다 → 사용자에게 저장 여부를 묻는다.
     - 인자 이름이 다르면 `call.ps1 -Tool describe_toolset -ArgsJson '{"toolset_name":"<툴셋>"}'`로 확인.
   - **에디터 파이썬 `load_level`/`new_level`은 쓰지 않는다(에디터 크래시 이력).** 레벨 열기는 MCP load_level만.
   - 복제 레벨은 World Partition이 아니어서 `-Mode save`로 저장된다. WP 레벨이면 `.claude/tools/editor-mcp/save_map.ps1`을 쓴다.
2. **현재 상태 읽기**: `-Mode inspect`. 라벨·클래스·위치·Size/Length·적용 중인 그래프 값·스플라인 점·ISM 수가 나온다.
   - 예전 방식 액터(그래프를 직접 넣은 `PCGVolume`, `BP_PCG_SplineSource`, `Ground_*` 타일)가 있으면 사용자에게 알리고 `-Mode migrate`로 바꾼다. 꺾인 마을 줄·성벽 기본선은 첫 점 Y의 직선이 된다(출력의 `..` 줄). 실내 볼륨은 스케일이 Size로 바뀐다.
3. **레이아웃 JSON 작성**: `layouts/castle_outskirts_example.json`을 작업 폴더(`%TEMP%/EclipseMcp/`)로 복사해 고친다. 스키마는 3장.
4. **적용**: `-Mode apply -Layout <json>`. 라벨이 같은 액터는 고치고, 없으면 만든다. 모든 PCG를 다시 생성하고, 3초 뒤 `--- 생성 결과` 아래에 액터별 인스턴스 수를 찍는다(생성이 비동기라 따로 셈).
   - 출력에 `!!`가 있으면 실패한 줄이다(속성 이름 오타, 같은 라벨의 다른 클래스 액터 등).
   - 인스턴스 0인 액터는 위치·크기·속성을 의심한다. 큰 레벨이라 아직 생성 중일 수 있으면 `-Mode count`로 다시 센다.
5. **눈으로 확인**: `-Mode shots`로 게임 카메라 시점을 찍고 Read로 이미지를 본다. 카메라마다 따로 찍고 파일을 기다린다. 에디터 창이 앞에 있어야 렌더된다(스크립트가 앞으로 가져오지만 다른 창이 막으면 MISSING → 사용자에게 에디터를 띄워 달라고 한다).
   - 확인할 것: 1열이 2열·성벽을 다 가리지 않는지, 전경이 화면 가운데를 막지 않는지, 원경 천수각이 보이는지, 지붕 계열이 블럭 안에서 섞이지 않는지, 바닥 덤불이 플레이 면에 없는지.
6. **저장**: 사용자 확인 후 `-Mode save`. 속성은 레벨 액터에 저장된다. **그래프 에셋 기본값은 바꾸지 않는다**(다른 레벨까지 바뀜).

## 3. 레이아웃 JSON 스키마
```jsonc
{
  "remove": ["라벨", ...],                         // 지울 액터(선택)
  "volumes": [ {
    "class": "Forest",                             // Forest | FarSilhouette | Foreground | Marker | Ground | Interior
    "label": "PCG_Forest_Mid", "folder": "PCG/Midground",
    "center": [x, y, z], "size": [sx, sy, sz],     // cm. 스케일은 1로 고정
    "kind": "PlayArea",                            // Marker만: PlayArea | Village
    "tags": [],                                    // 선택(표지 태그는 kind가 처리)
    "props": { "Seed": 3, "CellSpacing": 650 }     // C++ 속성 이름. 구조체 안은 "Params.Seed"(Ground·Interior)
  } ],
  "lines": [ {
    "class": "CastleWall",                         // TownRow | CastleWall
    "label": "PCG_CastleWall_Spline", "folder": "PCG/Midground",
    "start": [x, y, z], "length": 32500,           // 왼쪽 끝(월드), +X로 길이. 성벽 z는 바닥 높이
    "props": { "BastionPlacement": "Manual", "BastionPositions": [2000, 9000] }
  } ],
  "cameras": [ {"label": "Preview_GameCam_A", "x": 3000} ],  // (x, 1000, 190), yaw -90, FOV 65
  "moves": [ {"label": "Preview_PlayerProxy_0", "location": [5000, 0, 90]} ]  // PCG가 아닌 액터 옮기기(선택)
}
```
- `props`의 키는 C++ 속성 이름(PascalCase, bool은 `b` 접두사 있어도 됨)이며 스크립트가 파이썬 이름으로 바꾼다. 열거형은 이름 문자열(`"Manual"`), 벡터는 `[x,y,z]`, 에셋은 경로 문자열.
- 크기 배율류(`MapleScaleMin` 등)는 숫자 하나다(균일). 대나무 크기만 벡터(X·Y 굵기, Z 높이).
- `props`에 적은 것만 바꾼다. 나머지는 액터 기본값(= 툴팁의 권장값)이다.
- 길이를 바꾸면 PCG 외 액터(`Preview_PlayerProxy_0~6` 플레이어 크기 비교용 실린더)도 `moves`로 같이 옮긴다.

## 4. 사이드뷰 레이어 규칙 (반드시 지킬 것)
카메라는 **−Y를 본다**. 플레이 면은 **Y = 0**, 카메라는 Y +1000, 높이 190, FOV 65. 플레이 면에서 화면 가로 폭 ≈ 1270cm.

| 레이어 | Y 범위 | 액터 | 핵심 값 |
|---|---|---|---|
| 전경 | +300 ~ +680 | Foreground (center Y 490, size Y 380) | `MaxCount` 3~5. 시야를 가리므로 아주 드물게 |
| 플레이 면 | −300 ~ +300 | Marker `PlayArea` (size Y 600) | 아무것도 두지 않는다 |
| 바닥 | 마을·플레이 면 아래 | Ground (윗면 Z 0 → center Z −50, size Z 100) | 덤불은 플레이 면을 자동으로 피함(`Params.bAvoidPlayArea`) |
| 근경 1열 | ≈ −900 | TownRow | `TwoStoryRatio` 0, `HouseOverlap` 60 → 낮고 촘촘 |
| 근경 2열 | ≈ −2200 | TownRow | 1열과 **다른 Seed**, `RowOffsetX` 1000~2000, `TwoStoryRatio` 0.9, 골목 넓게 |
| 마을 표지 | −3400 ~ −400 | Marker `Village` | 숲이 마을을 피함 |
| 중경 성벽 | −5000 ~ −6700 | CastleWall (start z = 바닥) | 기본 선 Y에 두고 `BastionDepth`만큼 앞으로 돌출. 망루 수 = 돌출부 수 |
| 중경 숲 | −7200 ~ −10800 | Forest | `ClearCastleInterior` false(기본값) |
| 원경 숲 | −11000 ~ −21000 | Forest | `CellSpacing` 1300, `MapleScaleMin/Max` 4~5.5 |
| 원경 실루엣 | −18000 ~ −38000 | FarSilhouette (center Z 2500) | `TenshuCount` 1~2, `TenshuRangeX`를 플레이 구간 반폭 정도로 |

- **X 범위**: 모든 레이어를 플레이 구간 X보다 넓게. 뒤 레이어일수록 화면에 넓게 보이므로 더 넓게(원경은 플레이 길이 + 양쪽 1~2만).
- 표지 볼륨(`PCG_Exclusion`·`PCG_Village`)을 읽는 것은 **숲과 바닥 흩뿌림(플레이 면만)** 이다. 나머지는 Y 위치로 플레이 면을 피해야 한다.
- 생성물은 콜리전·내비 없음(배경 전용). 게임플레이 지형은 따로 만든다.

## 5. 요청 → 조절 대응표
| 요청 | 조절 |
|---|---|
| 다른 배치 | 해당 액터 `Seed` |
| 길이를 늘림/줄임 | 볼륨 `size` X·`center` X, 선 `length`·`start` X, 카메라 x |
| 집이 더 빽빽/듬성 | `HouseOverlap`↑/↓, `AlleyMin/Max`↓/↑, `BlockHousesMin/Max` |
| 2층 많게 | **뒷줄(2열)만** `TwoStoryRatio`↑. 앞줄(1열)은 0 유지 — 앞줄에 2층이 서면 뒤 레이어가 전부 가려진다 |
| 나무지붕 집이 튐 | `WoodRoofBlockRatio`↓ (0~0.5) |
| 두 줄이 같은 X에 반복 | 줄마다 다른 `Seed`, 뒷줄 `RowOffsetX` |
| 소품·가로등 | `PropDensity`, `AlleyPropDensity`, `AlleyFenceRatio`, `LampSpacing` |
| 망루 많음/적음·위치 | `BastionCount` 또는 `BastionPlacement` Manual + `BastionPositions`. 돌출부 없이 망루만 끄려면 `SpawnYagura` false |
| 성벽 높이 | `WallHeightScale`(액터 Z는 그대로) |
| 숲 밀도 | `CellSpacing`↑ 또는 `NoiseThreshold`↑ = 성김. `MapleRatio` 1 = 대나무 없음 |
| 원경 | `TenshuCount`, `TenshuRangeX`, `YaguraCount`, `RoofClusterCount`, `TreeSpacing`, `TreeNoiseThreshold` |
| 전경 | `MaxCount`, `Density`, `TreeRatio`, `MinSpacing` |
| 바닥 | `Params.Tiles`(타일 메시), `Params.ScatterDensity`(개/100㎡, 기본 0 = 흩뿌림 없음), `Params.ScatterClumping`, `Params.ScatterMinSpacing`, `Params.Scatter`(덤불·돌 목록) |
| 집 메시 교체 | TownRow `HouseSet`에 집 세트 데이터 에셋(`EclipseTownHouseSet`) 경로. 기본 `/Game/PCG/Data/DA_TownHouses_Default.DA_TownHouses_Default`. 새 세트는 이 에셋을 복제해 항목(Mesh, Roof, Stories, Weight, WidthOverride)을 바꿔 만든다 |
| 성벽·숲 등 메시 교체 | 그래프의 `...Mesh` 파라미터는 전용 액터에 없다 → 사용자가 PCG 컴포넌트 그래프 인스턴스에서 직접 지정하도록 안내 |

## 6. 함정
- 속성을 바꾼 뒤 결과가 안 바뀌면 액터의 **다시 생성** 버튼(`regenerate()`) 또는 `-Mode regen`.
- **그래프 자체를 수정할 일이 생기면**(노드 추가 등) 이 스킬 범위 밖이다. 마을 집·골목 나누기는 C++ 노드 `Eclipse Town Row`, 바닥은 `Eclipse Ground`가 한다.
- 전용 액터에 새 그래프 파라미터를 연결하려면 C++ 속성에 `meta = (PCGParam)`을 붙인다(이름이 같으면 자동 전달). 이 스크립트의 `PARAMS` 표는 inspect·migrate용이다.
- 알려진 문제: PCG_Foreground의 담장 기둥 스포너가 연결되지 않아 나오지 않는다. PCG_CastleKeep은 닫힌 성벽용이라 전용 액터가 없다.
- 단풍나무는 나나이트가 켜져 있다. 렉이 생기면 원경 나무 수(`TreeSpacing`·`CellSpacing`↑)부터 줄인다.
- 상태 표시줄 "N개 저장되지 않음" 버튼은 절대 누르지 않는다(모달로 MCP가 멈춤).

## 7. 보고
적용 후 사용자에게: 바꾼 액터·속성 목록, 액터별 인스턴스 수, 캡처 이미지(SendUserFile), 저장 여부.
