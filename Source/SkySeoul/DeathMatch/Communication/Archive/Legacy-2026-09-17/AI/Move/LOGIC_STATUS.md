# Combat Move Framework

Call-on-demand providers. No per-frame provider tick.
Situation brain: `AI/Situation/UDMSituationSupervisor`.

## Types
- `EDMMoveIntent` — None / Roam / Approach / HuntLost / Hold
- `FDMMoveGoal` — bValid, Intent, Location, RelatedTarget

## Components (on `DeathMatchRobotAIController`)
| Class | Intent | Goal |
|-------|--------|------|
| `UDMMoveGoalProvider_Roam` | Roam | Weighted `ADMMovePoint` |
| `UDMMoveGoalProvider_Approach` | Approach | Focus쪽 FireRange×0.85 지점 (Nav project) |
| `UDMMoveGoalProvider_HuntLost` | HuntLost | `LastSensedLocation` (Nav project) |
| Hold | — | Provider 없음 → Director `ClearCurrentGoal` |
| `UDMCombatMoveDirector` | — | Registry + `RequestGoal` / cache |

## Next
1. BT: `HasMoveGoal` → MoveTo; drop/legacy `SelectMoveLocation` when Supervisor drives roam
2. FocusChanged → Supervisor notify
3. Arrive → `NotifySituationChanged(MoveFinished)`
