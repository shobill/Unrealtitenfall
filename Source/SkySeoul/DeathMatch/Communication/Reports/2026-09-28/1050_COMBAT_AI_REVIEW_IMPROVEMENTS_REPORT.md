# DeathMatch Combat AI 개선 결과

작성: 2026-09-28, Asia/Seoul. 9월 23일 시작한 작업을 이어서 완료한 날짜 기준이다.

## 작업 및 검증 범위

기존 Combat Brain + Action Module, Threat Snapshot, Position Evaluator, Target Evaluator를 유지하고 코드 수정과 정적 확인만 수행했다. 별도 피해 상태 머신이나 SurpriseDamage 전용 Action은 추가하지 않았다.

- 전체 빌드/UHT 미실행.
- Unreal 자동화 테스트 미실행.
- PIE/BP 런타임 테스트 미실행.
- 컴파일 성공을 주장하지 않는다.
- 실제 BP 직렬화와 Details 표시를 확인했다고 주장하지 않는다.
- Asset Registry 조회, Blueprint 로드·재저장, 에셋 migration은 수행하지 않았다.
- `.uasset`, `.umap`, Config, Controller의 직렬화된 subobject, Movement Action 우선순위 및 Instanced 프로퍼티는 이번 작업에서 수정하지 않았다.
- 작업 시작 시 프로젝트에 기존 변경 및 untracked 파일이 다수 있었다. 기존 작업을 되돌리거나 일괄 초기화하지 않았다. DeathMatch 소스가 untracked 상태여서 `git diff --check`만으로 변경 검증을 보장할 수 없으며, 수정 파일 직접 읽기와 별도 정적 검사를 사용했다.

## 수정 파일

아래 파일은 모두 `Source/SkySeoul/DeathMatch/AI/Combat/` 아래에 있다.

1. `DMCombatTacticsComponent.h`: 공통 튜닝 설정, Target Selection 구조체, 피해 기억 방어 관찰 상태, 설정 메타데이터.
2. `DMCombatTacticsComponent.cpp`: Sense/LOS Context 분리, 이동 검증 설정 사용, 기억 상태 속도 및 디버그 상태, Blackboard 의미.
3. `DMCombatThreatMemory.cpp`: 비전투 피해 기억 연결, Sense 기반 Awareness, view/muzzle Combat LOS 검사.
4. `DMCombatThreatEvaluation.cpp`: dirty 갱신 조건, 감지/LOS 구분, 차단된 감지 타깃 선택, 점수 가중치, 전환 지연.
5. `DMCombatDamageAwareness.cpp`: dirty 책임 분리, 피해 기억 만료 무효화, 익명 피해 생산 경로 제거.
6. `DMCombatDamageStimulus.h`: 미사용 방향 기록 및 익명 native 데이터 제거.
7. `DMCombatThreatSnapshot.h`: 생산 경로 없는 익명 위협 배열 제거.
8. `DMCombatMovementDecision.cpp`: 명시 Intent 보존, 공통 위험/긴급 조건, 이동 속도, Micro Move, 위치 재사용 시간 분리.
9. `DMCombatPositionRisk.h`: 공통 위험 단계 설정, Soft Risk 표시 이름, 초과 페널티 배수와 관찰값.
10. `DMCombatPositionRisk.cpp`: 감지된 적의 가중 노출 기록, 초과 위험 배수, 캐시 크기 설정, 익명 위험 분기 제거.
11. `DMCombatModules.h`: Fixed/Rush 옵션, FindSight 평가기 재사용, Precision 이동 사격 설정, native 호환 API 설명.
12. `DMCombatModules.cpp`: 기본 Action별 Intent, MoveThenFire 이동 단계, FindSight 단계적 검색.
13. `DMCombatAttackPosition.h`: 중복 이동 사격 옵션 및 미사용 randomness Deprecated 처리.
14. `DMCombatAttackPosition.cpp`: 감지 중 LOS 차단 상황에서 후보 LOS 평가 허용, 검증 간격, Base Score 설명.
15. `DMCombatCoverModule.h`: Cover 검색 설정과 Legacy 기억 설정 표시.
16. `DMCombatCoverModule.cpp`: 방향·링·비용·검증·예약 설정 사용.
17. `DMCombatCoverTests.cpp`: Action이 기억 수명을 소유한다는 구형 테스트 assertion 2개 제거. 테스트 실행은 하지 않았다.
18. `DMCombatInvestigation.cpp`: 공통 방어 검증 간격 및 명시적 Intent.
19. `DMCombatFire_Adaptive.h`: 다른 Fire Action과 같은 이동 사격 Tooltip.
20. `DMCombatTacticsDebug.cpp`: Sense/LOS, 적용 속도 제한, 위험 조건, 피해 기억, Base/Final Score 구분.

## 원인과 수정

### 비전투 SurpriseDamage

기존에는 `ProcessDamageReevaluation`이 위험을 계산해도 `UpdateThreatAwareness`가 기존 교전/타깃 부재로 종료했다. 그래서 피해 기록만 생기고 실제 위치 판단으로 이어지지 않았다.

이제 기존 Snapshot에서 신뢰 가능한 피해 원점을 가진 미감지 적을 선택해 `Investigating` Context로 연결한다. 내부 `bDamageOnlyDefense`는 기존 기억 행동의 출처 표시이며 새 Awareness 상태가 아니다.

- 위치는 피해 당시 저장한 신뢰 가능한 원점 및 기존 기억 정보만 사용한다. 이후 숨은 공격자의 현재 Transform을 추적하지 않는다.
- 이 경로의 `bTargetSensed`, `bVisible`, `bFireActive`는 false다. Snapshot의 감지 증거도 피해로 승격하지 않는다.
- 이동 전에 현재 위치를 기존 `DMPositionRisk::Evaluate`로 평가한다. 하드 안전 조건을 만족하고 Safe이면 이동 없이 기억 위치를 주시한다.
- 안전하지 않으면 기존 Investigation Action을 평가하고, 사용할 수 없으면 기존 우선순위 순서의 Cover/Retreat/KeepDistance 방어 Action으로 fallback한다.
- Critical 위험이면 기억 Context에도 긴급 위험을 전달해 Cover가 Partial Cover를 완전 긴급 엄폐로 선택하지 않도록 한다. Surprise라는 분류 자체로 긴급 이동을 강제하지 않는다.
- 피해 기억 기반 교전은 `LastDamageTime + DamageThreatMemorySeconds`를 만료 기준으로 삼는다. 감지된 적/유효 피해 기억이 없으면 전투를 해제하고 Situation Brain의 Roam으로 돌아간다.
- 공격자를 실제 감지하면 정상 감지 전투로 전환한다. 공격 허가는 Sense와 Combat LOS를 모두 요구한다.
- 기존 `ResetCombat(true)`, Deactivate, EndPlay, Controller의 사망·UnPossess·재Possess 정리 경로를 유지한다. 일반 `ResetCombat(false)`는 기존 설계대로 피해 증거 수명 자체는 보존할 수 있지만, 교전과 이동 소유 상태는 해제한다.

### Perception과 Combat LOS

기존 `BuildContext`는 `IsVisibleHostile` 결과를 `bTargetSensed`와 `bVisible`에 동시에 넣었다. 이 때문에 LOS 차단을 감지 상실로 처리했고 FindSight 조건에 진입할 수 없었다.

- `IsSensedHostile`: 살아 있는 적대 타깃인지, Sense의 `bCurrentlySensed`가 참인지 확인한다.
- `IsVisibleHostile`: Sense 검사를 통과한 타깃에 대해서만 실제 view LOS와 무기 muzzle의 Combat aim point LOS를 검사한다.
- `BuildContext`: `bTargetSensed = IsSensedHostile`, `bVisible = bTargetSensed && IsVisibleHostile`.
- 감지 중 위치는 Sense의 `LastSensedLocation`을 사용한다. 미감지 상태는 RememberedThreat/RememberedThreatEye만 사용한다.
- Awareness의 기존 `Visible` enum 이름은 직렬화 호환을 위해 유지하되, 의미는 현재 감지가 유지되는 전투 상태다. 이 상태에서도 Combat LOS는 false일 수 있다.
- LostGrace, PendingTarget 유효성, 일반 전환 확인 시간은 Sense 상실 기준이다. 사선 차단만으로 LostGrace를 시작하지 않는다.
- Blackboard의 `IsFocusSeen`, `HasTarget`, `TargetActor`는 감지 상태를 반영한다. `WantAttack`은 별도 사격 허가를 통과해야 한다.
- Snapshot 생성은 감지 증거만 기록하고 `bVisible=false`로 시작한다. Target Evaluation 예산 안에서 실제 Combat LOS를 확인한 후보만 이를 갱신한다. 미평가 false를 “확인된 LOS 차단”과 동일한 증거로 해석하면 안 된다.
- 실제 발사/안전 검사에는 감지와 LOS 조건을 모두 유지했다. LOS가 없으면 거리 확률 준비도 승인하지 않는다.

### FindSight

클래스를 유지했다. 감지되지만 Combat LOS가 차단된 타깃도 Target Evaluation 후보가 되어 처음 교전할 수 있으며, FindSight 분기에 진입할 수 있다.

- 주변 최대 8방향 후보를 기존 Attack Position 평가기에 전달한다.
- 기존 `EvaluateCheapPosition → FinalizeCandidates`의 저비용 → LOS → Path 단계를 재사용한다.
- 다중 적 위험, Capsule/Nav, 점유, 아군 사선 검증을 함께 사용한다.
- 후보 발견 시 FastReposition, 사선 복구 후 HoldAndFire를 반환한다.
- 실패와 검색 쿨다운 중에는 `bCanExecute=false`로 다른 Action을 허용한다. 실패한 Action의 veto가 전체 사격 권한에 남지 않는다.
- 우선순위를 잃는 `Deactivate`에서는 검색 쿨다운을 보존하도록 수정했다. 전체 Reset은 기존처럼 검색 상태를 초기화한다.
- BP에 이 Action을 자동 삽입하지 않았다. 사용하려면 기존 인스턴스 설정에서 적절한 슬롯을 지정해야 한다.

### MovementIntent와 MoveThenFire

기존 Brain은 Attack Position이 아닌 Action의 FastReposition과 Cover가 아닌 Action의 EmergencyCover를 CoveringMove로 강등했다. 이 강등을 제거했다.

경로 상태가 Idle이라는 이유로 Fast/Emergency를 HoldAndFire로 바꾸고 사격을 즉시 재허용하던 후속 우회 분기도 제거했다. 도착·실패·timeout에 대한 다음 Action 평가가 Intent를 바꿀 때까지 명시적인 사격 금지를 유지한다.

- Fixed Fire: 사거리 안 HoldAndFire, 접근 CoveringMove. 새 `bFastApproach=false`; true이면 접근 FastReposition.
- Maintain Distance: 거리 조정 CoveringMove, 적정 거리 HoldAndFire.
- Strafe: 거리 조정 CoveringMove, 좌우 이동 StrafeAndFire.
- MoveThenFire: 사격 구간 HoldAndFire, 이동 구간 FastReposition. 도착 또는 MoveTimeoutSeconds까지 이동하고 사격을 금지한다. 사거리 밖 접근도 이동 단계와 timeout에 연결했다.
- Rush: 기본 FastReposition. 새 `bFireDuringRush=false`; true이면 CoveringMove.
- Retreat: 기본 CoveringMove. 긴급 조건이면 Brain이 EmergencyCover로 승격할 수 있다.
- FindSight: 사선 복구 FastReposition, 복구 후 HoldAndFire.
- Attack Position: 기본 CoveringMove, 점수 이득·위험·예상 이동 시간 조건을 만족하면 FastReposition. Brain의 Micro Move는 StrafeAndFire.
- Cover: 일반 MovingToHide는 CoveringMove, Critical/Escape는 EmergencyCover, MovingToPeek는 PeekAndFire, Exposed는 HoldAndFire. Hidden은 사격 금지.
- Investigation: 이동 제안 CoveringMove, 정지 HoldAndFire. 기억 상태의 사격 금지는 Brain이 별도로 강제한다.

기존 `bAllowFire=false` 하나로 감지 전투 사격을 모두 막지는 않는다. 대신 명시적인 FastReposition/EmergencyCover, Hidden Cover, Sense/LOS, Fire Action 속도 및 기존 발사 조건을 확인한다.

## 이동 사격 속도 책임

Brain은 의도별 원하는 속도, Fire Action은 발사 가능한 최대 속도, Movement Action은 Intent와 목적지를 소유한다.

CoveringMove/StrafeAndFire/PeekAndFire의 적용 속도는 `min(전투 기본 속도, Intent 속도, Fire Action 최대 이동 사격 속도)`다. FastReposition/EmergencyCover에는 Fire Action 최대 속도를 적용하지 않는다.

- 기존 CombatMaxWalkSpeed, CoveringMoveSpeed=200, StrafeFireMoveSpeed=180, InvestigationMaxWalkSpeed를 유지했다.
- 새 FastRepositionSpeed=0, EmergencyCoverSpeed=0: 0이면 전투 기본 속도/저장된 캐릭터 속도. 양수는 전투 기본 속도 이하로 제한한다.
- 새 PeekMoveSpeed=0: 0이면 CoveringMoveSpeed를 사용한다.
- InvestigationMaxWalkSpeed=0이면 저장된 캐릭터 MaxWalkSpeed를 사용한다. LostGrace/PendingSwitch도 원래 속도를 복원한다.
- 정지 사격 Fire Action 때문에 이동 사격 속도를 사용할 수 없고, 현재 LOS 또는 사거리가 없어 사격도 불가능한 경우 FastReposition으로 이동을 허용한다.
- AttackPosition.bAllowFireWhileMoving은 신규 판단에서 읽지 않고 AdvancedDisplay + DeprecatedProperty로 보존했다.
- Precision에 `bAllowFireWhileMoving=true`를 추가했다. 기존 `MaxMoveSpeed` 이름과 기본값 600을 유지하고 표시 이름만 Max Fire Move Speed로 바꿨다.
- Precision/Burst/Automatic/Adaptive는 false일 때 정지 사격만 허용하며 기존 정책에 맞춘 1 cm/s 허용 오차를 쓴다. 최대 속도 0 또는 매우 낮은 값은 정지에 가까운 사격을 뜻한다.
- 속도 때문에 발사가 거부될 때 실제 성공 발사 cadence를 소비하지 않는 기존 NotifyShotFired 경로를 유지했다.

## 위험, 긴급 조건, 타깃 점수

### 공통 Risk Tier

`FDMCombatRiskTierSettings`: CautionThreshold=40, DangerousThreshold=65, CriticalThreshold=85. 음수는 0으로 제한하고 매 평가 시 오름차순 정렬한 값을 공통 사용한다. 설정이 역전돼도 적용되는 단계 경계는 정렬된다. 저장된 에디터 숫자를 자동 재작성하지는 않는다.

Risk Tier는 행동 선택용이며 Soft Risk Limit는 점수 페널티용이다. Attack 위치 재사용, 기억 위치의 안전 판단, Brain의 긴급 판단에서 공통 Tier를 사용한다. Cover는 Brain의 RiskTier와 Context 긴급 여부를 받는다.

### Emergency Response

- EmergencyLowHealthRatio=0.3, EmergencyEnemyDistance=250 cm, EmergencyWeightedExposureThreshold=4.
- bLowHealthExposureTriggersEmergency=true, bCloseEnemyTriggersEmergency=true, bMultiExposureTriggersEmergency=true.
- 저체력은 현재 감지된 적에 대한 가중 노출도 있어야 해당 조건을 만족한다.
- 근거리 조건은 포커스 타깃 하나가 아니라 Snapshot의 현재 감지된 적을 검사한다.
- 다중 노출 조건은 현재 감지된 적의 가중 노출 합을 사용한다. 기억만 남은 낮은 신뢰도 적으로 직접 해당 조건을 충족하지 않는다.
- 공통 Critical 위험 단계는 별도로 Emergency를 요청할 수 있다. Caution/Dangerous만으로 EmergencyCover를 강제하지 않는다.

### Target Selection Weights

새 `FDMTargetSelectionWeights`의 기본값은 기존 계수를 따른다.

- VisibleBaseScore=60, AttackableBonus=40, NotAttackablePenalty=25, EffectiveRangeBonus=15.
- AimTurnPenaltyWeight=15, FinishTargetBonus=5, FriendlyFirePenalty=60, OutOfRangePenalty=50.
- CloseThreatContribution=0.5, ThreatScoreContribution=0.2.
- EmergencyDistanceAdvantage=100 cm, FacingThreatWeight=10, BaseThreatScore=10, ThreatDistanceFalloffRange=2000 cm.
- 기존 CurrentTargetStickinessScore=10은 구조체로 이동/복제하지 않았다. 기존 BP 값을 유지하기 위해 원래 프로퍼티에서 읽는다.
- 감지되지만 사선이 막힌 후보는 공격 가능 보너스를 받지 않으며, 실제 사격 타깃으로 승인되지 않는다. 단, 위치 복구를 위한 CombatTarget 후보가 될 수 있다.

### 전환 시간

감지 유지 중 일반 점수 전환은 TargetSwitchConfirmTime만 확인한다. 타깃을 놓친 경우 pending 후보 시작 시각부터 `max(TargetSwitchConfirmTime, TargetSwitchDelaySeconds)`가 지나고 LostGrace도 끝나야 한다. MinimumTargetHoldTime과 active Fire Cycle 보호는 기존대로 별도 적용한다. 긴급 전환은 기존 예외를 유지했다.

새 `TargetSwitchPositionReuseSeconds=2`로 위치 재사용 시간을 분리했다. 0은 재사용 유지 시간을 비활성화한다. MinimumTargetHoldTime은 타깃 교체 시간에만 사용한다.

## Micro Move 및 Cover 검색

### Micro Move Advanced 기본값

- MicroMoveRiskTolerance=10 점수.
- MicroMoveValidationInterval=0.5 seconds.
- MicroMoveReservationRadius=150 cm, MicroMoveReservationLifetime=2 seconds.
- MicroMoveHeightTolerance=60 cm, MicroMoveMinimumDistance=50 cm, MicroMoveAcceptanceRadius=20 cm.
- MicroMoveMaximumPathMultiplier=2.

기존 Min/Max 설정을 유지하고 역전값은 runtime Clamp/Max로 안전하게 처리한다. 현재 위치가 유효하고 GoodEnoughScore 이상이며 Dangerous 미만인 경우에만 새 Micro Move를 시작한다. Cover의 Hide/Peek 쌍은 제외하고 Precision의 기존 안정화 대기를 유지한다. 탐색/예약 실패 시 정지 사격 상태를 유지하며 전체 Action 실패로 바꾸지 않는다.

### Cover Search Advanced 기본값

- CoverDirections=12, CoverRings=3. 각각 4~32, 1~8로 제한하고 전체 후보 수는 곱으로 계산한다.
- CoverRiskCostWeight=10, CoverPathExposureCostWeight=10.
- CoverPreferredDistanceCostWeight=0.5, EscapeDistanceRewardWeight=0.65.
- CoverValidationInterval=0.5 seconds, CoverReservationLifetime=2 seconds.

기존 증분 cheap 후보 예산, LOS/Path finalist 예산, GoodEnoughCoverCost 조기 종료, Hide/Peek 기하 검증, 전체 위협 Soft Cost를 유지한다. 모든 후보에 Path를 요청하지 않는다. Critical의 Partial Cover 거부를 유지했다.

## 위험 설정 이름과 미사용 설정

- `bRejectDangerousPositions`: 이름을 삭제/변경하지 않고 표시 이름을 **Apply Excess Risk Penalty**로 변경했다.
- `MaxAcceptableRisk`: 표시 이름을 **Soft Risk Limit**로 변경했다.
- 새 `ExcessRiskPenaltyMultiplier=1`: 초과 위험에 곱한다. 0이면 초과분 추가 페널티가 없다. 위험 자체는 기존대로 점수에 반영된다.
- 계산은 `Risk += max(0, Risk - SoftRiskLimit) * max(0, Multiplier)`다. 위험 점수만으로 하드 거부하지 않는다.
- Navigation/실제 경로, 근접·경로 근접, 점유, 아군 사선, Capsule, 사거리 및 후보 사선 등 기존 하드 검증을 유지한다.
- `MinimumAcceptablePositionScore`: **선택 B**를 적용했다. 기존 `FinalScore + Risk` 비교를 보존하고 표시 이름을 **Minimum Base Position Score**로 명확히 했다. 기존 BP 값의 의미를 갑자기 Final Score 하한으로 바꾸지 않았다. 기본값은 0이다.
- `CandidateScoreRandomness`: 실제 사용 대신 AdvancedDisplay + DeprecatedProperty 처리했다. 실제 점수 jitter는 0이며 디버그에도 표시한다. 이번 작업은 random jitter를 새로 구현하지 않았다.
- Cover `ThreatMemorySeconds`: AdvancedDisplay + DeprecatedProperty. 기억 수명은 Sense/Brain 소유라고 설명했다.
- `GetTargetMemorySeconds`: native 호환 accessor는 유지하고 UE_DEPRECATED와 주석을 추가했다. 생산 호출이 없음을 정적으로 확인하고 구형 수명 소유 가정의 테스트 assertion을 제거했다.

## Snapshot Dirty와 피해 데이터

`bThreatSnapshotDirty`는 이제 `RefreshThreatSnapshot`의 실제 강제 갱신 조건이며 갱신 완료 후에만 해제한다. `bPositionReevaluationRequested`는 별도로 위치 재평가 요청을 처리한다. 피해 이벤트는 둘 다 설정하며, 피해 기억이 만료돼도 Snapshot을 무효화한다. 마지막 갱신/재평가 원인을 캐시해 표시한다.

무기/캐릭터 피해 생산 및 Source/Plugins 참조 검색에서 신뢰 가능한 익명 전투 증거를 생산하는 경로를 찾지 못했다. 따라서 다음과 같이 처리했다.

- 환경·낙하·출처 불명 피해는 타깃이나 익명 위험 Actor를 만들지 않는다.
- bUseAnonymousDamageDirection, AnonymousDamageThreatSeconds는 기존 이름의 Deprecated 프로퍼티를 유지하되 EditAnywhere를 제거해 Details 튜닝 옵션에서 숨겼다. 기존 Blueprint 접근용 프로퍼티는 남겼다.
- native FDMAnonymousDamageThreat, Snapshot/Brain 익명 배열, 익명 위험 계산, bAnonymousCombatEvidence를 제거했다.
- FDMDamageThreatRecord.LastKnownDamageDirection은 저장 외 소비가 없어 제거했다.
- AttackerController와 HitLocation은 삭제하지 않고 마지막 피해 디버그 출력에 실제 사용했다. HitPartName, DamageOrigin, DamageDirection도 사건 시점 디버그 증거로 유지한다.
- 해당 native 데이터 제거 후 Source/Plugins의 남은 참조를 정적으로 검색했다. 외부 미제공 모듈이나 별도 프로젝트의 호출자는 확인하지 않았다.

## Navigation Advanced 기본값

- StuckDetectionSeconds=2 seconds, ProgressDistanceThreshold=30 cm.
- MoveProjectionExtent=(100,100,250) cm.
- DefenseRiskValidationInterval=0.75 seconds, AttackPositionValidationInterval=0.6 seconds.
- PositionRiskCacheMaxEntries=32.
- ThreatSnapshotMovementInvalidationDistance=150 cm, ThreatLocationInvalidationDistance=150 cm.

새 검증 interval은 최소 0.05 seconds, 거리/extent는 필요한 지점에서 최소 1 cm, 캐시 크기는 1~256으로 제한한다. 0/음수는 무제한 재평가가 아니라 이 최소값을 사용한다. 각 설정 Tooltip에 단위와 특수값 의미를 적었다. 부동소수점 epsilon은 튜닝 항목으로 노출하지 않았다.

## BP 호환 및 유지한 Legacy

BP를 로드하지 않은 상황에서 안전한 migration을 입증할 수 없으므로, 저장된 UCLASS/UPROPERTY 이름을 삭제하거나 임의 이동하지 않는 보존 전략을 사용했다. 표시 이름, Tooltip, Category, Deprecated 메타데이터 변경만으로 호환이 실제 검증됐다는 뜻은 아니다.

- DMBehaviorModule_Attack, DMMoveGoalProvider_Approach, Approach/HuntLost, BP_DM_AttackModule 및 Controller 구형 subobject/property를 유지했다. 사용자 지시서의 참조 정보에 따라 삭제 대상에서 제외했다.
- FindSight, MoveThenFire, Strafe, Rush UCLASS와 EditInlineNew 선택지를 유지했다. Rush는 빠른 진입과 이동 사격형을 설정으로 구분한다.
- MovementModule/Priority2/Priority3, Instanced 플래그, 생성자 기본 subobject 이름과 기존 우선순위는 유지했다.
- Risk의 구형 프로퍼티 이름, Precision.MaxMoveSpeed, CurrentTargetStickinessScore를 유지했다.
- Fire Action ShotFired API는 native/test 호환을 위해 유지하고 역할을 주석으로 구분했다. 새 반복 실행기로 사용하지 않았다.
- 생산 발사는 기존 Ability/공통 Fire Cycle Runner → Brain.NotifyShotFired를 유지한다. Fire/FireCycle Notify, Montage, Weapon Actor의 사거리·거리별 확률·피해 감쇠 코드는 이번 작업에서 변경하지 않았다.

## dm.CombatDebug 2

기존 cached debug frame에 다음 정보를 추가/명확화했다.

- Perception Sensed와 Combat LOS를 별도 표시.
- Current/Pending 타깃, Snapshot의 sensed/visible/remembered 구분.
- Movement Action, MovementIntent, 현재 속도, DesiredCombatMoveSpeed, 실제 MaxWalkSpeed, Fire Action 속도 한계, 적용 속도 제한 원인.
- Fast/Emergency Intent의 사격 차단 이유.
- 실제 적용되는 정렬된 Caution/Dangerous/Critical 경계와 긴급 저체력·근거리·가중 노출 조건 결과.
- 전체 알려진 적 수, 위치 위험 평가 적 수, 현재 감지된 적의 가중 노출, 초과 위험 페널티.
- Expected/Surprise/Unknown, Damage-only defense 여부, 피해 기억 남은 시간, 공격자의 Snapshot Sensed/LOS, Threat/Target 보너스.
- 마지막 hit 위치와 instigator, Snapshot 갱신 원인, 위치 재평가 원인.
- CandidateScoreRandomness 실제 적용값 0/Deprecated, 위치 Final Score와 Base Score 하한 거부 의미.

디버그 캡처를 위해 추가 Trace, NavPath 또는 확률 Roll을 호출하지 않는다. 새 출력은 평가 중 저장한 상태와 기존 읽기 전용 관찰을 사용한다.

## 정적으로 확인한 호출 흐름

1. 캐릭터 피해 생산 → NotifyDamageReceived → 신뢰 가능한 적대 공격자 기록 → Snapshot dirty 및 위치 재평가 요청.
2. Tick/TryUpdateCombat → ProcessDamageReevaluation → RefreshThreatSnapshot → UpdateThreatAwareness → 피해 기억 Investigating Context.
3. EvaluateMemoryMovement → 현재 안전 판단 → 안전 시 Hold, 위험 시 기존 Investigation/방어 Action → 메모리 상태 Fire 금지.
4. DamageThreatMemorySeconds 만료 → PruneDamageThreats → 유효 피해 Context 없음 → ResetCombat → SituationFramework의 기존 Roam 판단.
5. Sense 감지 유지 → BuildContext의 별도 Combat LOS → 차단된 타깃도 EvaluateTargets 후보 → FindSight staged evaluator → FastReposition.
6. MoveThenFire 이동 Intent → ResolveVisibleMovement에서 보존 → bMovementAllowsFire=false → 기존 Fire gate/active cycle 정리 경로.
7. 사망/UnPossess/재Possess/Deactivate/EndPlay → 기존 ResetCombat(true) 또는 lifecycle 정리 → 저장 이동 속도 복원.

소스 텍스트 검사 89개를 통과했다. 범위는 수정 20개 파일의 lexical delimiter/충돌 마커/generated include 순서, 삭제 native 필드의 잔여 참조, 핵심 분기 연결, 신규 설정의 production 사용처다. 이는 C++ 문법·타입·링크·UHT·Blueprint 호환 검증을 대체하지 않는다.

## 사용자가 에디터에서 확인할 설정

- 기존 Movement Action 1~3 및 Instanced 값이 유지되는지 확인한다. FindSight를 검증할 BP에는 적절한 슬롯을 직접 지정한다.
- Combat|Movement Speeds의 0 fallback과 Fire Action의 Max Fire Move Speed/정지 사격 옵션을 함께 확인한다.
- RiskTierSettings의 역전 입력 시 실제 debug 경계가 정렬되는지 확인한다.
- Combat|Emergency Response의 세 토글을 각각 켜고 꺼 본다.
- TargetSelectionWeights 및 별도의 기존 CurrentTargetStickinessScore, TargetSwitchPositionReuseSeconds를 확인한다.
- Micro Move/Navigation/Cover Advanced 값을 확인한다.
- Apply Excess Risk Penalty, Soft Risk Limit, Minimum Base Position Score가 의도한 이름으로 보이는지 확인한다.
- Legacy randomness/ThreatMemory/Attack moving-fire 옵션의 Deprecated 표시와 익명 피해 옵션의 Details 숨김을 확인한다.
- Precision에 저장된 MaxMoveSpeed 값과 새 bAllowFireWhileMoving 기본값을 확인한다.

## 추후 PIE 시나리오 — 이번에는 실행하지 않음

1. Roam 중 미감지 적에게 피격: Sensed=false, LOS=false, WantAttack=false 유지. 안전한 장소에서는 정지 주시, 위험한 장소에서는 Investigation/방어 이동 확인.
2. 피격 후 공격자를 벽 뒤에서 이동: 기억 위치가 따라가지 않는지 확인. DamageThreatMemorySeconds 만료 후 Roam 및 속도 복원 확인.
3. 피해 기억 중 실제 감지: 정상 전투로 전환. 감지되지만 사선이 막힌 경우 Fire 금지 및 FindSight 진입 확인.
4. 감지는 유지하되 muzzle 앞에 장애물: LostGrace가 아니라 차단된 감지 전투로 유지되는지 확인.
5. FindSight 후보 없음/Path 실패/아군 사선/점유: 쿨다운 중 다음 Action 실행, LOS 복구 후 사격 복구 확인.
6. MoveThenFire: 이동 중 Fire 금지, 도착/timeout 후 HoldAndFire. 사거리 밖 첫 접근도 확인.
7. Fire Action 이동 허용 false 또는 매우 낮은 속도: 정지 사격, 사선/사거리 복구 이동, Fast/Emergency의 Fire 속도 비제한 확인.
8. Cover 각 phase와 Critical Partial Cover 거부, Escape Cover의 Emergency Intent 확인.
9. 여러 적 및 저체력: 긴급 토글 각각 검증. 기억만 남은 저신뢰 적의 노출로 직접 토글 조건이 켜지지 않는지 확인.
10. Micro Move: 오래 정지한 충분히 좋은 위치, 실패/예약 충돌, 역전 Min/Max, 좁은 Peek, Precision 안정화 상황 확인.
11. Target Hold/Confirm/Delay/Position Reuse를 각각 다르게 설정해 독립성 확인.
12. 교전 종료·사망·UnPossess·Respawn·Brain 비활성화·무기 교체에서 이동 속도와 예약, Fire 상태 정리 확인.
13. 환경/낙하/아군/출처 불명 피해가 공격 타깃을 만들지 않는지 확인.
14. dm.CombatDebug 2를 켜고 끌 때 추가 행동 평가·확률 Roll이 생기지 않는지 확인.

기존 제공 기능은 다중 적 위험 평가, 단계적 위치 탐색, Fire Cycle, 기억 및 피해 점수 기반이다. 이번 신규 변경은 이 흐름의 도달 불가능/정책 덮어쓰기 문제 연결, 설정 실제 사용 및 의미 정리, 피해 데이터 정리와 디버그 확장이다.
