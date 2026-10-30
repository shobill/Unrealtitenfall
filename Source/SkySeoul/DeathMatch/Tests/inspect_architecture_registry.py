"""Read-only Asset Registry census. Run with Unreal PythonScript commandlet."""
import json
from pathlib import Path
import unreal

registry = unreal.AssetRegistryHelpers.get_asset_registry()
registry.search_all_assets(True)
options = unreal.AssetRegistryDependencyOptions(True, True, True, True, True)
referencers = sorted(str(p) for p in registry.get_referencers('/Script/SkySeoul', options))
assets = registry.get_all_assets(True)
dm = sorted({str(a.package_name) for a in assets if str(a.package_name).startswith('/Game/DeathMatch/')})
output = Path(unreal.Paths.project_saved_dir()) / 'DMArchitectureAudit'
output.mkdir(parents=True, exist_ok=True)
(output / 'packages.txt').write_text('\n'.join(sorted(set(referencers + dm))), encoding='utf-8')
result = {'asset_count': len(assets), 'native_package_referencers': referencers, 'deathmatch_packages': dm, 'controllers': {}}
for name in ['BP_DeathMatchAIController', 'BP_DeathMatchAIController_Adaptive']:
    path = '/Game/DeathMatch/AI/Controller/' + name
    bp = unreal.load_asset(path)
    data = unreal.EditorAssetLibrary.find_asset_data(path)
    result['controllers'][name] = {'parent': str(data.get_tag_value('ParentClass')), 'referencers': [str(p) for p in registry.get_referencers(path, options)]}
(output / 'registry.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
unreal.log('DM_ARCH_REGISTRY assets=%d script_referencers=%d dm_packages=%d' % (len(assets), len(referencers), len(dm)))
