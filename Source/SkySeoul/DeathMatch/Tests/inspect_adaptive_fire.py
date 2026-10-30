import unreal
bp = unreal.load_asset('/Game/DeathMatch/AI/Controller/BP_DeathMatchAIController')
sub = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
for h in sub.k2_gather_subobject_data_for_blueprint(bp):
    obj = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(unreal.SubobjectDataBlueprintFunctionLibrary.get_data(h))
    if obj and obj.get_class().get_name() == 'DMCombatTacticsComponent':
        unreal.log('ADAPTIVE_INSPECT controller=' + obj.get_path_name() + ' mode=' + str(obj.get_editor_property('fire_mode')) + ' module=' + str(obj.get_editor_property('fire_module')))
fire = unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class('/Game/DeathMatch/AI/Abilities/BP_DM_AI_Fire'))
for name in ['fire_montage', 'montage_start_section', 'montage_play_rate', 'fire_cooldown', 'cooldown_gameplay_effect_class', 'fire_notify_name', 'fire_cycle_notify_name']:
    unreal.log('ADAPTIVE_INSPECT ' + name + '=' + str(fire.get_editor_property(name)))
montage = fire.get_editor_property('fire_montage')
unreal.log('ADAPTIVE_INSPECT montage_length=' + str(montage.get_play_length()))
for event in unreal.AnimationLibrary.get_animation_notify_events(montage):
    unreal.log('ADAPTIVE_INSPECT event=' + str(event))
unreal.log('ADAPTIVE_INSPECT ' + unreal.AnimNotify_DMNamed.inspect_fire_montage(montage))
