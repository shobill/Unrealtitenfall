"""Configure Root BT state decorators for active conditional aborts."""
import unreal


TREE_PATH = "/Game/DeathMatch/AI/BT/BT_main"
STATE_DECORATORS = {
    "BTDecorator_DM_IsDead",
    "BTDecorator_DM_IsEmergency",
    "BTDecorator_DM_ShouldRetreat",
    "BTDecorator_DM_HasVisibleTarget",
    "BTDecorator_DM_HasRememberedTarget",
    "BTDecorator_DM_ShouldInvestigate",
    "BTDecorator_DM_ShouldDisengage",
}

tree = unreal.load_asset(TREE_PATH)
if not tree:
    raise RuntimeError(f"Missing {TREE_PATH}")

changed = 0
package_name = tree.get_outermost().get_name()
for node in unreal.ObjectIterator(unreal.BTNode):
    if node.get_outermost().get_name() != package_name:
        continue
    if node.get_class().get_name() not in STATE_DECORATORS:
        continue
    if node.get_editor_property("FlowAbortMode") != unreal.BTFlowAbortMode.BOTH:
        node.set_editor_property("FlowAbortMode", unreal.BTFlowAbortMode.BOTH)
        node.modify()
        changed += 1

if changed:
    unreal.EditorAssetLibrary.save_loaded_asset(tree, only_if_is_dirty=False)
unreal.log(f"DM_STATE_ABORT_CONFIG changed={changed} asset={TREE_PATH}")

