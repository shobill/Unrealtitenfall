# BT 이동 실행 계층 — 2026-10-14

CombatMoveDirector는 BT가 제출한 하나의 이동 요청만 실행·취소하고 완료/실패를 기록한다. Provider registry, 목표 생성, Combat/Roam 판단, 행동 우선순위 정책은 없다.

Roam 영역 가중 선택과 영역 내부 Nav 위치 선택은 BT Task가 TacticalAreaSubsystem을 통해 수행한다. Incoming 예약, 최근 방문 감점, 영역 실패 cooldown은 Subsystem에 보관된다. MoveToGoal Abort는 자신이 시작한 요청과 Roam 예약을 정리한다.
