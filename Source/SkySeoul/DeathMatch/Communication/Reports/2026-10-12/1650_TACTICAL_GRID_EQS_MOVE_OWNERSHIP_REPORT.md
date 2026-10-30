# Tactical Grid 역할 축소 / EQS 전환 / 이동 소유권 정리 결과

작성 시각: 2026-10-12 16:50 KST

## 결과 요약

이동 책임을 다음 경계로 분리했다.

- 비전투: Tactical Grid가 전략 영역만 선택하고, Roam Provider가 그 영역 안의 실제 NavMesh 목적지를 만든다.
- 전투: Combat Brain이 행동을 고르고, 행동에 지정된 목적별 EQS가 실제 위치를 반환한다.
- 실행: `UDMCombatMoveDirector`만 `MoveToLocation`과 `StopMovement`를 호출한다.
- 통과성: 실제 캡슐 Scale 1.0 실패만 이동을 차단하고, 더 큰 clearance 실패는 품질 정보로만 취급한다.

에디터 빌드, UHT, 자동화 테스트 실행, BP 로드, PIE는 지시 범위에 따라 수행하지 않았다. 아래 검증 항목은 정적 코드 검토와 자동화 테스트 소스 작성 결과다.

## 주요 변경 파일

- `Navigation/DMTacticalGridTypes.h`
- `Navigation/DMTacticalGridSubsystem.h/.cpp`
- `Navigation/DMNavigationSafety.h/.cpp`
- `AI/Move/DMMoveTypes.h`
- `AI/Move/DMCombatMoveDirector.h/.cpp`
- `AI/Move/DMMoveGoalProvider_Roam.h/.cpp`
- `AI/Framework/DMBehaviorModule_PathMove.cpp` 및 기존 이동 중단 호출부
- `AI/Combat/DMCombatModules.h/.cpp`
- `AI/Combat/DMCombatEQS.cpp` (신규)
- `AI/Combat/DMCombatEQSContexts.h/.cpp` (신규)
- `AI/Combat/DMCombatTacticsComponent.h/.cpp`
- `AI/Combat/DMCombatThreatMemory.cpp`
- `AI/Combat/DMCombatAttackPosition.cpp`
- `AI/Combat/DMCombatCoverModule.cpp`
- `Controller/DeathMatchRobotAIController.cpp`
- `Tests/DMTacticalGridTests.cpp`
- `Tests/DMNavigationSafetyTests.cpp`
- `Tests/DMArchitectureOwnershipTests.cpp`
- `AI/Combat/DMCombatMovementSelectionTests.cpp`
- `plan/01_Architecture.md`, `AI/Move/LOGIC_STATUS.md`

## Tactical Grid

`CellSize` 기본값을 1200cm로 올리고 400~2400cm 범위에서 편집 가능하게 유지했다. 셀은 도착점이 아니라 큰 전략 영역이다. `FDMTacticalStrategicRegionSelection`은 Cell ID, Layer ID, Nav Region ID, Bounds, 목적, 점수, 이유, 이동 비용, 아군/혼잡 수와 원본 점수 상세를 제공한다.

`FindStrategicRegions`는 `DisengagedRoam`에서만 결과를 반환한다. Visible/기억/조사 등 전투 상태나 `bInCombat` 요청에는 결과를 주지 않는다. 기존 Unknown/Frontier/Known/Stale, SpawnControl, 관측, 전투 정보, 아군 밀도, 최근 방문, 방향 유지, 성향, 상태별 가중치, 그래프 이동 비용은 유지했다.

Cell ID의 Layer/Region 구성은 유지했다. 전략 결과에도 두 값을 명시해 같은 XY의 다른 층 또는 연결 단절 영역을 구분한다. 셀 일부나 중심이 막혔다는 이유로 큰 영역 전체를 예약 또는 제거하지 않는다. 실제 도달 가능 지점은 다음 단계의 NavMesh 후보 검사에서 결정한다.

## 영역 내부 NavMesh 목적지

Roam Provider는 선택 영역마다 다음 순서로 실제 목적지를 만든다.

1. 기존 유효 sample과 Bounds 내부 분산 sample을 합쳐 기본 12개 후보를 만든다.
2. 후보 부족 시 최대 2회의 제한된 추가 pass를 사용한다.
3. 요청 Pawn 위치와 선택 영역을 기반으로 안정적인 random seed를 만든다.
4. NavMesh projection, Bounds, 동일 Cell/Layer/Region을 검사한다.
5. 완전하고 non-partial인 실제 경로를 요구한다.
6. 실제 Pawn capsule Scale 1.0으로 목적지에 설 수 있는지 검사한다.
7. 예약된 정확한 목적지와 겹치는 후보를 제거한다.
8. 경로 길이, Frontier/SpawnControl 선호 방향, 벽 근접, 아군 혼잡, 최근 목적지와 최근 경로 실패 cooldown을 점수화한다.
9. 상위 기본 50% 후보 안에서 점수 기반 가중 랜덤으로 하나를 선택한다.

셀 중심은 실제 MoveTo 좌표로 사용하지 않는다. 후보/투영/완전 경로/통과 후보 수와 최종 점수는 목적지 선택 시점의 verbose 로그에 남긴다.

## 예약 정책

`StrategicCellCapacity`와 `MaximumControllersPerApproachRoute`는 soft crowding reference다. 한도 도달은 후보 제거 또는 예약 거부 조건이 아니며, 기존 점수 감점만 적용한다.

실제로 독점되는 것은 선택된 NavMesh FVector다. 기본 설정은 반경 220cm, 수명 4초다. 두 예약 원의 반경 합보다 가까우면 충돌로 처리한다. Director의 기존 수명주기에서 도착, 실패, stuck, 전투 진입, 목표 초기화, 사망/UnPossess 시 해제한다. 원점 FVector도 유효한 예약 좌표로 처리하도록 반경 제공 여부를 예약 존재 플래그로 사용한다.

## 전투 EQS 구조

각 Movement Action은 `PositionQuery`와 다음 목적 중 하나를 선언한다.

- AttackPosition: Fixed, Strafe, MoveThenFire, Rush, FindSight, AttackPosition
- CombatCover: CombatCover, PreparationCover
- Hide: Hiding
- Retreat: KeepDistance, Retreat
- Investigate: Investigation

Combat Brain은 행동의 cheap intent만 먼저 평가하며 이 단계에서 위치를 샘플링하지 않는다. 선택된 Action/Target 조합에 대해 하나의 EQS만 실행한다. 결과는 최대 이동 거리, 최소 점수, Controller의 Nav projection, 완전 경로, 실제 capsule Scale 1.0 검사를 다시 통과해야 한다.

결과는 캐시되어 재실행 간격 전에는 유지된다. 새 결과는 `EQSMinimumImprovementToReplace`(기본 0.05) 이상 좋아야 기존 유효 위치를 교체한다. 이동 실패 시 캐시를 폐기하고 재요청한다. 기본 `bAllowLegacyCombatPositionFallback=false`이므로 Query 누락/실패가 Tactical Grid 전투 위치 또는 구 C++ 위치 계산으로 자동 전환되지 않는다.

공통 Context는 전역 Actor 검색 대신 Combat Brain의 snapshot을 사용한다.

- `UDMEnvQueryContext_FocusingTarget`
- `UDMEnvQueryContext_VisibleEnemies`
- `UDMEnvQueryContext_RememberedEnemyLocations`
- `UDMEnvQueryContext_LastKnownThreatLocation`
- `UDMEnvQueryContext_DamageOrigin`
- `UDMEnvQueryContext_FriendlyActors`

Brain은 query 이름, 성공 여부, 마지막 위치/점수/실패 이유를 debug property로 보존한다.

## BP에서 생성/지정할 EQS Asset

C++에서 `.uasset`을 생성하지 않았다. 다음 다섯 개를 만들고 각 Movement Action의 `Combat > Movement > EQS > Position Query`에 목적과 일치하게 지정해야 한다.

### `EQS_AttackPosition`

- Generator: Querier 주변 또는 포커싱 대상 주변의 Projected Points Grid/Donut
- Context: FocusingTarget, VisibleEnemies, RememberedEnemyLocations, FriendlyActors
- 필수 Test: Pathfinding(도달 가능, non-partial), `MaxMoveDistance`, 포커싱 대상 LOS
- 점수: 무기 선호 거리/유효 거리, 이동 거리, 현재 위치 보너스, 다수 적 노출, 아군 간격, 과도한 전진 감점
- named params: `MaxMoveDistance`, `PreferredDistance`, `WeaponEffectiveRange`, `CurrentLocationBonus`

### `EQS_CombatCover`

- Generator: Querier 주변 Projected Points 또는 cover 후보 generator
- Context: FocusingTarget, VisibleEnemies, RememberedEnemyLocations, FriendlyActors
- 필수 Test: 도달 가능 경로, 대상 방향 엄폐, 엄폐 후 사격 LOS 또는 peek 가능성
- 점수: 이동 거리/시간, 무기 거리, 다수 적 노출, 아군 간격, 현재 위치 보너스

### `EQS_Hide`

- Generator: Querier/피해 원점 주변 Projected Points
- Context: VisibleEnemies, RememberedEnemyLocations, DamageOrigin, FriendlyActors
- 필수 Test: 도달 가능 경로, 알려진 적 전체로부터 LOS 차단
- 점수: 도달 시간, 경로 노출, 피해 방향 차단, 막다른 곳 감점, 추가 퇴로
- 사격 LOS/가치는 필수 점수로 두지 않는다.

### `EQS_Retreat`

- Generator: Querier 주변 Donut/Projected Points
- Context: FocusingTarget, VisibleEnemies, RememberedEnemyLocations, FriendlyActors
- 필수 Test: 도달 가능 경로, 최대 이동 거리
- 점수: 주요 위협과 거리 증가, 전체 노출 감소, 아군/지원 방향, 경로 안전, 막다른 곳 회피

### `EQS_Investigate`

- Generator: LastKnownThreatLocation 주변 Donut/Projected Points
- Context: LastKnownThreatLocation, RememberedEnemyLocations, FriendlyActors
- 필수 Test: 도달 가능 경로, 마지막 위치 관찰 가능성
- 점수: 관찰 거리, 정면 접근 감점, 경로 노출, 조사 후 퇴로, 아군 간격

EQS 내부 점수만으로 capsule 통과성을 대체하지 않는다. 최종 결과는 C++ post-validation을 항상 거친다.

## Combat Move Director

`MoveToLocation` 호출은 DeathMatch 소스에서 Director 한 곳만 남겼다. PathMove와 Combat Tactics는 `SubmitMoveRequest`를 사용한다. 우선순위는 다음과 같다.

1. EmergencyHide 50
2. Retreat 40
3. Combat 30
4. Investigate 20
5. StrategicRoam 10

더 높은 우선순위가 낮은 요청을 교체한다. 같은 소유자의 같은 목적지는 현재 이동을 재사용한다. completion은 완료 시점의 request owner에 따라 Roam 예약/관측 처리 또는 Combat Action 처리로 한 번만 전달한다. Roam 정리 경로가 `CancelAll`로 전투 이동까지 중단하던 중복 호출도 제거했다.

## WallClearance

Controller는 먼저 Scale 1.0 capsule path sweep을 실행하며 실패 시에만 목적지를 거부하고 cooldown을 기록한다. Roam/Combat 추가 clearance scale은 별도 sweep으로 실행하되 실패해도 verbose 품질 로그만 남긴다.

경로점은 capsule center 배열로 정규화한다. 첫 점이 Pawn actor/capsule center 높이면 그대로 사용하고, NavMesh 지면 점에는 half height를 한 번만 더한다. 보간 후 half height를 다시 더하지 않는다. 실패 결과에는 Actor, Component, 위치, 반경, 반높이, scale, segment, passability/quality 구분을 담는다.

## 설정 및 deprecated 영향

추가/정리한 주요 설정:

- Grid: `CellSize=1200`
- Roam: candidate count/retry, 최소 거리, 실제 목적지 예약 반경/수명, 벽/혼잡/최근 위치/최근 실패/경로 길이 감점, 상위 선택 비율
- Combat Action: Query Asset, purpose, 최대 거리, 재실행/실패 재시도, 최소 점수, 현재 위치 보너스, 교체 최소 개선량
- Brain: 명시적 `bAllowLegacyCombatPositionFallback` (기본 false)

`TacticalGridSearchRadius`, `MinimumTacticalGoalDistance`, Combat Brain의 구 전투 Grid search/selection property는 직렬화 호환을 위해 deprecated로 남겼다. 구 C++ Attack/Cover 위치 샘플러도 명시적 호환 옵션용으로 남았지만 Tactical Grid context를 더 이상 받지 않는다. EQS 연결이 완료되고 BP 참조가 없음을 확인한 뒤 별도 삭제할 수 있다.

## 자동화 테스트 소스

- Tactical Grid가 noncombat 전략 영역 구조를 반환하고 combat 요청을 거부하는지
- Layer/Region이 Cell identity와 전략 결과에 보존되는지
- 전략 셀/접근로 capacity가 hard cap이 아닌지
- 분리된 세 번째 실제 목적지 예약은 허용하고 겹친 exact goal은 거부하는지
- Scale 1.0 실패는 reject, 추가 clearance 실패는 quality-only인지
- Combat/Retreat/Emergency 요청이 Roam보다 높은 우선순위를 갖는지
- 주요 Action 클래스가 올바른 EQS purpose contract를 선언하는지

NavMesh projection/partial path/blocked center/가중 랜덤의 월드 통합 동작과 실제 EQS asset test 결과는 Asset 작성 후 에디터 자동화 또는 PIE에서 확인해야 한다.

## 정적 확인 및 알려진 제한

- DeathMatch 전체에서 `MoveToLocation` 호출은 Director 한 곳, AI 이동용 `StopMovement` 호출도 Director 안으로 수렴한 것을 검색으로 확인했다. Debug Observer의 `FloatingMovement->StopMovementImmediately`는 AI path request와 무관하다.
- Combat 경로의 Tactical Grid Cell/Bounds context 사용처를 제거했다.
- 빌드/UHT를 실행하지 않았으므로 새 UCLASS Context와 EnvQuery callback signature의 실제 엔진 버전 컴파일 성공은 아직 확인되지 않았다.
- EQS 생성 후보 수, 필수 통과 수, 개별 score breakdown은 EQS asset의 Debug/Gameplay Debugger 설정에 의존한다. C++은 query 이름, 성공/실패, 최종 위치/점수와 post-validation 실패를 제공한다.
- Roam의 실제 NavMesh 생성 테스트는 Navigation이 활성화된 editor world가 필요해 이번 정적 테스트 범위에서는 실행하지 않았다.

## 사용자 확인 항목

1. 위 다섯 EQS asset을 작성하고 해당 Action 인스턴스에 지정한다.
2. 각 asset의 Run Mode가 Single Result이며 named param 철자가 일치하는지 확인한다.
3. 큰 셀(1200cm)이 맵의 층/벽/통로 구성을 과도하게 합치지 않는지 Grid debug로 확인한다.
4. 여러 AI가 같은 전략 셀의 서로 다른 실제 목적지를 선택하는지 확인한다.
5. 전투 진입 때 Roam 예약이 해제되고 Combat/Emergency 요청이 즉시 우선하는지 확인한다.
6. 좁지만 통과 가능한 통로에서 1.15 clearance 실패가 이동을 차단하지 않는지 확인한다.
7. Automation: TacticalGrid, Navigation.Policy, Combat.Architecture.Ownership, Combat.Movement.ActionSetSelection을 실행한다.

