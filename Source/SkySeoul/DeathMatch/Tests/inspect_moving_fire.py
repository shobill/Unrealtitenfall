import unreal

bp = unreal.load_asset('/Game/DeathMatch/AI/Controller/BP_DeathMatchAIController')
sub = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
for handle in sub.k2_gather_subobject_data_for_blueprint(bp):
    obj = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle))
    if obj and obj.get_class().get_name() == 'DMCombatTacticsComponent':
        for name in ['movement_module', 'movement_module_priority2', 'movement_module_priority3', 'fire_module']:
            module = obj.get_editor_property(name)
            unreal.log('MOVING_FIRE ' + name + '=' + str(module))
            if module:
                for prop in ['max_move_speed', 'allow_fire_while_moving']:
                    try: unreal.log('MOVING_FIRE ' + prop + '=' + str(module.get_editor_property(prop)))
                    except Exception: pass
for path in unreal.EditorAssetLibrary.list_assets('/Game/DeathMatch', recursive=True):
    if 'BP_DM_AI_Fire.' in path:
        cdo = unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class(path))
        for prop in ['fire_montage', 'fire_notify_name', 'fire_on_anim_notify', 'fallback_fire_if_notify_missed', 'cooldown_gameplay_effect_class']:
            try: unreal.log('MOVING_FIRE ' + prop + '=' + str(cdo.get_editor_property(prop)))
            except Exception: pass
        montage = cdo.get_editor_property('fire_montage')
        if montage:
            for event in unreal.AnimationLibrary.get_animation_notify_events(montage):
                unreal.log('MOVING_FIRE notify=' + str(event))
data = unreal.load_asset('/Game/DeathMatch/AI/Character/DA_Parts_DM_Robot')
for row in data.get_editor_property('parts'):
    unreal.log('MOVING_FIRE PART=' + str(row))
