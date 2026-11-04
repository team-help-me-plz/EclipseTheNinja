# ---------- 그레이박스 레벨 키트 : runlib.ps1 -Kit 일 때만 core.py 뒤에 붙는다 ----------
# 기본값은 Week_3 설정이다. Week_2 맵을 만들 때는 run() 첫 줄에서 use_week(2) 를 부른다
# 횡스크롤 레인은 Y=1000 (지형 Y 700~1300, 배경은 Y<700, 앞면은 Y=1300)

LP = "/Game/_ExternalAssets/LevelPrototyping/Materials/"
J = "/Game/_ExternalAssets/JapaneseVillageKit/Meshes/"
Y = 1000.0
ENEMY_Z = 92.0
GOLD = (1.0, 0.82, 0.45)
RED = (1.0, 0.5, 0.42)
GREEN = (0.6, 1.0, 0.6)

MAT = {
    "dark": LP + "MI_PrototypeGrid_TopDark.MI_PrototypeGrid_TopDark",
    "gray": LP + "MI_PrototypeGrid_Gray.MI_PrototypeGrid_Gray",
    "gray2": LP + "MI_PrototypeGrid_Gray_02.MI_PrototypeGrid_Gray_02",
    "ground": "/Game/Materials/Blockout/MI_W3_Ground.MI_W3_Ground",
    "wall": "/Game/Materials/Blockout/MI_W3_Wall.MI_W3_Wall",
    "climb": "/Game/Materials/Blockout/MI_PrototypeGridSize_Blue.MI_PrototypeGridSize_Blue",
}

BP = {
    "checkpoint": "/Game/Blueprint/Checkpoint/BP_Checkpoint",
    "safepoint": "/Game/Blueprint/Checkpoint/BP_SafePoint",
    "switch": "/Game/Blueprint/Puzzle/MapObject/BP_CPP_InteractSwitch",
    "platform": "/Game/Blueprint/Puzzle/MapObject/BP_CPP_MovingPlatform",
    "hinged": "/Game/Blueprint/Puzzle/MapObject/BP_HingedDoor",
    "lever": "/Game/Blueprint/Puzzle/MapObject/BP_LeverSwitch",
    "sliding": "/Game/Blueprint/Puzzle/MapObject/BP_SlidingDoor",
    "plate": "/Game/Blueprint/Puzzle/MapObject/BP_Wooden_PressureSwitch",
    "arrowtrap": "/Game/Blueprint/Puzzle/Trap/BP_ArrowTrap",
    "breakable": "/Game/Blueprint/Puzzle/Trap/BP_BreakablePlatform",
    "falling": "/Game/Blueprint/Puzzle/Trap/BP_FallingObjectTest",
    "damage": "/Game/Blueprint/Week_1/Trap/Trap/BP_DamageTrap",
    "cube": "/Game/Blueprint/Week_1/Checkpoint/BP_PushableCube",
    "spikes": "/Game/Blueprint/Week_1/MeshAssembly/BP_WoodenSpikeTrap2by2MeshAssembly",
    "poolcfg": "/Game/Blueprint/Framework/BP_ObjectPoolLevelConfig",
    "vineblock": "/Game/Blueprint/Puzzle/MapObject/BP_VineBlock",
    "vinedoor": "/Game/Blueprint/Puzzle/MapObject/BP_VineDoor",
    "vinelever": "/Game/Blueprint/Puzzle/MapObject/BP_VineLever",
    "stealth": "/Game/Blueprint/Puzzle/MapObject/BP_StealthZone",
}

# 주차별로 달라지는 값 : 게임 모드, 적 BP, 전선 머티리얼
WEEK = {
    2: {
        "GM": "/Game/Blueprint/Integrated/Week_2_Integrated/Framework/GM_Week_2.GM_Week_2_C",
        "enemy": "/Game/Blueprint/Integrated/Week_2_Integrated/Enemy/Basic/BP_Week_2_Enemy",
        "kunai": "/Game/Blueprint/Integrated/Week_2_Integrated/Enemy/Kunai/BP_Week_2_KunaiEnemy",
        "wire": "/Game/Materials/Blockout/MI_PuzzleWire.MI_PuzzleWire",
    },
    3: {
        "GM": "/Game/Blueprint/Integrated/Week_3_Integrated/Framework/GM_Week_3.GM_Week_3_C",
        "enemy": "/Game/Blueprint/Integrated/Week_3_Integrated/Enemy/Basic/BP_Week_3_Enemy",
        "kunai": "/Game/Blueprint/Integrated/Week_3_Integrated/Enemy/Kunai/BP_Week_3_KunaiEnemy",
        "wire": "/Game/Materials/Blockout/M_PuzzleGlow.M_PuzzleGlow",
    },
}
GM = ""
WIRE_MAT = ""


def use_week(n):
    global GM, WIRE_MAT
    w = WEEK[n]
    GM = w["GM"]
    WIRE_MAT = w["wire"]
    BP["enemy"] = w["enemy"]
    BP["kunai"] = w["kunai"]


use_week(3)


# 키트가 쓰는 에셋 경로가 지금 프로젝트에 모두 있는지 확인한다 (아무것도 스폰하지 않음)
def kit_check():
    paths = list(MAT.values()) + list(BP.values())
    for n in WEEK:
        paths += [WEEK[n]["enemy"], WEEK[n]["kunai"], WEEK[n]["wire"]]
    missing = []
    for p in sorted(set(paths)):
        if not asset_exists(p.split(".")[0]):
            missing.append(p)
    return missing


# ---------- 지형 ----------
# 블록아웃 지형 : X/Z 범위로 큐브를 깐다 (Y는 700~1300)
def block(label, x0, x1, z0, z1, mat="gray", folder="Geometry", y0=700.0, y1=1300.0):
    a = spawn_asset("/Engine/BasicShapes/Cube", label, (x0 + x1) / 2.0, (y0 + y1) / 2.0, (z0 + z1) / 2.0, folder,
                    sx=(x1 - x0) / 100.0, sy=(y1 - y0) / 100.0, sz=(z1 - z0) / 100.0)
    c = comps(a, "/Script/Engine.StaticMeshComponent")
    setp(c[0], {"OverrideMaterials": [{"refPath": MAT[mat]}]})
    return a


# 바닥 : 모든 발 딛는 지형은 같은 머티리얼(MI_W3_Ground)을 쓴다
def ground(label, x0, x1, z0, z1, folder="Geometry"):
    return block(label, x0, x1, z0, z1, mat="ground", folder=folder)


# 벽·천장 : 발 딛지 않는 구조물
def wall(label, x0, x1, z0, z1, folder="Geometry"):
    return block(label, x0, x1, z0, z1, mat="wall", folder=folder)


# 벽타기 벽 : 파란 그리드 + WallClimb 채널 Block. bUseDefaultCollision 을 먼저 꺼야 프로필이 유지된다
def climb(label, x0, x1, z0, z1, folder="Geometry/Climb"):
    a = block(label, x0, x1, z0, z1, mat="climb", folder=folder)
    c = comps(a, "/Script/Engine.StaticMeshComponent")[0]
    setp(c, {"bUseDefaultCollision": False})
    setp(c, {"BodyInstance": {"CollisionProfileName": "Custom", "CollisionEnabled": "QueryAndPhysics", "ObjectType": "ECC_WorldStatic",
                              "CollisionResponses": {"ResponseArray": [{"Channel": "WallClimb", "Response": "ECR_Block"}]}}})
    return a


def mesh(name, label, x, y, z, yaw=0.0, folder="Deco", sx=1.0, sy=1.0, sz=1.0, pitch=0.0, nocol=True):
    a = spawn_asset(J + name, label, x, y, z, folder, yaw=yaw, pitch=pitch, sx=sx, sy=sy, sz=sz)
    if nocol:
        c = comps(a, "/Script/Engine.StaticMeshComponent")
        setp(c[0], {"BodyInstance": {"CollisionProfileName": "NoCollision"}})
    return a


def text(label, x, z, s, size=26.0, color=(1.0, 1.0, 1.0), y=690.0, folder="Guide"):
    a = spawn_class("/Script/Engine.TextRenderActor", label, x, y, z, folder, yaw=90.0)
    c = comps(a, "/Script/Engine.TextRenderComponent")
    setp(c[0], {"Text": s, "WorldSize": size, "HorizontalAlignment": "EHTA_Center",
                "TextRenderColor": {"r": color[0], "g": color[1], "b": color[2], "a": 1.0}})
    return a


def tp(label, x, z, y=Y, folder="Gameplay/Points"):
    return spawn_class("/Script/Engine.TargetPoint", label, x, y, z, folder)


def bp(kind, label, x, z, folder, y=Y, yaw=0.0, pitch=0.0, props=None, sx=1.0, sy=1.0, sz=1.0, roll=0.0):
    a = spawn_asset(BP[kind], label, x, y, z, folder, yaw=yaw, pitch=pitch, roll=roll, sx=sx, sy=sy, sz=sz)
    if props:
        setp(a, props)
    return a


# ---------- 게임플레이 ----------
# 체크포인트 리셋 구간 지정
def bound(label, cp):
    c = comps(A[label], "/Script/EclipseTheNinja.CheckpointBoundComponent")
    if c:
        setp(c[0], {"BoundCheckpoint": cp})
    return len(c)


def checkpoint(label, x, z, cp, first=False, folder="Gameplay/Checkpoint"):
    return bp("checkpoint", label, x, z, folder, props={"CheckpointId": cp, "bActivateOnBeginPlay": first})


# 안전 지점 볼륨 + 복귀 위치. 볼륨은 겹침 시작에만 등록되므로 PlayerStart 를 첫 볼륨 안에 두지 않는다
def safepoint(label, x0, x1, z, tx, folder):
    tp(label + "_TP", tx, z + 100.0, folder=folder)
    return bp("safepoint", label, (x0 + x1) / 2.0, z, folder, sx=(x1 - x0) / 60.0, sy=6.0, sz=5.0,
              props={"SafePoint": ref(label + "_TP")})


# 가시 함정 : 데미지 볼륨 + 2x2 가시 메시 (skip 칸은 메시를 비운다)
def spikes(label, x0, x1, z, folder, skip=None, thin=False):
    n = int(round((x1 - x0) / 200.0))
    for i in range(n):
        cx = x0 + 100.0 + i * 200.0
        if skip and any(abs(cx - s) < 150.0 for s in skip):
            continue
        bp("spikes", label + "_Vis" + str(i), cx, z, folder)
    # thin : 위로 플랫폼이 지나가는 가시밭. 판정을 바닥 30cm로 낮춰 플랫폼(상판 +35) 탑승자는 닿지 않게 한다
    if thin:
        return bp("damage", label, (x0 + x1) / 2.0, z + 15.0, folder, sx=(x1 - x0) / 200.0, sy=3.0, sz=0.3)
    return bp("damage", label, (x0 + x1) / 2.0, z + 40.0, folder, sx=(x1 - x0) / 200.0, sy=3.0, sz=0.8)


# 스위치와 대상을 잇는 전선 (텍스트 없이 연결을 보여준다). pts 는 (x, z) 꺾은선
def wire(label, pts, y=702.0, t=8.0, folder="Deco/Wires", mat=None):
    for i in range(len(pts) - 1):
        x0, z0 = pts[i]
        x1, z1 = pts[i + 1]
        lo_x, hi_x = min(x0, x1) - t / 2.0, max(x0, x1) + t / 2.0
        lo_z, hi_z = min(z0, z1) - t / 2.0, max(z0, z1) + t / 2.0
        a = spawn_asset("/Engine/BasicShapes/Cube", label + "_" + str(i), (lo_x + hi_x) / 2.0, y, (lo_z + hi_z) / 2.0, folder,
                        sx=(hi_x - lo_x) / 100.0, sy=t / 100.0, sz=(hi_z - lo_z) / 100.0)
        c = comps(a, "/Script/Engine.StaticMeshComponent")
        setp(c[0], {"OverrideMaterials": [{"refPath": mat if mat else WIRE_MAT}], "BodyInstance": {"collisionProfileName": "NoCollision"}})


# 화살 함정 : 발사 방향은 액터 로컬 +Z 이고 발사 위치는 +Z 100cm
def arrow_down(label, x, z, folder):
    return bp("arrowtrap", label, x, z, folder, yaw=0.0, pitch=0.0, roll=180.0)


def arrow_left(label, x, z, folder):
    return bp("arrowtrap", label, x, z, folder, pitch=90.0)


def enemy(kind, label, x, z, folder, yaw=0.0, pmin=0.0, pmax=0.0, extra=None):
    props = {"PatrolMinOffset": pmin, "PatrolMaxOffset": pmax}
    if extra:
        for k in extra:
            props[k] = extra[k]
    return bp(kind, label, x, z + ENEMY_Z, folder, yaw=yaw, props=props)


# 은신구역 : 숨는 지점은 안쪽(-Y 75). 뒤에 대나무 발을 걸어 숨는 곳임을 보여준다
def stealth(label, x, z, folder):
    a = bp("stealth", label, x, z, folder)
    mesh("SM_JVK_JVK_Sudare_Bamboo_02", label + "_Blind", x, 950.0, z + 230.0, folder="Deco/Stealth")
    light(label + "_L", x, z + 120.0, y=900.0, intensity=4.0, radius=260.0, folder="Lighting")
    return a


# 덩굴 블록 (100x100x160). 길을 막을 때는 천장 아래 틈에 쌓는다
def vine(label, x, z, folder, sz=1.0):
    return bp("vineblock", label, x, z, folder, sz=sz)


# 덩굴끼리 서로 불이 옮겨 붙도록 양방향으로 잇는다
def link_vines(labels):
    for n in labels:
        setp(A[n], {"NeighborVines": [ref(m) for m in labels if m != n]})


# ---------- 배경·장식 ----------
# 성채 돌벽 배경 (기단 4단 + 처마 + 기둥 + 울타리 + 뒷판)
def fort_wall(prefix, x0, x1, base, rows=4, folder="Deco/Fortress"):
    n = int(math.ceil((x1 - x0) / 800.0))
    for i in range(n):
        x = x0 + i * 800.0
        for r in range(rows):
            mesh("SM_JVK_Foundation_A_01_2X8", prefix + "_W" + str(i) + "_" + str(r), x, 520.0, base + 160.0 + 200.0 * r, folder=folder)
        mesh("SM_JVK_Roof_A_02_Eave_x8", prefix + "_Eave" + str(i), x + 400.0, 540.0, base + 190.0 + 200.0 * (rows - 1), yaw=-90.0, folder=folder)
        mesh("SM_JVK_Column_A_01_x3", prefix + "_Pil" + str(i), x, 610.0, base - 40.0, folder=folder, sz=(200.0 * rows - 110.0) / 300.0)
        if i % 2 == 0:
            mesh("SM_JVK_Inuyarai_A_01_x4", prefix + "_Fence" + str(i), x + 400.0, 600.0, base - 2.0, folder=folder)
    a = block(prefix + "_Backing", x0, x0 + n * 800.0, base - 200.0, base + 200.0 * rows, mat="dark", folder=folder, y0=380.0, y1=500.0)
    return a


# 실내 벽 배경 (나무 벽 + 기둥, 높이는 300 단위)
INTERIOR_WALLS = ["SM_JVK_Wall_D_Window_02_x8", "SM_JVK_Wall_D_01_x8", "SM_JVK_Wall_B_Window_01_x8", "SM_JVK_Wall_D_02_x8"]


def interior(prefix, x0, x1, base, tiers=1, folder="Deco/Interior", seed=0, top=None):
    n = int(math.ceil((x1 - x0) / 800.0))
    for i in range(n):
        cx = x0 + 400.0 + i * 800.0
        for t in range(tiers):
            mesh(INTERIOR_WALLS[(i + t + seed) % len(INTERIOR_WALLS)], prefix + "_Wall" + str(i) + "_" + str(t), cx, 600.0, base + 300.0 * t, folder=folder)
        mesh("SM_JVK_Column_A_01_x3", prefix + "_Col" + str(i), cx - 400.0, 612.0, base, folder=folder, sz=tiers)
        if (i + seed) % 2 == 0:
            mesh("SM_JVK_Lamp_Wall_01", prefix + "_Lamp" + str(i), cx, 610.0, base + 190.0, folder=folder)
    block(prefix + "_Backing", x0, x0 + n * 800.0, base - 100.0, top if top is not None else base + 300.0 * tiers + 100.0, mat="dark", folder=folder, y0=480.0, y1=590.0)


# 천장 서까래 (Y 방향 보를 X 간격으로)
def rafters(prefix, x0, x1, z, step=400.0, folder="Deco/Interior"):
    n = int((x1 - x0) // step) + 1
    for i in range(n):
        mesh("SM_JVK_Beam_A_01_x8", prefix + str(i), x0 + i * step, 1000.0, z, folder=folder)


# 멀리 보이는 천수각 실루엣
def keep_silhouette(prefix, x, y, base, s=1.0, folder="Deco/Landmark"):
    tiers = [(1800.0, 700.0), (1300.0, 550.0), (850.0, 450.0), (500.0, 400.0)]
    z = base
    for i, t in enumerate(tiers):
        w = t[0] * s
        h = t[1] * s
        block(prefix + "_T" + str(i), x - w / 2.0, x + w / 2.0, z, z + h, mat="dark", folder=folder, y0=y - 400.0 * s, y1=y + 400.0 * s)
        mesh("SM_JVK_Roof_A_02_Eave_x8", prefix + "_Roof" + str(i), x, y + 400.0 * s, z + h - 10.0, yaw=-90.0, folder=folder, sx=2.5 * s, sy=(w + 300.0) / 800.0, sz=2.5 * s)
        z += h


HOUSE_WALLS = ["SM_JVK_Wall_B_Window_01_x8", "SM_JVK_Wall_B_01_x8", "SM_JVK_Wall_D_Window_02_x8", "SM_JVK_Wall_C_01_x8"]


# 배경 집 한 줄 (Y=600, 8m 단위)
def backdrop(prefix, x0, x1, base, folder="Deco/Backdrop", y=600.0, seed=0):
    n = int((x1 - x0) // 800)
    for i in range(n):
        cx = x0 + 400.0 + i * 800.0
        k = (i + seed) % len(HOUSE_WALLS)
        mesh("SM_JVK_Foundation_A_01_2X8", prefix + "_Found" + str(i), cx - 400.0, y - 10.0, base, folder=folder)
        mesh(HOUSE_WALLS[k], prefix + "_Wall" + str(i), cx, y, base, folder=folder)
        mesh("SM_JVK_Roof_A_01_Eave_x8", prefix + "_Eave" + str(i), cx, y + 6.0, base + 290.0, yaw=-90.0, folder=folder)
        mesh("SM_JVK_Column_A_01_x3", prefix + "_Col" + str(i), cx - 400.0, y + 12.0, base, folder=folder)
        if (i + seed) % 2 == 0:
            mesh("SM_JVK_Lamp_Wall_01", prefix + "_WLamp" + str(i), cx + 200.0, y + 8.0, base + 190.0, folder=folder)


# 지형 앞면을 돌 기단으로 덮는다 (Y=1300)
def front_stone(prefix, x0, x1, top, folder="Deco/Front"):
    n = int(math.ceil((x1 - x0) / 800.0))
    for i in range(n):
        sx = min(1.0, (x1 - (x0 + i * 800.0)) / 800.0)
        mesh("SM_JVK_Foundation_A_01_2X8", prefix + str(i), x0 + i * 800.0, 1300.0, top, folder=folder, sx=sx)


def torch(label, x, z, y=760.0):
    return mesh("SM_JVK_Lamp_Torch_01", label, x, y, z)


def lamp(label, x, z, y=760.0):
    return mesh("SM_JVK_Lamp_Street_01", label, x, y, z)


# ---------- 조명·월드 ----------
def light(label, x, z, y=820.0, intensity=12.0, radius=1100.0, folder="Lighting"):
    a = spawn_class("/Script/Engine.PointLight", label, x, y, z, folder)
    c = comps(a, "/Script/Engine.PointLightComponent")
    setp(c[0], {"Intensity": intensity, "AttenuationRadius": radius, "LightColor": {"r": 255, "g": 196, "b": 140, "a": 255}, "CastShadows": False})
    return a


# 길을 비추는 따뜻한 등불 (카메라 쪽 앞에 두어 바닥과 블록 앞면을 밝힌다)
def path_light(label, x, z, intensity=7.0, radius=750.0, y=1150.0):
    return light(label, x, z, y=y, intensity=intensity, radius=radius)


# 밤 : 푸른 달빛 + 어두운 하늘 + 옅은 안개. 노출이 고정이라 세기를 낮추면 그대로 어두워진다
def night_env(moon=0.2, sky=0.15):
    dl = env_actor("/Script/Engine.DirectionalLight")
    force_xf(dl, {"rotation": {"pitch": -38.0, "yaw": -62.0, "roll": 0.0}})
    setp(comps(dl, "/Script/Engine.DirectionalLightComponent")[0], {"Intensity": moon, "LightColor": {"r": 150, "g": 170, "b": 255, "a": 255}})
    sl = env_actor("/Script/Engine.SkyLight")
    setp(comps(sl, "/Script/Engine.SkyLightComponent")[0], {"Intensity": sky, "LightColor": {"r": 120, "g": 140, "b": 210, "a": 255}})
    fog = env_actor("/Script/Engine.ExponentialHeightFog")
    setp(comps(fog, "/Script/Engine.ExponentialHeightFogComponent")[0], {"FogDensity": 0.035, "FogInscatteringLuminance": {"r": 0.012, "g": 0.018, "b": 0.04, "a": 1.0}})
    for a in find_actors(name="SM_SkySphere"):
        setp(comps(a, "/Script/Engine.StaticMeshComponent")[0], {"bVisible": False, "bHiddenInGame": True})


def world_settings():
    ws = find_actors(name="WorldSettings")
    setp(ws[0], {"DefaultGameMode": {"refPath": GM}})


def pool_config():
    a = env_actor("/Game/Blueprint/Framework/BP_ObjectPoolLevelConfig.BP_ObjectPoolLevelConfig_C")
    if a:
        force_xf(a, xf(-500.0, Y, 1400.0))
