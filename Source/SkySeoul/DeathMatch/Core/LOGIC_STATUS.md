# Core Helpers Status

GM에서 분리한 순수 헬퍼. UObject/컴포넌트 아님 (namespace).

| 파일 | 역할 |
|------|------|
| `DeathMatchTeamUtils` | TeamId get/set, AssignTeamIfNone, CountControllers |
| `DeathMatchSpawnUtils` | PlayerStart 검색, AIController 스폰, AI 리스폰, GatherControllers |

호출자는 `AGM_DeathMatch` (조율). BP 직접 호출 불필요.

다음 확장 시: ScorePolicy / RoundPolicy를 여기 또는 `Core/`에 추가 가능.
