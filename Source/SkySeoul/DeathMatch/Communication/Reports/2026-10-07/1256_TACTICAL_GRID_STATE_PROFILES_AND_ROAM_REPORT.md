# DeathMatch 상태별 전술 그리드 평가 및 Roam 연결 구현 결과

- 작성일: 2026-10-07 12:56 KST
- 범위: `Source/SkySeoul/DeathMatch`
- 구분: 구현 결과 및 정적 검증

## 요청 및 결론

전술 셀 평가의 기준을 고정 `FDMTacticalPersonality`에서 기존 전투 상태별 가중치 프로필로 옮겼다.
CombatTactics의 `EDMThreatAwareness`와 SituationFramework의 비전투 Roam을 얇은
`EDMTacticalEvaluationState`/`FDMTacticalEvaluationContext`로 변환하며, 이 타입은 상태 전이를 소유하지 않는다.

상태 프로필이 허용 행동과 기본 점수 방향을 결정하고 Personality는 활성 가중치에 제한된 보정만 한다.
후보 셀마다 허용 Purpose utility를 비교해 Purpose를 기록한다. 비전투 Roam은 같은 평가기를 사용하되 셀 중심이 아닌
실제 NavMesh sample과 non-partial path를 검증해 목적지를 만들며, 실패 시 기존 MovePoint 로밍으로 폴백한다.

## 수정 및 추가 파일

- `Navigation/DMTacticalGridTypes.h`: 평가 상태/컨텍스트, 상태별 프로필, 확장 점수 및 예약 디버그 자료.
- `Navigation/DMTacticalGridSubsystem.h/.cpp`: 상태 프로필 평가, 제한된 성향 보정, 후보별 Purpose, 기여도와 예약 충돌 처리.
- `AI/Combat/DMCombatTacticsComponent.h/.cpp`: ThreatAwareness 어댑터, 상태 전환 재평가, 최소 유지 시간, 체력/피해 컨텍스트와 디버그 상태.
- `AI/Move/DMMoveTypes.h`: Tactical Roam 목표 메타데이터.
- `AI/Move/DMMoveGoalProvider_Roam.h/.cpp`: grid 우선 후보, 셀 내부 실제 Nav 위치 탐색, MovePoint 폴백.
- `AI/Move/DMCombatMoveDirector.h/.cpp`: Roam 예약 갱신과 도착/실패/전투 진입 정리.
- `AI/Framework/DMBehaviorModule_PathMove.cpp`: validation 및 즉시 path 요청 실패를 grid 실패 캐시에 전달.
- `AI/Framework/DMSituationFramework.cpp`: stuck 실패를 grid 실패 캐시에 전달.
- `Controller/DeathMatchRobotAIController.cpp`: Roam move 완료 결과를 Director에 전달.
- `Tests/DMTacticalGridTests.cpp`: 상태 프로필, 극단 성향 제한, 방어 보정, stale 탐색, 예약 충돌 테스트 소스.
- `Navigation/LOGIC_STATUS.md`, `AI/Move/LOGIC_STATUS.md`: 현재 책임과 폴백 흐름 갱신.
- 본 보고서.

## 상태별 가중치 구조와 기본값

공통 구조 `FDMTacticalScoreWeights`는 Formation, CombatOpportunity, Ambush, Exploration, Coverage,
SupportedAdvance, IsolationRisk, Overcrowding, TravelCost, KnownEnemyExposure와 선택 항목인 StaleVisit,
EnemyApproachProbability, LastKnownThreat, CurrentCellRetention을 가진다.

- Visible: `1.0 / 1.0 / 0.5 / 0.0 / 0.4 / 0.8 / 1.0 / 1.0 / 0.5 / 0.7`, Retention `0.2`.
- LostGrace: `1.0 / 0.8 / 0.4 / 0.1 / 0.7 / 0.3 / 1.0 / 1.0 / 0.7 / 0.8`, LastKnown `0.9`, Retention `0.8`.
- PendingSwitch: `1.0 / 0.7 / 0.3 / 0.0 / 0.6 / 0.1 / 1.0 / 1.0 / 1.2 / 0.9`, LastKnown `0.5`, Retention `1.25`.
- Investigating: `0.8 / 0.4 / 0.5 / 0.6 / 0.6 / 0.4 / 1.0 / 0.8 / 0.8 / 1.0`, Stale `0.2`, LastKnown `1.3`, Retention `0.25`.
- WatchingLastKnown: `0.9 / 0.2 / 0.6 / 0.3 / 1.0 / 0.0 / 1.0 / 0.9 / 1.0 / 1.2`, Stale `0.15`, LastKnown `1.1`, Retention `1.1`.
- DisengagedRoam: `0.6 / 0.0 / 0.2 / 1.0 / 0.8 / 0.2 / 0.7 / 1.2 / 0.6 / 0.2`, Stale `1.0`, EnemyApproach `0.6`, Retention `0.15`.

앞의 10개 값 순서는 위 공통 구조의 첫 10개 항목과 같다. 모든 값은 Analysis Volume의
`Tactical Grid Settings > State Profiles`에서 Blueprint로 조절할 수 있다.

## 성향 보정 공식과 제한

활성 가중치의 기본 보정은 `Applied = StateWeight × (1 + 0.35 × clamp(PersonalityAxis, -1, 1))`이다.
Formation, Exploration, Coverage, SupportedAdvance는 같은 방향의 축을 사용한다. CombatPreference는
CombatOpportunity에 같은 방향, Ambush에 반대 방향으로 적용한다. Aggression은 SupportedAdvance에 같은 방향,
IsolationRisk와 KnownEnemyExposure에 반대 방향으로 적용한다.

따라서 보정 범위는 상태 기본값의 65~135%다. 상태값이 0이면 결과도 0이므로 Visible의 Exploration,
Roam의 CombatOpportunity, WatchingLastKnown의 SupportedAdvance는 극단 성향으로 활성화되지 않는다.

체력/피해는 Personality 이후 별도 방어 보정으로 적용한다.

- SupportedAdvance는 `1 - 0.6 × clamp(HealthRisk + 0.5 × DamagePressure)`만큼 억제한다.
- IsolationRisk는 최대 `1 + 0.5 × HealthRisk`만큼 증가한다.
- KnownEnemyExposure는 `1 + HealthRisk + 0.5 × DamagePressure`만큼 증가한다.

Aggression이 높아도 이 방어 보정을 제거하지 못한다.

## Purpose 선택 방식

하나의 Purpose를 먼저 전체 후보에 강제하지 않는다. 각 셀의 가중 점수 항목을 계산한 뒤 현재 상태에서 허용되는
Purpose만 utility 비교한다.

- Visible: Attack, Join, Ambush.
- LostGrace: Watch, Attack, Ambush.
- PendingSwitch: Watch, Attack, Join.
- Investigating: Explore, Watch, Ambush.
- WatchingLastKnown: Watch, Ambush, 제한된 Explore.
- DisengagedRoam: Explore, Watch.

같은 상태에서는 현재 셀/선택 유지 점수와 Combat Brain의 `TacticalPurposeMinimumHoldTime` 기본 0.75초가 흔들림을
줄인다. ThreatAwareness가 바뀌면 이전 예약을 해제하고 즉시 새 프로필로 재평가하므로 이전 상태의 Purpose나
프로필을 이어 쓰지 않는다.

## Roam과 TacticalGrid 연결 흐름

1. Roam Provider가 grid Ready 여부를 확인한다.
2. `DisengagedRoam` 컨텍스트와 Combat Brain의 Personality로 최대 8개 기본 후보를 요청한다.
3. 각 후보 셀의 `ValidSamples`를 사용하고 Nav projection과 valid/non-partial path를 검사한다.
4. 250cm보다 가까운 샘플과 임시 blocked 목표를 제외한다.
5. Explore/Watch 독점 예약에 성공한 첫 후보로 `FDMMoveGoal`을 만든다.
6. Director가 watchdog 동안 예약을 갱신하고 도착 시 ObservationConfidence를 보고한 뒤 예약을 해제한다.

셀 중심은 직접 목적지로 사용하지 않는다. TacticalGrid가 AIController에 MoveTo를 호출하지 않으며 기존 PathMove가
실제 이동 요청을 계속 소유한다.

## 기존 MovePoint 폴백 흐름

grid 미준비, 후보 없음, 후보의 실제 위치 생성 실패, 임시 blocked 목표, 모든 후보 예약 충돌에는 기존
`GatherMovePoints → SelectMovePoint → weighted random` 흐름을 실행한다. 폴백 목표에는
`bUsedMovePointFallback`을 기록하고 Verbose 로그로 원인을 남긴다. MovePoint도 없으면 기존과 같이 invalid goal을
반환해 Framework의 안전 정지/재평가 흐름을 유지한다.

## 미확인 지역, 예약 및 실패 처리

Roam 점수는 낮은 ObservationConfidence, 마지막 방문/관측 후 30초 정규화 경과 시간, FriendlyActorCount,
FriendlyReservedGoalCount, FocusDirectionOccupancy, EnemyApproachProbability, 이동 비용, Narrow/DeadEnd와 graph
도달 가능성을 조합한다. 방문/관측 정보는 영구 완료 플래그가 아니라 시간 점수이므로 다시 탐색 후보가 된다.

Explore/Watch는 셀당 1명, 일반 전투 Purpose는 기존 `MaximumReservationsPerCell`을 유지한다. 기존 예약을 가진 Actor가
이미 찬 독점 셀로 바꾸려는 경우에도 capacity를 먼저 검사해 기존 예약을 보존한다. 도착, path 실패, goal validation
실패, stuck, 전투 진입, actor 해제와 만료에서 예약을 정리한다. 실패는 Actor/Team/Cell/Action/Purpose별 cooldown에
기록하며 다음 후보를 사용할 수 있다.

## 디버그 정보

- Combat Brain에 현재 Evaluation State, 상태 기본 프로필, 성향 적용 프로필, 선택 Purpose/Cell/Score/Reason과
  `GridNotReady`, `CurrentCellMappingFailed`, `NoCandidates`, `ReservationConflict` 상태를 노출했다.
- `dm.TacticalGrid 3` 예약 표시에 Evaluation State, Purpose와 총점을 추가했다.
- `dm.TacticalGrid 4`에서 상위 3개 가산/감점 사유와 State→Applied 주요 가중치를 추가 표시한다.
- Roam 로그는 grid 선택, 셀 내부 위치 생성 실패, 예약 충돌, 후보 소진과 MovePoint 폴백을 구분한다.

## 자동화 테스트 추가 및 수정

`DMTacticalGridTests.cpp`에 다음 검증 소스를 추가했다.

- Visible에서 극단 ExplorationPreference가 ExplorationWeight 0을 깨지 않고 전투 Purpose만 만드는지.
- Roam에서 극단 CombatPreference가 CombatOpportunityWeight 0과 Explore/Watch Purpose를 깨지 않는지.
- 낮은 confidence와 미방문 셀이 최근 관측 셀보다 우선하는지.
- LostGrace가 Explore Purpose를 선택하지 않고 exploration 배율을 낮게 유지하는지.
- PendingSwitch가 현재 셀 유지 점수를 적용하는지.
- Investigating이 last-known 셀에 점수를 주는지.
- WatchingLastKnown이 SupportedAdvance를 0으로 유지하고 direct Attack을 선택하지 않는지.
- 체력 저하와 피해 압력이 전진을 줄이고 노출 비용을 높이는지.
- Personality 보정이 65~135% 범위를 넘지 않는지.
- Explore 독점 예약, 해제, 예약 변경 충돌 시 이전 예약 보존.
- 기존 grid 미완성 local fallback과 AttackPosition/CombatCover 셀 내부 exact-position gate는 기존 테스트를 유지한다.

## BP에서 조절할 변수와 권장 기본값

- `DeathMatch Tactical Analysis Volume > Tactical Grid Settings > State Profiles`: 위 상태별 기본값.
- `DM Combat Brain > Combat|Tactical Personality`: 5개 축, 각 -1~1. 상태 프로필을 대체하지 않는다.
- `DM Combat Brain > Combat|Tactical Grid > MaximumTacticalCellSearchRadius`: 4.
- `MaximumTacticalCellCandidates`: 3.
- `TacticalGridReevaluationInterval`: 1.5초.
- `TacticalPurposeMinimumHoldTime`: 0.75초.
- `Roam Provider > Tactical Grid Search Radius`: 6.
- `Tactical Grid Candidate Count`: 8.
- `Minimum Tactical Goal Distance`: 250cm.
- grid runtime의 일반 예약 수 3명, 예약 수명 4초, 실패 cooldown 3초는 기존 기본값을 유지한다.

## 아직 구현하지 않은 한계

- Watch의 실제 셀 간 시야는 현재 coverage gap, last-known 거리 ring과 정적 cell 특성의 근사이며 정밀 LOS 감시망은 아니다.
- EnemyApproachProbability는 기존 spawn 방향과 최근 목격 기반의 단순 확률이며 미래 경로 예측 모델은 아니다.
- Roam 실제 위치는 유효 sample/path를 사용하지만 별도 전용 Roam Movement Action 클래스는 만들지 않았다.
- 기존 Combat Debug HUD lane에 전체 가중치 표를 추가하지 않았고 상세 값은 component property와 `dm.TacticalGrid 4`에서 확인한다.

## 추후 PIE에서 확인할 사항

- Visible 교전 중 미확인 셀로 이탈하지 않고 Attack/Join/Ambush 지역을 고르는지.
- LostGrace와 PendingSwitch가 장거리 전진 없이 현재/last-known 주변을 유지하는지.
- Investigating과 WatchingLastKnown의 실제 맵 LOS 품질 및 조사→감시→Disengaged 전환.
- 여러 Roam AI가 서로 다른 Explore/Watch 셀을 예약하고 도착 후 오래된 지역을 다시 선택하는지.
- grid 생성 전과 Nav/후보 실패 시 MovePoint 폴백이 실제로 계속 이동시키는지.
- 전투 진입, 사망, UnPossess, path 실패와 stuck에서 Roam 예약이 즉시 사라지는지.
- AttackPosition과 CombatCover가 선택 셀 안에서 기존 LOS/cover/range/path 검증으로 정확 위치를 계속 찾는지.
- `dm.TacticalGrid 3/4`의 화면 밀도와 큰 맵에서 후보 평가 비용.

## 검증 상태

- 변경 파일은 모두 `Source/SkySeoul/DeathMatch` 내부다.
- 변경 파일의 중괄호 균형: 확인 완료.
- 충돌 marker 및 구형 Personality-only `FindTacticalCandidates`/`ChoosePurpose` 호출: 없음.
- 상태별/예약/방어 보정 자동화 테스트 소스: 추가 및 정적 확인 완료.
- Editor C++ 빌드: 사용자 요청에 따라 실행하지 않음.
- Native 자동화 테스트 실행: Editor 빌드를 수행하지 않아 실행하지 않음.
- Blueprint/PIE: 사용자 요청에 따라 실행하지 않음.
