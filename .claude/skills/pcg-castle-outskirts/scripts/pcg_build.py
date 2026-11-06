# 성 외곽 마을 PCG 레벨 빌더 (에디터 파이썬, 콘솔 `py <이 파일>`로 실행)
# 인자는 %TEMP%/EclipseMcp/pcg_build_args.json 에서 읽는다: {"mode": "...", "layout": "<json 경로>", ...}
# 결과 로그는 %TEMP%/EclipseMcp/pcg_build.out.txt 에 쓴다.
# 모드:
#   inspect : 레벨의 PCG 관련 액터(라벨 PCG*/Zone*/Preview*)·파라미터 덮어쓰기·스플라인·생성 ISM 수를 덤프
#   apply   : 레이아웃 JSON대로 액터를 만들거나(라벨 기준) 고치고, 파라미터를 넣고, 다시 생성
#   regen   : 지정 라벨(없으면 전부)의 PCG만 다시 생성
#   shots   : 지정 카메라로 고해상도 스크린샷 요청(비동기, run.ps1이 파일 생성을 기다림)
#   save    : 현재 레벨 저장(비 World Partition 레벨용)
import unreal, json, os, traceback

TMP = os.path.join(os.environ.get('TEMP', 'C:/Temp'), 'EclipseMcp')
ARGS = os.path.join(TMP, 'pcg_build_args.json')
OUT = os.path.join(TMP, 'pcg_build.out.txt')
GRAPH_DIR = '/Game/PCG/Graphs/'
SPLINE_BP = '/Game/PCG/Actors/BP_PCG_SplineSource.BP_PCG_SplineSource_C'
GROUND_MESH = '/Game/_ExternalAssets/JapaneseVillageKit/Meshes/SM_JVK_Paving_Ground_Dirt_01_16x16'

# 그래프별 덮어쓸 수 있는 파라미터와 타입(i=int32, d=double, b=bool, v=vector, s=에셋 경로 문자열). 메시(소프트 오브젝트) 파라미터는 제외
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


def graph_name(a):
    g = pcg(a).get_graph()
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


def same(a, b):
    if isinstance(a, str) or isinstance(b, str):
        return str(a).split('.')[0] == str(b).split('.')[0]
    if isinstance(a, list):
        return all(abs(float(x) - float(y)) < 1e-3 for x, y in zip(a, b))
    if isinstance(a, bool) or isinstance(b, bool):
        return bool(a) == bool(b)
    return abs(float(a) - float(b)) < 1e-3


def set_param(a, name, v):
    gname = graph_name(a)
    types = PARAMS.get(gname, {})
    if name not in types:
        log('  !! unknown param', name, 'for', gname, '(가능:', ', '.join(sorted(types)), ')')
        return
    t = types[name]
    gi = pcg(a).get_editor_property('graph_instance')
    if t == 'b':
        H.set_bool_parameter(gi, name, bool(v))
    elif t == 'v':
        H.set_vector_parameter(gi, name, vec(v))
    elif t == 'i':
        H.set_int32_parameter(gi, name, int(v))
    elif t == 's':
        H.set_soft_object_path_parameter(gi, name, unreal.SoftObjectPath(v))
    else:
        H.set_double_parameter(gi, name, float(v))
    # set 함수는 반환값이 없으므로 다시 읽어 확인
    got = get_param(gi, name, t)
    want = [float(x) for x in v] + [0.0] * (3 - len(v)) if t == 'v' else v
    if same(want, got):
        log('  param', name, '=', v)
    else:
        log('  !! param', name, 'want', v, 'got', got)


def set_common(a, d):
    if 'folder' in d:
        a.set_folder_path(d['folder'])
    if 'tags' in d:
        a.set_editor_property('tags', [unreal.Name(t) for t in d['tags']])


def apply_graph_and_params(a, d):
    c = pcg(a)
    if c is None:
        return
    if d.get('graph'):
        g = unreal.EditorAssetLibrary.load_asset(GRAPH_DIR + d['graph'])
        if g is None:
            log('  !! graph not found', d['graph'])
            return
        if c.get_graph() != g:
            c.set_graph(g)
    for k, v in d.get('params', {}).items():
        set_param(a, k, v)


def apply_volume(d):
    label = d['label']
    a = by_label(label)
    center = d['center']
    if a is None:
        a = EAS.spawn_actor_from_class(unreal.PCGVolume, vec(center))
        a.set_actor_label(label)
        log('new volume', label)
    else:
        log('update volume', label)
    a.set_actor_location(vec(center), False, False)
    h = d['half']
    # PCG Volume 브러시는 반폭 100cm 정육면체라 스케일 = 반폭 / 100
    a.set_actor_scale3d(unreal.Vector(h[0] / 100.0, h[1] / 100.0, h[2] / 100.0))
    set_common(a, d)
    apply_graph_and_params(a, d)
    return a


def apply_spline(d):
    label = d['label']
    a = by_label(label)
    loc = d.get('location', [0, 0, 0])
    if a is None:
        cls = unreal.load_object(None, SPLINE_BP)
        a = EAS.spawn_actor_from_class(cls, vec(loc))
        a.set_actor_label(label)
        log('new spline', label)
    else:
        log('update spline', label)
    a.set_actor_location(vec(loc), False, False)
    set_common(a, d)
    if 'points' in d:
        s = a.get_component_by_class(unreal.SplineComponent)
        # BP 기본 스플라인으로 되돌아가지 않게 편집 표시
        s.set_editor_property('spline_has_been_edited', True)
        # UE 5.8: add_spline_point(..., False)로 넣은 점은 개수에 잡히지 않는다 → 한 번에 넣는다
        pts = [unreal.Vector(p[0], p[1], p[2] if len(p) > 2 else 0) for p in d['points']]
        s.set_spline_points(pts, unreal.SplineCoordinateSpace.LOCAL, True)
        if d.get('linear', False):
            for i in range(len(d['points'])):
                s.set_spline_point_type(i, unreal.SplinePointType.LINEAR, False)
        s.set_closed_loop(d.get('closed', False), False)
        s.update_spline()
        s.modify()
        log('  spline points', s.get_number_of_spline_points(), 'len', round(s.get_spline_length()))
    apply_graph_and_params(a, d)
    return a


def apply_ground(g):
    # 기존 Ground_* 액터를 지우고 줄 정의대로 다시 깐다
    for a in list(all_actors()):
        if a.get_actor_label().startswith('Ground_'):
            EAS.destroy_actor(a)
    mesh = unreal.EditorAssetLibrary.load_asset(g.get('mesh', GROUND_MESH))
    n = 0
    for row in g['rows']:
        sc = row.get('scale', 1)
        step = 1600 * sc
        x = row['x_from']
        while x <= row['x_to']:
            a = EAS.spawn_actor_from_object(mesh, unreal.Vector(x, row['y'], 0))
            a.set_actor_label('Ground_%d' % n)
            a.set_folder_path('Ground')
            a.set_actor_scale3d(unreal.Vector(sc, sc, 1))
            n += 1
            x += step
    log('ground tiles', n)


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
        log('instances', a.get_actor_label(), n, '' if n > 0 else '(0: 위치·크기·파라미터 확인. CastleKeep은 열린 성벽이면 정상)')


def apply_moves(moves):
    # PCG가 아닌 액터(플레이어 대역 등) 위치 옮기기
    for m in moves:
        a = by_label(m['label'])
        if a is None:
            log('  !! move target not found', m['label'])
            continue
        a.set_actor_location(vec(m['location']), False, False)
        log('moved', m['label'], m['location'])


def inspect():
    for a in all_actors():
        lab = a.get_actor_label()
        if not (lab.startswith('PCG') or lab.startswith('Zone') or lab.startswith('Preview')):
            continue
        l, s = a.get_actor_location(), a.get_actor_scale3d()
        log('== %s [%s] loc=(%.0f,%.0f,%.0f) scale=(%.2f,%.2f,%.2f) tags=%s folder=%s' % (
            lab, a.get_class().get_name(), l.x, l.y, l.z, s.x, s.y, s.z,
            [str(t) for t in a.tags], a.get_folder_path()))
        c = pcg(a)
        if c is not None:
            g = c.get_graph()
            gname = g.get_name() if g else None
            log('   graph=%s' % gname)
            gi = c.get_editor_property('graph_instance')
            # 현재 적용 중인 값(덮어쓴 값 또는 그래프 기본값)
            ov = []
            for name, t in sorted(PARAMS.get(gname, {}).items()):
                v = get_param(gi, name, t)
                if t == 'v':
                    v = [round(x, 2) for x in v]
                elif t == 'd':
                    v = round(v, 3)
                ov.append('%s=%s' % (name, v))
            if ov:
                log('   params: ' + ', '.join(ov))
        for sp in a.get_components_by_class(unreal.SplineComponent):
            n = sp.get_number_of_spline_points()
            pts = [sp.get_location_at_spline_point(i, unreal.SplineCoordinateSpace.LOCAL) for i in range(n)]
            log('   spline closed=%s pts=%s' % (sp.is_closed_loop(), ['(%.0f,%.0f)' % (p.x, p.y) for p in pts]))
        counts = {}
        for m in a.get_components_by_class(unreal.InstancedStaticMeshComponent):
            sm = m.get_editor_property('static_mesh')
            k = sm.get_name() if sm else 'None'
            counts[k] = counts.get(k, 0) + m.get_instance_count()
        if counts:
            log('   ism', sorted(counts.items(), key=lambda kv: -kv[1]))


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
        if 'ground' in lay:
            apply_ground(lay['ground'])
        for d in lay.get('volumes', []):
            apply_volume(d)
        for d in lay.get('splines', []):
            apply_spline(d)
        if 'cameras' in lay:
            apply_cameras(lay['cameras'])
        if 'moves' in lay:
            apply_moves(lay['moves'])
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
