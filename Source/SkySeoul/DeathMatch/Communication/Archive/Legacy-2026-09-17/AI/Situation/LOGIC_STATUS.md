# Situation Supervisor

AI-wide situation brain on `DeathMatchRobotAIController`.
Deeper than MoveDirector: decides **what** to do; MoveDirector only **where** when asked.

## Dual path
| Path | When | API |
|------|------|-----|
| Event | focus / arrive / parts / possess | `NotifySituationChanged(Reason)` |
| Watchdog | every `WatchdogInterval` (default 1s) | `WatchdogCheck` → same evaluate |

Providers are **never** ticked. Supervisor calls `CombatMoveDirector->RequestGoal` only when `bRefreshMoveGoal`.

## Flow
```
BuildSnapshot → Evaluate → FDMSituationCommand → ApplyCommand
  ├─ ClearCurrentGoal / RequestGoal(Intent)
  └─ BB: WantAttack, MoveIntent, HasMoveGoal, MoveGoalLocation
```

## Intent policy (aggressive B-tier)
| Situation | MoveIntent | WantAttack |
|-----------|------------|------------|
| No hostile | Roam | false |
| Hostile unseen | HuntLost | false |
| Seen, out of FireRange | Approach | false |
| Seen, in range | Hold | CanShoot |

Watchdog: if Should attack but `TimeSinceLastAttackAttempt >= AttackStuckTimeout`, keeps forcing `WantAttack`.

## Call sites (now)
- `OnPossess` → `Possessed` (after BT)
- Timer → `Watchdog`

## Call sites (wire later)
- Sense FocusChanged → `FocusChanged` / `TargetLost` / `TargetAcquired`
- Move arrive/fail → `MoveFinished` / `MoveFailed`
- Parts gate change → `PartsChanged`
- Fire ability start → `NotifyAttackAttempted()`

## Blackboard keys (add on asset)
- `WantAttack` (Bool)
- `MoveIntent` (Enum / Byte — matches `EDMMoveIntent`)

## Next
1. ~~Concrete Providers~~ — Roam / Approach / HuntLost DONE
2. BT branch on WantAttack / HasMoveGoal
3. FocusChanged → NotifySituationChanged
