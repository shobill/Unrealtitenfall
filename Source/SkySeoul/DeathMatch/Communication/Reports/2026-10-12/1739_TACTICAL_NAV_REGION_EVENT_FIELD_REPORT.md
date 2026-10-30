# Tactical Nav Region / Event Field 전환 구현 결과

- 작성일: 2026-10-12 17:39 KST
- 범위: DeathMatch Navigation, Strategic Roam, Combat Brain 이벤트 보고, Combat Move Director, 테스트와 현행 설계 문서
- 구분: 구현 결과

## 요청

사각형 Tactical Grid를 전략 판단의 주 시스템에서 제외하고, 실제 Recast NavMesh polygon/portal 연결을 분석한 Tactical Nav Region Graph와 팀별 Event Field로 전환한다. 비전투 Roam만 Region이 전략 영역을 공급하고, 실제 로밍 목적지는 Region 내부 NavPoint에서 선택한다. 전투의 실제 위치는 목적별 EQS, 최종 이동 요청은 Combat Move Director가 계속 소유한다.

## 결론

현행 경로를 다음과 같이 전환했다.

`Recast polygon/portal → UDMTacticalNavRegionSubsystem → Region event scores → Roam Provider → stored NavPoint + complete path → CombatMoveDirector`

- Strategic Roam은 더 이상 `UDMTacticalGridSubsystem`에 후보를 요청하지 않는다.
- Legacy Grid runtime은 `dm.LegacyTacticalGridRuntime=0` 기본값에서 build/update하지 않는다.
- Combat fire/damage/threat/presence/observation은 새 Subsystem에 이벤트로 보고한다.
- Combat Context는 현재 Region의 객관적 runtime data를 조회하지만, Region이 전투 목적지 FVector를 제공하지 않는다.
- 목적별 EQS와 Combat Move Director의 기존 소유권은 유지했다.
- UHT 및 `SkySeoulEditor Win64 Development` 전체 C++ 빌드는 성공했다.
- BP/EQS Asset과 PIE는 수정·실행하지 않았다. 신규 자동화 테스트 소스는 컴파일됐지만 실행하지 않았다.

## 새 클래스와 파일

- `Navigation/DMTacticalNavRegionTypes.h`
  - Region/build/event/failure enum
  - `FDMTacticalNavRegionStaticData`, `FDMTacticalNavRegionConnection`, `FDMTacticalNavPoint`
  - `FDMTacticalRegionEvent`, `FDMTacticalNavRegionRuntimeData`
  - 전략 요청/후보와 exact NavPoint reservation
- `Navigation/DMTacticalNavRegionSubsystem.h/.cpp`
  - Recast 분석, Region graph, spawn 도달성, 이벤트 전파, 전략 후보, 예약/실패, debug
- `Navigation/DMTacticalNavRegionVolume.h/.cpp`
  - 선택적 맵 분석 Bounds, Region 생성 설정, 기본 Event Profile, debug 기본값
- `Navigation/DMTacticalEventProfile.h/.cpp`
  - 이벤트 종류별 강도/거리/시간 감쇠/누적 상한 Data Asset 타입
- `Tests/DMTacticalNavRegionTests.cpp`
  - 이벤트 거리·시간 감쇠, 연결 경로, narrow 감쇠, 팀 분리, point cooldown 회귀 소스

연결 변경:

- `AI/Move/DMMoveGoalProvider_Roam.h/.cpp`
- `AI/Move/DMMoveTypes.h`
- `AI/Move/DMCombatMoveDirector.cpp`
- `AI/Combat/DMCombatTacticsComponent.cpp`
- `AI/Combat/DMCombatDamageAwareness.cpp`
- `AI/Combat/DMCombatEQS.cpp`
- `AI/Combat/DMCombatModules.h`
- `Navigation/DMTacticalGridTypes.h`
- `Navigation/DMTacticalGridSubsystem.h/.cpp`
- `Navigation/DMTacticalAnalysisVolume.h`
- `plan/01_Architecture.md`, `Navigation/LOGIC_STATUS.md`, `AI/Move/LOGIC_STATUS.md`

## Region 생성 알고리즘

기본 `ARecastNavMesh`의 모든 유효 tile에서 polygon ref, center, vertex, 면적, bounds와 portal neighbor를 읽는다. 선택적 `ADMTacticalNavRegionVolume`이 있으면 그 Bounds 안 polygon만 사용하고, 없으면 실제 NavMesh Bounds를 사용한다. 전체 NavMesh 원본을 별도 persistent 형식으로 복제하지 않는다.

polygon 병합 순서:

1. 안정적인 poly ref 순서로 seed를 잡는다.
2. 직접 portal로 연결된 이웃만 flood fill한다.
3. portal 폭이 `NarrowPortalWidth`보다 작거나 높이 차가 `RegionHeightBoundary`보다 크면 Region 경계로 남긴다.
4. 누적 면적이 `MaximumRegionArea`를 넘거나 seed에서 `TargetRegionSize`보다 멀면 별도 Region으로 분할한다.
5. `MinimumRegionArea`보다 작은 Region은 최대 면적을 넘지 않는 인접 Region 중 가장 넓은 portal 쪽으로 병합한다.
6. 최종 ID를 연속 정수로 다시 부여한다.

거리만 가까운 polygon, 벽 반대편, 동일 XY의 다른 층을 합치지 않는다. 사각 Cell, X/Y index, Walkable Ratio는 새 알고리즘에 없다.

## NavMesh 캐시와 정적 데이터

저장 정적 데이터:

- Region ID, bounds, 대표 위치
- 포함 Nav poly ref
- 실제 NavMesh 위의 분산 NavPoint
- 면적, 근사 직경, 평균 openness, narrow/dead-end/isolated
- 인접 Region, travel cost, portal 입출구/폭/clearance/type/passability
- 팀별 spawn 도달 여부와 기본 path cost
- navigation revision

정적 Region과 팀별 runtime event score는 다른 컨테이너다. 이벤트 갱신은 graph를 재구성하지 않는다. `OnNavigationGenerationFinishedDelegate`에서 기본 NavData 갱신을 감지하면 정적 cache를 무효화하고 rebuild한다. poly ref는 현재 revision 안에서만 사용한다.

## Region 내부 NavPoint

각 Region의 polygon center를 실제 Nav 후보로 수집하고 wall clearance가 높은 순서에서 공간 간격을 둬 기본 10개까지 보존한다. 각 점은 location, poly ref, Region ID, wall clearance, openness, 최근 실패 시간/횟수를 가진다. 좁은 Region은 후보가 적어도 유지한다.

대표 위치는 graph/debug용이며 MoveTo 목적지가 아니다. Roam Provider는 저장 NavPoint마다 현재 Pawn에서 동기 Nav path를 만들고 valid/non-partial complete path만 허용한다. 경로 길이, openness, 최근 방문, exact reservation과 failure cooldown을 점수화하고 상위 비율 안에서 가중 선택한다.

## 작은 검사 캡슐

정적 생성 검사 기본 기준은 현재 캐릭터 기준 Radius 34cm/Half Height 88cm에 각각 0.9를 적용한 Radius 30.6cm/Half Height 79.2cm다. 캡슐 중심은 NavMesh 지면 좌표에 half height와 2cm 여유를 더한다.

이 검사는 NavMesh polygon/portal 연결을 1차 근거로 삼은 뒤 명백한 `WorldStatic` 겹침만 제거한다. Pawn/문/mover 같은 일시적인 WorldDynamic을 영구 실패로 기록하지 않는다. Roam 요청마다 경로 전체에 실제 캡슐 sweep을 수행하지 않는다. 실제 이동 실패는 point cooldown과 connection failure API가 처리한다.

## Region 연결

서로 다른 Region의 Recast portal을 집계하고 가장 넓은 실제 portal을 대표 연결로 저장한다. 연결 비용은 Region 대표 위치 사이 거리, 입출구는 portal midpoint 양쪽, clearance는 portal 반폭과 wall clearance의 작은 값이다. 좁은 portal과 높이 변화는 connection type에 기록한다.

NavPoint 실패는 2초부터 반복 횟수에 따라 최대 12초까지 임시 cooldown한다. 연결 실패는 누적 score 3부터 8초 임시 차단하고 이후 감쇠·복구한다. 현재 Move completion에는 정확히 통과한 edge 정보가 없으므로 stuck은 point failure에 직접 연결되고 connection failure는 API 사용 경로만 열려 있다.

## 팀별 Spawn 도달 가능성

모든 `APlayerStartByTeam`을 소속 Region에 투영해 팀별 multi-source Dijkstra를 실행한다. 일반 `APlayerStart`는 발견된 팀의 공용 source로 포함한다. 명시 팀 start가 전혀 없으면 현재 DeathMatch 기본 두 팀 0/1을 사용한다.

각 Region은 팀별 도달 여부와 누적 connection travel cost를 저장한다. 어느 spawn에서도 도달하지 못한 Region은 isolated 전략 후보가 된다. 팀 전용 Region은 해당 팀 요청에만 남고 공용 Region은 여러 팀 reachability를 가진다.

## Tactical Event

지원 종류:

- `Combat`
- `Threat`
- `FriendlyPresence`
- `EnemyPresence`
- `Exploration`
- `SpawnPressure`

발생 위치:

- 실제 성공 사격: Combat Brain `NotifyShotFired`
- 실제 피해 수신: `NotifyDamageReceived`에서 Combat와 피해 origin Threat
- visible/remembered 적 정보: Combat Brain threat snapshot 갱신 경로
- 아군/적 점유와 교전 여부: 등록 Actor의 event-driven presence 갱신
- 방문/관측: Roam 선택/도착과 Combat current Region
- SpawnPressure: 상대 팀 spawn graph cost에서 runtime 기본값 계산

같은 source/type/team/origin이 0.3초 안에 반복되면 새 이벤트를 무한 누적하지 않고 기존 강도/시간을 갱신한다. 전체 Actor를 매 Tick 검색하지 않는다.

## 이벤트 전파와 감쇠

이벤트 발생 시 origin부터 설정 hop까지 Region graph를 순회한다.

`Strength = BaseStrength × DistanceFalloff^Hop × (1 - Age/Lifetime)^TimeDecay × ConnectionModifier`

ConnectionModifier는 portal 폭, travel cost와 임시 failure/block을 반영한다. `MinimumPropagationScore` 아래는 전파하지 않고 Lifetime이 끝난 이벤트는 제거한다. 기본 event field refresh는 0.25초이며 처리 이벤트 수를 제한한다. Combat은 객관적 사건으로 현재 알려진 팀 모두에 적용하고, Threat/Presence/Exploration/SpawnPressure는 source team 관점으로 분리한다. 점수는 안전 상한 10으로 clamp한다.

## Strategic Roam 연결

`UDMMoveGoalProvider_Roam`은 `FindReachableStrategicRegions`에 현재 위치, 팀, Tactical Personality, 최근 방문 Region을 전달한다. 후보 점수는 다음 객관값을 AI 성향으로 해석한다.

- Exploration, Combat, Threat
- Friendly/Enemy Presence
- SpawnPressure
- 실제 Region graph path cost
- narrow/dead-end
- 최근 방문과 같은 Region의 exact-point 예약 수

Region 선택 뒤 저장 NavPoint에 대한 complete path를 마지막으로 확인한다. Region 전체나 접근로를 hard reserve하지 않고 실제 점만 기본 220cm/4초로 예약한다. 전투 진입, 도착, 실패, stuck과 lifecycle reset에서 Director가 예약을 정리한다. 새 시스템이 준비되지 않았거나 모든 후보가 실패한 경우에만 `ADMMovePoint`를 최후 fallback으로 사용한다.

## Combat Brain과 EQS

Combat Brain은 현재 Region에 actor presence, 관측과 threat event를 보고한다. `FDMCombatContext`에는 현재 Region ID와 `FDMTacticalNavRegionRuntimeData`를 조회해 넣으므로 Action/전략 profile이 객관 event field를 해석할 수 있다. 현재 변경은 기존 Movement Action priority와 사격 정책을 자동 변경하지 않았다.

전투의 실제 FVector는 계속 Action의 `PositionQuery`가 만든다. Region representative/NavPoint/Grid는 EQS 실패 fallback이 아니다. EQS 결과는 기존 C++ post-validation과 Combat Move Director를 통과해야 한다.

## 기존 Tactical Grid에서 제거한 책임

- Strategic Roam 후보 공급
- 로밍 실제 목적지 후보 생성
- Combat fire/damage/threat/presence 집계
- Combat EQS FriendlyActors 공급
- 런타임 기본 build/update

`UDMTacticalGridSubsystem`과 `ADMTacticalAnalysisVolume`은 저장 BP/직렬화 호환과 별도 asset reference audit를 위해 삭제하지 않았다. Display name/deprecation message를 추가했고 runtime은 기본 꺼져 있다. `DMCombatMoveDirector`의 legacy `bUsesTacticalGrid` 정리 분기는 이미 직렬화된 구 goal 호환용으로만 남았다.

## Deprecated 설정/API

다음 Grid 전용 필드를 `DeprecatedProperty`와 `AdvancedDisplay`로 이동했다.

- CellSize, SamplesPerAxis, MinimumWalkableRatio, LayerHeightThreshold
- MaximumNeighborDistanceScale, MaximumGridColumns
- MinimumRoamCellHops, PreferredRoamCellHopsMin/Max
- UnknownRegionSizeWeight, HopDistancePreferenceWeight
- StrategicCellCapacity, StrategicReservationPenaltyPerController
- MaximumControllersPerApproachRoute, ApproachRouteCongestionPenaltyPerController

Roam Provider의 기존 `TacticalGridSearchRadius`, `MinimumTacticalGoalDistance`도 compatibility field로 남는다. 신규 로밍 설정은 DestinationCandidateCount, MinimumGoalDistance, GoalReservationRadius, RecentVisitMemorySeconds, 경로/벽/최근 위치 감점과 top selection ratio다.

## 최종 Editor 설정

맵 구조/Region 생성:

- 선택적으로 맵에 `DeathMatch Tactical Nav Region Volume` 배치
- `TargetRegionSize=1200`
- `MinimumRegionArea=250000`, `MaximumRegionArea=2250000`
- capsule scale 0.9/0.9
- `RegionSamplePointCount=10`
- 고급: `NarrowPortalWidth=140`, `RegionHeightBoundary=180`

Event Profile은 필요할 때 다음 이름의 `DMTacticalEventProfile` Data Asset을 만드는 것을 권장한다.

- `DA_DM_TacticalEvent_Combat`
- `DA_DM_TacticalEvent_Threat`
- `DA_DM_TacticalEvent_Exploration`
- `DA_DM_TacticalEvent_Presence`
- `DA_DM_TacticalEvent_SpawnPressure`

Volume의 `EventProfiles` map에 type별로 연결한다. Asset이 없어도 C++ 기본값을 사용한다.

## EQS Asset 연결

이번 변경에서 `.uasset`은 만들거나 수정하지 않았다. 직전 EQS 전환 보고서와 동일하게 다음 목적별 asset을 만들고 `BP_DM_Brain_Combat`의 각 Movement Action에서 `Combat > Movement > EQS > Position Query`에 연결해야 한다.

- `EQS_AttackPosition`
- `EQS_CombatCover`
- `EQS_Hide`
- `EQS_Retreat`
- `EQS_Investigate`

각 Action의 `EQSQueryPurpose`와 asset 목적이 일치해야 한다. 공통 named param은 `MaxMoveDistance`, `PreferredDistance`, `WeaponEffectiveRange`, `CurrentLocationBonus`다. 프로젝트에서 확인된 기존 EQS 파일은 Robot/개발 테스트용 이름뿐이며 위 DeathMatch 전용 5개 asset 존재는 확인되지 않았다.

## Debug

- `dm.TacticalNavRegions 1`: Region 경계/ID/대표 위치
- `dm.TacticalNavRegions 2`: 전체 연결선과 narrow 연결
- `dm.TacticalNavRegions 3`: 팀별 Combat/Threat/Friendly/Enemy/Exploration/SpawnPressure
- `dm.TacticalNavRegions 4`: 저장 NavPoint와 실패 cooldown
- `dm.TacticalNavRegionTeam <id>`: level 3 표시 팀
- `dm.LegacyTacticalGridRuntime 1`: 문제 비교 때만 legacy grid 임시 재활성화

## 자동화 테스트 소스

`DMTacticalNavRegionTests.cpp`가 다음을 확인한다.

- origin 최대 강도, hop 감쇠, 시간 감쇠, lifetime 제거
- connection modifier/narrow edge 추가 감쇠
- 명시 연결만 사용하는 Region path와 누적 비용
- team-scoped Threat score
- runtime NavPoint failure cooldown

테스트 소스는 Editor target 전체 빌드에서 컴파일됐다. Unreal Automation 실행은 사용자 지시에 따라 생략했다.

## 성능상 주의점

- 가장 큰 비용은 초기 Recast polygon 열거, polygon graph partition, Region 연결과 spawn Dijkstra다.
- runtime은 등록 presence, 활성 event와 제한 hop graph만 갱신하며 월드 전체 Actor scan을 하지 않는다.
- 모든 Region 쌍 거리표, LOS table, NavMesh 원본 복제 또는 disk bake를 만들지 않는다.
- 현재 build state는 단계 이름을 가지지만 한 번의 초기 build tick 안에서 전체 단계를 수행한다. `MaximumBuildTimePerTick`/`MaximumRegionOperationsPerTick`은 노출돼 있으나 이번 1차 버전에서 세부 cursor budget으로 분할되지 않았다. 큰 맵 profiling 후 실제 incremental cursor로 나누는 후속 작업이 필요하다.

## 알려진 한계

- Recast가 아닌 NavigationData는 분석하지 않는다.
- Region sample은 polygon center 기반이며, vertex 내부 random/Poisson 보강은 아직 없다.
- off-mesh link의 정밀 단방향/점프/낙하 유형 판정은 아직 없다.
- connection failure API는 있으나 final path completion에서 어느 edge가 실패했는지 추적하지 않아 자동 edge 귀속은 아직 없다.
- Event Profile의 `MaximumAccumulatedStrength`는 타입에 존재하지만 현재 runtime 공통 score cap 10이 최종 상한이며 profile별 상한 적용은 후속 대상이다.
- Combat Context에 Region score를 제공하지만 기존 Action별 해석 가중치를 자동 추가하지 않았다. 기존 전투 우선순위/밸런스를 보존한 의도적인 1차 연결이다.
- BP Asset Registry 직렬화 전수 audit와 기존 `DMTacticalGrid` C++ 파일 최종 삭제는 수행하지 않았다.

## BP/PIE에서 확인할 사항

1. 실제 맵에서 Region이 벽/층/좁은 portal 기준으로 분리되는지 `dm.TacticalNavRegions 2`로 확인한다.
2. 팀별 모든 `PlayerStartByTeam.TeamId`가 올바르고 공용/팀 전용 Region이 의도대로인지 확인한다.
3. 저장 NavPoint가 바닥/벽/좁은 문에서 유효하며 Region center로 이동하지 않는지 확인한다.
4. 사격/피격/적 관측 때 level 3 점수가 거리와 시간에 따라 감소하는지 확인한다.
5. 여러 AI가 같은 Region을 선택해도 exact destination이 분산되는지 확인한다.
6. Combat 진입 시 Roam이 중단되고 목적별 EQS만 실제 전투 위치를 공급하는지 확인한다.
7. `BP_DM_Brain_Combat`의 Movement Action 5종에 목적별 EQS asset이 실제로 연결됐는지 확인한다.
8. runtime NavMesh 재생성 뒤 revision과 Region graph가 rebuild되는지 확인한다.

## 다음 정리 작업에서 삭제 가능한 기존 코드

BP/Asset Registry에서 class/property import 0을 확인한 뒤 별도 변경으로 다음을 삭제할 수 있다.

- `DMTacticalGridSubsystem.h/.cpp`
- `DMTacticalAnalysisVolume.h/.cpp`
- Grid 전용 Cell/Connection/SpawnControl/Frontier/Reservation/Score 타입
- `DMTacticalGridTests.cpp`
- `FDMMoveGoal`의 `bUsesTacticalGrid`, `TacticalCell`, `TacticalPurpose`, `TacticalPath`
- Combat Brain의 deprecated Tactical Grid debug/property block
- Combat Move Director의 legacy Grid reservation/completion 분기
- `dm.TacticalGrid*`, `dm.LegacyTacticalGridRuntime`

## 검증 상태

- Communication 규칙 및 최신 Tactical Grid/EQS/Move ownership 보고서 확인: 완료
- C++/Config Grid 참조 검색: 완료
- 신규 UHT: 성공
- `SkySeoulEditor Win64 Development` 전체 C++ 빌드: 성공
- 신규 테스트 소스 컴파일: 성공
- Unreal Automation 실행: 미실행
- BP/EQS Asset 수정: 하지 않음
- BP load/PIE: 미실행, 사용자 확인 필요
- Git 상태: workspace 상위 Git metadata가 현재 checkout 경로와 일치하지 않아 `git status/diff`를 사용할 수 없었음
