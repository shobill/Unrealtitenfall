"""Read-only audit for DeathMatch fire grants and combat BT intent service placement."""
import unreal


def value(obj, name, default=None):
    try:
        return obj.get_editor_property(name)
    except Exception:
        return default


def describe_ability_set(asset, label):
    if not asset:
        unreal.log(f"DM_FIRE_AUDIT {label}=None")
        return
    unreal.log(f"DM_FIRE_AUDIT {label}={asset.get_path_name()}")
    for collection in ("DefaultGrantedGameplayAbilities", "SelectGrantedGameplayAbilities"):
        for index, entry in enumerate(value(asset, collection, []) or []):
            ability = value(entry, "Ability")
            unreal.log(
                f"DM_FIRE_AUDIT {label}.{collection}[{index}]="
                f"{ability.get_path_name() if ability else 'None'}"
            )


def describe_service(service, indent):
    name = service.get_class().get_name()
    details = []
    if name == "BTService_DM_ActionIntent":
        for prop in (
            "ActionIntent",
            "bOverrideExecutionSettings",
            "bUseADSWhileMoving",
            "bAllowFireWhileMoving",
            "bUseBlackboardExecutionSettings",
            "ShouldADSKey",
            "FireWhileMovingKey",
        ):
            details.append(f"{prop}={value(service, prop)}")
    unreal.log(f"DM_BT_AUDIT {indent}SERVICE {name} {' '.join(details)}")


def describe_node(node, indent=""):
    if not node:
        return
    unreal.log(f"DM_BT_AUDIT {indent}NODE {node.get_class().get_name()} name={value(node, 'NodeName', '')}")
    for service in value(node, "Services", []) or []:
        describe_service(service, indent + "  ")
    for child in value(node, "Children", []) or []:
        child_node = value(child, "ChildComposite") or value(child, "ChildTask")
        describe_node(child_node, indent + "  ")


robot_class = unreal.EditorAssetLibrary.load_blueprint_class(
    "/Game/DeathMatch/AI/Character/BP_DeathMatchAIRobot"
)
robot = unreal.get_default_object(robot_class)
robot_ability_sets = list(value(robot, "AbilitySets", []) or [])
unreal.log(f"DM_FIRE_AUDIT robot.ability_set_count={len(robot_ability_sets)}")
for index, ability_set in enumerate(robot_ability_sets):
    describe_ability_set(ability_set, f"robot.ability_sets[{index}]")

for weapon_path in (
    "/Game/DeathMatch/AI/Weapon/BP_DM_AIWeapon",
    "/Game/DeathMatch/Weapon/BP_DM_Gun",
):
    weapon_class = unreal.EditorAssetLibrary.load_blueprint_class(weapon_path)
    weapon = unreal.get_default_object(weapon_class)
    describe_ability_set(value(weapon, "AbilitySetWhileActive"), weapon_path)

for tree_path in (
    "/Game/DeathMatch/AI/BT/BT_main",
    "/Game/DeathMatch/AI/BT/BT_Combat",
    "/Game/DeathMatch/AI/BT/BT_Roam",
):
    tree = unreal.load_asset(tree_path)
    unreal.log(f"DM_BT_AUDIT TREE {tree_path}")
    describe_node(value(tree, "RootNode"))

service_class = unreal.load_class(None, "/Script/SkySeoul.BTService_DM_ActionIntent")
for service in unreal.ObjectIterator(service_class):
    if service.get_path_name().startswith("/Game/DeathMatch/AI/BT/BT_Combat.BT_Combat:"):
        unreal.log(f"DM_BT_AUDIT SERVICE_PATH {service.get_path_name()}")
        describe_service(service, "  ")
