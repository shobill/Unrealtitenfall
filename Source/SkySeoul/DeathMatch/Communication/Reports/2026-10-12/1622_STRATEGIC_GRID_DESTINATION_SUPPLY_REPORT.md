# DeathMatch Strategic Grid 목적지 공급 및 이동 가능 영역 개선 결과

- 작성일: 2026-10-12 16:22 KST
- 범위: `Source/SkySeoul/DeathMatch`의 Tactical Grid, Roam Provider, 관련 테스트·현행 문서
- 구분: 구현 결과 및 정적 검토

## 요청 또는 증상

Strategic Roam에서 소수 AI만 이동하고 나머지가 멈추는 원인이 될 수 있는 셀 독점 예약, 동일 접근로 독점,
6-hop 탐색 제한, Frontier/SpawnControl 후보 유형 삭제와 로딩 단계 이동 불가 셀 포함을 개선했다. 목적지를 전혀
찾지 못했을 때의 새 상태, 무작위 이동, 근거리 순찰 fallback과 전투 위치 탐색 재설계는 추가하지 않았다.

## 결론

비전투 Strategic Roam은 현재 연결 영역의 로딩 단계 검증 전략 셀 전체를 한 번의 그래프 Dijkstra로 탐색한다.
기본 전략 셀 수용량은 2명, 같은 SpawnControl 접근로의 선호 한도는 3명이다. 셀 예약과 접근로 이용량은 설정 가능한
감점으로 점수에 반영되고, 셀은 수용량에 도달했을 때만 제거된다. 접근로는 빈 경로가 있으면 포화 경로 예약을 막되
모든 경로가 포화됐을 때 최소 혼잡 후보를 완화 허용하므로 혼잡만으로 전체 후보가 사라지지 않는다.

로딩 단계에서는 팀별 대표 PlayerStartByTeam을 NavMesh에 투영한 뒤 두 대표 스폰의 graph flood fill 결과를 셀에
기록한다. 기본 정책은 양 팀 대표 스폰에서 모두 도달 가능한 셀만 Strategic 후보로 사용한다. 제외된 셀은 삭제하지
않고 상태와 사유를 보존한다.

## 변경 파일과 클래스

- `Navigation/DMTacticalGridTypes.h`
  - 전략 도달 정책·제외 사유 enum, 셀 활성/팀별 도달 정보, 예약·혼잡 점수 진단값, 예약 정확 위치를 추가했다.
  - `StrategicCellCapacity=2`, `MaximumControllersPerApproachRoute=3`, 감점/도착 분산/tie-break 설정을 추가했다.
- `Navigation/DMTacticalGridSubsystem.h/.cpp`
  - 대표 스폰 선정·투영, 팀별 flood fill, 전략 셀 활성화, 전체 영역 후보 탐색, 정적 path cache와 동적 지표 분리,
    셀/접근로 감점과 완화 예약, 세부 로그·그리드 시각화를 구현했다.
- `AI/Move/DMMoveGoalProvider_Roam.h/.cpp`
  - 최종 후보 기본값을 16으로 늘리고 기존 SearchRadius를 deprecated 호환 값으로 남겼다.
  - 같은 셀의 예약 좌표와 Pawn capsule을 고려해 분리된 NavMesh 도착점을 선택한다.
- `Tests/DMTacticalGridTests.cpp`
  - 대표 스폰 도달성/제외 사유, 전체 영역 Roam, 두 명 셀 예약, 세 명 접근로, 포화 경로 완화, 후보 보존 테스트를
    추가·변경했다.
- `Navigation/LOGIC_STATUS.md`, `AI/Move/LOGIC_STATUS.md`, `plan/01_Architecture.md`
  - 현행 정책과 전투/비전투 탐색 경계를 갱신했다.
- 본 보고서.

## 목적지 선점 정책

- `ExploreFrontier`와 `SpawnControl`이 공유하는 `StrategicCellCapacity` 기본값은 2다.
- 같은 팀의 현재 전략 예약자 수만 세며 요청자 자신의 갱신 예약은 제외한다.
- 예약자 수 × `StrategicReservationPenaltyPerController` 기본 14를 점수에서 차감한다.
- 수용량 미만 셀은 계속 후보로 남고, 수용량 도달 셀은 정밀 평가 후보를 제한하기 전에 제외한다.
- 기존 일반 Combat/Explore/Watch 예약 수용량은 변경하지 않았다.
- 전역 `TeamSpawnControlOccupancyLimit` 하드 gate는 사용하지 않고 deprecated 표시했다. 셀/접근로 정책과 중복 적용하지
  않는다.
- 예약 생성, 동일 Actor 갱신, 만료, 도착/실패/stuck/전투 진입/사망/UnPossess 해제 API는 기존 흐름을 유지한다.

## 접근 경로 혼잡 정책

- `MaximumControllersPerApproachRoute` 기본값은 3이다.
- 이용자 수 × `ApproachRouteCongestionPenaltyPerController` 기본 10을 점수에서 차감한다.
- 한도 미만 경로가 하나라도 있으면 포화 경로를 최종 예약할 수 없고, 예약 가능한 후보가 먼저 최종 CandidateCount를
  채운다.
- 모든 SpawnControl 경로가 포화됐을 때만 `bApproachRouteCapacityRelaxed` 후보를 허용한다.
- Route ID는 기존 적 팀 + 8방향만 쓰지 않고 graph Region과 4-cell coarse corridor bucket을 포함한다. 맵 전체의
  서로 다른 실제 통로가 같은 방향이라는 이유만으로 하나로 합쳐지는 범위를 줄였다.

## 전체 그리드 탐색과 후보 순서

- `DisengagedRoam`일 때만 hop 반경을 무제한으로 두고 검증 connection graph의 도달 가능한 전체 영역을 Dijkstra로
  펼친다. Visible/Lost/Investigating/Watch 등 전투·근거리 호출은 기존 1~12 SearchRadius를 그대로 사용한다.
- 전체 셀에 대해 정적 활성/도달성, Frontier/SpawnControl 가치와 실제 graph 거리로 rough sort한다.
- 최종 후보 수의 4배, 최소 32·최대 128개까지만 상세 path 지표를 만들며 최종 기본 16개를 반환한다.
- full cell을 최종 제한 전에 제외하고 포화 접근로보다 즉시 예약 가능한 후보를 먼저 둔다.
- Frontier와 SpawnControl은 서로 삭제하지 않는다. `MinimumFrontierValueBeforeSpawnControl`은 기본 60점 목적 보너스를
  어느 유형에 줄지만 다른 유형은 차선 후보로 유지한다.
- Actor Unique ID와 Cell ID hash의 작은 안정 편차를 적용해 같은 프레임의 동점 목적지를 분산한다. 평가마다 무작위로
  흔들리는 값은 사용하지 않는다.

## 대표 스폰과 이동 가능 셀 판정

- 모든 `PlayerStartByTeam`을 default NavData agent radius/height를 사용해 NavMesh에 투영한다.
- 팀별 유효 스폰 목록은 기존 SpawnControl multi-source field에 유지한다.
- 대표 스폰은 팀별 유효 후보를 이름 순으로 정렬한 첫 항목이며, 가장 낮은 두 Team ID를 Team A/B로 사용한다.
- 대표 셀마다 connection graph flood fill을 한 번 수행해 `ReachableTeamMask`와 Team A/B bool을 기록한다.
- 기본 판정은 Nav projection 성공, valid non-partial/capsule-passable connection, 유효 Region, 두 대표 스폰 모두
  도달 가능이다.
- 설정 enum은 `ReachableFromBothTeams`(기본), `ReachableFromEitherTeam`, `ReachableFromRequestingTeam`을 제공한다.
- 셀은 `NoNavProjection`, `NoValidConnection`, `UnreachableFromTeamA/B`, `IsolatedRegion`,
  `InvalidSpawnReference` 사유 배열을 보존한다.

## 정적 경로와 동적 점수

정적 path cache key에서 `TeamRuntimeRevision`과 Requester를 제거했다. Start/Destination/Team/GridRevision으로 topology를
재사용하고 cache hit마다 관측, 점유, 예약, 실패, 위험·노출 지표를 초기화 후 다시 집계한다. 예약이나 관측 변화가
정적 연결 경로를 매번 폐기하지 않는다. Strategic 후보 평가는 기존 Provider 의사결정 호출 때만 실행되며 매 Tick마다
전체 Nav path query를 수행하지 않는다.

## 도착점 분산

Roam Provider는 같은 셀의 예약된 정확한 월드 위치를 조회한다. 기존 ValidSamples를 우선 사용하고 필요한 경우 셀
내부에서 8방향 분산 sample을 만든 뒤 Pawn capsule 지름 + `StrategicGoalAcceptanceRadius` 기본 150cm와
`StrategicArrivalSeparation` 기본 180cm 중 큰 간격을 적용한다. projection extent도 Pawn capsule을 사용하며 최종
non-partial Nav path를 다시 확인한다.

## 디버깅 변경

- 초기 완료 로그: 전체/활성/비활성 셀 수, 제외 사유별 수, 정책, Team A/B.
- 대표 스폰 로그: Team, Actor 이름, 투영된 Cell. 투영·셀 매핑 실패 이유.
- 후보 평가 로그: full-grid 여부, 전체 셀, 상세 평가/반환 후보 수, 요청 Region.
- 선택/충돌 로그: Cell ID, 점수, 셀 예약 인원/수용량·감점, Route ID, 이용 인원/한도·감점, 완화 여부,
  최종 예약 성공/실패.
- `dm.TacticalGrid`: Nav 실패/고립/Team A 전용/Team B 전용/비활성 색과 전략 활성 bool, 예약 인원과 route 혼잡을
  추가했다. 디버그 0에서는 기존처럼 draw하지 않는다.

## 자동화 테스트 소스

`DMTacticalGridTests.cpp`에 다음 회귀 기대를 반영했다.

- 양 팀 대표 스폰 도달 셀 활성, 한 팀만 도달하는 셀 기본 제외.
- Nav projection 실패와 연결 없는 고립 셀 제외, 대표 스폰 누락 사유.
- 요청 SearchRadius 밖의 먼 Frontier가 Strategic Roam 후보에 남음.
- Frontier 우선 상황에도 SpawnControl 차선 후보 유지.
- 부분 점유 셀의 감점과 후보 유지, 셀 2명 성공/3번째 제한, full top cell의 최종 후보 미점유.
- 같은 접근로 3명 성공, 4번째 일반 제한, 모든 경로 포화 시 완화 예약 성공.
- 포화 경로만 남아도 SpawnControl 후보가 사라지지 않고 완화 표시됨.
- 기존 weighted Dijkstra, 경로 위험, 관측, 상태별 프로필과 Combat 지역 탐색 테스트는 유지했다.

## BP에서 확인할 사항

- 맵의 `DeathMatch Tactical Analysis Volume > Tactical Grid Settings > Strategic`에서 다음을 확인한다.
  - `StrategicCellCapacity = 2`
  - `MaximumControllersPerApproachRoute = 3`
  - `StrategicReservationPenaltyPerController = 14`
  - `ApproachRouteCongestionPenaltyPerController = 10`
  - `StrategicReachabilityPolicy = ReachableFromBothTeams`
  - `StrategicArrivalSeparation = 180`
  - `StrategicGoalAcceptanceRadius = 150` (Roam PathMove의 실제 ArrivalAcceptanceRadius와 같은 값으로 유지)
- 기존 BP가 과거 `MaximumControllersPerApproachRoute=1`을 직렬화했다면 C++ 새 기본값 3을 자동 상속하지 않을 수
  있으므로 해당 인스턴스 값을 3으로 변경해야 한다.
- Roam Provider의 기존 `TacticalGridCandidateCount=8` 직렬화 값은 런타임에서 최소 16으로 보정하지만 Details에서도
  16 이상인지 확인한다.
- 최소 두 팀의 `PlayerStartByTeam` TeamId와 NavMesh projection이 유효한지 초기 로그로 확인한다.
- 팀 전용 안전 구역 또는 일방통행 의도가 있다면 BothTeams 정책 대신 Either/RequestingTeam을 맵 단위로 검토한다.

## 남아 있는 위험 요소

- 대표 팀은 가장 낮은 두 Team ID를 Team A/B로 해석한다. 두 팀보다 많은 모드에서는 명시적인 팀 역할 설정이 후속
  확장 후보이다.
- ApproachRouteId는 정밀 Recast corridor cluster가 아니라 Region/방향/coarse cell bucket 기반이다. 복잡한 순환형
  맵은 PIE 시 route grouping을 확인해야 한다.
- NavData agent 크기는 default NavData config를 사용한다. 서로 다른 capsule/NavAgent profile의 AI가 같은 맵에
  공존하면 profile별 grid bake는 별도 작업이다.
- 도착점 분산은 셀의 사용 가능한 Nav 면적이 좁으면 두 번째 유효 점을 만들지 못할 수 있다. 이 경우 Provider는 해당
  후보 실패를 기록하고 다음 후보를 시도한다.
- 목적지가 완전히 없는 경우 기존 MovePoint fallback만 남아 있다. 신규 실패 행동 설계에서는 연속 실패 횟수,
  비활성 셀 사유, 마지막 유효 Region, 셀/route 포화도를 입력으로 사용하되 이번 전략 공급 정책과 중복 완화하지
  않아야 한다.

## 검증 상태

- 작업 전 `.cursor/rules`의 DeathMatch 규칙, 활성 plan/LOGIC_STATUS, Communication 운영 규칙과 2026-10-06~12
  Tactical Grid 관련 보고서를 확인했다.
- 변경 범위는 `Source/SkySeoul/DeathMatch` 내부다. Blueprint와 Unreal asset은 수정하지 않았다.
- 변경 C++ 6개 파일의 중괄호 수 일치: 확인 완료.
- 구형 1명/8후보 기본값, Frontier/SpawnControl 유형 삭제 코드, RuntimeRevision path cache key, conflict marker 정적
  검색: 활성 코드에서 없음.
- 요구 기능별 자동화 테스트 소스: 추가 및 정적 검토 완료.
- 최초 Editor C++ 빌드에서 `GatherAnalysisBounds()` 내부의 `NavData` 로컬 재선언이 C4456으로 검출되었다.
  이미 null 검증한 외부 `NavData`를 그대로 재사용하도록 수정했다.
- `SkySeoulEditor Win64 Development` 재빌드: 성공 (`Result: Succeeded`).
- Unreal 자동화 테스트 실행: 요청 범위에 따라 실행하지 않음.
- Blueprint/PIE: 요청 범위에 따라 실행하지 않음.
- 컴파일 성공은 확인했다. 실제 맵 대표 스폰 선택, Route grouping과 다수 AI 이동 결과는 후속 자동화/Editor/PIE
  확인이 필요하다.
