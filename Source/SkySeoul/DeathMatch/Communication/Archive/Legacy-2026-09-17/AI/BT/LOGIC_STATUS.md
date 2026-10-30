# DeathMatch BT Nodes

## Phase 2 — Bridge
`UBTService_DM_UpdateCombatState` → BB keys (`DMBlackboardKeys`)

## Phase 3 — Actions (minimal)
| 노드 | 역할 |
|------|------|
| `BTDecorator_DM_HasTarget` | `HasTarget` |
| `BTDecorator_DM_CanShoot` | `CanShoot` |
| `BTTask_DM_FaceTarget` | TargetActor/Location으로 yaw |
| `BTTask_DM_ActivateAbility` | Tag로 GA 활성화 (기본 Fire) |
| `BTTask_DM_SelectMoveLocation` | MovePoint 수집 → BB (선택 로직 stub) |
| 엔진 `MoveTo` | `TargetLocation` / `MoveGoalLocation` |

## BB keys (추가)
| Key | Type | Writer |
|-----|------|--------|
| MoveGoalLocation | Vector | SituationSupervisor |
| HasMoveGoal | Bool | SituationSupervisor |
| WantAttack | Bool | SituationSupervisor |
| MoveIntent | Enum/Byte | SituationSupervisor |

## 예시 트리
```
Root + Service(UpdateCombatState)
 ├─ Decorator WantAttack
 │    └─ Face → ActivateAbility
 ├─ Decorator HasMoveGoal → MoveTo (MoveGoalLocation)
 └─ (fallback roam via Supervisor Refresh)
```

## BP
1. BB 키 생성 (위 + Target/CanShoot 등 Phase 2)
2. BT에 Service + 위 노드 배치
3. 로봇 AbilitySets에 Fire GA
4. 컨트롤러에 BT 지정
