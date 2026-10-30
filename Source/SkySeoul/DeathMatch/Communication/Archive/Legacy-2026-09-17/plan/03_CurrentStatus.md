# DeathMatch Current Status

기준: `Source/SkySeoul/DeathMatch/` C++ 소스 점검 시점의 요약.

## 한 줄 판정
팀 데스매치 **보드(팀·스폰·사망 리스폰·메시지 파이프)** 는 깔려 있고,  
**매치 룰·완성 AI 전투·UI** 는 프로토타입 직전.  
모듈식은 **AI 쪽만 시작**, Core는 GameMode 집중형.

---

## 구현됨
| 영역 | 내용 |
|------|------|
| 팀 배정 | 2팀(0/1), 라운드로빈, `IGenericTeamAgentInterface` |
| 팀별 스폰 | `PlayerStartByTeam` |
| AI fill | 팀당 N마리 (`NumAIPerTeam` 기본 3) |
| 사망 → 리스폰 | `State.Dead` → `Event.Death` → GM Restart |
| Pawn/PC 주입 | `UDeathMatchGameInstane` Subsystem |
| GS 복제 골격 | RemainingTime, RoundState, TeamScores + OnRep |
| AI 모듈 골격 | Sense, SingleShoot, ActionBase, LocalMessage |
| UI C++ 뼈대 | `Presentation/` HUD + 4 위젯, GS 델리게이트 구독 |

## 반쯤 / 구멍
| 문제 | 내용 |
|------|------|
| 점수 | GS가 직접 Score++ / `AddScoreForTeam` 미사용 |
| TeamScores | BeginPlay 초기화 없음 |
| 라운드 타이머 | 필드만 있고 감소·타임아웃 없음 |
| Round/MatchOver | enum·일부 로직만, 전환 플로우 미완 |
| SingleShoot | Focus 페이로드 미연결, 발사는 BP 위임 |
| Brain | 오케스트레이터 없음 |

## 거의 비어 있음
- `DeathMatchRobotCharacter` / `DeathMatchAIController` — 빈 파생
- DM UI **BP 디자인** (C++ 뼈대는 `Presentation/` DONE)
- 매치 로비·카운트다운 플로우
- Photon 룸과 DM GM의 명시적 연동

## 모듈식인가?
- **부분적.** AIModules는 역할 분리·Action 계약·LocalMessage 의도가 있음.
- **완결 아님.** Brain 부재, Sense↔Shoot 끊김, Core 모놀리식, ScorePolicy 없음.

## 관련 소스 위치
```
DeathMatch/
  GameMode/          GM_DeathMatch, GS_DeathMatch
  PlayerState/       DeathMatchPlayerState
  Character/         DeathMatchRobotCharacter
  Controller/        DeathMatchAIController
  PlayerStart/       PlayerStartByTeam
  GameInstance/      DeathMatchGameInstane
  GameplayMessageStructure/
  AIModules/         Sense, SingleShoot, Base, LocalMsg, Tags
  Presentation/      HUD + Widgets (GS 구독 UI 뼈대)
  plan/              ← 본 정리 문서
```
