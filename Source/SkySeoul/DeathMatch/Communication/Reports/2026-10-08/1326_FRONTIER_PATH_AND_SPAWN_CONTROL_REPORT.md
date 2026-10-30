# DeathMatch Frontier 탐색·전술 경로·Spawn Control 구현 결과

- 작성일: 2026-10-08 13:26 KST
- 범위: `Source/SkySeoul/DeathMatch`
- 구분: 구현 결과 및 정적 검증

## 요청 또는 증상

비전투 Roam이 미확인 지역으로 확장하지 않고 현재 위치와 아군 주변을 반복했다. 기존 후보 탐색은 연결 그래프를
최초 방문 BFS로 순회해 가중 TravelCost를 최단경로로 반영하지 않았고, 현재 셀과 1-hop 셀도 일반 후보였다.
낮은 ObservationConfidence 자체를 목적지 점수로 사용했기 때문에 확인 영역의 안전한 경계와 Unknown 내부를 구분하지
않았으며, 목적지까지 통과하는 narrow/contested/occupied/failed 셀도 최종 점수에 충분히 반영하지 않았다.
적 spawn 평가는 PlayerStart 평균 방향에 가까웠고 실제 그리드 접근로와 전선 진행도를 갖지 않았다.

## 결론

기존 책임과 상태 전이를 유지하면서 TacticalGrid의 지역 선택 기능을 확장했다. TacticalGrid는 연결 그래프 관측,
Frontier, PlayerStartByTeam 기반 팀별 거리 필드, SpawnControl 정적 점수와 가중 경로 비용으로 셀을 선택한다.
MoveGoalProvider_Roam은 선택된 셀 안의 ValidSamples에서 실제 non-partial NavMesh 경로가 있는 정확한 위치를 선택한다.
새로운 AI 상태 머신은 만들지 않았고 전투 인지가 생기면 기존 CombatTactics가 Roam 목표와 예약을 정리한다.

## 수정 및 추가 파일

- `Navigation/DMTacticalGridTypes.h`: 탐색 상태, 정적 연결/셀 정보, 팀 spawn 필드, SpawnControl, 경로 결과,
  전략 BP 설정, ExploreFrontier/SpawnControl Purpose.
- `Navigation/DMTacticalGridSubsystem.h/.cpp`: 정적 그래프 보강, 저빈도 N-hop 관측, Frontier/Unknown component,
  multi-source Dijkstra, SpawnControl 평가, 가중 전술 경로와 캐시, Roam 점수/예약/디버그.
- `AI/Move/DMMoveTypes.h`: Roam 목표에 전술 경로와 접근 방향 메타데이터 저장.
- `AI/Move/DMMoveGoalProvider_Roam.h/.cpp`: 최근 Roam 이력, 방향 유지, Frontier/SpawnControl 셀 내부 위치 선택.
- `AI/Move/DMCombatMoveDirector.cpp`: 유효한 현재 Roam 목표 유지, 접근로 예약 갱신 및 종료 정리.
- `Tests/DMTacticalGridTests.cpp`: 관측, Frontier, Dijkstra, spawn 필드, control, 예약과 fallback 테스트 소스.
- `Navigation/LOGIC_STATUS.md`, `AI/Move/LOGIC_STATUS.md`, `plan/01_Architecture.md`: 현행 책임과 흐름 갱신.
- 본 보고서.

## 정적 이동 그래프

셀은 `bTraversable`, Layer/Region, team reachability, ConnectionCount, EscapeRouteCount, dead-end/junction,
narrow type, Openness, AverageWallClearance, ValidNavSampleCount를 가진다. 연결은 valid Nav path, partial 여부,
ActualPathLength, Type, MinimumClearance, HeightDelta, Agent Capsule 통과 여부를 가진다.

연결 생성은 실제 NavMesh path가 없거나 partial이면 거부한다. path/direct 비율 제한과 중간 capsule clearance도
통과해야 한다. 정적 capsule·wall 검사는 `WorldStatic`만 사용하므로 문, mover, pawn 같은 동적 장애물을 영구
차단하지 않는다. 런타임 path/clearance 실패는 기존 Actor/Team/Cell/Action/Purpose 실패 캐시에 기록한다.

## 탐색 상태와 N-hop 관측

상태는 서로 배타적인 enum 하나가 아니라 동시 표현 가능한 bool이다.

- Unknown: 방문·관측 시각이 없고 confidence가 threshold 이하.
- Frontier: 이동 가능하고 확인/covered/stale/occupied 상태이며 연결된 Unknown 이웃이 있음.
- RecentlyObserved: 마지막 관측이 기본 6초 이내.
- Known: 관측 이력이 있고 recent/stale가 아닌 상태.
- Stale: 마지막 관측이 기본 30초보다 오래됨.
- Occupied: 현재 아군 Actor가 셀에 있음.
- Covered: 아군의 연결 그래프 관측 반경 안이지만 현재 셀은 아님.
- InferredCovered: 직접 covered/occupied가 아니지만 모든 연결 이웃이 covered/occupied.
- Contested: visible/remembered 적, damage/fire event 또는 combat intensity가 있음.

등록된 아군 셀에서 연결 그래프 BFS를 수행한다. 기본 0/1/2-hop confidence는 1.0/0.8/0.5이며 연결되지 않은
옆방, 다른 Region과 층은 관측되지 않는다. `ObservationRefreshInterval` 기본 0.5초로만 갱신하고 전체 셀 LOS는
실행하지 않는다. Unknown connected component 크기는 Frontier 점수에 사용한다.

## 전술 경로 분석

후보 요청마다 연결별 실제 TravelCost를 사용하는 Dijkstra를 한 번 실행한다. 최초 방문 BFS를 사용하지 않는다.
Roam은 목적 점수의 상위 `MaximumCandidates × 3`(8~48 제한) 후보만 선행 노드로 경로를 복원해 상세 지표를 만든다.

경로 결과는 reachable, PathCells, hop, 실제 거리, 예상 시간, narrow/dead-end/unknown/frontier/contested,
friendly occupied/reserved/recently failed 수, 평균·최대 위험, 근사 노출과 TotalPathCost를 가진다. 최종 점수는
목적지 점수에서 실제 거리, hop, narrow, contested, congestion, failed route, risk/exposure 비용을 차감한다.
partial/invalid/capsule 불가 연결 또는 팀 도달 불가 셀은 경로에 들어가지 않는다.

캐시 키는 Requester, start, destination, team, GridRevision, TeamRuntimeRevision이다. 최대 256개를 유지하며 grid
재구축, 관측, 예약, 실패와 동적 집계 revision 변경 뒤에는 이전 key가 재사용되지 않는다. 경로 계산은 Roam 목표
선택/전술 재평가 때만 실행하고 매 tick 전체 경로를 계산하지 않는다.

## Frontier, 최근 방문과 반복 억제

일반 Roam은 현재 셀과 1-hop 셀을 제외한다. 기본 최소 2-hop, 선호 3~6-hop이다. 유효한 Frontier/SpawnControl이
없을 때만 가까운 grid 셀을 허용하고, 그것도 실패하면 기존 MovePoint를 사용한다.

AI별 최근 5개 Roam 셀과 선택 시각을 Provider가 저장한다. 기본 감점은 최근 순서에 따라 100/60/30/15/8이며
30초 동안 감소한다. 선택 방향은 기본 5초 유지하며 반대 방향 후보를 감점한다. Director는 현재 목표가 유효하면
일반 Decision refresh로 교체하지 않는다. 도착, 실패, path 차단, 전투 진입 또는 명시적 목표 정리 뒤에만 새 목표를
고른다. DisengagedRoam의 CurrentCellRetentionWeight는 0이다.

## 팀별 Spawn Cell, 거리 필드와 FrontProgress

빌드 단계에서 `PlayerStartByTeam`을 TeamId별로 수집해 가장 가까운 유효 셀에 투영하고 중복 없는 Spawn Cell Set을
만든다. 각 팀의 모든 spawn 셀을 source로 build budget 기반 multi-source Dijkstra를 수행해 Hops, PathDistance,
NextDirectionTowardSpawn을 모든 도달 셀에 저장한다.

각 팀 관점 FrontProgress는 `FriendlyDistance / (FriendlyDistance + nearest EnemyDistance)`다. 0은 아군 후방,
0.5는 중앙, 1은 적 spawn 방향이다. 팀 spawn이 없으면 spawn 필드와 SpawnControl을 만들지 않고 일반 Frontier와
기존 fallback을 사용한다. 기존 SpawnCenters는 런타임 호환 자료로 남지만 SpawnControl은 월드 평균 방향을 쓰지 않는다.

## SpawnControl 정적 평가와 Spawn Exclusion

적 spawn 자체가 아니라 실제 그래프 접근로의 셀을 평가한다. 가산은 junction, narrow connection 기반 choke,
EscapeRouteCount, 접근 방향과 맞는 openness, 적 spawn 4~8-hop 선호 구간, 약 0.7 FrontProgress, 아군과 적 spawn
사이 최단 주 접근로 여부다. dead-end, narrow cell 자체와 퇴로 부족은 감점/제외한다.

적 spawn 기본 3-hop 이내는 `bInsideSpawnExclusionArea`이며 후보가 아니다. 후보는 아군 spawn에서도 도달 가능하고
퇴로가 2개 이상이어야 한다. 각 결과는 score, candidate, ControlledApproachDirection, enemy team/hops/distance와
ApproachRouteId를 저장한다. 따라서 적 spawn 셀을 직접 목적지로 사용하지 않는다.

## ExploreFrontier와 SpawnControl 선택

DisengagedRoam 후보는 명시적인 `ExploreFrontier` 또는 `SpawnControl` Purpose다. Frontier 값은 Unknown 이웃 수,
Unknown region 크기, 방문/관측 경과, 선호 hop, 경로 위험, 아군 밀집/예약, 최근 이력을 포함한다.

최고 Frontier 값이 `MinimumFrontierValueBeforeSpawnControl` 기본 18 이상이면 SpawnControl 후보를 제거해 탐색을
우선한다. 그보다 낮고 유효한 SpawnControl이 있으면 control을 선택한다. 둘 다 없거나 모든 예약/실제 위치/path가
실패하면 가까운 grid fallback, 마지막으로 기존 weighted MovePoint fallback을 사용한다. Personality는 기존 65~135%
제한 보정만 하며 Roam CombatOpportunityWeight 0을 활성화하지 않는다.

## 예약과 실제 위치

ExploreFrontier와 SpawnControl은 셀당 1명이다. SpawnControl은 팀 전체 기본 3명, 같은 ApproachRouteId 기본 1명으로
제한한다. 예약 충돌은 다음 후보를 시도하며 정지하지 않는다. 도착, validation/path 실패, stuck, 전투 진입, 사망,
UnPossess와 만료에서 기존 Director/Grid 정리 흐름을 사용한다.

Frontier 실제 위치는 선택 셀의 ValidSamples 중 Unknown 이웃 방향을 우선한다. SpawnControl은 fake enemy Actor를
만들지 않고 ControlledApproachDirection 반대편 sample을 우선해 접근 방향을 바라볼 공간을 확보한다. 두 목적 모두
Nav projection과 Pawn 기준 valid/non-partial path를 다시 검사한다. 실패는 캐시에 기록하고 다음 후보로 넘어간다.

## 성능 제한

- 정적 spawn 필드는 팀별로 기존 connection-check/time build budget을 사용한다.
- SpawnControl cell 평가는 sample/time budget으로 분할한다.
- 관측과 파생 상태는 기본 0.5초 저빈도 갱신이다.
- 전체 셀 LOS와 매 tick path search는 없다.
- 후보는 단일 Dijkstra 뒤 상위 8~48개만 상세 경로 지표를 만든다.
- 경로 결과는 revision key로 최대 256개 캐시한다.
- 동적 장애물은 정적 그래프를 영구 변경하지 않고 짧은 실패 cooldown을 사용한다.

## BP 설정과 권장 기본값

`DeathMatch Tactical Analysis Volume > Tactical Grid Settings`에서 다음을 조절한다.

- Observation: RadiusHops 2, ConfidenceAtSelf 1.0, ConfidencePerHop 0.2, graph observation true,
  refresh 0.5초, unknown threshold 0.1, recent 6초, stale 30초.
- Roam: Minimum 2-hop, preferred 3~6-hop, history 5, recent penalty 100, decay 30초, direction hold 5초.
- Purpose: Frontier/UnknownRegion/Hop preference weight 1.0, minimum frontier before control 18,
  Explore/SpawnControl purpose weight 1.0, team controllers 3, route controllers 1.
- Path: estimated speed 450cm/s, hop .5, narrow 4, contested 7, congestion 3, failed 8, risk 10.
- SpawnControl: exclusion 3-hop, preferred 4~8-hop, junction 18, choke 12, escape 6, openness 8,
  front progress 15, dead-end penalty 40, narrow penalty 20.
- DisengagedRoam profile: Formation .2, Combat 0, Ambush .15, Exploration 1.3, Coverage .6,
  Advance .2, Isolation .6, Overcrowding 1.4, Travel .2, Exposure .2, Stale 1.2,
  EnemyApproach .5, Retention 0.

## 디버그 사용법

- `dm.TacticalGrid 3`: Unknown/Frontier/Recent/Known/Stale/Covered/Inferred/Contested 색과 예약.
- `dm.TacticalGrid 4`: 상태/성향 가중치, 상위 가산·감점과 선택 이유.
- `dm.TacticalGrid 5`: 팀 spawn hops/distance, FrontProgress, exclusion, SpawnControl score/route,
  선택 경로 화살표와 hop/distance/time/narrow/frontier/contested/risk.
- `dm.TacticalGridTeam <id>`: 표시 팀.
- `dm.TacticalGridActor <UniqueID>`: 예약/경로 Actor 필터, `-1`은 전체.

## 자동화 테스트 추가 내역

`SkySeoul.DeathMatch.TacticalGrid.StrategicRoam` 소스에 다음을 추가했다.

- 현재/1-hop 일반 후보 제외와 전략 후보 부재 시 가까운 셀 fallback.
- graph 0/1/2-hop 관측 1.0/0.8/0.5, disconnected region 미관측.
- Known/Unknown 경계 Frontier, disconnected Unknown의 non-Frontier.
- 최근 셀 감점과 다른 미방문 Frontier 우선.
- 비싼 직결보다 싼 2-edge를 고르는 Dijkstra와 실제 거리.
- contested 경유 위험 증가, disconnected와 partial-only path 거부.
- PlayerStartByTeam 자동 spawn-cell set.
- 팀별 hops/path distance와 FrontProgress.
- spawn exclusion, preferred junction 후보, dead-end/퇴로 부족 제외.
- 높은 Frontier의 탐색 우선과 낮은 탐색 가치의 SpawnControl 전환.
- 동일 SpawnControl 접근로 중복 예약 거부와 정리 API.

기존 Core/Runtime 테스트의 상태 프로필, Personality 0-weight 보존, 방어 보정, 실패 범위, 독점 예약과 전투 셀
정확 위치 gate 소스도 유지했다.

## 아직 남은 한계

- SpawnControl 내부 위치는 정적 sample/접근 방향/경로를 사용한다. 실제 적이 없는 상태에서 CombatCover의 적 Actor
  기반 LOS/사격 trace를 호출하지 않으므로 PIE에서 엄폐·감시 품질을 확인해야 한다.
- NavMesh 자체가 runtime dynamic modifier를 bake한 상태라면 정적 path 결과에도 그 영향이 남을 수 있다.
- `MinimumClearance`는 현재 통과한 Agent capsule 지름 기준의 보수적 값이며 corridor 전 구간의 정밀 폭 최소값은 아니다.
- RouteId는 enemy team과 spawn 방향 bucket 기반 근사다. 복잡한 순환형 맵에서는 접근로 clustering을 추가할 수 있다.
- 테스트 소스는 추가했지만 이번 요청 범위에 따라 Editor build와 자동화 실행을 수행하지 않았다.

## PIE에서 확인할 사항

- 여러 AI가 현재 주변을 반복하지 않고 서로 다른 Frontier로 2-hop 이상 확장하는지.
- 벽 너머 방과 다른 층이 연결 없이는 Covered가 되지 않는지.
- 높은 Frontier가 남아 있는 동안 SpawnControl로 조기 전환하지 않는지.
- 탐색이 줄어든 뒤 적 spawn 4~8-hop의 접근 교차로를 분산 점유하는지.
- 적 spawn 3-hop 이내와 spawn 자체로 돌진하지 않는지.
- SpawnControl sample에서 실제 접근 방향, 엄폐, 사격 공간과 퇴로가 자연스러운지.
- 도착/실패/stuck/전투/사망/UnPossess에서 셀·접근로 예약이 사라지는지.
- grid 준비 전, 후보/예약/Nav 위치 실패 시 MovePoint 로밍이 계속되는지.
- `dm.TacticalGrid 3/4/5`의 화면 밀도와 큰 맵 build/후보 선택 프레임 비용.

## 검증 상태

- 저장소와 DeathMatch 내부에서 `AGENTS.md`는 발견되지 않았다. 대신 `Communication/00_README.md`, 확정 plan 문서와
  2026-10-07 최신 TacticalGrid 보고서를 작업 전 확인했다.
- 변경 범위: `Source/SkySeoul/DeathMatch` 내부만 수정.
- 변경 C++ 파일 중괄호 수 일치: 확인 완료.
- conflict marker 및 구형 Roam Explore/Watch 테스트 기대값: 정리 완료.
- 요구 기능별 자동화 테스트 소스: 추가 및 정적 확인 완료.
- Editor C++ 빌드/UHT: 작업 지시에 따라 수행하지 않음.
- Unreal 자동화 테스트 실행: Editor 빌드를 수행하지 않아 미실행.
- Blueprint/PIE: 작업 지시에 따라 수행하지 않음.
- 따라서 컴파일 성공과 실제 맵 행동 성공은 확정하지 않으며 후속 Editor/PIE 확인이 필요하다.
