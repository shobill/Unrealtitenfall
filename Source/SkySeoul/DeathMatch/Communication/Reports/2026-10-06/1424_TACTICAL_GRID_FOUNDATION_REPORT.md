# DeathMatch NavMesh 전술 그리드 및 성향 기반 지역 선택 기본 구조 구현 결과

- 작성일: 2026-10-06 14:24 KST
- 범위: `Source/SkySeoul/DeathMatch`
- 구분: 구현 결과 및 C++ 빌드 검증

## 요청 및 결론

월드당 하나의 `UDMTacticalGridSubsystem`이 NavMesh 분석, 정적 셀/연결 그래프, 팀별 동적 전장 정보, 진형 Snapshot, 셀 예약, 실패 쿨다운과 디버그 Snapshot을 소유하도록 1차 기반을 구현했다. Combat Brain은 전술 셀과 Purpose만 선택하며, 기존 Movement Action 우선순위를 변경하지 않는다. `Attack Position`과 `Combat Cover`는 선택 셀을 검색 범위 힌트로 받아 기존 LOS·엄폐·사거리·위험·점유·실제 경로 검증으로 정확한 좌표를 다시 선택한다.

그리드는 NavData와 분석 Bounds가 준비된 뒤 `WaitingForNavigation → GeneratingSamples → BuildingCells → BuildingConnections → MarkingReachability → CalculatingStaticMetrics → Ready` 순서로 프레임 분할 생성된다. `Ready` 이전, 빌드 실패, 현재 셀 매핑 실패, 후보 부재에는 Tactical Context를 제공하지 않으므로 기존 로컬 이동 판단이 계속 작동한다.

## 변경 파일

신규:

- `Navigation/DMTacticalGridTypes.h`
- `Navigation/DMTacticalAnalysisVolume.h/.cpp`
- `Navigation/DMTacticalGridSubsystem.h/.cpp`
- `Tests/DMTacticalGridTests.cpp`
- 본 보고서

연결 및 보강:

- `AI/Combat/DMCombatModules.h`
- `AI/Combat/DMCombatTacticsComponent.h/.cpp`
- `AI/Combat/DMCombatThreatMemory.cpp`
- `AI/Combat/DMCombatDamageAwareness.cpp`
- `AI/Combat/DMCombatAttackPosition.cpp`
- `AI/Combat/DMCombatCoverModule.cpp`
- `AI/Combat/DMCombatAttackPositionTests.cpp`
- `AI/Combat/DMCombatDebugTests.cpp`
- `AI/Combat/DMCoverFallbackTests.cpp`

마지막 세 테스트 파일은 기존 포인터 비교의 파생/기반 타입 불일치 때문에 전체 C++ 빌드를 막던 구문을 명시적 기반 포인터 비교로 정리했다. 테스트 의미는 변경하지 않았다.

## 추가 클래스와 구조체

- `UDMTacticalGridSubsystem`: `UTickableWorldSubsystem` 기반 단일 월드 관리자.
- `ADMTacticalAnalysisVolume`: 선택적 최우선 분석 Bounds와 맵별 설정 소유자.
- `FDMTacticalCellId`: `X/Y/Layer/Region`, 동등성 및 `GetTypeHash` 제공.
- `FDMTacticalGridSettings`: Build/Sampling/Runtime/Observation 설정 묶음.
- `FDMTacticalCellStaticData`: Bounds, 대표 위치, 유형, 샘플, 연결, 개방도, 높이, dead-end/junction, Spawn 접근성.
- `FDMTacticalCellDynamicData`: 아군/교전/예약/물리 적/알려진 위협/전투 사건/관측/감시 방향 집계.
- `FDMTacticalPersonality`: Formation, Combat, Exploration, Aggression, Coverage의 독립 `-1..1` 축.
- `FDMTacticalCellScore`: 공통 점수 항목별 결과와 선택 사유.
- `FDMTacticalFormationSnapshot`, `FDMTacticalCellReservation`, 연결/Purpose/실패 enum.

## 기본 Cell Size와 샘플링

- Cell Size: 600cm.
- Samples Per Axis: 3, 즉 높이 probe별 3×3 XY 샘플.
- Minimum Walkable Ratio: 0.35.
- Layer Height Threshold: 250cm.
- 프레임당 최대 Nav 투영 샘플: 96.
- 프레임당 최대 연결/도달성 처리: 24.
- soft build budget: 2ms/tick.
- Cell Size는 300~1200cm, Samples Per Axis는 2~5로 clamp한다. 400cm 미만 또는 4 초과 샘플은 non-Shipping 경고를 낸다.

맵별 값은 `DeathMatch Tactical Analysis Volume > DeathMatch|Tactical Grid`에서 변경한다. 해당 Volume이 없으면 subsystem 기본값을 사용한다.

## 분석 범위와 이동 불가능 셀 판정

분석 Bounds 우선순위는 다음과 같다.

1. 활성 `ADMTacticalAnalysisVolume`
2. 모든 `ANavMeshBoundsVolume`의 개별 Bounds
3. 기본 `ANavigationData` 실제 Bounds

복수 Bounds는 하나의 큰 사각형 내부를 무조건 채우지 않고 각 Bounds와 겹치는 XY column만 task로 만든다. Bounds 사이의 배경 간격은 분석 column에서 제외한다.

NavData 또는 유효 Bounds가 없으면 실패로 고정하지 않고 `WaitingForNavigation`에서 0.5초 저빈도로 재시도한다. Column 안전 상한은 기본 20,000개다.

각 샘플은 기본 NavData 투영 성공, 설정된 agent capsule의 WorldStatic/WorldDynamic overlap 여유, 8방향 wall probe를 검사한다. 동일 XY sample에서 같은 높이로 중복 투영된 결과는 제거한다. `WalkableRatio < MinimumWalkableRatio`이거나 유효 standing sample이 없으면 그래프용 `Blocked`로 처리한다. 일반 분류 함수의 기준은 `<0.20 Blocked`, `<0.50 Narrow`, `<0.80 Partial`, 그 이상은 개방도 0.72를 기준으로 Normal/Open이다.

대표 위치는 사각형 중심이 아니라 해당 Layer의 유효 Nav 위치 평균에 가장 가까운 실제 샘플이다.

## Layer와 Region 분리

분석 Bounds 높이를 제한된 수의 vertical seed로 나눠 각 XY sample을 여러 높이에서 투영한다. 유효 결과의 Z 차이가 `LayerHeightThreshold`를 넘으면 같은 XY에서도 별도 Layer가 된다. Layer 사이 연결은 거리만으로 만들지 않고 실제 비부분 Nav 경로 검증을 통과해야 한다.

Region은 모든 연결 검증 후 유효 셀 그래프의 connected component flood fill로 부여한다. 따라서 같은 XY라도 높이 Layer 또는 실제 연결 component가 다르면 다른 Cell ID다. 현재 구현은 Recast 내부 poly region 번호를 그대로 저장하지 않고, 검증된 전술 연결 그래프의 안정적인 런타임 Region 번호를 사용한다.

## 셀 연결 검증

후보는 주변 8개 XY와 같은 XY의 다른 Layer로 제한한다. 양쪽 비 Blocked 셀에 대해 다음을 검사한다.

- 동기 Nav 경로 존재 및 유효성
- partial path 아님
- 경로 길이 / 직선거리 기본 2.5 이하
- 대표 위치 거리 기본 Cell Size × 2.25 이하
- 중간 지점 agent capsule clearance

연결에는 이웃 Cell ID, 실제 Nav 경로 길이 Travel Cost, 최소 clearance와 Connection Type을 저장한다. 현재 타입 판정은 Normal/Narrow/Stair/Ramp까지의 보수적 휴리스틱이며 Drop/OneWay 정밀 판정은 구조만 열어 두었다.

## Spawn 기반 접근성

모든 `APlayerStart`를 시작점으로 셀에 투영하고 검증 연결 그래프를 분할 flood fill한다. `APlayerStartByTeam`은 해당 Team bit를 기록하고, 일반 PlayerStart는 모든 팀 접근 가능으로 처리한다. 어떤 Spawn에서도 도달하지 못한 셀은 `bReachableFromAnySpawn=false`가 되어 기본 후보 검색에서 제외된다. Spawn이 하나도 없으면 초기 동적 맵 호환을 위해 모든 유효 셀을 임시 접근 가능으로 둔다. 동적 Spawn 변경 후에는 `RequestRebuild()`로 다시 판정할 수 있다.

## 분할 빌드와 Ready 시점

샘플 투영, 셀 조립, 연결 경로 검사, 도달성 flood fill, 정적 metric 계산을 상태별 cursor와 프레임 예산으로 나눴다. 마지막 static metric cursor가 끝나는 Tick에 `Ready`가 된다. `Ready` 로그에는 셀 수, Cell Size와 sample 수가 출력된다.

그 전에는 Combat Brain이 `bHasSelectedTacticalCell=false`인 Context를 사용하므로 기존 AI가 계속 움직인다. 그리드 생성 실패가 Movement Action을 정지시키는 새 gate는 없다.

## 정적 셀 정보

저장 항목은 Cell ID, Bounds, 실제 Nav 대표 위치, Cell Type, Walkable Ratio, 유효 sample, 연결, 평균 wall clearance, Openness, Height, 평균 이동 비용, 상대 높이, 주요 개방 방향, 좁은 연결 수, Dead End/Junction, Spawn 접근 여부와 Team mask다. 모든 셀 조합 거리표, 전체 LOS 표와 Macro Region은 만들지 않았다.

## 팀별 동적 정보와 Actor 등록

Combat Brain component가 가진 Pawn을 이벤트성/0.25초 저빈도 검증으로 등록하고 셀 경계 이동, Team, 교전 여부와 focus 방향을 갱신한다. 사망, UnPossess와 EndPlay에서는 등록 및 셀 예약을 제거한다. 매 Tick 월드 전체 Actor 순회는 하지 않는다. 빌드 시 Bounds/Spawn 검색만 제한적으로 Actor iterator를 사용한다.

집계 항목은 Friendly Actor/Combatant/Reserved Goal, Physical Enemy, Known/Visible/Remembered/Engagement Threat, 최근 Damage/Fire 사건, Combat Intensity, Observation Confidence와 시각, 평균 Focus 방향 및 각도 bucket이다.

## Physical Enemy와 Known Threat 구분

`PhysicalEnemyCount`는 subsystem에 등록된 다른 팀 Actor의 실제 셀 위치로 만든 서버 통계/디버그 값이다. Combat Brain의 지역 점수에는 직접 사용하지 않는다.

`KnownThreatCount`는 Combat Brain의 `FDMCombatThreatSnapshot`에서 전달된 고유 Actor map으로 집계한다. 여러 아군이 같은 적을 보고해도 Team별 threat map의 같은 Actor key 하나로 합쳐진다. Visible/Remembered/Engagement를 분리하며, 기본 8초 뒤 만료한다. 즉 AI 판단용 적 수는 실제 월드의 숨은 적 전체가 아니라 Perception/Threat Memory로 알려진 위협이다.

## Combat Intensity

현재 기본식은 다음 요소를 합한다.

- Friendly Combatant Count × 1
- Visible Threat Count × 2
- 최근 Damage Event × 최대 1.5
- 최근 Fire Event × 최대 1

Damage/Fire 기여는 5초 동안 선형 감쇠하며 이후 제거한다. 사건 개수 필드는 디버그용 현재 memory 개수이고, 실제 intensity에는 감쇠된 가중치를 사용한다.

## Observation Confidence

0~1 값이며 기본 초당 0.04 감소한다. 아군이 셀을 점유/방문하면 최소 0.75, 현재 셀을 전투 Brain이 관측 보고하면 0.8 또는 visible 전투 시 1.0, visible 위협을 확인한 셀은 최소 0.65로 올린다. 시간 경과 감쇠와 마지막 방문/관측/적 목격 시간을 분리했다. 맵 전체 LOS 감시망과 시야 차단 기반 정밀 감쇠는 아직 구현하지 않았다.

## 아군 진형 Snapshot

단순 평균과 함께 좌표별 median을 구하고, 기본 2400cm 밖의 outlier를 제외한 dense set 평균을 Robust Formation Center로 사용한다. Formation Radius, unique friendly cell 수, 요청자 기준 최근접 아군 거리, local density, isolated 수와 combat cell 목록을 제공한다. 기본 cache 주기는 0.35초이며 actor/dynamic refresh 시 무효화한다.

## 성향 축과 셀 점수

Combat Brain의 `Combat|Tactical Personality`에서 다음 독립 축을 조절한다.

- Formation Preference: Solo(-1) ↔ Join(+1)
- Combat Preference: Ambush(-1) ↔ Direct Combat(+1)
- Exploration Preference: Known Front(-1) ↔ Unconfirmed(+1)
- Aggression: Hold(-1) ↔ Advance(+1)
- Coverage Preference: Reinforce(-1) ↔ Unwatched Coverage(+1)

후보는 현재 셀에서 기본 4 graph hop 안만 보고 상위 3개만 남긴다. 점수는 Formation, Combat Opportunity, Ambush, Exploration, Coverage, Supported Advance를 더하고 Isolation, Overcrowding, Travel, Known Enemy Exposure와 active failure cooldown을 뺀다. 각 항목과 Reason을 `FDMTacticalCellScore`에 보존한다. Purpose는 가장 강한 축에 따라 Explore/Ambush/Join/Watch/Attack으로 정한다.

Join은 진형/교전 아군 근접을 보상하고 혼잡/예약/Narrow를 감점한다. Solo는 진형과 어느 정도 떨어진 셀을 보상하지만 5칸을 넘는 고립 비용을 받는다. Direct Combat은 알려진/visible 위협과 combat intensity를, Ambush는 junction과 중간 개방도를, Explore는 낮은 Observation Confidence와 junction을, Coverage는 비어 있는 focus bucket을 선호한다.

완전한 적 시야 우회 경로, 무기별 셀 간 사격 가능성, 정교한 flank angle은 아직 아니다.

## 적 Spawn 정보 추상화

정확한 enemy spawn 좌표 map은 subsystem 내부 build 자료로만 유지한다. Team runtime에는 여러 적 팀 Spawn 중심을 향한 `ExpectedEnemyDirection`과 초기 `EnemyApproachProbability`만 제공한다. 실제 최근 적 목격이 있으면 확률 기본값을 0.75로 올리고, 없으면 0.25로 둔다. 정교한 이동 확률 갱신과 미래 경로 예측은 TODO 범위다.

## 전술 셀 예약

기존 `DMCombatPositionRegistry`는 정확한 좌표 점유를 계속 담당한다. 새 예약은 어느 지역을 맡는지만 나타내며 Team/Actor/Cell/Purpose/Expiration을 저장한다. 기본 4초, 일반 셀 기본 3명, Explore/Watch는 1명으로 제한한다. 같은 Actor의 새 예약은 기존 지역 예약을 갱신한다. 선택 변경, 실패, Reset, 사망/UnPossess, EndPlay와 만료 때 해제한다.

## Combat Brain 연결

그리드가 Ready이고 현재 위치가 접근 가능한 셀로 매핑되면 Combat Brain이 후보를 요청한다. 전투 시작, 타깃 변경, 이동 실패 및 기본 1.5초 저빈도 주기에서 재평가한다. 선택 결과로 Cell ID, Purpose, 전체 점수, Reason, 선택 시간과 상위 후보 배열을 보관한다.

`MovementActions`의 강한 Priority 계층은 변경하지 않았다. 셀 점수는 Action Priority나 CanExecute, ADS, Fire 정책을 대체하지 않는다. Hiding 미등록 시 새 Hiding을 만들지 않으며 Micro Movement는 기존 로컬 보조 이동 역할만 유지한다.

## 셀 내부 정확한 위치와 Action 연결

`FDMCombatContext`에 선택 여부, Cell ID, Bounds, 실제 Nav 대표 위치, Purpose와 Grid Score를 추가했다.

- Attack Position: 현재 위치의 기존 로컬 유지/재배치를 먼저 보존한다. full reposition 때 선택 셀의 대표 sample 주위에 기존 ring 후보를 만들되 Bounds 밖 후보와 투영 결과는 제외하고, 기존 cheap → LOS → path finalist와 위험/예약 검증을 그대로 수행한다.
- Combat Cover: 선택 셀의 대표 sample을 search origin으로 쓰고 Bounds 안의 hide/peek 후보만 기존 cover/risk/path/capsule/예약 검증에 통과시킨다.

대표 위치나 사각형 중심을 곧바로 `MoveTo`하지 않는다. 대표 위치는 후보 생성의 기준일 뿐이며, 실제 목적지는 Movement Action 검증을 통과한 별도 좌표다. Investigate와 다른 Action은 아직 Tactical Bounds를 직접 소비하지 않으므로 기존 로컬 로직으로 작동한다.

## 셀·행동별 실패 캐시와 fallback

실패 key는 Actor, Team, Cell, Action class name, Purpose이며 기본 3초 뒤 만료한다. 같은 셀의 AttackPosition 실패가 CombatCover 또는 Hide Purpose를 막지 않는다. MoveTo/stuck/path 요청 실패는 `InvalidPath`, terminal attack/cover 검색 실패는 `NoValidPosition` 또는 `NoCover`로 기록한다.

선택 셀에서 terminal 정확 위치 검색이 실패하면 예약을 풀고 해당 셀/Purpose를 cooldown 동안 제외한 뒤 다음 Combat Brain update에서 다음 상위 셀을 재선택한다. 후보가 없거나 Action이 Tactical Context를 지원하지 않으면 기존 로컬 판단/하위 Action/Hold 경로가 유지된다. 한 update에서 무제한 셀을 연쇄 검사하지 않는다.

## 디버그

- `dm.TacticalGrid 0`: 끔
- `dm.TacticalGrid 1`: 셀, 대표 위치, 연결선, 접근 불가 표시
- `dm.TacticalGrid 2`: Layer/Region, Cell Type, ratio, openness 정적 label
- `dm.TacticalGrid 3`: 지정 Team의 아군/교전/known/visible/intensity/confidence/예약, 예약 Actor/Purpose, 실패 cooldown 표시
- `dm.TacticalGridTeam N`: 레벨 3에서 표시할 Team ID

디버그가 0이면 DrawDebug 호출을 하지 않는다. 전체 static/dynamic/reservation 복사 Snapshot API도 non-Shipping에만 제공한다. 상위 후보별 항목은 Combat Brain 메모리에 보존하지만 기존 HUD lane까지의 통합은 이번 범위에 포함하지 않았다.

## 테스트 소스

`DMTacticalGridTests.cpp`에 다음 native 테스트 소스를 추가했다.

- Cell ID 동등성/hash, 같은 XY의 Layer/Region 분리
- ratio/openness 셀 유형
- Observation Confidence 감쇠와 clamp
- 그리드 미완성 Context의 unrestricted local fallback
- 선택 Bounds 안/밖 정확 후보 gate
- Spawn flood fill 및 거리만 가까운 disconnected region 제외
- Actor 셀 등록/이전/제거
- Physical Enemy와 Known Threat 분리
- 복수 아군의 같은 적 unique 집계
- Visible/Engagement threat 분리
- Explore 예약 중복 방지와 해제
- 셀/Action/Purpose별 실패 cooldown 분리
- graph radius/top-N 후보 제한
- Join 및 Exploration 성향 후보/Purpose 생성

기존 Movement Action Selection 테스트가 강한 Priority와 빈 배열 Hold를 계속 담당한다. Attack Position/Cover 기존 테스트가 exact 후보 실패 시 하위 Priority/local 동작을 담당한다. 테스트 소스는 컴파일했지만 자동화 테스트 자체는 실행하지 않았다.

## 구현하지 않고 구조/TODO만 남긴 항목

- Macro Region 자동 생성
- 에디터 Bake와 디스크 cache
- Recast poly reference/내부 poly region의 영구 저장
- 모든 셀 간 LOS/거리 테이블
- 완전한 ambush 우회 경로와 모든 통로 자동 검출
- 적 미래 경로 확률 모델
- Drop/OneWay의 정밀 방향성 판정
- 별도 Ambush/Watch Movement Action
- Investigate/Hiding/Retreat 전용 Tactical Bounds 검색
- NavMesh runtime regeneration delegate에 의한 자동 부분 rebuild
- 기존 Combat Debug HUD의 상위 후보별 점수 lane 통합

## Blueprint에서 확인할 설정

Blueprint 에셋은 수정하지 않았다. 사용자가 확인할 위치는 다음과 같다.

- 선택 사항: 맵에 `DeathMatch Tactical Analysis Volume`을 배치하고 `DeathMatch|Tactical Grid`에서 Cell Size, Samples, Layer, build/runtime/observation 값을 조절한다. 없으면 NavMesh Bounds와 C++ 기본값을 사용한다.
- 각 `DM Combat Brain > Combat|Tactical Personality`에서 5개 성향 축을 조절한다.
- `Combat|Tactical Grid`에서 Search Radius, Top Candidates, Reevaluation Interval을 조절한다.
- 기존 `MovementActions`에 Attack Position/Combat Cover가 등록되어 있어야 해당 정밀 검색을 사용한다. Priority 배열을 자동 변경하거나 Action을 자동 생성하지 않는다.

## 성능 비용과 제한

가장 큰 초기 비용은 Nav projection 수와 인접 후보의 동기 path check다. 둘 다 Tick별 개수와 soft ms 예산으로 제한했지만, 매우 큰 복수 Bounds에서는 Ready까지 여러 프레임이 필요하다. 600cm/3×3 기본값보다 셀을 작게 하거나 sample을 늘리면 비용이 급격히 증가한다. 최대 vertical seed와 column cap으로 비정상 Bounds 비용을 제한한다.

런타임에는 등록 Actor와 Team threat map만 저빈도로 갱신한다. 전체 Actor scan, 전체 셀 매 요청 평가, 전체 셀 쌍 path/LOS, 거리 테이블은 없다. Formation과 dynamic 집계는 짧게 cache/refresh한다.

현재 build sampling은 기본 NavData와 설정 capsule을 사용한다. AI별 서로 다른 Nav Agent profile을 각각 bake하는 구조는 아니다. runtime NavMesh가 크게 재생성되면 자동 감지하지 않으므로 현재는 `RequestRebuild()` 호출이 필요하다.

## NavMesh 재생성 여부

이미 정상 생성된 NavMesh가 있는 맵은 별도 재생성이 필요 없다. subsystem이 게임 시작 후 해당 NavData를 읽는다. NavData가 늦게 준비되면 Waiting 상태에서 재시도한다. 맵 geometry/NavMesh 설정을 바꾸거나 runtime regeneration이 크게 일어난 경우에는 현재 버전에서 subsystem `RequestRebuild()`가 필요하다. 디스크 cache나 에디터 bake는 사용하지 않는다.

## 검증 상태

- UnrealHeaderTool: 성공.
- `SkySeoulEditor Win64 Development` 전체 C++ 빌드: 성공.
- 신규 `DMTacticalGridTests.cpp` 포함 테스트 소스 컴파일: 성공.
- 충돌 marker 정적 검색: 새 소스에서 없음.
- Blueprint 에셋 수정: 하지 않음.
- Blueprint 테스트/PIE: 실행하지 않음.
- Native 자동화 테스트 실행: 사용자 지시에 따라 생략하고 소스만 추가.

초기 요청에서는 전체 에디터 빌드를 생략해도 된다고 했지만, 실제로는 새 WorldSubsystem/UHT/API 연결을 검증하기 위해 전체 Editor target 빌드를 수행했고 성공했다. 이는 PIE, Blueprint 로드 또는 실제 맵에서의 NavMesh build 시간·셀 분포·AI 전술 동작 성공을 보증하지 않는다.
