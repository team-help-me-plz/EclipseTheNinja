import unreal
import json
from pathlib import Path

def cdo(path):
    bp = unreal.load_asset(path)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    return unreal.get_default_object(bp.generated_class())

player = cdo('/Game/Blueprint/CharacterTest/BP_CharacterTest_Character')
legacy = cdo('/Game/Blueprint/Week_1/CloneTech/BP_CharacterCloneTech')
assert player.get_editor_property('basic_attack_ability_class')
assert player.get_editor_property('basic_attack_montage')
assert player.get_editor_property('clone_tech_action') == legacy.get_editor_property('clone_tech_action')
infos = player.get_editor_property('ability_class_array')
assert len(infos) == 1 and infos[0].input_id == 100
assert infos[0].ability_class == legacy.get_editor_property('ability_class_array')[0].ability_class
ability = unreal.get_default_object(infos[0].ability_class)
assert ability.get_editor_property('instancing_policy') == unreal.GameplayAbilityInstancingPolicy.INSTANCED_PER_ACTOR
assert ability.get_editor_property('clone_class')
for prop in ['accept_input_action', 'cancel_input_action']:
    assert ability.get_editor_property(prop).get_editor_property('consume_input')
assert ability.get_editor_property('targeting_mapping_priority') > 0
for path in ['/Game/Blueprint/CharacterTest/BP_CharacterTestController', '/Game/Blueprint/Week_1/CloneTech/Framwork/PC_CloneTech', '/Game/Blueprint/CameraTest/BP_CameraMoveTestController']:
    controller = cdo(path)
    assert isinstance(controller, unreal.PlayerCharacterController)
    assert controller.get_editor_property('character_class') == player.get_class()
    assert controller.get_editor_property('default_mapping_contexts')
mode = cdo('/Game/Blueprint/Week_1/CloneTech/Framwork/GM_CloneTech')
assert mode.get_editor_property('default_pawn_class') == player.get_class()
mapping = unreal.load_asset('/Game/Input/IMC_TestCharacter')
keys = [str(m.key.export_text()) for m in mapping.get_editor_property('default_key_mappings').mappings]
Path(unreal.Paths.project_saved_dir(), 'CloneMerge/validation.json').write_text(json.dumps({'result':'PASS','keys':keys}), encoding='utf-8')
unreal.log('CLONE_MERGE_VALIDATION_PASS')
