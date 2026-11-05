import json
import math

# ---------- 공용 기반 : 모든 MCP 스크립트 앞에 붙는다 (runlib.ps1) ----------
# 스크립트 본문은 def run(): 을 정의하고 dict 를 반환해야 한다

# 이번 스크립트에서 만든 액터를 라벨로 찾아 쓰기 위한 표
A = {}


# 툴 호출 : 전체 이름(toolset.Tool.func)과 dict 인자를 받아 returnValue 만 돌려준다
def T(name, args):
    return execute_tool(name, json.dumps(args))["returnValue"]


def xf(x, y, z, yaw=0.0, pitch=0.0, roll=0.0, sx=1.0, sy=1.0, sz=1.0):
    return {"location": {"x": x, "y": y, "z": z},
            "rotation": {"pitch": pitch, "yaw": yaw, "roll": roll},
            "scale": {"x": sx, "y": sy, "z": sz}}


def setp(obj, values):
    T("editor_toolset.toolsets.object.ObjectTools.set_properties", {"instance": obj, "values": json.dumps(values)})


def comps(actor, cls):
    return T("editor_toolset.toolsets.actor.ActorTools.get_components", {"actor": actor, "component_type": {"refPath": cls}})


def place(actor, label, folder):
    T("editor_toolset.toolsets.actor.ActorTools.set_label", {"actor": actor, "label": label})
    T("editor_toolset.toolsets.scene.SceneTools.set_actor_folder", {"actor": actor, "folder_path": folder})
    A[label] = actor
    return actor


# 스폰 시 겹침 보정으로 위치가 밀릴 수 있어 트랜스폼을 다시 지정한다
def force_xf(a, t):
    T("editor_toolset.toolsets.actor.ActorTools.set_actor_transform", {"actor": a, "xform": t, "worldspace": True})


def spawn_asset(path, label, x, y, z, folder, yaw=0.0, pitch=0.0, roll=0.0, sx=1.0, sy=1.0, sz=1.0):
    t = xf(x, y, z, yaw, pitch, roll, sx, sy, sz)
    a = T("editor_toolset.toolsets.scene.SceneTools.add_to_scene_from_asset", {"asset_path": path, "name": label, "xform": t})
    force_xf(a, t)
    return place(a, label, folder)


def spawn_class(cls, label, x, y, z, folder, yaw=0.0, pitch=0.0, sx=1.0, sy=1.0, sz=1.0):
    t = xf(x, y, z, yaw, pitch, 0.0, sx, sy, sz)
    a = T("editor_toolset.toolsets.scene.SceneTools.add_to_scene_from_class", {"actor_type": {"refPath": cls}, "name": label, "xform": t})
    force_xf(a, t)
    return place(a, label, folder)


def ref(label):
    return {"refPath": A[label]["refPath"]}


# find_actors 는 name/tag/collision_channels 인자가 모두 있어야 한다
def find_actors(name="", tag="", cls=None):
    args = {"name": name, "tag": tag, "collision_channels": []}
    if cls:
        args["actor_type"] = {"refPath": cls}
    return T("editor_toolset.toolsets.scene.SceneTools.find_actors", args)


def env_actor(cls):
    f = find_actors(cls=cls)
    return f[0] if f else None


def asset_exists(path):
    return T("editor_toolset.toolsets.asset.AssetTools.exists", {"path": path})
