# DeathMatch Internal Cleanup Backlog

손댈 범위: `Source/SkySeoul/DeathMatch/` 내부만.  
입구(맵 GM 교체)·SkySeoul 근안은 제외.

---

## P0 — 모드가 성립하려면

### 1. 점수 경로 단일화 — DONE
- 사망 메시지 → `AddScoreForTeam` **한 길**로만.
- GS `OnDeathMessageReceived` 직접 `Score++` 제거.
- `AwardKillScoreForVictimTeam` → `AddScoreForTeam` 경유.

### 2. TeamScores 초기화 — DONE
- BeginPlay(서버)에서 GM `GetTeamIds()`로 점수 슬롯 미리 생성.
- `InitializeTeamScores` 추가.
- GM에 `GetTeamIds()` getter 추가.

### 3. 킬 / 라운드 판정 연결 — DONE
- `AddScoreForTeam`이 `TargetKillNum` → RoundOver, `TargetRound` → MatchOver 처리.
- `TeamScores[TeamId]` 인덱스 버그 수정 (`FindTeamScoreIndex`).
- `InProgress`가 아닐 때 점수 무시 (중복 라운드 승 방지).

### 4. 라운드 전환 골격 — DONE
- **RoundOver:** `NotifyRoundOver` → `RoundRestartDelay` → 킬 Score 리셋 → 전원 리스폰 → `InProgress`
- **MatchOver:** 재시작 타이머 취소, 사망 리스폰 중단
- 상세: `GameMode/LOGIC_STATUS.md`

---

## P1 — 모듈식 AI가 돌아가려면 — SKIP (보류)

사용자 요청으로 일단 패스. 이후 재개.

### 5. Brain (오케스트레이터) 추가
### 6. Sense → Shoot 배선
### 7. 모듈 조립 지점 고정
### 8. 발사 계약만 고정

---

## P2 — Core 책임 분리 — DONE

### 9. GM 역할 축소 — DONE
- `Core/DeathMatchTeamUtils` — 팀 배정
- `Core/DeathMatchSpawnUtils` — 스폰/리스폰/PlayerStart
- GM은 조율(사망 리스폰, AI fill 루프, 라운드 재시작 타이머)만 유지

### 10. 라운드 타이머 — DONE
- GM `RoundDuration` (기본 180, 0=무제한)
- GS `StartRoundClock` / `TickRoundClock` (1초)
- 0 → RoundOver (RoundScore 미증가)
- `OnRemainingTimeChanged` 델리게이트 추가
- 상세: `GameMode/LOGIC_STATUS.md`, `Core/LOGIC_STATUS.md`

---

## P3 — 후순위

11. HUD / 스코어보드 (GS 델리게이트 구독) — **C++ 뼈대 DONE** (`Presentation/`)
12. MatchOver 결과 화면 — **C++ 뼈대 DONE** (`DeathMatchMatchResultWidget`)
13. Photon ↔ DM 매치메이킹
14. 폴더 재배치 (`Core/` 일부 완료, AI 등은 보류)
15. 파일명 오타 수정: `DeathMatchGameInstane` → `Instance`

BP: 위젯 상속·디자인·애님 + `BP_DeathMatchHUD`에 클래스 지정. 상세: `Presentation/LOGIC_STATUS.md`

---

## 추천 진행 순서
1. 점수·라운드 (P0) — DONE
2. Brain + Sense↔Shoot (P1) — SKIP
3. GM 책임 쪼개기 + 타이머 (P2) — DONE
4. UI C++ 뼈대 (P3.11~12) — DONE
5. UI BP 디자인 / Photon / 폴더 정리 — NEXT
