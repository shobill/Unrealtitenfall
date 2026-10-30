"""Source audit only; does not compile C++, run UHT, load BP or execute Unreal tests."""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[2]
files = [
    'PartsCharacter/PartsHitCollision.h', 'PartsCharacter/PartsComponent.h',
    'PartsCharacter/PartsComponent.cpp', 'PartsCharacter/PartsDamageManager.h',
    'PartsCharacter/PartsDamageManager.cpp', 'DeathMatch/Abilities/DMGameplayAbility_Fire.h',
    'DeathMatch/Abilities/DMGameplayAbility_Fire.cpp', 'DeathMatch/Tests/DMHitRoutingTestTypes.h',
    'DeathMatch/Tests/DMHitRoutingTests.cpp', 'DeathMatch/Tests/DMPartsSeparationTests.cpp',
]
texts = {f: (root / f).read_text(encoding='utf-8-sig') for f in files}
checks = 0
def check(ok, message):
    global checks
    assert ok, message
    checks += 1

for name, text in texts.items():
    check(not re.search(r'^(<<<<<<<|=======|>>>>>>>)', text, re.M), name + ' conflict marker')
    code = re.sub(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'', '', text, flags=re.S)
    stack = []
    for char in code:
        if char in '({[': stack.append(char)
        elif char in ')}]':
            assert bool(stack) and stack.pop() == dict(zip(')}]', '({['))[char], name + ' delimiter mismatch'
    check(not stack, name + ' unclosed delimiter')
    includes = re.findall(r'^#include[^\n]*', text, re.M)
    if '.generated.h' in text:
        check('.generated.h' in includes[-1], name + ' generated include must be last')

p = texts['PartsCharacter/PartsComponent.cpp']
d = texts['PartsCharacter/PartsDamageManager.cpp']
f = texts['DeathMatch/Abilities/DMGameplayAbility_Fire.cpp']
for option, source, header in [
    ('bAttachedPartsBlockTraces', p, texts['PartsCharacter/PartsComponent.h']),
    ('bConfigureOwnerMeshForWeaponTrace', p, texts['PartsCharacter/PartsComponent.h']),
    ('bIgnoreTargetPartMeshes', f, texts['DeathMatch/Abilities/DMGameplayAbility_Fire.h']),
]:
    check(option not in source, option + ' must not control production behavior')
    declaration = re.search(r'UPROPERTY\(([^\n]*)\)\s*bool ' + option + r'\s*=', header)
    check(declaration is not None, option + ' serialized bool retained')
    for metadata in ['AdvancedDisplay', 'DeprecatedProperty', 'DeprecationMessage', 'ToolTip']:
        check(metadata in declaration[1], option + ' missing ' + metadata)
check('ECC_GameTraceChannel1' in texts['PartsCharacter/PartsHitCollision.h'], 'HitTrace default')
for name in ['PartsCharacter/PartsComponent.h', 'DeathMatch/Abilities/DMGameplayAbility_Fire.h']:
    check('= PartsHitCollision::DefaultWeaponTraceChannel' in texts[name], name + ' shared default')
check('!OwnerMesh->IsQueryCollisionEnabled()' in p, 'PhysicsOnly must be repaired too')
check('Capsule->SetCollisionResponseToChannel(WeaponTraceChannel, ECR_Ignore)' in p, 'Capsule policy')
check('OwnerMesh->SetCollisionResponseToChannel(WeaponTraceChannel, ECR_Block)' in p, 'Mesh policy')
attach = p.split('void UPartsComponent::CreateAndAttachMeshes()')[1].split('void UPartsComponent::')[0]
check('QueryOnly' not in attach and attach.count('SetCollisionEnabled(ECollisionEnabled::NoCollision)') == 2, 'Attached policy before/after registration')
check('CollectAttachedPartComponents' not in f and 'AddIgnoredComponent' not in f, 'No redundant per-shot component ignores')
check(f.count('LineTraceSingleByChannel(') == 3, 'Existing aim/shot/safety rays only; no new fallback/debug ray')
check('WarnedTraceChannelPairs.Contains(Pair)' in p and 'WarnedTraceChannelPairs.Add(Pair)' in p, 'Mismatch warning deduplicated per owner/channel pair')
check('Parts->ValidateWeaponTraceChannel(this, TraceChannel)' in f, 'Actual ability channel validation')
check('FindClosestBone' not in d and 'MyBoneName' not in d, 'No fabricated target bone')
check('if (!MappedFromBone.IsValid()) return FGameplayTag();' in p, 'Unmapped torso cannot select descendant arm')
check('Hit.GetActor() != GetOwner()' in d and 'Hit.GetComponent()->GetOwner() != GetOwner()' in d, 'Foreign hit rejected')
check('RoutedHit->GetComponent() == Character->GetMesh()' in d, 'Destroyed bypass only for owner mesh')
check('!bDestroyed && Parts->ApplyPartDamage' in d, 'Destroyed bypass cannot repeat destruction')
check('Damage * BodyDamageOnPartHitScale' in d and 'Damage * BodyDamageOnNoPartScale' in d, 'Both body scales retained')
check('FMath::Max(0.f, Before - Character->CurHP)' in d, 'Body damage records actual HP delta')
check('Char->OnHitEvent(Attacker, Actual)' in d, 'Damage awareness receives actual body damage')
check('Result.PartDamageApplied > 0.0f || Result.BodyDamageApplied > 0.0f' in d, 'Applied flag reflects health loss')
check('Debris->Tags.AddUnique(PartsHitCollision::DetachedDebrisTag)' in p and 'TEXT("DetachedDebris")' in f, 'Debris identified and rejected')
check('HideBoneByName(Bone, EPhysBodyOp::PBO_None)' in p, 'Safe bone hiding retained')
body_toggle = p.split('void UPartsComponent::SetOwnerBoneHitEnabled')[1].split('void UPartsComponent::')[0]
check('->SetCollision' not in body_toggle and 'TermBodiesBelow(' not in p, 'No per-body collision mutation introduced')
for key in ['component=%s', 'class=%s', 'bone=%s', 'impact=%s', 'channel=%d', 'complex=%d',
            'exists=%d', 'destroyed=%d', 'ownerMesh=%d', 'attachedPart=%d', 'detachedDebris=%d',
            'bodyOnly=%d', 'partApplied=%.3f', 'bodyApplied=%.3f', 'friendlyBlocked=%d', 'zeroReason=%s',
            'base=%.3f', 'distanceMultiplier=%.3f', 'requested=%.3f', 'totalApplied=%.3f']:
    check(key in f, 'Missing diagnostic ' + key)
diag = f.split('if (DMIsCombatDiagEnabled())', 1)[1].split('#endif', 1)[0]
check(not re.search(r'LineTrace|ApplyHitDamage\(|ApplyPartDamage\(|SetCollision|FRand', diag), 'Diagnostics observe only')
check('#if UE_BUILD_SHIPPING\n#define DM_FIRE_LOG(...) do {} while (false)' in (root / 'DeathMatch/Abilities/DMFireAttempt.h').read_text(), 'Shipping skips log argument evaluation')
print(f'PASS: {checks} source checks. Unreal tests/build/UHT/BP/PIE NOT run.')
