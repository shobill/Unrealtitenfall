# DeathMatch 현행 구조 — 2026-09-17

기준 설계는 [01_Architecture.md](01_Architecture.md)다. 완료 범위는 [03_CurrentStatus.md](03_CurrentStatus.md), 호환성 때문에 남긴 항목은 [02_InternalCleanup.md](02_InternalCleanup.md)에 기록한다. 이전 문서는 Communication/Archive/Legacy-2026-09-17/에 보존했다.

Sense는 감지와 기억, SituationFramework는 비전투 Roam과 전투 상태 전환, CombatTactics는 교전 판단을 소유한다. BT는 사격 Ability 실행, GameplayAbility_Fire는 실제 발사 사이클, Weapon Actor는 사거리·확률·피해 데이터를 소유한다.

GameMode 진입점, 기존 맵의 Precision Controller, DeathMatchGameInstane 이름을 유지한다. Adaptive는 별도로 선택하는 Controller다. 기능 추가나 전투 점수·사격 밸런스 변경은 포함하지 않는다.
