# DeathMatch 상태 전환 명령 정리 결과 보고서

- 날짜: 2026-10-15
- 범위: Root BT 상태 Abort, Branch 소유 Move/Fire/Focus/Intent, Roam/Combat 예약 정리, Dead 전량 정리

## 결과

Root Behavior Tree가 상태 우선순위를 계속 소유하도록 유지하면서, 실행 중 조건 변화가 실제 Branch Abort로 이어지도록 상태 Decorator를 반응형으로 변경했다. `BT_main`의 기존 상태 Decorator는 모두 `Observer Aborts = Both`이며, 누락되어 있던 `DM Is Dead`도 동일하게 저장했다.

`Action Intent`는 이제 `EDMBehaviorState`(Roam, LostTarget, Combat, Retreat, Hide, Dead)와 `EDMActionIntent`(Attack, FastReposition 등)를 별도로 노출한다. 새 Controller 상태 모듈은 추가하지 않았다. Action Intent 요청의 승자가 상위 상태를 바꿀 때 Move Director가 새 상태와 호환되지 않는 이전 이동 소유자를 새 이동 제출 전에 취소한다.

`DM Move To Goal`은 Move Director가 발급한 Request ID와 시작 Completion Serial을 Node Memory에 저장한다. Abort는 Owner enum 전체가 아니라 자신이 만든 Request ID만 취소하므로, 같은 `Combat` 소유자의 새 Attack 이동을 이전 Branch 정리가 지우지 않는다. Request ID 0, 이미 교체된 요청, 중복 Abort는 모두 no-op이다.

## 상태별 정리 계약

| 진입 상태 | 허용 Move 소유자 | 이전 상태 정리 | 유지 정보 |
|---|---|---|---|
| Roam | StrategicRoam | Combat/Retreat/EmergencyHide/Investigate 이동 취소, Combat Position 해제, 활성 선택 타겟 해제 | Knowledge의 기억 레코드 정책 |
| LostTarget | Investigate | Combat/Retreat/EmergencyHide/StrategicRoam 이동 취소, Combat Position 해제, 이전 Branch Fire/Focus/Intent ID 종료 | CombatTarget, 마지막 위치, 기억 |
| Combat | Combat | StrategicRoam/Investigate/Retreat/EmergencyHide 이동 취소, Roam Incoming AI·SelectedTacticalArea·Roam MoveGoal 해제 | 선택된/재선택할 전투 타겟과 기억 |
| Retreat | Retreat | 다른 Move 소유자 취소, 이전 Combat Position 해제 | 전투 타겟과 기억 |
| Hide | EmergencyHide | 다른 Move 소유자 취소, 이전 Combat Position 해제 | 전투 타겟과 기억 |
| Dead | 없음 | 모든 Move/Fire/Focus/Intent, Tactical Area, Combat Position, ADS, Blackboard 실행 타겟/목표, Knowledge 초기화 | 없음 |

Fire, Focus, Action Intent는 기존 Branch Request ledger를 계속 사용한다. `OnCeaseRelevant()`는 Node Memory의 ID 하나만 종료하고, 남은 요청이 있으면 해당 요청의 Fire/Focus/ADS 정책이 유지 또는 복원된다. `DM Disengage Combat`은 생존 상태의 Roam 복귀에서 더 이상 전역 Reset을 호출하지 않으며, Dead일 때만 전량 정리를 수행한다.

가시성은 발견 시 즉시 Combat 조건을 만족하지만, 상실 시 기본 0.2초 Exit Grace를 적용한다. 유예 후 LostTarget으로 전환되며, 한 번의 Perception 누락으로 Combat과 LostTarget이 즉시 왕복하는 현상을 줄인다. 기억이 만료되기 전에는 Roam으로 직접 전환하지 않는다.

## Roam 종료 세부 동작

- StrategicRoam Request ID만 취소하고 Path Following을 중단한다.
- Tactical Area Incoming AI 예약을 해제한다.
- `SelectedTacticalArea`와 Roam `MoveGoal`을 지운다.
- Abort는 Completion Serial 성공을 만들지 않으므로 `ReportRoamAreaReached()`를 호출하지 않는다.
- Roam Action Intent Request는 Service의 `OnCeaseRelevant()`에서 자신의 ID만 종료한다.

## 변경 파일

- `Content/DeathMatch/AI/BT/BT_main.uasset`
  - `DM Is Dead`의 Observer Aborts를 `Both`로 변경
- `Source/SkySeoul/DeathMatch/AI/BT/DMBTDataNodes.h/.cpp`
  - 반응형 상태 Decorator 공통 수명, 주기 재평가, `ConditionalFlowAbort()`, Visible Exit Grace
- `Source/SkySeoul/DeathMatch/AI/BT/BTTask_DM_MoveToGoal.h/.cpp`
  - Move Request ID/Completion Serial Node Memory, ID 기반 Abort, Roam 예약·Blackboard 정리
- `Source/SkySeoul/DeathMatch/AI/BT/BTTask_DM_DisengageCombat.cpp`
  - 생존 Roam 복귀의 전역 Reset 제거, Dead만 전량 정리
- `Source/SkySeoul/DeathMatch/AI/Execution/DMActionIntentTypes.h`
  - 상위 `EDMBehaviorState` 추가
- `Source/SkySeoul/DeathMatch/AI/Execution/DMActionIntentComponent.h/.cpp`
  - 상위 상태와 하위 Intent 분리, 상태 진입 전 이동/예약 조정
- `Source/SkySeoul/DeathMatch/AI/Execution/DMCombatFocusComponent.h`
  - 요청 수 자동화 검증 accessor
- `Source/SkySeoul/DeathMatch/AI/Move/DMCombatMoveDirector.h/.cpp`
  - Request ID 발급/취소, 상태별 허용 소유자 계약
- `Source/SkySeoul/DeathMatch/Controller/DeathMatchRobotAIController.cpp`
  - Dead/Respawn 전체 실행 상태, Tactical/Combat 예약 정리 보강
- `Source/SkySeoul/DeathMatch/Tests/DMBTFoundationTests.cpp`
  - 전환/중복 종료/Dead/Root Abort/Move Owner 계약 자동화
- `Source/SkySeoul/DeathMatch/Tests/configure_state_transition_aborts.py`
  - Root BT 상태 Decorator Abort 설정 스크립트
- `Source/SkySeoul/DeathMatch/Tests/inspect_state_transition_bt.py`
  - BT 상태/Service/Abort 읽기 전용 감사 스크립트

## 자동화 및 정적 검증

- `SkySeoulEditor Win64 Development`: 성공
- `SkySeoul.DeathMatch.BTFoundation`: 16개 실행, 16개 성공, 실패 0
- 신규 상태 전환 테스트:
  - `StateTransition.DeadClearsExecutionState`: 성공
  - `StateTransition.MoveOwnerContract`: 성공
  - `StateTransition.RequestOwnership`: 성공
  - `StateTransition.RootAbortContract`: 성공
- 기존 회귀 항목:
  - Action Intent 수명/ADS/실행 설정: 성공
  - Maintain Fire Request 소유권: 성공
  - Focus 중복 종료 안전성: 성공
  - BT_Combat 단일 Fire 실행자 계약: 성공
- BT 읽기 전용 감사:
  - `BT_main`의 IsDead, ShouldRetreat, Visible(정/역), Remembered 상태 Decorator가 모두 `Observer Aborts = Both`
  - Combat의 Clear Shot은 `Both`; Attack/FastReposition Intent와 Fire/Focus Service 유지
  - Roam/Hide/LostTarget의 Action Intent가 각각 Roam/Hide/Observe로 유지

## 사용자 PIE 확인 항목

1. Roam MoveTo 도중 적을 발견하면 기존 경로를 기다리지 않고 즉시 정지하고 Combat으로 전환되는지 확인한다.
2. Combat 진입 후 이동 Debug Owner가 Combat이며 이전 Roam 목적지로 되돌아가지 않는지 확인한다.
3. 시야 상실 직후 0.2초 유예 뒤 LostTarget/Observe로 전환하고 마지막 위치 Focus가 유지되는지 확인한다.
4. 기억 만료 후 Roam으로 복귀할 때 Fire Ability와 ADS가 해제되고 CombatTarget 선택만 비워지는지 확인한다.
5. Hide 진입/복귀 시 EmergencyHide 이동이 종료되고 새 Attack 이동·Focus·Fire가 정상 복구되는지 확인한다.
6. 사망 시 Move/Fire/Focus/Intent와 Tactical/Combat 예약이 모두 사라지는지 확인한다.
7. 위 상태를 반복 왕복해도 요청 수와 예약이 누적되지 않는지 확인한다.

PIE 자체는 이번 검증 범위에서 실행하지 않았다.
