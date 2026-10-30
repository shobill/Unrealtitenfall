"""Read-only asset inspection; run with Unreal PythonScript commandlet."""
import unreal

def show(obj, names):
    unreal.log('DM_INSPECT object=' + obj.get_path_name())
    for name in names:
        try:
            unreal.log('DM_INSPECT ' + name + '=' + str(obj.get_editor_property(name)))
        except Exception as e:
            unreal.log('DM_INSPECT unavailable ' + name + ': ' + str(e))

for path in ['/Game/DeathMatch/AI/Controller/BP_DeathMatchAIController', '/Game/DeathMatch/AI/Character/BP_DeathMatchAIRobot']:
    bp = unreal.load_asset(path)
    cls = unreal.EditorAssetLibrary.load_blueprint_class(path)
    cdo = unreal.get_default_object(cls)
    show(cdo, ['parts_setup_data', 'parts_component'])
    for comp in cdo.get_components_by_class(unreal.ActorComponent):
        if 'Tactics' in comp.get_class().get_name():
            show(comp, ['movement_module', 'movement_module_priority2', 'movement_module_priority3'])
        if 'PartsComponent' in comp.get_class().get_name():
            show(comp, ['setup_data', 'use_default_death_match_parts_if_no_setup_data'])
    sub = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    for handle in sub.k2_gather_subobject_data_for_blueprint(bp):
        obj = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle))
        if obj:
            show(obj, ['setup_data'] if 'PartsComponent' in obj.get_class().get_name() else ['movement_module', 'movement_module_priority2', 'movement_module_priority3'] if 'Tactics' in obj.get_class().get_name() else [])
    task = unreal.AssetExportTask()
    task.object = bp
    task.filename = unreal.Paths.project_saved_dir() + '/DM_' + bp.get_name() + '.copy'
    task.automated = True
    task.prompt = False
    task.replace_identical = True
    unreal.Exporter.run_asset_export_task(task)
data = unreal.load_asset('/Game/DeathMatch/AI/Character/DA_Parts_DM_Robot')
for row in data.get_editor_property('parts'):
    unreal.log('DM_INSPECT ROW tag=' + unreal.GameplayTagLibrary.get_tag_name(row.get_editor_property('part_tag')).__str__() + ' ' + str(row))
