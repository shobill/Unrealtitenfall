# DeathMatch Sense — 2026-09-17

AIPerception Sight → 적대·생존 필터 → Seen/Known/FDMAwareTarget → focus/기억 → SituationFramework 직접 통지.

Sense와 Controller의 HasHostile/GetFocusTarget/GetSeenHostiles/GetKnownHostiles/FindBestHostile/GetAwareTarget 조회 API를 유지한다. 감지/기억은 Sense, 교전 판단은 CombatTactics가 담당한다.

수신자가 없는 LocalMessage 방송·payload·tag 멤버 및 MessageManager 주입 계층을 제거했다. FocusChanged/TargetAcquired/TargetLost의 직접 Framework 호출은 유지했다. AIModuleBase는 Sense의 직렬화 부모로 남는다.

사망·소유 해제·리스폰 때 ResetAwareness로 Perception 기록과 Seen/Known/Awareness/focus를 초기화한다. 새 Pawn은 이전 생애의 대상을 상속하지 않고 새 감지로 교전을 시작한다.


2026-09-22: Perception Focus는 후보 추천이며 Combat/Pending Target은 Brain 소유다. hostile affiliation을 sensed로 간주하던 경로를 제거했다. 성공 Sight에서만 LastSensedLocation/Time을 갱신하고, sight loss는 이전 기록을 보존한다. FindBestHostile의 기억 후보와 숨은 Focus 위치는 저장 기록만 읽는다. 새 상태 흐름은 `../plan/01_Architecture.md` 참조. 빌드/PIE/BP/자동화 미실행.
