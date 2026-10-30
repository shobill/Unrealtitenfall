# DeathMatch Navigation — Tactical Nav Region 전환 (2026-10-12)

## 경로 사전 검사 완화 (2026-10-14)

이동 가능 여부는 실제 NavAgent로 투영한 complete/non-partial Nav 경로가 기본 판정이다. 추가 검사는 실제 캡슐이 아닌 기본 Radius 0.70/HalfHeight 0.80의 loose passability probe를 실제 `UNavigationPath::PathPoints` 세그먼트마다 수행한다. 250cm보다 긴 세그먼트만 내부 분할하며 Nav 코너를 직선으로 건너지 않는다.

높이 변화가 5cm 이상인 세그먼트는 Character Movement `MaxStepHeight`만큼 probe를 올린다. WorldStatic이 probe와 접촉해도 시작 겹침과 가장자리 접촉은 quality 신호로만 남기고, 같은 고정 컴포넌트가 경로 중심까지 막을 때만 `StaticPassabilityBlocked`로 거부한다. Pawn은 동적 장애물로 기록할 수 있지만 목적지 실패나 cooldown 원인이 아니다. 최종 목적지는 실제 캡슐의 0.90 배율로 WorldStatic overlap을 별도 확인한다.

## 현행 전략 구조

`Recast NavMesh polygon/portal → UDMTacticalNavRegionSubsystem → Region event scores → Roam Provider → stored NavPoint + complete path → CombatMoveDirector`

- `UDMTacticalNavRegionSubsystem`은 기본 Recast NavMesh의 polygon과 portal 인접성을 읽어 전략 Region을 만든다.
- XY 사각 Cell, Cell Center 이동, Walkable Ratio 기반 전략 선택은 현행 경로에서 사용하지 않는다.
- Region 정적 데이터와 팀별 이벤트 런타임 데이터는 분리한다.
- Subsystem은 `MoveTo`를 호출하지 않는다.
- `UDMMoveGoalProvider_Roam`이 Region 내부의 저장 NavPoint 중 complete non-partial path가 있는 지점을 고른다.
- 실제 이동 요청은 `UDMCombatMoveDirector`만 호출한다.
- 전투 위치는 목적별 EQS가 정하며 Tactical Region은 Combat/Threat/Presence 같은 상황 정보만 제공한다.

## Region 생성과 연결

Recast polygon을 하나씩 Region으로 쓰지 않는다. 넓은 portal, 높이 연속성, 목표 크기와 최대 면적을 기준으로 인접 polygon을 flood fill하고, 작은 Region은 결합 가능한 가장 넓은 인접 portal 쪽으로 병합한다. 좁은 portal과 큰 높이 변화는 Region 경계로 남긴다.

Region은 polygon ref, bounds, 대표 위치, 분산된 NavPoint, 면적/직경/개방도, dead-end/narrow/isolated, 팀별 spawn 도달성과 비용을 가진다. Region 연결은 portal 폭, 입출구, 비용, clearance와 연결 유형을 가진다. 대표 위치는 디버그와 그래프 계산용이며 실제 MoveTo 목적지가 아니다.

기본 정적 검사는 Radius 34/HalfHeight 88의 0.9 배율 캡슐을 사용한다. NavMesh가 연결을 보증한 뒤 WorldStatic에 대한 명백한 방해만 제외하며, 매 로밍 요청마다 경로 전체를 캡슐 sweep하지 않는다.

## 이벤트와 런타임 실패

Combat, Threat, FriendlyPresence, EnemyPresence, Exploration, SpawnPressure 이벤트를 Region graph를 따라 거리·시간·연결 폭/비용으로 감쇠 전파한다. 반복 같은 이벤트는 짧은 시간 창에서 갱신하며 전체 Actor scan은 하지 않는다. Combat Brain, fire, damage, roam 경로가 사건 발생 시 직접 보고한다.

NavPoint 실패는 짧은 point cooldown이고, 연결 반복 실패는 임시 감점 후 8초 차단된다. 둘 다 영구 정적 실패로 저장하지 않는다.

## 팀 Spawn과 로밍

모든 `PlayerStartByTeam`을 팀별 multi-source 시작점으로 사용해 Dijkstra 도달성과 비용을 계산한다. 일반 PlayerStart는 존재하는 팀의 공용 시작점이다. 어느 spawn에서도 도달하지 않는 Region은 전략 후보에서 제외한다.

Roam은 Exploration/Combat/Threat/Presence/SpawnPressure, 실제 graph path cost, 최근 방문과 AI 성향을 해석해 Region을 고른다. Region 전체를 독점하지 않고 실제 NavPoint만 반경/수명으로 예약한다. Region 실패 또는 시스템 미준비 시 기존 `ADMMovePoint`는 명시적 최후 fallback이다.

## Legacy Grid

`UDMTacticalGridSubsystem`, `ADMTacticalAnalysisVolume`, Grid 설정/API는 BP 직렬화 호환과 별도 제거 작업을 위해 남겨 두었다. `dm.LegacyTacticalGridRuntime` 기본값 0이라 legacy square grid는 생성·갱신되지 않으며 Strategic Roam 또는 Combat 목적지를 공급하지 않는다. 기존 Cell 전용 주요 설정은 deprecated/AdvancedDisplay 처리했다.

## Debug

- `dm.TacticalNavRegions 1`: Region ID/경계/대표 위치
- `dm.TacticalNavRegions 2`: Region 연결과 narrow portal
- `dm.TacticalNavRegions 3`: `dm.TacticalNavRegionTeam` 팀의 이벤트 점수
- `dm.TacticalNavRegions 4`: 저장 NavPoint와 실패 cooldown
- `dm.LegacyTacticalGridRuntime 1`: 임시 호환 진단용 legacy grid 재활성화
