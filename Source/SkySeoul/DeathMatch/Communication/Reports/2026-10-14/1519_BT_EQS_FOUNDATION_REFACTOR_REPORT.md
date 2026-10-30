# DeathMatch AI BT/EQS 기반 전환 결과

- 작성일: 2026-10-14 15:19 KST
- 범위: `DeathMatch/AI`, Robot AI Controller, Fire Ability 연결, Tactical Area Roam, Combat Debug, 자동화 테스트
- 구분: 구현 결과

## 요청 또는 증상

Situation Framework, Situation Brain, Behavior Module, Combat Tactics와 C++ 전투 위치선정 모듈이 행동 판단·위치 선택·사격·속도·기억을 나누어 소유했다. 이 구조를 제거하고 Root Behavior Tree가 행동 판단을, 목적별 EQS가 위치 선택을 소유할 수 있는 C++ 기반이 필요했다.

## 결론

Controller는 더 이상 Brain을 선택하거나 Situation Framework/Move Provider/Combat Tactics를 생성하지 않는다. 단일 `RootBehaviorTree`만 실행하며 미지정 시 Warning 후 안전하게 idle 상태를 유지한다.

전투 데이터, 사격 실행, 이동 속도, 이동 요청을 각각 Knowledge/Fire Control/Speed Policy/Move Director로 분리했다. BT Task/Service/Decorator 기반과 EQS Context/Named Parameter 계약을 추가했다. 기존 C++ 후보 생성 및 행동 상태 머신은 삭제했으며 자동 fallback을 남기지 않았다.

## 삭제한 파일과 클래스

Situation/Behavior 계층:

- `AI/Framework/DMSituationFramework.h/.cpp` — `UDMSituationFramework`
- `DMSituationBrainBase`, `DMSituationBrain_Combat`, `DMSituationBrain_RoamOnly`
- `DMBehaviorModuleBase`, `Attack`, `Hold`, `Move`, `PathMove`, `Approach`, `HuntLost`
- `DMAIFrameworkTypes.h`, `AI/Situation/DMSituationTypes.h`

Move Provider 계층:

- `DMMoveGoalProviderBase`, `Roam`, `Approach`, `HuntLost`의 `.h/.cpp`

Combat 판단·위치선정 계층:

- `DMCombatTacticsComponent.h/.cpp`와 분할 구현 `DMCombatTacticsDebug`, `DMCombatThreatMemory`, `DMCombatThreatEvaluation`, `DMCombatDamageAwareness`, `DMCombatEQS`
- `DMCombatModules.h/.cpp`, `DMCombatMovementDecision.cpp`
- `DMCombatAttackPosition.h/.cpp`
- `DMCombatCoverModule.h/.cpp`
- `DMCombatInvestigation.h/.cpp`
- `DMCombatMicroMovementModule.h/.cpp`
- `DMCombatWorldDebugSubsystem.h/.cpp`

삭제한 전용 테스트:

- Attack Position/Cover/Cover Fallback/Combat Modules/Movement Selection/Movement Speed/Combat Debug/Adaptive Fire/Weapon Range 전용 테스트와 TestProbe
- 기존 Architecture PIE/Ownership/Adaptive Fire PIE 테스트

삭제한 클래스를 같은 이름의 빈 호환 클래스로 남기지 않았다.

## 유지한 기반 시스템

- `DMCombatPositionRisk.h/.cpp`: `FDMPositionRiskInput` 기반의 Data-only 순수 평가 함수로 변경
- `DMCombatPositionRegistry.h/.cpp`: Controller/Actor Owner 예약 유지, Movement Module Context/Token 의존 제거
- `DMCombatThreatSnapshot.h`: Data-only snapshot 타입으로 유지
- `DMCombatHealthEvaluation.h`와 기존 순수 계산 테스트 유지
- `DMFireCycle.h`, `DMCombatFire_Adaptive.h/.cpp`, `DMGameplayAbility_Fire`, `DMFireAttempt`, `AnimNotify_DMNamed` 유지
- `DMNavigationSafety.h/.cpp`, Tactical Area 위험도/볼륨/Subsystem 유지

## 새로 만든 컴포넌트

### Combat Knowledge

- `AI/Knowledge/DMCombatKnowledgeComponent.h/.cpp`
- `AI/Knowledge/DMCombatKnowledgeTypes.h`

Perception의 보이는/기억 중인 위협, 현재 Target, 마지막 위치/감지 시각, Target Lost 시각, 피해 원점/방향/시각, Expected/Surprise 분류와 Threat Awareness를 보관한다. 공격·이동·엄폐·EQS를 실행하지 않는다.

### Combat Fire Control

- `AI/Fire/DMCombatFireControlComponent.h/.cpp`
- `AI/Fire/DMCombatFireTypes.h/.cpp`

BT의 명시적 `StartFire/StopFire` 요청만 처리한다. Target/Parts/가시성/무기/사거리/총구 경로/아군 오사/이동 허용/발사 cadence를 검사하고 Gameplay Ability를 실행한다. Adaptive/Single/Burst/Automatic cycle state는 Fire Control이 소유한다. 자동 전투 Tick은 없다.

### Movement Speed Policy

- `AI/Movement/DMMovementSpeedPolicyComponent.h/.cpp`
- `AI/Movement/DMMovementPolicyTypes.h`

Roam, MovementAction, ADS, Firing, ExternalStatus 배율을 Source별로 보관하며 최저 배율을 적용한다. 행동 종류를 스스로 판단하지 않는다.

## 새로 만든 BT 기반

Task:

- `BTTask_DM_SelectTacticalArea`
- `BTTask_DM_SelectRoamLocation`
- `BTTask_DM_MoveToGoal`
- `BTTask_DM_StartFire`
- `BTTask_DM_StopFire`
- `BTTask_DM_ClearCombatTarget`
- `BTTask_DM_ReserveCombatPosition`
- `BTTask_DM_ReleaseCombatPosition`

Service:

- `BTService_DM_UpdateAIState`
- `BTService_DM_UpdateCombatBlackboard`
- `BTService_DM_UpdateHealthRisk`
- `BTService_DM_UpdateFireState`

Decorator:

- `BTDecorator_DM_HasVisibleTarget`
- `BTDecorator_DM_HasRememberedTarget`
- `BTDecorator_DM_IsEmergency`
- `BTDecorator_DM_ShouldRetreat`
- `BTDecorator_DM_NeedsCombatCover`
- `BTDecorator_DM_CanFire`

`BTTask_DM_FaceTarget`은 Knowledge Target 기준으로 변경했다. `BTTask_DM_ActivateAbility`는 Fire Control Target/attempt를 사용한다. 기존 `BTTask_DM_SelectMoveLocation`의 Framework/Tactics 차단 의존성은 제거했다.

## Controller 최종 구성

`ADeathMatchRobotAIController` native subobject:

- AI Perception Component
- Sense Module
- Combat Knowledge Component
- Combat Fire Control Component
- Movement Speed Policy Component
- Combat Move Director

Blackboard/BehaviorTree 런타임은 `AAIController::RunBehaviorTree`가 관리한다. Controller 설정에는 `RootBehaviorTree` 하나만 남는다.

## Roam 계산 이전 결과

Tactical Area 위험도 값과 기존 가중식은 변경하지 않았다. `BTTask_DM_SelectTacticalArea`가 Danger fit, friendly/incoming 혼잡, 거리, 최근 방문, 현재 영역, 개인 danger bias를 계산한다. `BTTask_DM_SelectRoamLocation`은 선택 볼륨 안에서 complete/non-partial Nav 경로가 있는 random reachable point를 반환한다.

`UDMTacticalAreaSubsystem`은 Incoming AI 예약, AI별 최근 방문 영역, 영역 실패 cooldown을 보관한다. Move Task 성공/실패/Abort에서 예약을 정리하고 성공 시 최근 방문을 기록한다.

## Move Director 변경

Provider registry, 목표 생성·캐시, Roam/Combat 판단과 Tactics callback을 제거했다. BT가 제출한 한 건의 MoveTo 실행, 취소, 완료 serial/result만 보관한다. `BTTask_DM_MoveToGoal`은 Blackboard `MoveGoal`을 읽고 완료 결과를 반환하며 Abort 시 자신이 시작한 Request를 취소한다.

## EQS Context 및 계약

다음 Context는 `UDMCombatKnowledgeComponent`를 읽도록 변경했다.

- FocusingTarget
- VisibleEnemies
- RememberedEnemyLocations
- LastKnownThreatLocation
- DamageOrigin

FriendlyActors는 `DMCombatPositionRegistry`의 프레임 공유 snapshot을 사용하므로 Context마다 무제한 전역 검색하지 않는다.

`DMEQSParameters.h`에 다음 Named Parameter를 중앙화했다.

- `MaxMoveDistance`
- `PreferredDistance`
- `WeaponEffectiveRange`
- `CurrentLocationBonus`

EQS가 없거나 실패했을 때 C++ 후보 생성 fallback은 없다. 해당 BT Branch가 실패하고 Selector의 다음 Branch가 평가되어야 한다.

## Blackboard Key 목록

- `AIState`
- `CombatTarget`
- `TargetVisible`
- `HasRememberedTarget`
- `LastKnownTargetLocation`
- `LastDamageOrigin`
- `ThreatAwareness`
- `HealthRatio`
- `RiskTier`
- `SelectedTacticalArea`
- `MoveGoal`
- `MoveIntent`
- `EQSResultLocation`
- `ShouldADS`
- `ShouldFire`
- `FireWhileMoving`

기존 asset key 호환을 위해 `TargetActor`, `TargetLocation`, `HasTarget`, `CanShoot`, `FocusLocation`, `IsFocusSeen`, `MoveGoalLocation`, `HasMoveGoal`, `WantAttack` 이름 상수는 유지했지만 레거시 판단 owner는 남기지 않았다.

## 디버그 변경

Movement Module/Situation/Tactics 이름과 후보 상태 머신 중심 debug snapshot을 제거했다. HUD는 현재 Health, Combat Target, Threat Awareness, Move owner/goal, Fire state/failure, Speed multiplier를 새 컴포넌트에서 읽는다. Debug가 행동이나 EQS를 재실행하지 않는다.

## BP 및 에셋 마이그레이션

바이너리 문자열 검사에서 제거된 클래스 참조가 확인된 에셋:

- `Content/DeathMatch/AI/Controller/BP_DeathMatchAIController.uasset`
- `Content/DeathMatch/AI/Controller/BP_DeathMatchAIController_Adaptive.uasset`
- `Content/DeathMatch/AI/BrainModule/BP_DM_Brain_Combat.uasset`
- `Content/DeathMatch/AI/ActionModule/BP_DM_AttackModule.uasset`

에디터에서 수행할 작업:

1. 두 Controller BP를 열어 Missing Class/Invalid Property를 정리한다.
2. 기존 SituationFramework, CombatTactics, MoveProviderRoam/Approach/HuntLost 컴포넌트 또는 override를 제거한다.
3. native `CombatKnowledge`, `CombatFireControl`, `MovementSpeedPolicy`, `CombatMoveDirector`가 보이는지 확인한다.
4. `RootBehaviorTree`에 새 Root BT를 지정한다.
5. 새 Blackboard asset을 Root BT에 연결하고 위 Key의 타입을 맞춘다.
6. `BP_DM_Brain_Combat`, `BP_DM_AttackModule`은 더 이상 런타임에 연결하지 말고 참조 제거 후 삭제 여부를 결정한다.
7. 기존 `BT_Combat.uasset`은 제거된 노드/키 의존성을 점검한 뒤 새 Root 구조로 교체하거나 재작성한다.
8. Controller BP와 관련 Character/Map을 다시 열어 Compile/Save한다.

## 아직 만들지 않은 에셋

- `BT_DM_AI_Root`
- 새 DeathMatch Blackboard asset
- `EQS_DM_AttackPosition`
- `EQS_DM_CombatCover`
- `EQS_DM_Hide`
- `EQS_DM_Retreat`
- `EQS_DM_Investigate`

## 남은 레거시 참조

- C++ `.h/.cpp` 검색 결과 제거된 Situation/Brain/Behavior/Tactics/Movement Module/Move Provider 클래스 참조는 0건이다.
- 위 4개 `.uasset`에는 아직 제거된 클래스 문자열이 존재한다. 에디터 마이그레이션 전 상태로 의도적으로 수정하지 않았다.
- 과거 `Communication/Reports`, `Communication/Archive`, `plan` 문서에는 역사 기록으로 레거시 명칭이 남는다.
- 실제 Root BT/Blackboard/EQS asset이 없으므로 현재 Controller BP를 마이그레이션하기 전에는 RootBehaviorTree Warning 후 idle이 정상이다.

## 자동화 테스트

새 `SkySeoul.DeathMatch.BTFoundation` 4개: 4/4 성공, Exit Code 0.

- Controller.Components
- Knowledge.DamageOrigin
- PositionReservation.ControllerOwner
- SpeedPolicy.MinimumWins

기존 `SkySeoul.DeathMatch.Roam.TacticalArea` 5개를 새 BT Task 가중 함수 연결로 실행: 5/5 성공, Exit Code 0.

- Bounds
- Danger
- Overlap
- Reservation
- Weight

## 검증 상태

- 제거 클래스 C++ 참조 검색: 0건
- UnrealHeaderTool: 성공
- `SkySeoulEditor Win64 Development` C++ 빌드: 성공
- BT Foundation 자동화: 4/4 성공
- Tactical Area 자동화: 5/5 성공
- Blueprint 수정/저장: 수행하지 않음
- Root BT/Blackboard/EQS uasset 제작: 수행하지 않음
- PIE/실제 전투 동작 확인: 수행하지 않음

## 권장 후속 작업

1. Controller BP의 Missing component/property를 먼저 정리하고 `RootBehaviorTree`를 지정한다.
2. Blackboard와 Root BT Selector(Dead/Combat/LostTarget/Roam)를 만든다.
3. 목적별 EQS 5개를 Context/Named Parameter 계약에 맞춰 만든다.
4. Combat Target 선택·전환 정책을 BT Service/Task 또는 별도 data-only policy로 확정한다.
5. 각 Branch 실패가 다음 Branch로 정상 fallback하는지 PIE에서 확인한다.
6. Move Abort, Fire Control safety, EQS Context world integration 테스트를 에셋 연결 후 추가한다.
