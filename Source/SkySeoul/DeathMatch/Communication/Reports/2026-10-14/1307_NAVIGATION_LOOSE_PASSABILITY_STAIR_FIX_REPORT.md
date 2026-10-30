# DeathMatch 경로 캡슐 검사 완화 및 계단 오판 수정 결과

- 작성일: 2026-10-14 13:07 KST
- 범위: DeathMatch Navigation, Robot AI Controller, Roam Move/Recovery, 자동화 테스트와 현행 LOGIC_STATUS
- 구분: 구현 결과

## 요청 또는 증상

`ADeathMatchRobotAIController::ValidateMoveGoal()`이 Full Nav 경로를 만든 뒤 실제 캐릭터 반경의 캡슐로 전체 경로를 다시 검사했다. 전체 경로 길이를 균등 샘플링한 점 때문에 Nav 코너 사이를 직선으로 건널 수 있었고, 캡슐 중심을 계단 단차에 그대로 Sweep해 실제 이동 가능한 계단·좁은 통로를 `WallClearance`로 거부할 수 있었다. Pawn 충돌도 영구 목적지 실패와 cooldown으로 연결됐다.

## 결론

NavAgent 기반 투영과 valid/full/non-partial Nav 경로를 기본 통행 판정으로 유지하고, 추가 캡슐 검사를 loose Passability Probe와 non-blocking Quality Probe로 분리했다. Passability는 기본 Radius 0.70/HalfHeight 0.80의 축소 캡슐로 고정 WorldStatic이 경로 중심까지 명백하게 막는 경우만 거부한다. Quality 실패와 Pawn 감지는 현재 이동 요청과 blocked-goal cooldown을 변경하지 않는다.

## 수정한 파일

- `Navigation/DMNavigationSafety.h/.cpp`
- `Controller/DeathMatchRobotAIController.h/.cpp`
- `AI/Move/DMCombatMoveDirector.h/.cpp`
- `AI/Move/DMMoveGoalProvider_Roam.h`
- `AI/Framework/DMSituationFramework.h/.cpp`
- `Tests/DMNavigationSafetyTests.cpp`
- `Navigation/LOGIC_STATUS.md`
- `Controller/LOGIC_STATUS.md`
- `AI/Move/LOGIC_STATUS.md`
- 본 결과 보고서

`DMTacticalArea`, Tactical 위험도, Weighted Random 영역 선택, Combat 위치 선정과 Blueprint/uasset은 변경하지 않았다.

## Passability와 Quality 분리

Hard Passability는 축소 캡슐로 `WorldStatic`과 `Pawn`을 함께 조회하되 Pawn은 `DynamicObstacleDetected`로만 남긴다. WorldStatic 접촉도 다음 조건을 모두 만족해야 `StaticPassabilityBlocked`가 된다.

- 시작 지점 초기 겹침이 아님
- 축소 캡슐 Sweep 이후에도 충돌함
- 작은 중심선 확인 Sphere가 같은 Actor/Component를 다시 검출함
- WorldStatic이며 Pawn이 아님

캡슐 가장자리만 닿거나 시작 겹침인 경우는 `WallClearanceQualityLow`로 낮춘다. 별도의 Quality Probe는 Roam/Combat wall scale을 반경에 적용하지만 결과가 나빠도 이동을 거부하지 않는다.

## 축소 캡슐과 계단 Raised Probe

기본 계산은 다음과 같다.

```text
ProbeRadius = ActualRadius * 0.70
ProbeHalfHeight = Max(ProbeRadius, ActualHalfHeight * 0.80)
```

Path segment의 Z 차이가 5cm 이상이면 해당 segment 전체에 다음 높이를 더한다.

```text
RaisedHeight = CharacterMovement.MaxStepHeight * 1.0
ProbeCenter = GroundPathPoint + (0, 0, ProbeHalfHeight + 2cm + RaisedHeight)
```

평지 segment는 RaisedHeight를 적용하지 않는다. 첫 PathPoint가 Pawn capsule center로 들어온 경우 실제 HalfHeight를 빼 ground point로 정규화한 뒤 동일 공식을 사용한다.

## Nav Path 세그먼트 검사

전체 경로 길이 균등 샘플과 `PathClearanceSampleCount`를 제거했다. `UNavigationPath::PathPoints`의 실제 구간을 순서대로 검사하고, 250cm보다 긴 구간만 `Ceil(SegmentLength / 250cm)`개로 나눈다. 서로 다른 Path segment의 끝과 시작을 하나의 Sweep으로 연결하지 않으므로 코너 안쪽 구조물을 가로지르는 검사가 발생하지 않는다.

## 다른 AI와 최종 목적지 처리

다른 Pawn은 Passability hard block이 아니다. 자기 Pawn과 부착 Actor는 query에서 제외하고, Pawn object collision은 `DynamicObstacleDetected` 품질 정보만 남긴다. WorldDynamic 무기·투사체·분리 파츠도 Hard Passability 대상이 아니며, RVO/Move retry/Stuck Recovery가 런타임 동적 충돌을 처리한다.

최종 목적지는 projected ground point 위에 실제 캡슐 크기의 0.90 배율 capsule을 별도 overlap한다. Object query는 WorldStatic만 사용하므로 Pawn은 목적지를 영구 차단하지 않는다. 실패 사유는 `DestinationStaticBlocked`다.

## 실패 사유

Hard Failure:

- `NavigationUnavailable`
- `NavProjectionFailed`
- `InvalidOrPartialPath`
- `DestinationStaticBlocked`
- `StaticPassabilityBlocked`

Non-blocking Quality:

- `NarrowPath`
- `VeryNarrowPath`
- `RaisedStepProbeUsed`는 `bRaisedStepProbeUsed` 상태로 보존
- `WallClearanceQualityLow`
- `DynamicObstacleDetected`

경로 사전 검사에서는 Hard Failure만 `RecordMoveFailure()`와 `BlockedPathCooldown`에 연결한다. Quality 결과는 Verbose 로그와 debug 상태만 갱신한다.

## Stuck Recovery 연결

Roam `RoamStuckTimeout` 기본값과 Framework fallback timeout을 2초에서 3초로 완화했다. 첫 no-progress timeout은 예약과 현재 목표를 유지한 채 CombatMoveDirector가 같은 목표를 한 번 다시 요청한다. 다시 timeout이 발생하면 그때 현재 목적지를 runtime failure/cooldown으로 처리하고 기존 다른 위치/영역 재선택 흐름으로 넘긴다. 진행 거리와 속도/목표 방향 검사는 기존대로 유지된다.

## 에디터 변수

추가:

- Passability `PathPassabilityRadiusScale = 0.70`
- Passability `PathPassabilityHalfHeightScale = 0.80`
- Passability `DestinationCapsuleScale = 0.90`
- Passability `bRaisePassabilityProbeOnVerticalSegments = true`
- Passability `VerticalSegmentThreshold = 5cm`
- Passability `StepHeightProbeMultiplier = 1.0`
- Passability `PathClearanceSampleSpacing = 250cm`

변경:

- `RoamWallClearanceScale = 1.15`, `CombatWallClearanceScale = 1.0`을 `Navigation|Quality` 범주로 이동하고 non-blocking 의미를 Tooltip에 명시
- `RoamStuckTimeout` 기본값 2초 → 3초
- Framework fallback `MoveStuckTimeSeconds` 2초 → 3초

제거:

- `PathClearanceSampleCount`

Passability scale, Quality scale, Raised Probe의 역할은 각 UPROPERTY Tooltip에 명시했다.

## 로그와 디버그

`dm.CombatDebug 2` Navigation 요약은 다음 형식을 사용한다.

```text
Nav: Full | Pass: LooseOK | Quality: Narrow | StepProbe: 1
```

동적 장애물을 감지하면 Quality 문자열에 `+DynamicObstacle`이 붙는다. 실제 거부만 Warning으로 기록하고 AI, Goal, Reason, Hit Actor/Component, Probe 크기, Raised 높이와 Path segment를 남긴다. Quality는 Verbose다.

월드 probe 색상은 일반 loose=초록, raised=청록, quality narrow=주황, hard block=빨강, destination capsule=파랑이다.

## BP에서 확인할 사항

1. AI Controller BP의 새 Passability 변수들이 C++ 기본값 0.70/0.80/0.90, 5cm, 1.0, 250cm를 사용하는지 확인한다.
2. 기존 `PathClearanceSampleCount` override가 사라진 뒤 새 spacing 값이 의도대로 보이는지 확인한다.
3. Roam Provider의 `RoamStuckTimeout`이 기존 BP 저장값 2초로 override되어 있지 않은지 확인하고 필요하면 3초로 맞춘다.
4. `RoamWallClearanceScale`과 `CombatWallClearanceScale`은 품질 표시 전용이며 통행 차단 튜닝으로 사용하지 않는다.
5. 실제 계단의 NavMesh agent radius/height와 Character Movement `MaxStepHeight`가 캐릭터 capsule과 맞는지 확인한다.

## 자동화 테스트 결과

`SkySeoul.DeathMatch.Combat.Navigation.*` 6개를 분리 실행했으며 6/6 성공, Exit Code 0이다.

- CapsuleDimensions: 성공
- LooseProbe: 성공
- Mismatch: 성공
- PathSegmentSweeps: 성공
- Policy: 성공
- StuckEvidence: 성공

추가 검증 범위는 0.70/0.80 축소 계산과 HalfHeight>=Radius, vertical/flat raised 판정, MaxStepHeight 기반 높이, actual PathPoint segment별 subdivision과 코너 보존, Pawn/초기 overlap non-hard 정책, destination/static hard 사유, quality non-cooldown 정책, partial path 거부와 one-shot repath 정책이다.

참고로 전체 `SkySeoul.DeathMatch.Combat` 묶음도 실행했다. 이번 Navigation 테스트는 모두 성공했지만 기존 변경 범위 밖 Adaptive Fire, Architecture, AttackPosition, Cover, Debug, Parts, Cadence 테스트 실패가 있어 전체 프로세스는 Exit Code 1이었다. 이 보고서에서는 해당 기존 실패를 Navigation 변경 성공으로 오인하지 않는다.

## 검증 상태

- UnrealHeaderTool: 성공
- `SkySeoulEditor Win64 Development` C++ 빌드: 성공
- Navigation 자동화: 6/6 성공, Exit Code 0
- Blueprint 저장/수정: 수행하지 않음
- 실제 계단 맵 수동 PIE: 수행하지 않음
- 전체 Combat 자동화: 변경 범위 밖 기존 실패로 Exit Code 1

## 실제 계단 PIE 확인 항목

1. 기존에 `WallClearance`로 거부되던 좁은 계단 목적지가 Full Nav + LooseOK로 승인되는지 확인한다.
2. `StepProbe: 1`과 청록 capsule이 Z 변화 segment에만 표시되는지 확인한다.
3. 평지 segment가 초록 capsule이며 RaisedHeight를 사용하지 않는지 확인한다.
4. 계단 코너에서 Sweep이 다음 PathPoint로 꺾이고 코너 안쪽 벽을 대각선으로 가로지르지 않는지 확인한다.
5. 다른 AI가 계단이나 목적지에 잠시 있어도 goal rejection/cooldown이 발생하지 않는지 확인한다.
6. 실제 고정 벽이 경로 중심 또는 최종 목적지를 막으면 각각 `StaticPassabilityBlocked`, `DestinationStaticBlocked`가 되는지 확인한다.
7. 첫 정체는 같은 목표 repath 1회, 반복 정체는 목표 실패와 다른 위치 재선택으로 이어지는지 확인한다.
