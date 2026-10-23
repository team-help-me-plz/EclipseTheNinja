"""
걷기 시퀀스마다 첫 발 접지 시점을 찾아 Noise(Footstep) 노티파이를 하나만 넣는다.

에디터 Output Log(Cmd 대신 Python 선택)에서 실행:
    py "C:/GeunSu/UnrealProject/EclipseTheNinja/Content/Python/add_first_footstep_noise.py"

- 결과는 저장하지 않는다. 타임라인을 확인한 뒤 직접 저장하거나 Ctrl+Z로 되돌린다.
- 다시 실행하면 "Noise" 트랙을 지우고 새로 넣는다. 다른 트랙의 노티파이는 건드리지 않는다.
"""

import unreal

SEQUENCE_PATHS = [
    "/Game/Anims/Anim_Seq/Walk/Ninja_Walk_Forward",
    "/Game/Anims/Anim_Seq/Walk/Ninja_Walk_Back",
    "/Game/Anims/Anim_Seq/Walk/Ninja_Walk_Left",
    "/Game/Anims/Anim_Seq/Walk/Ninja_Walk_Right",
    "/Game/Anims/Anim_Seq/Walk/Ninja_Walk_ForwardLeft",
    "/Game/Anims/Anim_Seq/Walk/Ninja_Walk_ForwardRight",
    "/Game/Anims/Anim_Seq/Walk/Ninja_Walk_BackLeft",
    "/Game/Anims/Anim_Seq/Walk/Ninja_Walk_BackRight",
]
BLEND_SPACE_PATH = "/Game/Anims/BS_Ninja_Idle_Walk_Run"

FOOT_BONES = ["foot_l", "foot_r"]
NOTIFY_TRACK = "Noise"
SAMPLE_RATE = 120.0  # Hz

# 발 높이 범위(최저~최고) 중 아래쪽 이 비율 안에 들어오면 땅에 닿은 것으로 본다.
GROUND_RATIO = 0.2
# 발을 거의 들지 않는 시퀀스에서 범위가 너무 좁아지지 않게 하는 최소 높이(cm).
MIN_GROUND_THRESHOLD = 1.5

# True면 시간만 출력하고 에셋은 바꾸지 않는다.
DRY_RUN = False
# 블렌드 스페이스가 섞이는 모든 시퀀스의 노티파이를 발동하지 않도록 가장 가중치가 큰 것만 발동하게 바꾼다.
SET_BLEND_SPACE_TRIGGER_MODE = True


def make_eval_options():
    options = unreal.AnimPoseEvaluationOptions()
    options.set_editor_property("evaluation_type", unreal.AnimDataEvalType.RAW)
    # 루트 모션이 섞이면 발 높이가 아니라 이동량이 바뀌므로 제자리 포즈로 평가한다.
    options.set_editor_property("incorporate_root_motion_into_pose", False)
    return options


def sample_foot_heights(sequence, length, options):
    """컴포넌트 공간 기준 발 높이(Z)를 일정 간격으로 샘플링한다."""
    count = max(2, int(length * SAMPLE_RATE) + 1)
    times = [min(length, i / SAMPLE_RATE) for i in range(count)]
    heights = {bone: [] for bone in FOOT_BONES}
    for t in times:
        pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(sequence, t, options)
        for bone in FOOT_BONES:
            transform = unreal.AnimPoseExtensions.get_bone_pose(pose, bone, unreal.AnimPoseSpaces.WORLD)
            heights[bone].append(transform.translation.z)
    return times, heights


def find_first_contact(times, heights):
    """발이 공중에서 땅으로 내려오는 첫 순간을 찾는다. 루프 이음새(마지막→처음)도 접지로 본다."""
    ground = min(heights)
    top = max(heights)
    threshold = ground + max(MIN_GROUND_THRESHOLD, (top - ground) * GROUND_RATIO)
    grounded = [h <= threshold for h in heights]

    if grounded[0] and not grounded[-1]:
        return times[0]
    for i in range(1, len(grounded)):
        if grounded[i] and not grounded[i - 1]:
            return times[i]
    return None


def set_blend_space_trigger_mode():
    blend_space = unreal.load_asset(BLEND_SPACE_PATH)
    if not blend_space:
        unreal.log_warning(f"[Footstep] 블렌드 스페이스를 찾지 못함: {BLEND_SPACE_PATH}")
        return
    try:
        blend_space.set_editor_property("notify_trigger_mode", unreal.NotifyTriggerMode.HIGHEST_WEIGHTED_ANIMATION)
        unreal.log(f"[Footstep] {BLEND_SPACE_PATH}: Notify Trigger Mode -> Highest Weighted Animation")
    except Exception as error:
        unreal.log_warning(f"[Footstep] Notify Trigger Mode를 바꾸지 못함 ({error}). 블렌드 스페이스에서 직접 바꿔 주세요.")


def main():
    notify_class = unreal.AnimNotify_Noise.static_class()
    options = make_eval_options()

    with unreal.ScopedEditorTransaction("Add First Footstep Noise Notifies"):
        for path in SEQUENCE_PATHS:
            sequence = unreal.load_asset(path)
            if not isinstance(sequence, unreal.AnimSequence):
                unreal.log_warning(f"[Footstep] 시퀀스를 찾지 못함: {path}")
                continue

            length = unreal.AnimationLibrary.get_sequence_length(sequence)
            times, heights = sample_foot_heights(sequence, length, options)

            contacts = {bone: find_first_contact(times, heights[bone]) for bone in FOOT_BONES}
            found = {bone: t for bone, t in contacts.items() if t is not None}
            if not found:
                unreal.log_warning(f"[Footstep] {path}: 접지 시점을 찾지 못함. 직접 배치해 주세요.")
                continue

            bone, contact_time = min(found.items(), key=lambda item: item[1])
            detail = ", ".join(f"{b}={t:.3f}s" if t is not None else f"{b}=없음" for b, t in contacts.items())
            unreal.log(f"[Footstep] {path}: {contact_time:.3f}s ({bone}) / 길이 {length:.3f}s / {detail}")

            if DRY_RUN:
                continue

            if unreal.AnimationLibrary.is_valid_anim_notify_track_name(sequence, NOTIFY_TRACK):
                unreal.AnimationLibrary.remove_animation_notify_events_by_track(sequence, NOTIFY_TRACK)
                unreal.AnimationLibrary.remove_animation_notify_track(sequence, NOTIFY_TRACK)
            unreal.AnimationLibrary.add_animation_notify_track(sequence, NOTIFY_TRACK)

            notify = unreal.AnimationLibrary.add_animation_notify_event(sequence, NOTIFY_TRACK, contact_time, notify_class)
            if notify:
                notify.set_editor_property("noise_type", unreal.NoiseType.FOOTSTEP)

        if SET_BLEND_SPACE_TRIGGER_MODE and not DRY_RUN:
            set_blend_space_trigger_mode()

    if not DRY_RUN:
        unreal.log("[Footstep] 완료. 타임라인을 확인한 뒤 저장하세요.")


main()
