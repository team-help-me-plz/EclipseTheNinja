# 성 외곽 마을 PCG 레벨 빌더 (에디터 파이썬, 콘솔 `py <이 파일>`로 실행)
# 인자는 %TEMP%/EclipseMcp/pcg_build_args.json 에서 읽는다: {"mode": "...", "layout": "<json 경로>", ...}
# 결과 로그는 %TEMP%/EclipseMcp/pcg_build.out.txt 에 쓴다.
# 모드:
#   inspect : 레벨의 PCG 관련 액터(전용 액터·라벨 PCG*/Zone*/Preview*)의 크기·길이·적용 중인 그래프 값·ISM 수를 덤프
#   apply   : 레이아웃 JSON대로 전용 액터를 만들거나(라벨 기준) 고치고 다시 생성
#   migrate : 예전 방식 액터(PCG Volume + 그래프, BP_PCG_SplineSource, Ground_* 타일)를 전용 액터로 바꾼다
#   regen   : 지정 라벨(없으면 전부)의 PCG만 다시 생성
#   count   : 액터별 생성 인스턴스 수
#   shots   : 지정 카메라로 고해상도 스크린샷 요청(비동기, run.ps1이 파일 생성을 기다림)
#   save    : 현재 레벨 저장(비 World Partition 레벨용)
import unreal, json, os, re, traceback

TMP = os.path.join(os.environ.get('TEMP', 'C:/Temp'), 'EclipseMcp')
ARGS = os.path.join(TMP, 'pcg_build_args.json')
OUT = os.path.join(TMP, 'pcg_build.out.txt')

# 레이아웃 JSON의 class 이름 → C++ 클래스
CLASSES = {
    'Forest': 'EclipseForestVolume',
    'FarSilhouette': 'EclipseFarSilhouetteVolume',
    'Foreground': 'EclipseForegroundVolume',
    'Marker': 'EclipsePCGMarkerVolume',
    'Ground': 'EclipseGroundVolume',
    'Interior': 'EclipseInteriorVolume',
    'TownRow': 'EclipseTownRowActor',
    'CastleWall': 'EclipseCastleWallActor',
}
LINE_CLASSES = ('TownRow', 'CastleWall')

# inspect·migrate가 읽는 그래프 파라미터와 타입(i=int32, d=double, b=bool, v=vector, s=에셋 경로)
_COMMON = {'Seed': 'i'}
PARAMS = {
    'PCG_TownRow': dict(_COMMON, LampSpacing='d', PropDensity='d', FacingSide='d', PlaceStreet='b',
                        BlockHousesMin='i', BlockHousesMax='i', HouseOverlap='d', AlleyMin='d', AlleyMax='d',
                        AlleyFenceRatio='d', AlleyPropDensity='d', TwoStoryRatio='d', RowOffsetX='d',
                        WoodRoofBlockRatio='d', WoodBlockHousesMin='d', WoodBlockHousesMax='d', HouseSet='s'),
    'PCG_CastleWall': dict(_COMMON, WallHeightScale='d', SpawnFence='b', CornerMinTurn='d', FenceOffset='v',
                           FlipOutward='b', SpawnYagura='b', YaguraRatio='d', YaguraOffset='v', YaguraMinSpacing='d'),
    'PCG_CastleKeep': dict(_COMMON, TreeScaleMin='v', TreeScaleMax='v', YaguraCount='i', TreeDensity='d',
                           FillFloor='b', SpawnTenshu='b'),
    'PCG_Forest': dict(_COMMON, CellSize='v', NoiseThreshold='d', MapleRatio='d', CastleClearance='d',
                       MapleScaleMin='v', MapleScaleMax='v', BambooScaleMin='v', BambooScaleMax='v',
                       BambooKeepRatio='d', GridExtents='v', ClearCastleInterior='b', WallBandHalfWidth='d'),
    'PCG_FarSilhouette': dict(_COMMON, TenshuCount='i', TenshuZone='v', TenshuScaleMin='v', TenshuScaleMax='v',
                              YaguraCount='i', RoofClusterCount='i', TreeCellSize='v', TreeNoiseThreshold='d',
                              TreeScaleMin='v', TreeScaleMax='v', Extents='v'),
    'PCG_Foreground': dict(_COMMON, Density='d', MinSpacing='d', MaxCount='i', TreeRatio='d',
                           TreeScaleMin='v', TreeScaleMax='v', Extents='v'),
}

L = []
EAS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
H = unreal.PCGGraphParametersHelpers


def log(*a):
    L.append(' '.join(str(x) for x in a))


def all_actors():
    return EAS.get_all_level_actors()


def by_label(label):
    for a in all_actors():
        if a.get_actor_label() == label:
            return a
    return None


def pcg(a):
    return a.get_component_by_class(unreal.PCGComponent)


def vec(v):
    return unreal.Vector(float(v[0]), float(v[1]), float(v[2]) if len(v) > 2 else 0.0)


def ucls(key):
    return getattr(unreal, CLASSES[key])


def is_eclipse(a):
    return any(isinstance(a, getattr(unreal, c, type(None))) for c in CLASSES.values())


def snake(name):
    # C++ 속성 이름(PascalCase, bool은 b 접두사) → 파이썬 이름
    if re.match(r'^b[A-Z]', name):
        name = name[1:]
    return re.sub(r'(?<=[a-z0-9])(?=[A-Z])|(?<=[A-Z])(?=[A-Z][a-z])', '_', name).lower()


def graph_name(a):
    c = pcg(a)
    g = c.get_graph() if c else None
    return g.get_name() if g else None


def get_param(gi, name, t):
    if t == 'i':
        return H.get_int32_parameter(gi, name)
    if t == 'd':
        return H.get_double_parameter(gi, name)
    if t == 'b':
        return H.get_bool_parameter(gi, name)
    if t == 's':
        return str(H.get_soft_object_path_parameter(gi, name).export_text())
    p = H.get_vector_parameter(gi, name)
    return [p.x, p.y, p.z]


def convert(cur, v):
    # JSON 값을 현재 속성 타입에 맞춘다
    if isinstance(cur, unreal.Vector):
        return vec(v)
    if isinstance(cur, unreal.EnumBase):
        # 파이썬 열거형 이름은 PLAY_AREA 꼴이다. "PlayArea"·"PLAY_AREA" 둘 다 받는다
        name = str(v)
        return getattr(type(cur), name.upper(), None) or getattr(type(cur), snake(name).upper())
    if isinstance(cur, bool):
        return bool(v)
    if isinstance(cur, int):
        return int(v)
    if isinstance(cur, float):
        return float(v)
    if isinstance(cur, unreal.Array):
        return [float(x) if isinstance(x, (int, float)) else x for x in v]
    if isinstance(cur, unreal.SoftObjectPath):
        return unreal.SoftObjectPath(v)
    if isinstance(cur, unreal.Object) or cur is None or isinstance(cur, str):
        return unreal.load_asset(v) if isinstance(v, str) else v
    return v


def set_props(a, props):
    # "Params.Seed"처럼 점으로 구조체 안 값을 고친다(바닥·실내 볼륨). 구조체는 고친 뒤 통째로 다시 넣는다
    structs = {}
    for key, v in props.items():
        parts = key.split('.')
        try:
            if len(parts) == 2:
                sname = snake(parts[0])
                st = structs.get(sname) or a.get_editor_property(sname)
                structs[sname] = st
                fname = snake(parts[1])
                st.set_editor_property(fname, convert(st.get_editor_property(fname), v))
            else:
                pname = snake(key)
                a.set_editor_property(pname, convert(a.get_editor_property(pname), v))
            log('  prop', key, '=', v)
        except Exception as e:
            log('  !! prop', key, e)
    for sname, st in structs.items():
        a.set_editor_property(sname, st)


def spawn_or_get(d, key, loc):
    label = d['label']
    a = by_label(label)
    cls = ucls(key)
    if a is not None and not isinstance(a, cls):
        log('  !! %s 는 %s가 아니다(%s). migrate 모드로 먼저 바꾸거나 remove 후 다시 만든다' % (label, key, a.get_class().get_name()))
        return None
    if a is None:
        a = EAS.spawn_actor_from_class(cls, vec(loc))
        a.set_actor_label(label)
        log('new', key, label)
    else:
        log('update', key, label)
    a.set_actor_location(vec(loc), False, False)
    a.set_actor_scale3d(unreal.Vector(1, 1, 1))
    if 'folder' in d:
        a.set_folder_path(d['folder'])
    if 'tags' in d:
        a.set_editor_property('tags', [unreal.Name(t) for t in d['tags']])
    return a


def apply_volume(d):
    key = d['class']
    a = spawn_or_get(d, key, d['center'])
    if a is None:
        return
    a.set_editor_property('size', vec(d['size']))
    if key == 'Marker' and 'kind' in d:
        set_props(a, {'Kind': d['kind']})
    set_props(a, d.get('props', {}))


def apply_line(d):
    key = d['class']
    a = spawn_or_get(d, key, d['start'])
    if a is None:
        return
    a.set_editor_property('length', float(d['length']))
    set_props(a, d.get('props', {}))


def apply_cameras(cams):
    for c in cams:
        a = by_label(c['label'])
        loc = unreal.Vector(c['x'], c.get('y', 1000), c.get('z', 190))
        rot = unreal.Rotator(roll=0, pitch=0, yaw=-90)
        if a is None:
            a = EAS.spawn_actor_from_class(unreal.CameraActor, loc)
            a.set_actor_label(c['label'])
        a.set_actor_location_and_rotation(loc, rot, False, False)
        a.set_folder_path('Preview')
        cc = a.camera_component
        cc.set_editor_property('field_of_view', 65.0)
        cc.set_editor_property('aspect_ratio', 16.0 / 9.0)
        log('camera', c['label'], c['x'])


def apply_moves(moves):
    # PCG가 아닌 액터(플레이어 대역 등) 위치 옮기기
    for m in moves:
        a = by_label(m['label'])
        if a is None:
            log('  !! move target not found', m['label'])
            continue
        a.set_actor_location(vec(m['location']), False, False)
        log('moved', m['label'], m['location'])


def generate(labels=None):
    # 생성은 비동기라 결과 수는 다음 실행(count 모드)에서 센다. run.ps1이 apply/regen 뒤에 자동으로 부른다
    unreal.SystemLibrary.execute_console_command(None, 'pcg.FlushCache')
    for a in all_actors():
        c = pcg(a)
        if c is None or c.get_graph() is None:
            continue
        if labels and a.get_actor_label() not in labels:
            continue
        c.generate(True)
        log('generate requested', a.get_actor_label())


def count(labels=None):
    for a in all_actors():
        c = pcg(a)
        if c is None or c.get_graph() is None:
            continue
        if labels and a.get_actor_label() not in labels:
            continue
        n = 0
        for m in a.get_components_by_class(unreal.InstancedStaticMeshComponent):
            n += m.get_instance_count()
        log('instances', a.get_actor_label(), n, '' if n > 0 else '(0: 위치·크기·파라미터 확인)')


def inspect():
    for a in all_actors():
        lab = a.get_actor_label()
        if not (is_eclipse(a) or lab.startswith('PCG') or lab.startswith('Zone') or lab.startswith('Preview')):
            continue
        l, s = a.get_actor_location(), a.get_actor_scale3d()
        extra = ''
        if isinstance(a, unreal.EclipsePCGBoxVolume):
            sz = a.get_editor_property('size')
            extra = ' size=(%.0f,%.0f,%.0f)' % (sz.x, sz.y, sz.z)
        elif isinstance(a, unreal.EclipsePCGLineActor):
            extra = ' length=%.0f' % a.get_editor_property('length')
        log('== %s [%s] loc=(%.0f,%.0f,%.0f) scale=(%.2f,%.2f,%.2f)%s tags=%s folder=%s' % (
            lab, a.get_class().get_name(), l.x, l.y, l.z, s.x, s.y, s.z, extra,
            [str(t) for t in a.tags], a.get_folder_path()))
        c = pcg(a)
        if c is not None:
            gname = graph_name(a)
            log('   graph=%s' % gname)
            gi = c.get_editor_property('graph_instance')
            # 현재 적용 중인 값(전용 액터가 넣은 값 또는 그래프 기본값)
            ov = []
            for name, t in sorted(PARAMS.get(gname, {}).items()):
                v = get_param(gi, name, t)
                if t == 'v':
                    v = [round(x, 2) for x in v]
                elif t == 'd':
                    v = round(v, 3)
                ov.append('%s=%s' % (name, v))
            if ov:
                log('   graph values: ' + ', '.join(ov))
        for sp in a.get_components_by_class(unreal.SplineComponent):
            n = sp.get_number_of_spline_points()
            pts = [sp.get_location_at_spline_point(i, unreal.SplineCoordinateSpace.LOCAL) for i in range(n)]
            log('   spline pts=%s' % ['(%.0f,%.0f,%.0f)' % (p.x, p.y, p.z) for p in pts])
        counts = {}
        for m in a.get_components_by_class(unreal.InstancedStaticMeshComponent):
            sm = m.get_editor_property('static_mesh')
            k = sm.get_name() if sm else 'None'
            counts[k] = counts.get(k, 0) + m.get_instance_count()
        if counts:
            log('   ism', sorted(counts.items(), key=lambda kv: -kv[1]))


# ───────────── migrate: 예전 방식 → 전용 액터 ─────────────

# 그래프 값 → 전용 액터 속성. (그래프 이름, 타입, 액터 속성, 벡터면 몇 번째 성분)
MIGRATE_MAP = {
    'PCG_Forest': ('Forest', [('Seed', 'i', 'Seed', None), ('CellSize', 'v', 'CellSpacing', 0),
                              ('NoiseThreshold', 'd', 'NoiseThreshold', None), ('MapleRatio', 'd', 'MapleRatio', None),
                              ('MapleScaleMin', 'v', 'MapleScaleMin', 0), ('MapleScaleMax', 'v', 'MapleScaleMax', 0),
                              ('BambooKeepRatio', 'd', 'BambooKeepRatio', None),
                              ('BambooScaleMin', 'v', 'BambooScaleMin', 'all'), ('BambooScaleMax', 'v', 'BambooScaleMax', 'all'),
                              ('WallBandHalfWidth', 'd', 'WallBandHalfWidth', None),
                              ('ClearCastleInterior', 'b', 'ClearCastleInterior', None),
                              ('CastleClearance', 'd', 'CastleClearance', None)]),
    'PCG_FarSilhouette': ('FarSilhouette', [('Seed', 'i', 'Seed', None), ('TenshuCount', 'i', 'TenshuCount', None),
                                            ('TenshuZone', 'v', 'TenshuRangeX', 0),
                                            ('TenshuScaleMin', 'v', 'TenshuScaleMin', 0), ('TenshuScaleMax', 'v', 'TenshuScaleMax', 0),
                                            ('YaguraCount', 'i', 'YaguraCount', None), ('RoofClusterCount', 'i', 'RoofClusterCount', None),
                                            ('TreeCellSize', 'v', 'TreeSpacing', 0), ('TreeNoiseThreshold', 'd', 'TreeNoiseThreshold', None),
                                            ('TreeScaleMin', 'v', 'TreeScaleMin', 0), ('TreeScaleMax', 'v', 'TreeScaleMax', 0)]),
    'PCG_Foreground': ('Foreground', [('Seed', 'i', 'Seed', None), ('Density', 'd', 'Density', None),
                                      ('MinSpacing', 'd', 'MinSpacing', None), ('MaxCount', 'i', 'MaxCount', None),
                                      ('TreeRatio', 'd', 'TreeRatio', None),
                                      ('TreeScaleMin', 'v', 'TreeScaleMin', 0), ('TreeScaleMax', 'v', 'TreeScaleMax', 0)]),
}
TOWN_KEYS = ['Seed', 'LampSpacing', 'PropDensity', 'PlaceStreet', 'BlockHousesMin', 'BlockHousesMax', 'HouseOverlap',
             'AlleyMin', 'AlleyMax', 'AlleyFenceRatio', 'AlleyPropDensity', 'TwoStoryRatio', 'RowOffsetX',
             'WoodRoofBlockRatio', 'WoodBlockHousesMin', 'WoodBlockHousesMax', 'HouseSet']
WALL_KEYS = ['Seed', 'WallHeightScale', 'SpawnFence', 'FenceOffset', 'SpawnYagura']


def read_props(a, gname, mapping):
    gi = pcg(a).get_editor_property('graph_instance')
    props = {}
    for gkey, t, akey, comp in mapping:
        v = get_param(gi, gkey, t)
        # 성분 번호가 있으면 그 성분만(균일 배율·간격), 없거나 'all'이면 벡터 그대로
        if t == 'v' and isinstance(comp, int):
            v = v[comp]
        props[akey] = v
    return props


def replace(old, key, loc, d):
    # 같은 라벨·폴더로 새 액터를 만들고 예전 액터는 지운다
    label, folder = old.get_actor_label(), old.get_folder_path()
    tags = [str(t) for t in old.tags if str(t) not in ('PCG_Exclusion', 'PCG_Village', 'PCG_CastleWall')]
    EAS.destroy_actor(old)
    d = dict(d, label=label, folder=str(folder), **({'tags': tags} if tags else {}))
    if key in LINE_CLASSES:
        d.update(start=loc)
        apply_line(dict(d, **{'class': key}))
    else:
        d.update(center=loc)
        apply_volume(dict(d, **{'class': key}))


def spline_points_world(a):
    sp = a.get_component_by_class(unreal.SplineComponent)
    n = sp.get_number_of_spline_points()
    return [sp.get_location_at_spline_point(i, unreal.SplineCoordinateSpace.WORLD) for i in range(n)]


def wall_bastions(pts, base_y):
    # 앞(+Y)으로 튀어나왔다가 다시 들어가는 구간 = 돌출부. (중심 X, 폭, 깊이)
    out = []
    i = 0
    while i < len(pts) - 1:
        if pts[i + 1].y > pts[i].y + 100:
            j = i + 1
            while j < len(pts) - 1 and abs(pts[j + 1].y - pts[j].y) <= 100:
                j += 1
            if j < len(pts) - 1 and pts[j + 1].y < pts[j].y - 100:
                x0, x1 = pts[i + 1].x, pts[j].x
                out.append(((x0 + x1) / 2, abs(x1 - x0), pts[i + 1].y - pts[i].y))
            i = j
        i += 1
    return out


def migrate():
    for a in list(all_actors()):
        if is_eclipse(a):
            continue
        gname = graph_name(a)
        cname = a.get_class().get_name()
        is_volume = isinstance(a, unreal.PCGVolume)

        if is_volume and gname in MIGRATE_MAP:
            key, mapping = MIGRATE_MAP[gname]
            s = a.get_actor_scale3d()
            size = [200 * abs(s.x), 200 * abs(s.y), 200 * abs(s.z)]
            props = read_props(a, gname, mapping)
            l = a.get_actor_location()
            log('migrate volume', a.get_actor_label(), gname, '->', key, 'size', [round(x) for x in size])
            replace(a, key, [l.x, l.y, l.z], {'size': size, 'props': props})
        elif is_volume and gname is None and any(str(t) in ('PCG_Exclusion', 'PCG_Village') for t in a.tags):
            s = a.get_actor_scale3d()
            size = [200 * abs(s.x), 200 * abs(s.y), 200 * abs(s.z)]
            kind = 'Village' if any(str(t) == 'PCG_Village' for t in a.tags) else 'PlayArea'
            l = a.get_actor_location()
            log('migrate marker', a.get_actor_label(), '->', kind)
            replace(a, 'Marker', [l.x, l.y, l.z], {'size': size, 'kind': kind})
        elif gname == 'PCG_TownRow' and not is_volume:
            pts = spline_points_world(a)
            props = read_props(a, gname, [(k, PARAMS[gname][k], k, None) for k in TOWN_KEYS])
            start = [pts[0].x, pts[0].y, pts[0].z]
            length = pts[-1].x - pts[0].x
            if any(abs(p.y - pts[0].y) > 50 for p in pts):
                log('  .. %s: 꺾인 줄을 첫 점 Y(%.0f) 직선으로 바꿈' % (a.get_actor_label(), pts[0].y))
            log('migrate town row', a.get_actor_label(), 'start', [round(x) for x in start], 'length', round(length))
            replace(a, 'TownRow', start, {'length': length, 'props': props})
        elif gname == 'PCG_CastleWall' and not is_volume:
            pts = spline_points_world(a)
            props = read_props(a, gname, [(k, PARAMS[gname][k], k, None) for k in WALL_KEYS])
            base_y = pts[0].y
            bastions = wall_bastions(pts, base_y)
            ground_z = pts[0].z - 400 * props['WallHeightScale']
            start = [pts[0].x, base_y, ground_z]
            length = pts[-1].x - pts[0].x
            if bastions:
                props['BastionPlacement'] = 'Manual'
                props['BastionPositions'] = [b[0] - pts[0].x for b in bastions]
                props['BastionWidth'] = max(800, sum(b[1] for b in bastions) / len(bastions))
                props['BastionDepth'] = max(400, sum(b[2] for b in bastions) / len(bastions))
            else:
                props['BastionPlacement'] = 'Auto'
                props['BastionCount'] = 0
            if any(abs(p.y - base_y) > 50 and abs(p.y - base_y) < 300 for p in pts) or len(set(round(p.y) for p in pts)) > 2 * len(bastions) + 1:
                log('  .. %s: 기본 선이 여러 Y였음 → 첫 점 Y(%.0f) 직선 + 돌출부 %d개로 바꿈' % (a.get_actor_label(), base_y, len(bastions)))
            log('migrate wall', a.get_actor_label(), 'start', [round(x) for x in start], 'length', round(length),
                'bastions', [(round(b[0]), round(b[1]), round(b[2])) for b in bastions])
            replace(a, 'CastleWall', start, {'length': length, 'props': props})
        elif cname == 'EclipseInteriorVolume':
            pass

    # 실내 볼륨: 스케일 → 크기
    for a in all_actors():
        if isinstance(a, unreal.EclipseInteriorVolume):
            s = a.get_actor_scale3d()
            if abs(s.x - 1) > 1e-3 or abs(s.y - 1) > 1e-3 or abs(s.z - 1) > 1e-3:
                a.convert_scale_to_size()
                sz = a.get_editor_property('size')
                log('migrate interior', a.get_actor_label(), 'size', [round(sz.x), round(sz.y), round(sz.z)])

    # Ground_* 흙 타일 → 바닥 볼륨 하나
    tiles = [a for a in all_actors() if a.get_actor_label().startswith('Ground_') and isinstance(a, unreal.StaticMeshActor)]
    if tiles:
        x0 = y0 = 1e12
        x1 = y1 = -1e12
        z = 0.0
        for t in tiles:
            l, s = t.get_actor_location(), t.get_actor_scale3d()
            h = 800 * abs(s.x)
            x0, x1 = min(x0, l.x - h), max(x1, l.x + h)
            y0, y1 = min(y0, l.y - h), max(y1, l.y + h)
            z = l.z
        for t in tiles:
            EAS.destroy_actor(t)
        center = [(x0 + x1) / 2, (y0 + y1) / 2, z - 50]
        apply_volume({'class': 'Ground', 'label': 'PCG_Ground', 'folder': 'PCG/Ground',
                      'center': center, 'size': [x1 - x0, y1 - y0, 100]})
        log('migrate ground tiles', len(tiles), '-> PCG_Ground', [round(c) for c in center])


def shots(args):
    # 한 실행에 한 장만 요청한다(같은 프레임에 여러 장 요청하면 마지막 장만 저장됨). run.ps1이 카메라마다 따로 부른다
    cam, path = args['shot']
    a = by_label(cam)
    if a is None:
        log('!! camera not found', cam)
        return
    unreal.AutomationLibrary.take_high_res_screenshot(1600, 900, path.replace('\\', '/'), a)
    log('shot requested', cam, path)


def main():
    args = json.loads(open(ARGS, encoding='utf-8-sig').read())
    mode = args['mode']
    if mode == 'inspect':
        inspect()
    elif mode == 'apply':
        lay = json.loads(open(args['layout'], encoding='utf-8-sig').read())
        for lbl in lay.get('remove', []):
            a = by_label(lbl)
            if a is not None:
                EAS.destroy_actor(a)
                log('removed', lbl)
        for d in lay.get('volumes', []):
            apply_volume(d)
        for d in lay.get('lines', []):
            apply_line(d)
        if 'cameras' in lay:
            apply_cameras(lay['cameras'])
        if 'moves' in lay:
            apply_moves(lay['moves'])
        generate(None)
    elif mode == 'migrate':
        migrate()
        generate(None)
    elif mode == 'regen':
        generate(args.get('labels'))
    elif mode == 'count':
        count(args.get('labels'))
    elif mode == 'shots':
        shots(args)
    elif mode == 'save':
        r = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
        log('save_current_level', r)
    else:
        log('!! unknown mode', mode)


try:
    main()
except Exception:
    L.append(traceback.format_exc())
open(OUT, 'w', encoding='utf-8').write('\n'.join(L))
