import unreal


ASSETS = [
    "/Game/DeathMatch/AI/BT/BT_main",
    "/Game/DeathMatch/AI/BT/BT_Dead",
    "/Game/DeathMatch/AI/BT/BT_Hide",
    "/Game/DeathMatch/AI/BT/BT_Combat",
    "/Game/DeathMatch/AI/BT/BT_LostTarget",
    "/Game/DeathMatch/AI/BT/BT_Roam",
]


def value(obj, name, default=None):
    try:
        return obj.get_editor_property(name)
    except Exception:
        return default


def describe_node(node, indent=0):
    if not node:
        return
    prefix = "  " * indent
    decorators = value(node, "Decorators", []) or []
    services = value(node, "Services", []) or []
    unreal.log(f"{prefix}NODE {node.get_name()} class={node.get_class().get_name()}")
    for decorator in decorators:
        unreal.log(
            f"{prefix}  DEC {decorator.get_name()} class={decorator.get_class().get_name()} "
            f"abort={value(decorator, 'FlowAbortMode')} inverse={value(decorator, 'bInverseCondition')}"
        )
    for service in services:
        unreal.log(
            f"{prefix}  SVC {service.get_name()} class={service.get_class().get_name()} "
            f"intent={value(service, 'ActionIntent')}"
        )
    for child in value(node, "Children", []) or []:
        composite = value(child, "ChildComposite")
        task = value(child, "ChildTask")
        child_decorators = value(child, "Decorators", []) or []
        for decorator in child_decorators:
            unreal.log(
                f"{prefix}  EDGE_DEC {decorator.get_name()} class={decorator.get_class().get_name()} "
                f"abort={value(decorator, 'FlowAbortMode')} inverse={value(decorator, 'bInverseCondition')}"
            )
        if task:
            unreal.log(f"{prefix}  TASK {task.get_name()} class={task.get_class().get_name()}")
        describe_node(composite, indent + 1)


for asset_path in ASSETS:
    tree = unreal.load_asset(asset_path)
    unreal.log(f"=== {asset_path} ===")
    if not tree:
        unreal.log_error(f"Missing {asset_path}")
        continue
    root = value(tree, "RootNode")
    describe_node(root)
    package_name = tree.get_outermost().get_name()
    for node in unreal.ObjectIterator(unreal.BTNode):
        if node.get_outermost().get_name() != package_name:
            continue
        unreal.log(
            f"OBJECT {node.get_path_name()} class={node.get_class().get_name()} "
            f"abort={value(node, 'FlowAbortMode')} inverse={value(node, 'bInverseCondition')} "
            f"intent={value(node, 'ActionIntent')}"
        )

