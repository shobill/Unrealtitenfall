# DeathMatch Presentation (UI) Status

C++ 뼈대만. BP는 상속 후 레이아웃·브러시·애님만.

## 폴더
```
DeathMatch/Presentation/
  HUD/DeathMatchHUD
  Widgets/
    DeathMatchWidgetBase
    DeathMatchMatchHUDWidget
    DeathMatchScoreboardWidget
    DeathMatchRoundBannerWidget
    DeathMatchMatchResultWidget
```

## 데이터
- 소스: `AGS_DeathMatch` (RemainingTime / RoundState / TeamScores)
- 방식: 델리게이트 구독(push) + 최초 `RefreshFromGameState`(pull)
- UI에서 점수·라운드 판정 금지

## 역할
| 클래스 | 역할 |
|--------|------|
| `ADeathMatchHUD` | 위젯 생성, RoundState 전환, Scoreboard, **C++ 조준선** |
| `UDeathMatchWidgetBase` | GS 바인드/언바인드, 애님용 `BP_On*Anim` |
| MatchHUD | 타이머·팀 점수 텍스트 갱신 |
| Scoreboard | Tab 등 — `HUD->ToggleScoreboard()` |
| RoundBanner | Waiting / RoundOver 배너 |
| MatchResult | MatchOver 승팀·라운드 스코어 |

## 조준선 / 전투 HUD (C++ Canvas)
- `DrawHUD` 중앙 크로스 (기본 on, ADS 중에만)
- **타이머 + 팀 킬점수** Canvas 오버레이 (`bDrawCombatHUD`, BP 위젯 없어도 표시)
- BP MatchHUD 쓰면 `bDrawCombatHUD = false` 또는 위젯 완성 후 끄기

## BP 작업 (옵션 — 예쁜 UI)
1. `BP_DeathMatchHUD` ← `ADeathMatchHUD`
2. 위젯 4종 BP 자식 만들고 HUD의 Class 슬롯에 지정
3. BindWidgetOptional 이름 맞추기:
   - `Text_RemainingTime`, `Text_Team0Score`, `Text_Team1Score`
   - `Text_Team0RoundScore`, `Text_Team1RoundScore`
   - `Text_RoundState`, `Text_Banner`, `Text_Winner`
   - `Panel_Root`, `Panel_Combat`
4. 애님 이벤트만 구현 (`BP_On*Anim`)
5. GM BP의 HUD Class → `BP_DeathMatchHUD` (C++ 기본은 `ADeathMatchHUD`)

## 입력
- Scoreboard: `ADeathMatchHUD::ToggleScoreboard` (PC/입력은 이후 연결)

## 이후
- Killer / 개인 K-D 행
- Scoreboard 플레이어 리스트
