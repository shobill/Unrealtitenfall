# DeathMatch AI Framework (modular Brain + Behavior)

## Layout
`DeathMatch/AI/Framework/`
- `DMAIFrameworkTypes` — `EDMBehaviorId`, `FDMSituationDecision`
- `UDMSituationBrainBase` — personality + **AllowedBehaviorClasses** (behavior menu)
- `UDMBehaviorModuleBase` / `PathMove` — action modules
- `UDMSituationFramework` — Brain 생성 → Allowed만 spawn → Evaluate → ActivateBehavior → BB

## Flow
```
EnsureBrain()
  → NewObject Brain
  → Spawn each AllowedBehaviorClasses onto AIController
NotifySituationChanged
  → Brain.Evaluate → Decision
  → FindBehavior(id)->ActivateBehavior()
  → Push WantAttack / MoveIntent to BB
```

## Behaviors (classes; spawned only if Brain lists them)
| Module | Id |
|--------|-----|
| `UDMBehaviorModule_Move` | Roam |
| `UDMBehaviorModule_Approach` | Approach |
| `UDMBehaviorModule_HuntLost` | HuntLost |
| `UDMBehaviorModule_Hold` | Hold |
| `UDMBehaviorModule_Attack` | Attack |

## Brains
| Class | Allowed (default) | Role |
|-------|-------------------|------|
| `UDMSituationBrain_RoamOnly` | Move | 시험용 배회 |
| `UDMSituationBrain_Combat` | Move/Approach/HuntLost/Hold/Attack | 적 유무·시야·사거리로 행동 선택 |

### Combat 선택
- 적 없음 → Roam
- 적 있음 + 안 보임 → HuntLost
- 보임 + 사거리 밖 → Approach
- 보임 + 사거리 안 + CanShoot → Attack (`WantAttack`)
- 보임 + 사거리 안 + !CanShoot → Hold

AIController → SituationFramework → **Brain Class** = `DMSituationBrain_Combat` (또는 BP 자식)

## BP
1. Brain BP → **Behavior Tree** 슬롯에 BT 에셋 지정 (+ Allowed Behavior Classes)
2. AIController → SituationFramework → **Brain Class** = 그 Brain
3. Controller의 Behavior Tree Fallback은 Brain에 BT 없을 때만 사용

## Behavior extension
- Framework → `ActivateBehavior` → **`ExecuteBehaviorLogic` (virtual)**
- PathMove helpers: `ResolveMoveGoal`, `IssueMoveToGoal`, `ClearMoveGoalState`, …
