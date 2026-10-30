"""Scoped, backed-up migration of the inspected DeathMatch assets."""
import datetime
import pathlib
import shutil
import unreal

controller_path = '/Game/DeathMatch/AI/Controller/BP_DeathMatchAIController'
data_path = '/Game/DeathMatch/AI/Character/DA_Parts_DM_Robot'
fire_path = '/Game/DeathMatch/AI/Abilities/BP_DM_AI_Fire'
fire = unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class(fire_path))
montage = fire.get_editor_property('fire_montage')
assert montage and montage.get_name() == 'AM_02_Fire2'
root = pathlib.Path(unreal.Paths.project_dir()).resolve()
backup = root / 'Saved/DMAssetBackups' / (datetime.datetime.now().strftime('%Y%m%d_%H%M%S') + '_MovingFire')
for path in [controller_path, data_path, montage.get_path_name().split('.')[0]]:
    rel = pathlib.Path('Content') / (path.removeprefix('/Game/') + '.uasset')
    dest = backup / rel
    dest.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(root / rel, dest)
unreal.log('MOVING_CONFIG backup=' + str(backup))

data = unreal.load_asset(data_path)
rows = list(data.get_editor_property('parts'))
for row in rows:
    if not row.get_editor_property('separable'):
        row.set_editor_property('destroyed_visual_policy', unreal.PartsDestroyedVisualPolicy.KEEP_ATTACHED_DISABLED)
data.set_editor_property('parts', rows)
assert unreal.EditorAssetLibrary.save_loaded_asset(data, only_if_is_dirty=False)

robot = unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class('/Game/DeathMatch/AI/Character/BP_DeathMatchAIRobot'))
speed = robot.get_component_by_class(unreal.CharacterMovementComponent).get_editor_property('max_walk_speed')
bp = unreal.load_asset(controller_path)
sub = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
count = 0
for handle in sub.k2_gather_subobject_data_for_blueprint(bp):
    obj = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle))
    if obj and obj.get_class().get_name() == 'DMCombatTacticsComponent':
        module = obj.get_editor_property('fire_module')
        assert module.get_class().get_name() == 'DMCombatFire_Precision'
        module.set_editor_property('max_move_speed', max(600.0, speed))
        count += 1
assert count == 1
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
assert unreal.EditorAssetLibrary.save_loaded_asset(bp, only_if_is_dirty=False)
converted = unreal.AnimNotify_DMNamed.upgrade_montage_fire_notifies(montage)
events = unreal.AnimationLibrary.get_animation_notify_events(montage)
assert any(e.get_editor_property('notify') and e.get_editor_property('notify').get_class().get_name() == 'AnimNotify_DMNamed' for e in events)
assert unreal.EditorAssetLibrary.save_loaded_asset(montage, only_if_is_dirty=False)
unreal.log('MOVING_CONFIG walk_speed=' + str(speed) + ' max_fire_speed=' + str(max(600.0, speed)) + ' converted=' + str(converted))
