# DeathMatch Abilities

## 계층
```
UCharacterGameplayAbility
 └─ UDMGameplayAbilityBase     ← CanShoot 게이트, Focus 타겟 헬퍼
      └─ UDMGameplayAbility_Fire  ← Ability.DeathMatch.Fire
         LineTrace → Victim ApplyHitDamage
```

## Fire (C++ 기본)
1. CommitAbility
2. Start: active weapon **Muzzle** (fallback: view)
3. Aim: FocusTarget / FocusLocation / **camera crosshair trace**
4. `LineTraceSingleByChannel` (`FireRange`, `TraceChannel`)
5. Hit → `APartsCharacterBase::ApplyHitDamage`
6. `NotifyAttackAttempted` (SituationSupervisor)

튜닝: `FireRange`, `FireDamage`, `TraceChannel`, `bRequireTarget`, `bUseWeaponMuzzle`

BP는 `ExecuteFire`에서 몽타주/FX 추가 가능 (Super 호출 권장).

## 지급
`AbilitySets`에 Fire GA 등록 → Possess 시 Give.

## BP
1. `BP_GA_DM_Fire` ← `UDMGameplayAbility_Fire` (선택, 수치/FX)
2. `DA_DM_RobotAbilitySet`에 넣고 로봇 `AbilitySets`에 지정
