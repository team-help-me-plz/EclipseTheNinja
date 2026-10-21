# 레버 스켈레탈 메시 + 애니메이션 생성 스크립트 (Blender 4.x)
# 실행: blender --background --factory-startup --python build_lever.py
# 결과: SK_Lever.fbx (메시 + 스켈레톤 + 애니메이션 Lever_Toggle), SK_Lever.blend
#
# 외형: 에도 시대 마을(JapaneseVillageKit) 톤의 목재 장치(からくり) 레버
#   - 받침: 모서리를 깎은 주춧돌 (MI_Foundation_A_01 계열)
#   - 지지대: 짙은 목재 기둥 2개 + 쇠 캡, 쇠 축
#   - 손잡이: 팔각 목재 막대 + 쇠 고리 + 새끼줄 감은 손잡이 끝
#   - 장식: 기둥 바깥 면에 붙은 부적(お札)
#
# 본 구조
#   Root    : 받침/지지대. 원점(바닥 중앙)
#   Handle  : 손잡이. 축 위치에서 회전 (UE 기준 X축 방향 = 횡스크롤 화면 좌우로 젖힘)
#             레퍼런스(바인드) 포즈 자체가 Off(-35도)로 기울어진 상태다
#
# 애니메이션 (30fps, 0.5초)
#   Lever_Toggle : Off(-35도) -> On(+35도), ease-in-out, 오버슈트 없음
#                  본 로컬 기준으로는 0도 -> +70도
#   On -> Off는 같은 시퀀스를 역재생한다 (PlayRate -1)
#
# 머티리얼 슬롯 (UE에서 JVK 머티리얼 인스턴스로 교체 권장)
#   M_Lever_Stone -> MI_Foundation_A_01
#   M_Lever_Wood  -> MI_JVK_Wood_01
#   M_Lever_Iron, M_Lever_Rope, M_Lever_Paper

import bpy
import math
import os
from mathutils import Matrix, Vector

OUT_DIR = os.path.dirname(os.path.abspath(__file__))
FPS = 30
DURATION_FRAMES = 15
OFF_ANGLE = -35.0
ON_ANGLE = 35.0

# 크기 (m 단위, UE 임포트 시 cm로 변환됨)
BASE_SIZE = (0.70, 0.45, 0.18)
POST_SIZE = (0.10, 0.10, 0.42)
POST_GAP = 0.09                 # 기둥 안쪽 면 사이 절반 거리 (Y축)
PIVOT_Z = 0.48
HANDLE_LEN = 0.85
HANDLE_RADIUS = 0.032
GRIP_LEN = 0.22

PALETTE = {
    "M_Lever_Stone": (0.42, 0.40, 0.37),
    "M_Lever_Wood":  (0.20, 0.12, 0.07),
    "M_Lever_Iron":  (0.08, 0.08, 0.09),
    "M_Lever_Rope":  (0.62, 0.52, 0.34),
    "M_Lever_Paper": (0.85, 0.80, 0.66),
}


def reset_scene():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.unit_settings.system = 'METRIC'
    scene.unit_settings.scale_length = 1.0
    scene.render.fps = FPS


def make_materials():
    mats = {}
    for name, color in PALETTE.items():
        mat = bpy.data.materials.new(name)
        mat.use_nodes = True
        bsdf = mat.node_tree.nodes.get("Principled BSDF")
        bsdf.inputs["Base Color"].default_value = (*color, 1.0)
        bsdf.inputs["Roughness"].default_value = 0.45 if name == "M_Lever_Iron" else 0.85
        bsdf.inputs["Metallic"].default_value = 0.8 if name == "M_Lever_Iron" else 0.0
        mat.diffuse_color = (*color, 1.0)
        mats[name] = mat
    return mats


class Builder:
    def __init__(self, mats):
        self.mats = mats
        self.parts = []

    def _finish(self, obj, mat, group, bevel=0.0, segments=2):
        obj.data.materials.clear()
        obj.data.materials.append(self.mats[mat])
        if bevel > 0:
            mod = obj.modifiers.new("Bevel", 'BEVEL')
            mod.width = bevel
            mod.segments = segments
            mod.limit_method = 'ANGLE'
            bpy.context.view_layer.objects.active = obj
            bpy.ops.object.modifier_apply(modifier=mod.name)
        bpy.ops.object.select_all(action='DESELECT')
        obj.select_set(True)
        bpy.context.view_layer.objects.active = obj
        bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
        vg = obj.vertex_groups.new(name=group)
        vg.add([v.index for v in obj.data.vertices], 1.0, 'REPLACE')
        self.parts.append(obj)
        return obj

    def box(self, center, size, mat, group, bevel=0.01, rot=(0, 0, 0)):
        bpy.ops.mesh.primitive_cube_add(size=1.0, location=center, rotation=rot)
        obj = bpy.context.active_object
        obj.scale = size
        return self._finish(obj, mat, group, bevel)

    def cyl(self, center, radius, depth, mat, group, rot=(0, 0, 0), verts=12, bevel=0.0):
        bpy.ops.mesh.primitive_cylinder_add(vertices=verts, radius=radius, depth=depth,
                                            location=center, rotation=rot)
        return self._finish(bpy.context.active_object, mat, group, bevel, 1)

    def ring(self, center, major, minor, mat, group, rot=(0, 0, 0), seg=16):
        bpy.ops.mesh.primitive_torus_add(major_radius=major, minor_radius=minor,
                                         major_segments=seg, minor_segments=6,
                                         location=center, rotation=rot)
        return self._finish(bpy.context.active_object, mat, group)


def build_parts(b):
    bx, by, bz = BASE_SIZE

    # ---- Root: 주춧돌 받침 (2단) ----
    b.box((0, 0, bz * 0.35), (bx, by, bz * 0.7), "M_Lever_Stone", "Root", bevel=0.03)
    b.box((0, 0, bz * 0.85), (bx * 0.78, by * 0.8, bz * 0.3), "M_Lever_Stone", "Root", bevel=0.025)

    # ---- Root: 목재 기둥 2개 + 쇠 캡 + 쇠 띠 ----
    px, py, pz = POST_SIZE
    for side in (-1, 1):
        y = side * (POST_GAP + py / 2)
        b.box((0, y, bz + pz / 2), (px, py, pz), "M_Lever_Wood", "Root", bevel=0.012)
        b.box((0, y, bz + pz + 0.015), (px + 0.02, py + 0.02, 0.03), "M_Lever_Iron", "Root", bevel=0.006)
        b.box((0, y, bz + 0.05), (px + 0.012, py + 0.012, 0.025), "M_Lever_Iron", "Root", bevel=0.004)
        # 축 끝 쇠 와셔
        b.cyl((0, side * (POST_GAP + py + 0.008), PIVOT_Z), 0.035, 0.016, "M_Lever_Iron", "Root",
              rot=(math.radians(90), 0, 0), verts=12)
    # 기둥 아래를 잇는 목재 하단 보
    b.box((0, 0, bz + 0.035), (px * 0.8, POST_GAP * 2, 0.07), "M_Lever_Wood", "Root", bevel=0.008)
    # 쇠 축 (Y축 방향)
    b.cyl((0, 0, PIVOT_Z), 0.016, (POST_GAP + py) * 2 + 0.03, "M_Lever_Iron", "Root",
          rot=(math.radians(90), 0, 0), verts=10)

    # 부적 (기둥 바깥 면, 횡스크롤 카메라 쪽에서 보이도록 양쪽 모두)
    for side in (-1, 1):
        face_y = side * (POST_GAP + py)
        b.box((0, face_y + side * 0.002, bz + 0.17), (0.06, 0.004, 0.17), "M_Lever_Paper", "Root", bevel=0.0)
        b.box((0, face_y + side * 0.005, bz + 0.17), (0.012, 0.003, 0.12), "M_Lever_Iron", "Root", bevel=0.0)

    # ---- Handle: 축 허브 + 팔각 막대 + 쇠 고리 + 새끼줄 손잡이 ----
    b.box((0, 0, PIVOT_Z), (0.09, POST_GAP * 2 - 0.01, 0.09), "M_Lever_Wood", "Handle", bevel=0.012)
    shaft_bottom = PIVOT_Z + 0.04
    shaft_len = HANDLE_LEN - 0.04
    b.cyl((0, 0, shaft_bottom + shaft_len / 2), HANDLE_RADIUS, shaft_len, "M_Lever_Wood", "Handle",
          verts=8, bevel=0.004)
    for z in (shaft_bottom + 0.02, shaft_bottom + shaft_len * 0.45):
        b.cyl((0, 0, z), HANDLE_RADIUS + 0.008, 0.03, "M_Lever_Iron", "Handle", verts=8, bevel=0.003)

    # 새끼줄 감기: 살짝씩 기울인 고리를 쌓아 꼬인 느낌을 낸다
    grip_start = PIVOT_Z + HANDLE_LEN - GRIP_LEN - 0.03
    count = 11
    step = GRIP_LEN / count
    for i in range(count):
        tilt = math.radians(9 if i % 2 == 0 else -9)
        b.ring((0, 0, grip_start + step * (i + 0.5)), HANDLE_RADIUS + 0.005, step * 0.55,
               "M_Lever_Rope", "Handle", rot=(tilt, 0, 0), seg=12)
    # 끝 쇠 마개
    top = PIVOT_Z + HANDLE_LEN
    b.cyl((0, 0, top - 0.01), HANDLE_RADIUS + 0.01, 0.03, "M_Lever_Iron", "Handle", verts=8, bevel=0.004)


def join_parts(parts):
    bpy.ops.object.select_all(action='DESELECT')
    for p in parts:
        p.select_set(True)
    bpy.context.view_layer.objects.active = parts[0]
    bpy.ops.object.join()
    obj = bpy.context.active_object
    obj.name = "SK_Lever"
    obj.data.name = "SK_Lever"

    # 월드 크기 기준 박스 투영 UV (타일링 머티리얼용, 1UV = 1m)
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.mesh.remove_doubles(threshold=0.0001)
    bpy.ops.uv.cube_project(cube_size=1.0, correct_aspect=True, scale_to_bounds=False)
    bpy.ops.object.mode_set(mode='OBJECT')
    for poly in obj.data.polygons:
        poly.use_smooth = False

    # 손잡이를 Off 각도로 눕혀 레퍼런스 포즈를 만든다 (UV 투영 후라 텍스처는 손잡이를 따라 기운다)
    tilt = off_tilt_matrix()
    handle_group = obj.vertex_groups["Handle"].index
    for v in obj.data.vertices:
        if any(g.group == handle_group for g in v.groups):
            v.co = tilt @ v.co
    return obj


def off_tilt_matrix():
    # 월드 Y축 회전: 음수 각도면 +Z가 -X 쪽으로 기운다
    pivot = Vector((0, 0, PIVOT_Z))
    return (Matrix.Translation(pivot)
            @ Matrix.Rotation(math.radians(OFF_ANGLE), 4, 'Y')
            @ Matrix.Translation(-pivot))


def build_armature():
    arm_data = bpy.data.armatures.new("Armature")
    # UE는 오브젝트 이름이 "Armature"이면 추가 루트 본을 만들지 않는다
    arm_obj = bpy.data.objects.new("Armature", arm_data)
    bpy.context.collection.objects.link(arm_obj)

    bpy.ops.object.select_all(action='DESELECT')
    bpy.context.view_layer.objects.active = arm_obj
    bpy.ops.object.mode_set(mode='EDIT')
    root = arm_data.edit_bones.new("Root")
    root.head = (0, 0, 0)
    root.tail = (0, 0, PIVOT_Z)
    handle = arm_data.edit_bones.new("Handle")
    handle.head = (0, 0, PIVOT_Z)
    handle.tail = off_tilt_matrix() @ Vector((0, 0, PIVOT_Z + HANDLE_LEN))
    # 로컬 Z축을 월드 +Y에 맞춰 로컬 Z 회전 = 월드 Y축 회전(화면 좌우로 젖힘)이 되게 한다
    handle.align_roll(Vector((0, 1, 0)))
    handle.parent = root
    handle.use_connect = False
    bpy.ops.object.mode_set(mode='OBJECT')
    return arm_obj


def bind(mesh_obj, arm_obj):
    mesh_obj.parent = arm_obj
    mod = mesh_obj.modifiers.new("Armature", 'ARMATURE')
    mod.object = arm_obj


def handle_rotation(angle_deg):
    # 레퍼런스 포즈가 이미 Off 각도이므로 그 차이만큼만 회전한다
    return (0.0, 0.0, math.radians(angle_deg - OFF_ANGLE))


def make_action(arm_obj, name, keys):
    """keys: [(frame, angle_deg), ...]"""
    action = bpy.data.actions.new(name)
    action.use_fake_user = True
    arm_obj.animation_data_create()
    arm_obj.animation_data.action = action

    pb = arm_obj.pose.bones["Handle"]
    pb.rotation_mode = 'XYZ'
    root_pb = arm_obj.pose.bones["Root"]
    root_pb.rotation_mode = 'XYZ'
    for frame, angle in keys:
        pb.rotation_euler = handle_rotation(angle)
        # 기본 키 보간(Bezier, Auto Clamped)은 양 끝 기울기가 0이라 ease-in-out이 되고 역재생해도 대칭이다
        pb.keyframe_insert("rotation_euler", frame=frame)
        # Root도 키를 박아 두어야 모든 본 트랙이 익스포트된다
        root_pb.rotation_euler = (0, 0, 0)
        root_pb.keyframe_insert("rotation_euler", frame=frame)
    action.frame_range = (keys[0][0], keys[-1][0])
    return action


def main():
    reset_scene()
    mats = make_materials()
    b = Builder(mats)
    build_parts(b)
    mesh_obj = join_parts(b.parts)
    arm_obj = build_armature()
    bind(mesh_obj, arm_obj)

    end = DURATION_FRAMES
    make_action(arm_obj, "Lever_Toggle", [(0, OFF_ANGLE), (end, ON_ANGLE)])
    bpy.context.scene.frame_start = 0
    bpy.context.scene.frame_end = end

    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(OUT_DIR, "SK_Lever.blend"))

    bpy.ops.object.select_all(action='DESELECT')
    mesh_obj.select_set(True)
    arm_obj.select_set(True)
    bpy.ops.export_scene.fbx(
        filepath=os.path.join(OUT_DIR, "SK_Lever.fbx"),
        use_selection=True,
        object_types={'ARMATURE', 'MESH'},
        apply_unit_scale=True,
        apply_scale_options='FBX_SCALE_UNITS',
        add_leaf_bones=False,
        use_armature_deform_only=True,
        bake_anim=True,
        bake_anim_use_all_actions=True,
        bake_anim_use_nla_strips=False,
        bake_anim_force_startend_keying=True,
        bake_anim_simplify_factor=0.0,
        mesh_smooth_type='FACE',
    )
    print("Exported:", os.path.join(OUT_DIR, "SK_Lever.fbx"))


main()
