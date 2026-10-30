+# DeathMatch 전술 영역 Roam 1차 구현 결과

- 작성일: 2026-10-14 11:19 KST
- 범위: `Unreal/Source/SkySeoul/DeathMatch`의 Navigation, AI Move/Framework, Controller, Combat 이벤트 연결, HUD Debug, Tests
- 구분: 구현 결과

## 요청 또는 증상

비어 있던 `UDMMoveGoalProvider_Roam`에 다음 최소 흐름을 다시 구현한다.

전술 볼륨 배치 → 팀별 위험도 관리 → 위험도 기반 목표 볼륨 선택 → 볼륨 내부 랜덤 NavMesh 위치 선택 → 기존 Move Director를 통한 이동 → 적 발견 시 Combat 전환

Tactical Grid 기반 Roam, 볼륨 내부 셀, 엄폐/노출/구조물 분석은 포함하지 않는다.

## 결론

수동 배치 가능한 `ADMTacticalAreaVolume`과 이벤트 기반 `UDMTacticalAreaSubsystem`을 추가했다. Roam Provider는 활성 볼륨을 팀 기준으로 점수화하고, 최대 3개 볼륨에서 각 최대 3회의 `GetRandomReachablePointInRadius` 결과를 검증한다. 유효한 위치는 기존 `UDMCombatMoveDirector`에 전달되며 Roam 모듈은 직접 `MoveTo`를 호출하지 않는다.

예약은 목표 확정 시 증가하고, 도착·실패·Combat 진입·죽음·UnPossess·Actor 파괴·Provider EndPlay에서 해제된다. 적 감지 시 기존 Combat Tactics가 Roam 요청을 선점하고 현재 Roam 목표와 예약을 정리한다.

## 추가한 파일

- `Navigation/DMTacticalAreaTypes.h`
- `Navigation/DMTacticalAreaVolume.h`
- `Navigation/DMTacticalAreaVolume.cpp`
- `Navigation/DMTacticalAreaSubsystem.h`
- `Navigation/DMTacticalAreaSubsystem.cpp`
- `Tests/DMTacticalAreaRoamTests.cpp`

## 수정한 파일

- `AI/Move/DMMoveGoalProvider_Roam.h/.cpp`
- `AI/Move/DMMoveTypes.h`
- `AI/Move/DMCombatMoveDirector.cpp`
- `AI/Framework/DMBehaviorModule_PathMove.cpp`
- `AI/Framework/DMSituationFramework.cpp`
- `Controller/DeathMatchRobotAIController.cpp`
- `AI/Combat/DMCombatTacticsComponent.cpp`
- `AI/Combat/DMCombatDamageAwareness.cpp`
- `Presentation/HUD/DeathMatchHUD_CombatDebug.cpp`

삭제한 파일은 없다.

## 전술 볼륨 등록 및 위험도 관리 구조

`ADMTacticalAreaVolume`은 하나의 큰 Roam 목표 단위다. 내부 셀이나 그리드는 없다.

볼륨 런타임 정보:

- 고유 `AreaId`와 활성화 여부
- 확장용 `EDMTacticalAreaType`
- 팀별 `TeamDanger`
- 팀별 내부 인원 수
- 요청 팀 기준 아군/적군 수 조회
- 현재 교전 참여자 수
- 팀별 이동 예정 AI 수
- 디버그 이름과 색상

볼륨은 BeginPlay/EndPlay에 World Subsystem에 등록·해제되고, Begin/End Overlap 이벤트로 내부 인원을 갱신한다. Subsystem은 캐릭터와 예약 Actor의 파괴 Delegate를 추적하여 비정상 종료에서도 카운터를 정리한다.

위험도는 팀별 Map으로 분리된다. Enemy Seen과 Damage Received는 관측/피해 팀에만 단발성으로 적용된다. 시간 감쇠와 인접 볼륨 확산은 없다.

준비된 이벤트 API:

- Character Spawn / Death
- Volume Enter / Exit
- Combat Start / End
- Enemy Seen
- Damage Received
- Sound Event: API만 존재하며 현재 위험도에 반영하지 않음

Controller의 Possess/죽음/Respawn/UnPossess, Combat Tactics의 교전 시작·종료, Damage Awareness의 피해 입력을 연결했다.

## 목표 볼륨 점수 계산 방식

```text
VolumeScore =
    TeamDanger * DangerWeight
  - FriendlyCount * FriendlyCrowdingWeight
  - SameTeamIncomingAI * IncomingAIWeight
  - (DistanceCm / 1000) * DistanceWeight
  - RecentVolumePenalty (최근 도착 볼륨이며 쿨다운 중일 때)
```

점수가 음수여도 후보에서 제거하지 않으므로 위험도 0인 활성 볼륨도 fallback 후보가 된다. 점수 내림차순으로 평가하며 동점은 AreaId로 안정 정렬한다. 실패 쿨다운은 Provider 인스턴스에 보관되므로 AI별로 독립적이다.

## 랜덤 NavMesh 위치 선택 과정

1. 목표 볼륨 Actor 위치를 대표점으로 사용한다.
2. 볼륨 Bounds를 Extent로 대표점을 NavMesh에 투영한다.
3. XY Bounds 대각선 반경으로 `GetRandomReachablePointInRadius`를 호출한다.
4. 25 cm 이내 중복 결과를 거부한다.
5. `EncompassesPoint`로 실제 볼륨 Shape 내부인지 확인한다.
6. 현재 Pawn 위치에서 후보 위치까지 동기 경로를 구한다.
7. 경로 없음과 Partial Path를 모두 거부한다.
8. 유효 후보만 Move Goal로 확정한다.

Move Goal에는 `TargetVolume`, `GoalLocation`, `Intent=Roam`, 볼륨 점수, 랜덤 시도 횟수, Acceptance Radius, Stuck Timeout이 포함된다.

## 반복 제한 및 실패 처리

기본값:

- `MaxRandomLocationAttempts = 3`
- `MaxVolumeSelectionAttempts = 3`
- `VolumeFailureCooldown = 3초`

지원 실패 사유:

- NoNavProjection
- RandomNavQueryFailed
- DuplicateRandomPoint
- OutsideTargetVolume
- NoCompletePath
- PartialPath
- VolumeAttemptLimit
- VolumeCooldown
- MoveRequestRejected
- TargetVolumeDisabled
- GoalNavInvalid

한 볼륨의 위치 선택이 모두 실패하면 해당 AI에서만 쿨다운을 기록하고 다음 점수 볼륨을 평가한다. 이동 중에는 기존 Watchdog 호출 때 목표 볼륨 활성화, 목표 위치 NavMesh 투영, 현재 위치부터의 완전 경로, 예약을 다시 확인한다.

## 기존 Move Director 연결 방식

Roam Provider는 목적지만 계산한다. `DMBehaviorModule_PathMove`는 Goal의 Acceptance Radius를 읽고 기존 `UDMCombatMoveDirector::SubmitMoveRequest`에 `StrategicRoam` 소유권과 우선순위로 전달한다.

Move Director는 목표 완료·실패·취소 시 Provider에 결과를 통지하여 Incoming AI 예약을 해제한다. Tactical Area 목표는 기존 Tactical Grid 예약과 별도 필드로 유지되며, 과거 Tactical Grid Roam 코드는 복원하지 않았다.

## Roam에서 Combat으로 전환되는 과정

기존 Combat Tactics가 적 후보를 채택하면 다음 순서가 유지된다.

1. Enemy Seen / Combat Start 이벤트를 Tactical Area Subsystem에 전달
2. `CancelAllMoveRequests(true)`
3. `ClearCurrentGoal()`로 Roam 목표와 Incoming 예약 해제
4. 기존 Combat Target 및 Combat EQS/행동 모듈 실행

Combat 상태에서는 Tactical Area 랜덤 위치 선택을 호출하지 않는다.

## 도착 및 Hold

Roam 도착 성공 시 예약을 해제하고 최근 방문 볼륨과 방문 시각을 기록한다. `RoamHoldDuration` 동안 Provider가 새 목표를 제공하지 않으며, 기존 Situation Framework Watchdog가 Hold 종료 후 다음 Roam 평가를 수행한다. 도착만으로 위험도를 제거하지 않는다.

## 에디터 조절 변수와 기본값

`MoveProviderRoam` 컴포넌트:

- `bEnableTacticalAreaRoam = true`
- `DangerWeight = 1.0`
- `FriendlyCrowdingWeight = 0.5`
- `IncomingAIWeight = 0.75`
- `DistanceWeight = 0.15` (1000 cm당)
- `RecentVolumePenalty = 2.0`
- `RecentVolumeCooldown = 8초`
- `MaxRandomLocationAttempts = 3`
- `MaxVolumeSelectionAttempts = 3`
- `VolumeFailureCooldown = 3초`
- `RoamAcceptanceRadius = 150 cm`
- `RoamHoldDuration = 1초`
- `RoamStuckTimeout = 2초`

## 디버그 확인 방법

기존 `dm.CombatDebug`를 활성화하면 Roam AI 화면 정보가 다음 핵심 값으로 표시된다.

- State: Roam
- TargetVolume
- 팀 기준 Danger와 최종 Score
- Goal 좌표와 Random Attempt

목표 선택 때 월드에 목표 볼륨 Bounds, 목적지, AI-목적지 선을 표시한다. 실패 후보는 빨간 Sphere/선과 실패 사유 문자열로 3초간 표시한다. 로그는 목표 확정, 볼륨 최종 실패, 전체 선택 실패만 출력하고 Tick 로그는 추가하지 않았다.

## BP에서 사용자가 직접 설정해야 하는 항목

1. 레벨에 `DeathMatch Tactical Area Volume`을 여러 개 배치한다.
2. 각 Brush가 의도한 큰 전략 영역을 덮도록 크기를 조정한다.
3. 각 볼륨의 `AreaId`를 맵에서 고유하게 지정한다.
4. 필요 시 Debug Display Name/Color를 설정한다.
5. 볼륨의 Overlap 이벤트가 Pawn에 대해 발생하는 Collision 설정인지 확인한다.
6. 각 볼륨 내부와 대표점 주변을 NavMesh가 충분히 덮는지 확인한다.
7. AI Controller BP의 상속된 `MoveProviderRoam` 컴포넌트에서 가중치와 이동 값을 맵 규모에 맞게 조정한다.
8. AI/Player의 TeamId가 255(NoTeam)가 아닌지 확인한다.

별도 Tactical Grid, EQS, MovePoint BP 설정은 필요하지 않다.

## 제외한 기능

- 볼륨 내부 셀/그리드 및 세부 위험도
- 위치별 예약
- 위험도의 시간 감쇠·공간 확산
- 엄폐/사격/Peek/노출도/구조물 분석
- 볼륨 간 시야·포탈 관계
- 적 출현 예상 위치
- 고급 역할 분담·협동 이동
- Tactical Grid 기반 Roam 복구
- Combat 위치 선정 변경
- Sound Event의 실제 위험도 정책

## 문제점 및 위험

- 실제 맵 Brush Collision/Overlap 설정과 NavMesh 배치는 C++ 자동화 테스트로 확인하지 못했다. PIE 확인 필요.
- 볼륨이 서로 겹치면 Bounds가 더 작은 볼륨을 위치 소유자로 선택한다. 맵 설계에서 의도적 중첩 여부 확인이 필요하다.
- 위험도는 이번 단계에서 누적만 되고 해소되지 않는다. 장기 라운드에서는 후속 위험도 소거 정책이 필요하다.
- Hold 재평가 해상도는 Situation Framework의 Judgment Watchdog 주기에 따른다.
- 기존 작업 트리에 본 작업 이전부터 다수의 미커밋 변경이 있었으며, 본 보고서의 추가·수정 목록 외 파일은 정리하거나 되돌리지 않았다.

## 권장 후속 작업

1. 실제 DeathMatch 맵에 3개 이상 Tactical Area Volume을 배치한다.
2. 서로 다른 팀 AI에서 같은 사건의 TeamDanger 값이 분리되는지 PIE로 확인한다.
3. 좁거나 NavMesh가 끊긴 볼륨에서 3회 제한과 다음 볼륨 fallback을 확인한다.
4. 적 감지·사망·UnPossess 중 Incoming AI Count가 즉시 0으로 복구되는지 확인한다.
5. 플레이 시간 기준 위험도 해소/확신도 정책을 별도 설계한다.

## 검증 상태

- `SkySeoulEditor Win64 Development` 빌드: 성공
- UnrealHeaderTool: 성공
- 자동화 `SkySeoul.DeathMatch.Roam.TacticalArea.Score`: 성공
- 자동화 `SkySeoul.DeathMatch.Roam.TacticalArea.Reservation`: 성공
- Editor 수동 조작/Blueprint 저장/실제 맵 PIE: 수행하지 않음

