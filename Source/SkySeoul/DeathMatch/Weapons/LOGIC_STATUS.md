# DeathMatch Weapons — status

## Done (C++)
- `DMWeaponTypes` — `FDMWeaponDefinition` / `FDMWeaponRuntimeSlot`
- `ADMWeaponActor` — mesh + `Muzzle` + **`AbilitySetWhileActive`** (grant while selected)
- `UDMWeaponLoadoutComponent` — acquire / cycle / select; reads AbilitySet from active weapon actor
- `UDMAimZoomComponent` — RMB hold ADS (FOV + spring arm)
- `ADeathMatchPlayerCharacter` — WeaponLoadout + AimZoom
- `ADeathMatchRobotCharacter` — WeaponLoadout (StartingWeapons / muzzle)
- `ADeathMatchPlayerController` — Wheel cycle + RMB aim
- `UDMGameplayAbility_Fire` — start at active muzzle; aim via camera/crosshair (or AI focus)
- `ADeathMatchHUD` — C++ center crosshair (tightens with aim alpha)

## BP
1. Weapon BP → **Ability Set While Active** = Fire DA  
2. Character StartingWeapons → WeaponActorClass + socket only (AbilitySet은 무기 BP)
