"""Read-only inspection of the exact tactics template and robot capsule."""
import unreal

path = '/Game/DeathMatch/AI/Controller/BP_DeathMatchAIController'
bp = unreal.load_asset(path)
sub = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
for handle in sub.k2_gather_subobject_data_for_blueprint(bp):
    obj = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle))
    if obj and obj.get_class().get_name() == 'DMCombatTacticsComponent':
        unreal.log('ATTACK_BP template=' + obj.get_path_name())
        for name in ['movement_module', 'movement_module_priority2', 'movement_module_priority3']:
            module = obj.get_editor_property(name)
            unreal.log('ATTACK_BP ' + name + '=' + (module.get_path_name() + ' class=' + module.get_class().get_name() if module else 'None'))
robot = unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class('/Game/DeathMatch/AI/Character/BP_DeathMatchAIRobot'))
capsule = robot.get_component_by_class(unreal.CapsuleComponent)
unreal.log('ATTACK_BP capsule=' + str(capsule.get_editor_property('capsule_radius')) + ',' + str(capsule.get_editor_property('capsule_half_height')))
