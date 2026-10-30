# DeathMatch Modular Architecture

## 목표
DeathMatch만 모듈식으로 운영한다. SkySeoul 전체를 모듈식으로 바꾸지 않는다.

## 레이어
| 레이어 | 역할 | 비고 |
|--------|------|------|
| Match Core | 규칙·점수·라운드·리스폰 | GM / GS / ScorePolicy |
| Team / Spawn | 팀 ID, 스폰 포인트 | PlayerStartByTeam, GenericTeam |
| Combat Bridge | 사망·킬을 모드로 전달 | Event.Death + Payload |
| AI Brain | 액션 선택 (오케스트레이터) | **필수 뼈대, 현재 부재** |
| AI Modules | 감각·공격·이동 등 단위 능력 | Sense, SingleShoot, LocalMsg |
| Presentation | HUD / 스코어보드 | 후순위, BP 가능 |

## 원칙
1. 모듈은 한 가지 일만 한다.
2. **오케스트레이터(Brain)만** 어떤 액션을 활성화할지 선택한다.
3. 통신 이원화:
   - **LocalMessage** — AI 모듈 간 (컨트롤러 로컬)
   - **GameplayMessage** — 매치 전역 (Death, Round 등)
4. Core는 개별 AI 모듈을 모른다. “킬/라운드” 사실만 방송한다.
5. 확장 = 모듈 추가. Core 수정은 최소화.

## 추천 폴더 골격 (정리 시)
```
DeathMatch/
  plan/                 ← 본 기획/정리 문서
  Core/                 GM, GS, RoundState, ScorePolicy
  Team/                 TeamId, Attitude, PlayerStart
  Events/               Death, Round payloads
  AI/
    Brain/              Orchestrator
    Modules/            Sense / Move / Shoot / Utility
    Messaging/          LocalGameplayMessageManager
  Character/
  Controller/
  PlayerState/
  GameInstance/
  Presentation/         (선택)
```

## AI 계약 (이미 시작된 것 고정)
- **Channel:** Movement / Attack / Aim / Utility / SpecialAttack
- **Action:** Evaluate → Activate → Deactivate
- **Sense → FocusChanged → Attack 모듈 구독**
- **Brain 위치 권장:** `DeathMatchAIController` (Perception·Possess·리스폰과 동일 소유자)

## Adapter (근안 터치 0)
| Adapter | 역할 |
|---------|------|
| DeathMatchPlayerState | Dead 태그 → Event.Death |
| DeathMatchGameInstane (Subsystem) | Pawn/Controller 클래스 주입 |
| DeathMatchRobotCharacter | Robot 파생, DM 모듈 조립만 |
| GM_DeathMatch | 맵 진입점, 팀/스폰/AI fill 조율 |
