"""One-shot, idempotent DeathMatch BT intent and duplicate Fire grant migration."""
import datetime
import pathlib
import shutil
import unreal


root = pathlib.Path(unreal.Paths.project_dir()).resolve()
stamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S") + "_FireIntentADS"
backup = root / "Saved" / "DMAssetBackups" / stamp


def backup_asset(game_path):
    relative = pathlib.Path("Content") / (game_path.removeprefix("/Game/") + ".uasset")
    destination = backup / relative
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(root / relative, destination)


combat_path = "/Game/DeathMatch/AI/BT/BT_Combat"
robot_path = "/Game/DeathMatch/AI/Character/BP_DeathMatchAIRobot"
weapon_path = "/Game/DeathMatch/AI/Weapon/BP_DM_AIWeapon"
expected_set_path = "/Game/DeathMatch/AI/Weapon/DA_DM_AS_AI.DA_DM_AS_AI"

combat = unreal.load_asset(combat_path)
robot_bp = unreal.load_asset(robot_path)
robot = unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class(robot_path))
weapon = unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class(weapon_path))
robot_sets = list(robot.get_editor_property("AbilitySets"))
weapon_set = weapon.get_editor_property("AbilitySetWhileActive")

assert combat and robot_bp and robot and weapon
assert len(robot_sets) <= 1
assert not robot_sets or robot_sets[0].get_path_name() == expected_set_path
assert weapon_set and weapon_set.get_path_name() == expected_set_path

backup_asset(combat_path)
backup_asset(robot_path)

services = {
    service.get_editor_property("ActionIntent"): service
    for service in unreal.ObjectIterator(
        unreal.load_class(None, "/Script/SkySeoul.BTService_DM_ActionIntent")
    )
    if service.get_path_name().startswith(combat_path + ".BT_Combat:")
}
attack = services[unreal.DMActionIntent.ATTACK]
reposition = services[unreal.DMActionIntent.FAST_REPOSITION]

for service, use_ads, allow_fire in (
    (attack, True, True),
    (reposition, False, False),
):
    service.set_editor_property("bOverrideExecutionSettings", True)
    service.set_editor_property("bUseBlackboardExecutionSettings", False)
    service.set_editor_property("bUseADSWhileMoving", use_ads)
    service.set_editor_property("bAllowFireWhileMoving", allow_fire)

assert unreal.EditorAssetLibrary.save_loaded_asset(combat, only_if_is_dirty=False)

# The active weapon owns this grant and its tracked handles remove it on weapon swap.
# Keeping the same set on CharacterBase grants an untracked second Fire spec.
robot.set_editor_property("AbilitySets", [])
unreal.BlueprintEditorLibrary.compile_blueprint(robot_bp)
assert unreal.EditorAssetLibrary.save_loaded_asset(robot_bp, only_if_is_dirty=False)

unreal.log(f"DM_FIRE_INTENT_CONFIG backup={backup}")
unreal.log("DM_FIRE_INTENT_CONFIG BT Attack fixed ADS/moving-fire=True")
unreal.log("DM_FIRE_INTENT_CONFIG BT FastReposition fixed ADS/moving-fire=False")
unreal.log("DM_FIRE_INTENT_CONFIG robot default DA_DM_AS_AI grant removed; weapon grant retained")
