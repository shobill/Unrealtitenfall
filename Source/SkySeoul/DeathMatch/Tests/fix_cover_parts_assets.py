"""One-off, scoped migration for the inspected DeathMatch assets. No save-all."""
import unreal
import pathlib
import shutil
import datetime

root = pathlib.Path(unreal.Paths.project_dir()).resolve()
backup = root / 'Saved' / 'DMAssetBackups' / datetime.datetime.now().strftime('%Y%m%d_%H%M%S')
paths = ['AI/Controller/BP_DeathMatchAIController', 'AI/Character/BP_DeathMatchAIRobot', 'AI/Character/DA_Parts_DM_Robot']
for path in paths:
    source = root / 'Content' / 'DeathMatch' / (path + '.uasset')
    destination = backup / (path + '.uasset')
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, destination)
unreal.log('DM_FIX backup=' + str(backup))

controller = unreal.load_asset('/Game/DeathMatch/' + paths[0])
sub = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
found = False
for handle in sub.k2_gather_subobject_data_for_blueprint(controller):
    obj = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle))
    if obj and obj.get_class().get_name() == 'DMCombatTacticsComponent':
        found = True
        for name in ['movement_module', 'movement_module_priority2', 'movement_module_priority3']:
            unreal.log('DM_FIX before ' + name + '=' + str(obj.get_editor_property(name)))
        if obj.get_editor_property('movement_module_priority2') is None:
            cls = unreal.load_class(None, '/Script/SkySeoul.DMCombatMovement_KeepDistance')
            fallback = unreal.new_object(cls, outer=obj, name='CoverFallbackKeepDistance')
            obj.set_editor_property('movement_module_priority2', fallback)
assert found, 'Missing tactics template; do not save unexpected blueprint'
unreal.BlueprintEditorLibrary.compile_blueprint(controller)
assert unreal.EditorAssetLibrary.save_loaded_asset(controller, only_if_is_dirty=False)

robot = unreal.load_asset('/Game/DeathMatch/' + paths[1])
robot_class = unreal.EditorAssetLibrary.load_blueprint_class('/Game/DeathMatch/' + paths[1])
cdo = unreal.get_default_object(robot_class)
data = unreal.load_asset('/Game/DeathMatch/' + paths[2])
unreal.log('DM_FIX before Pawn.PartsSetupData=' + str(cdo.get_editor_property('parts_setup_data')))
cdo.set_editor_property('parts_setup_data', data)
for comp in cdo.get_components_by_class(unreal.PartsComponent):
    comp.set_editor_property('setup_data', data)
# Preserve the head's functional impairment that was active with the fallback definition.
# HP, meshes, separation flags, arm settings and damage values remain as authored in the DA.
rows = list(data.get_editor_property('parts'))
head_rows = [row for row in rows if str(unreal.GameplayTagLibrary.get_tag_name(row.get_editor_property('part_tag'))) == 'Parts.Head']
assert len(head_rows) == 1, 'Unexpected duplicate/missing head row'
assert head_rows[0].get_editor_property('separable') is False
head_rows[0].set_editor_property('impair_head_on_destroy', True)
data.set_editor_property('parts', rows)
assert unreal.EditorAssetLibrary.save_loaded_asset(data, only_if_is_dirty=False)
unreal.BlueprintEditorLibrary.compile_blueprint(robot)
assert unreal.EditorAssetLibrary.save_loaded_asset(robot, only_if_is_dirty=False)
unreal.log('DM_FIX complete; verify in fresh process with inspect_cover_parts.py and automation')
