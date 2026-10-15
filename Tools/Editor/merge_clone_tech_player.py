"""Run once with UE Python after building the PlayerCharacter clone integration.

Preserves the legacy test assets, backs up changed packages under Saved/CloneMerge,
and connects the existing player Blueprint to the existing clone ability assets.
"""
import json
import shutil
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir())
BACKUP = ROOT / 'Saved/CloneMerge/AssetBackup'
PLAYER = '/Game/Blueprint/CharacterTest/BP_CharacterTest_Character'
LEGACY = '/Game/Blueprint/Week_1/CloneTech/BP_CharacterCloneTech'
CAMERA_CONTROLLER = '/Game/Blueprint/CameraTest/BP_CameraMoveTestController'
CONTROLLERS = [
    '/Game/Blueprint/CharacterTest/BP_CharacterTestController',
    '/Game/Blueprint/Week_1/CloneTech/Framwork/PC_CloneTech',
]
MODE = '/Game/Blueprint/Week_1/CloneTech/Framwork/GM_CloneTech'
MAPPING = '/Game/Input/IMC_TestCharacter'


def backup(path):
    relative = Path('Content') / (path.removeprefix('/Game/') + '.uasset')
    destination = BACKUP / relative
    destination.parent.mkdir(parents=True, exist_ok=True)
    if not destination.exists():
        shutil.copy2(ROOT / relative, destination)


def defaults(path):
    return unreal.get_default_object(unreal.load_asset(path).generated_class())


def save_blueprint(path):
    bp = unreal.load_asset(path)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    assert unreal.EditorAssetLibrary.save_loaded_asset(bp, only_if_is_dirty=False), path


for path in [PLAYER, MAPPING, MODE] + CONTROLLERS:
    backup(path)

player = defaults(PLAYER)
legacy = defaults(LEGACY)
abilities = list(player.get_editor_property('ability_class_array'))
for info in legacy.get_editor_property('ability_class_array'):
    if not any(existing.ability_class == info.ability_class for existing in abilities):
        abilities.append(info)
player.set_editor_property('ability_class_array', abilities)
for prop in ['clone_tech_action', 'interact_box_size']:
    player.set_editor_property(prop, legacy.get_editor_property(prop))
save_blueprint(PLAYER)

# Keep all original mappings. Do not add IMC_CloneTech: its movement actions
# differ and its gamepad clone key conflicts with the player's platform drop.
mapping = unreal.load_asset(MAPPING)
clone_action = player.get_editor_property('clone_tech_action')
interact_action = player.get_editor_property('interact_action')
for action, key_name in [(clone_action, 'R'), (clone_action, 'Gamepad_RightShoulder'), (interact_action, 'E')]:
    mappings = mapping.get_editor_property('default_key_mappings').get_editor_property('mappings')
    key = unreal.Key(key_name)
    if not any(m.action == action and m.key == key for m in mappings):
        mapping.map_key(action, key)
assert unreal.EditorAssetLibrary.save_loaded_asset(mapping, only_if_is_dirty=False)

reference = defaults(CAMERA_CONTROLLER)
for path in CONTROLLERS:
    bp = unreal.load_asset(path)
    unreal.BlueprintEditorLibrary.reparent_blueprint(bp, unreal.PlayerCharacterController.static_class())
    cdo = unreal.get_default_object(bp.generated_class())
    cdo.set_editor_property('default_mapping_contexts', [mapping])
    cdo.set_editor_property('character_class', unreal.load_asset(PLAYER).generated_class())
    cdo.set_editor_property('player_camera_manager_class', reference.get_editor_property('player_camera_manager_class'))
    save_blueprint(path)

mode = defaults(MODE)
mode.set_editor_property('default_pawn_class', unreal.load_asset(PLAYER).generated_class())
save_blueprint(MODE)

ability = defaults('/Game/Blueprint/Week_1/CloneTech/Spawn/GA/GA_CloneTech')
backup('/Game/Blueprint/Week_1/CloneTech/Spawn/GA/GA_CloneTech')
ability.set_editor_property('instancing_policy', unreal.GameplayAbilityInstancingPolicy.INSTANCED_PER_ACTOR)
save_blueprint('/Game/Blueprint/Week_1/CloneTech/Spawn/GA/GA_CloneTech')
targeting = ability.get_editor_property('targeting_mapping_context')
target_mappings = targeting.get_editor_property('default_key_mappings').get_editor_property('mappings')
for prop in ['accept_input_action', 'cancel_input_action']:
    action = ability.get_editor_property(prop)
    assert action.get_editor_property('consume_input'), prop
    assert any(m.action == action for m in target_mappings), prop
assert ability.get_editor_property('targeting_mapping_priority') > 0

report = {
    'player': PLAYER,
    'startup_abilities': str(defaults(PLAYER).get_editor_property('ability_class_array')),
    'controllers': CONTROLLERS,
    'clone_game_mode_pawn': str(defaults(MODE).get_editor_property('default_pawn_class')),
    'targeting_mappings': str(target_mappings),
    'result': 'PASS',
}
(ROOT / 'Saved/CloneMerge/migration.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
unreal.log('CLONE_MERGE_ASSETS_PASS')
