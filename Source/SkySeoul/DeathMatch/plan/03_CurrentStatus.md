# DeathMatch 현재 상태 — 2026-09-17

팀 배정, 팀별 스폰, 사망 리스폰, 점수 집계, 라운드 타이머와 RoundOver/MatchOver 흐름은 구현되어 있다. Robot/Controller는 빈 파생 클래스가 아니며 Sense·전투·무기·파츠 기능과 연결되어 있다.

교전 판단은 CombatTactics가 담당한다. Framework의 중복 전투 판단을 제거하고 Roam/전환/fallback만 남겼다. AI별 Framework 생성 Behavior는 저장된 Combat Brain에서도 Roam 하나다. 외부 및 에셋 참조가 없는 LocalMessage/SingleShoot/Supervisor 계층과 BT 노드 3종을 제거했다.

기존 Precision BP와 Adaptive BP의 설정·맵 기본값은 보존한다. 참조가 발견된 구형 Behavior/Provider는 로드 호환용으로 유지한다. 최신 빌드·자동화·PIE 결과 및 미검증 범위는 같은 날짜의 DEATHMATCH_ARCHITECTURE_CLEANUP_RESULT_REPORT가 최종 기준이다.

UI/Photon/패키징 전체 완성을 이 작업으로 보증하지 않는다. 이번 검증 범위는 DeathMatch AI 구조와 Combat 회귀 테스트다.
