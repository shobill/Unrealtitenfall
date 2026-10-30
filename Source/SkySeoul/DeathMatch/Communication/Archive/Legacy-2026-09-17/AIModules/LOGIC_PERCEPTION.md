# DM Perception Gather (Phase 1)

## Flow
```
AIPerception (Sight, enemies only)
  → UAIModule_Sense
       filter: valid / alive / hostile team
       Seen / Known / FDMAwareTarget
       Focus (nearest + hold)
       LocalMessage FocusChanged …
  → Query API on Sense / DeathMatchRobotAIController
```

## API
| 함수 | 의미 |
|------|------|
| HasHostile | Known에 유효 적 존재 |
| GetSeenHostiles / GetKnownHostiles | 목록 |
| GetFocusTarget / GetFocusLocation | 현재 포커스 |
| FindBestHostile | Focus와 동일 정책 |
| GetAwareTarget | LastLoc / Dist / Seen 플래그 |

## Files
- `DMPerceptionTypes.h` — `FDMAwareTarget`
- `AIModule_Sense` — 취합 엔진
- `DeathMatchRobotAIController` — Perception + Sense + Message + Team

## Not in Phase 1
- Blackboard 기록 (Phase 2)
- BT Task (Phase 3)
- Sight 수치 폴리시 디테일
