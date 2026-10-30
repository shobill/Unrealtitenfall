"""Opt-in Adaptive controller; preserve the existing controller and source montage."""
import datetime
import pathlib
import shutil
import unreal

controller_path = '/Game/DeathMatch/AI/Controller/BP_DeathMatchAIController'
variant_path = '/Game/DeathMatch/AI/Controller/BP_DeathMatchAIController_Adaptive'
fire_path = '/Game/DeathMatch/AI/Abilities/BP_DM_AI_Fire'
montage_path = '/Game/DeathMatch/AI/Abilities/AM_DM_AI_AdaptiveFire'
root = pathlib.Path(unreal.Paths.project_dir()).resolve()
backup = root / 'Saved/DMAssetBackups' / (datetime.datetime.now().strftime('%Y%m%d_%H%M%S') + '_AdaptiveFire')
for path in [fire_path, variant_path, montage_path]:
    rel = pathlib.Path('Content') / (path.removeprefix('/Game/') + '.uasset')
    if (root / rel).exists():
        dest = backup / rel
        dest.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(root / rel, dest)
fire_bp = unreal.load_asset(fire_path)
fire = unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class(fire_path))
source = unreal.load_asset('/Game/SuperBotPackVol2/Etasphera02/Demo/Montage/AM_02_Fire2')
montage = unreal.load_asset(montage_path) if unreal.EditorAssetLibrary.does_asset_exist(montage_path) else unreal.EditorAssetLibrary.duplicate_asset(source.get_path_name().split('.')[0], montage_path)
assert montage
assert unreal.AnimNotify_DMNamed.add_fire_cycle_notify(montage, 0.12)
assert unreal.EditorAssetLibrary.save_loaded_asset(montage, only_if_is_dirty=False)
fire.set_editor_property('fire_montage', montage)
fire.set_editor_property('fire_cycle_notify_name', 'FireCycle')
unreal.BlueprintEditorLibrary.compile_blueprint(fire_bp)
assert unreal.EditorAssetLibrary.save_loaded_asset(fire_bp, only_if_is_dirty=False)
variant = unreal.load_asset(variant_path) if unreal.EditorAssetLibrary.does_asset_exist(variant_path) else unreal.EditorAssetLibrary.duplicate_asset(controller_path, variant_path)
sub = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
count = 0
for h in sub.k2_gather_subobject_data_for_blueprint(variant):
    obj = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(unreal.SubobjectDataBlueprintFunctionLibrary.get_data(h))
    if obj and obj.get_class().get_name() == 'DMCombatTacticsComponent':
        obj.set_editor_property('fire_mode', unreal.DMFireMode.ADAPTIVE)
        assert obj.get_editor_property('adaptive_settings')
        count += 1
assert count == 1
unreal.BlueprintEditorLibrary.compile_blueprint(variant)
assert unreal.EditorAssetLibrary.save_loaded_asset(variant, only_if_is_dirty=False)
unreal.log('ADAPTIVE_CONFIG backup=' + str(backup))
unreal.log('ADAPTIVE_CONFIG variant=' + variant_path + ' mode=Adaptive; original controller unchanged')
unreal.log('ADAPTIVE_CONFIG ' + unreal.AnimNotify_DMNamed.inspect_fire_montage(montage))
assert fire.get_editor_property('fire_montage') == montage
