# DeathMatch Controllers

```
AAIC_EnemyControllerBase
 └─ ADeathMatchAIController          ← DM 공통 (GetAIPawnClass)
      └─ ADeathMatchRobotAIController ← 로봇 + Parts + Perception 취합
```

## Phase 1 — Perception gather (DONE)
`UAIModule_Sense` + Robot AI Perception:
- Sight → Hostile/Alive 필터 → Seen / Known / `FDMAwareTarget`
- Focus = 최근접 + Hold 보너스
- API: `HasHostile`, `GetFocusTarget`, `GetSeenHostiles`, `FindBestHostile`, `GetAwareTarget`

## Robot AI API
| 함수 | 용도 |
|------|------|
| CanShoot / Arm·Head | 파츠 디버프 |
| NotifyPossessedRobotDied | 사망 파츠 폭발 |
| GetFocusTarget / HasHostile / … | 취합 조회 |
| GetCombatMoveDirector | Move goal registry |
| GetSituationSupervisor | 주기·이벤트 상황 판단 |

GM `AIControllerClass` → `DeathMatchRobotAIController` (또는 BP).

Components: Perception, Sense, Message, CombatMoveDirector, SituationSupervisor,
MoveProviderRoam / Approach / HuntLost.

## Next
- FocusChanged → `SituationSupervisor->NotifySituationChanged`
- Providers + BT WantAttack branch
- 상세: `AI/Situation/LOGIC_STATUS.md`
