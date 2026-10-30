# GameMode — Score / Round Logic Status

관련 코드: `GS_DeathMatch`, `GM_DeathMatch`, `Core/DeathMatchTeamUtils`, `Core/DeathMatchSpawnUtils`  
최종 반영: P0 완료 + P2 (라운드 타이머, GM 헬퍼 분리) / P1 AI는 보류

---

## 1. C++ 로직 현황

### 점수 흐름
```
Event.Death → GS AwardKillScoreForVictimTeam → AddScoreForTeam
  → TargetKillNum 도달 시 RoundScore++ → RoundOver 또는 MatchOver
```

### 라운드 전환
```
RoundOver → GM NotifyRoundOver → RoundRestartDelay
  → ResetRoundKillScores → RespawnAll → InProgress

MatchOver → 타이머 취소, 사망 리스폰 중단
```

### 라운드 시계 (P2)
```
InProgress 진입 → StartRoundClock
  RemainingTime = RoundDuration (GM EditDefaultsOnly, 기본 180)
  1초마다 TickRoundClock → RemainingTime--
  RemainingTime <= 0 → RoundOver

RoundOver / MatchOver → StopRoundClock
RoundDuration == 0 → 시간 제한 없음 (시계 미작동)
```

타임아웃 RoundOver는 **RoundScore를 올리지 않음** (킬 목표 달성만 라운드 승). 정책 변경은 이후.

### GM 책임 (조율만)
| GM | Core 헬퍼 |
|----|-----------|
| 사망 리스폰 조율 | `DeathMatchTeamUtils` 팀 get/set/배정 |
| AI fill 루프 | `DeathMatchSpawnUtils` 스폰/리스폰/PlayerStart |
| RoundRestart 타이머 | |
| Payload / PS / GS 클래스 주입 | |

### 완료
| 항목 | 상태 |
|------|------|
| P0 점수·라운드 전환 | DONE |
| P2 RemainingTime 시계 | DONE |
| P2 Team/Spawn 헬퍼 분리 | DONE |
| P1 모듈식 AI | SKIP (보류) |

---

## 2. Blueprint 작업

### GameMode BP
- `TargetRound`, `TargetKillNum`, `TeamIds`
- **`RoundDuration`** (초, 0=무제한)
- **`RoundRestartDelay`**
- `NumAIPerTeam`, `AIControllerClass`
- 맵에 GM Override + `PlayerStartByTeam`

### UI
| 델리게이트 | 용도 |
|------------|------|
| `OnTeamScoreChanged` | 킬 / 라운드 승 |
| `OnRoundStateChanged` | RoundOver/MatchOver 연출 |
| **`OnRemainingTimeChanged`** | 타이머 텍스트 (초) |

조회: `GetRemainingTime`, `IsRoundInProgress`, `GetRoundState`, `GetTeamScore`

### 테스트
1. `RoundDuration`을 10으로 → 10초 후 RoundOver → Delay 후 InProgress, 시계 리셋
2. `RoundDuration` 0 → 시간으로 라운드 안 끝남
3. 킬로 RoundOver / MatchOver 시 시계 멈춤
4. MatchOver 중 사망 리스폰 없음

---

## 3. 경계
- DeathMatch 폴더만 수정, 근안 무터치
- 점수는 `AddScoreForTeam`만
- BP에서 `SetRoundState`로 강제 재개하지 말 것
