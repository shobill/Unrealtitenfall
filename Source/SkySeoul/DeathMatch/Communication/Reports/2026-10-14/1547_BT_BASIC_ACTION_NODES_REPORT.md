# DeathMatch BT 기본 행동 노드 구현 결과

- 작성일: 2026-10-14 15:47 KST
- 범위: `DeathMatch/AI/BT`, Knowledge, Combat Status, Action Execution, Targeting, Fire Control, Movement Speed Policy, Robot AI Controller, AimZoom, 자동화 테스트
- 구분: 구현 결과

## 요청 또는 증상

BT가 내부 전투 상태를 다시 계산하거나 Knowledge가 자동으로 CombatTarget을 선택하면 상태 소유권이 겹친다. 즉시 성공형 StartFire와 직접 Focus 설정은 Branch Abort 뒤 실행 효과가 남을 수 있다. 전투 위치 선택/EQS를 추가하지 않은 채, BT가 상태를 읽고 행동 의도만 선택하며 실행 효과가 Branch 수명에 맞춰 정리되는 기본 노드가 필요했다.

## 결론

BT와 내부 실행 시스템의 경계를 다음과 같이 정리했다.

```text
Sense / Knowledge / Combat Status
→ 감지·기억·현재 타겟·체력·사망·위험·조건 보관

Behavior Tree
→ 상태 조회, Target Selection 실행 시점, ActionIntent 선택

AimZoom / Movement Speed Policy / Fire Control / Focus / Move Director
→ ADS·속도·사격·Focus·이동 실행

Branch 종료 또는 Abort
→ 해당 Branch가 만든 Intent·Focus·Fire 요청만 해제
```

전투 위치 후보 생성, EQS Query/Context/Test, Cover/Hide/Retreat/Investigate 위치 선택은 구현하지 않았다. 기존 Tactical Area Roam은 변경하지 않았다.

## 추가한 파일

- `AI/Execution/DMActionIntentTypes.h`
- `AI/Execution/DMActionIntentComponent.h/.cpp`
- `AI/Execution/DMCombatFocusComponent.h/.cpp`
- `AI/State/DMCombatStatusTypes.h`
- `AI/State/DMCombatStatusComponent.h/.cpp`
- `AI/Targeting/DMCombatTargetSelectionPolicy.h/.cpp`
- `AI/BT/DMBTExecutionServices.h/.cpp`
- `AI/BT/BTTask_DM_SelectCombatTarget.h/.cpp`
- `AI/BT/BTTask_DM_DisengageCombat.h/.cpp`
- `Tests/DMFireControlPolicyTests.cpp`
- 본 결과 보고서

## 수정한 파일

- `AI/Knowledge/DMCombatKnowledgeComponent.h/.cpp`
- `AI/Fire/DMCombatFireControlComponent.h/.cpp`
- `AI/Movement/DMMovementPolicyTypes.h`
- `AI/Movement/DMMovementSpeedPolicyComponent.h/.cpp`
- `AI/BT/DMBTDataNodes.h/.cpp`
- `AI/BT/LOGIC_STATUS.md`
- `Controller/DeathMatchRobotAIController.h/.cpp`
- `Weapons/DMAimZoomComponent.h/.cpp`
- `Tests/DMBTFoundationTests.cpp`

삭제한 파일은 없다. 기존 StartFire/StopFire, Reserve/ReleaseCombatPosition, SelectMoveLocation과 기존 EQS Context는 직렬화 및 에셋 호환을 위해 유지했다. 새 기본 구조에서는 MaintainFire와 DisengageCombat을 우선 사용한다.

## 상태 소유권

- `UAIModule_Sense`: 실제 감지와 기억 원본
- `UDMCombatKnowledgeComponent`: 적 후보 snapshot, 현재 CombatTarget, 마지막 확인 위치, 피해 원점/방향/시각, Threat Awareness
- `UDMCombatStatusComponent`: 현재 체력 비율, 사망, RiskTier, AIState와 Emergency/Retreat/Cover/Investigate/Disengage 조건
- `UDMCombatFireControlComponent`: 무기·사거리·LOS·아군 오사·cadence·발사 모드·이동 사격 허용 검사

Knowledge는 0.1초 내부 Tick에서 Sense snapshot을 갱신한다. UpdateCombatBlackboard는 갱신을 유발하지 않고 저장된 값만 복사한다. UpdateAIState와 UpdateHealthRisk도 Combat Status 값을 복사할 뿐 자체 기준으로 재계산하지 않는다. Decorator는 내부 컴포넌트의 캐시와 경량 getter만 읽는다.

## 행동 의도와 정책 적용 구조

행동 의도는 다음 10개다.

- None
- Roam
- Observe
- Attack
- AttackMove
- FastReposition
- CombatCover
- Hide
- Retreat
- Investigate

`DM Action Intent` Service의 에디터 설정은 `ActionIntent` enum 하나다. 활성화 시 `UDMActionIntentComponent`가 요청 ID를 만들고, 가장 최근 활성 요청을 AimZoom·Movement Speed Policy·Fire Control에 전달한다.

- ADS 여부: `UDMAimZoomComponent::IntentUsesADS`
- 실제 속도 배율: `UDMMovementSpeedPolicyComponent::GetActionIntentMultiplier`
- 사격 및 이동 중 사격 허용: `UDMCombatFireControlComponent::IntentAllowsFire/IntentAllowsMovingFire`

따라서 BT 노드에는 속도 배율, ADS 배율, 이동 사격 수치가 없다. Service 종료/Abort 시 자기 요청만 제거하고 이전 활성 Intent가 있으면 그 정책을 복원한다.

## 타겟 선택 구조

Knowledge의 자동 선택 코드(`Sense->GetFocusTarget()`을 CombatTarget으로 대입)를 제거했다.
`HasVisibleTarget`/`HasRememberedTarget`은 아직 선택되지 않은 후보도 감지할 수 있도록 Knowledge 후보 집합을 조회한다. Fire Control은 별도 `IsCombatTargetVisible` 검사로 실제 선택된 CombatTarget만 발사 대상으로 인정한다.

`DM Select Combat Target` Task가 다음 순서로 동작한다.

1. Knowledge의 `FDMRememberedThreat` 후보를 읽는다.
2. `UDMCombatTargetSelectionPolicy`를 호출한다.
3. Visible 보너스, 현재 타겟 유지 보너스, 마지막 감지 위치까지의 거리 비용으로 점수를 비교한다.
4. 선택된 결과만 Knowledge CombatTarget과 Blackboard에 설정한다.

정책은 재사용 가능한 별도 UObject이며 BT Task는 실행 시점만 결정한다.

## Focus와 Fire의 Branch 생명주기

`DM Maintain Focus`는 CombatTarget, LastKnownTargetLocation, MoveGoal을 지원한다. Focus 컴포넌트가 요청 ID와 activation serial을 보관하므로 Branch 종료 시 그 요청만 제거하고 이전 Focus 요청을 복원한다. Knowledge의 타겟이나 기억은 변경하지 않는다.

`DM Maintain Fire`는 Branch 활성 중 Fire Control 요청을 유지하고, Fire Control이 실제 발사 가능 여부를 계속 판정한다. Branch 종료/Abort 시 마지막 요청이 제거되면 `StopFire()`가 활성 Fire Ability를 취소한다. ActionIntent가 사격 금지로 바뀌는 경우도 즉시 StopFire한다.

Fire Control이 유지하는 검사는 다음과 같다.

- 유효 Target과 visible evidence
- CanShoot/무기 존재
- 최대 사거리
- 시야 차단과 아군 오사
- cadence
- Single/Burst/Automatic/Adaptive cycle
- Fire Action과 ActionIntent의 이동 중 사격 허용

## 행동 종료 정리

`DM Disengage Combat`은 다음 실행 효과만 정리한다.

- Fire 요청과 활성 Fire Ability
- ActionIntent가 적용한 ADS와 속도 Source
- 강제 Focus 요청
- Combat Move Director 요청
- Combat Position Registry 예약
- CombatTarget

적 감지 후보, 적 기억, 마지막 감지 위치, 피해 정보, 체력/위험 상태는 삭제하지 않는다. CombatTarget을 지운 뒤 Knowledge는 남아 있는 기억에 따라 WatchingLastKnown 상태를 유지할 수 있다.

## Roam 유지 여부

`DM Select Tactical Area`, `DM Select Roam Location`, `DM Move To Goal`과 Tactical Area 위험 가중식은 수정하지 않았다. Roam Branch에는 `ActionIntent=Roam`만 추가해 사용할 수 있다.

## 사용자가 BT에서 배치할 노드

최소 Root BT는 다음과 같이 구성할 수 있다.

```text
Root Selector
├─ Dead [DM Is Dead]
├─ Combat [DM Has Visible Target]
│  ├─ DM Select Combat Target
│  ├─ Service: DM Action Intent = Attack
│  ├─ Service: DM Maintain Focus = CombatTarget
│  └─ Service: DM Maintain Fire
├─ Lost Target [DM Should Investigate]
│  ├─ Service: DM Action Intent = Observe
│  └─ Service: DM Maintain Focus = LastKnownTargetLocation
└─ Roam
   ├─ Service: DM Action Intent = Roam
   ├─ DM Select Tactical Area
   ├─ DM Select Roam Location
   └─ DM Move To Goal
```

추가 Decorator는 DM Has Remembered Target, DM Has Valid Combat Target, DM Is Emergency, DM Should Retreat, DM Needs Combat Cover, DM Can Fire, DM Should Disengage다. 전투 종료 시 명시적 정리가 필요한 Branch에는 DM Disengage Combat을 배치한다.

## 테스트와 결과

직접 수행한 검증:

- UnrealHeaderTool: 성공
- `SkySeoulEditor Win64 Development`: 성공
- `SkySeoul.DeathMatch.BTFoundation`: 8/8 성공, Exit Code 0
  - ActionIntent Branch 종료 시 이전 정책 복원
  - Controller 구성
  - MaintainFire 요청 Abort 정리
  - MaintainFocus 요청 Abort 정리
  - 피해 정보 보존
  - 위치 예약 기본 규칙
  - 속도 정책 minimum-wins
  - Target Selection 점수 정책
- `SkySeoul.DeathMatch.Combat.FireControl`: 5/5 성공, Exit Code 0
  - Adaptive 거리 구간과 최대 사거리
  - 아군 오사 blocker 분류
  - 이동 중 사격 허용/차단
  - Single/Burst/Automatic 설정
  - Weapon effective/max range 계약
- `SkySeoul.DeathMatch.Roam.TacticalArea`: 5/5 성공, Exit Code 0
  - Bounds, Danger, Overlap, Reservation, Weight

참고로 전체 `SkySeoul.DeathMatch.Combat` 묶음도 실행했다. 신규 FireControl 4개(아군 오사 테스트 추가 전)는 모두 성공했지만, Root BT asset 미지정 상태를 전제로 하지 않은 기존 `Combat.Fire.PIE`와 이번 범위 밖 기존 `Combat.Parts.HitRouting`이 실패해 전체 Exit Code는 1이었다. 이 두 실패를 신규 정책 테스트 실패로 계산하지 않았다.

## 제외한 위치 선택/EQS 작업

다음은 만들거나 변경하지 않았다.

- 전투 위치 후보 생성과 NavMesh 후보 생성
- EQS Query/Context/Test 추가
- Attack/Cover/Hide/Retreat/Investigate/Micro Movement 위치 선택
- 구조물·엄폐·노출도 분석
- 기존 EQS Context 확장 또는 삭제

## 후속 EQS 작업 순서

1. Controller BP의 `RootBehaviorTree`와 새 Blackboard key 타입을 연결한다.
2. 위 최소 Root Selector를 구성하고 Branch Abort 시 ADS/속도/Focus/Fire 복원을 PIE에서 확인한다.
3. Attack Position EQS를 먼저 추가하고 현재 위치 유지/최소 개선 조건을 검증한다.
4. Combat Cover, Hide, Retreat EQS를 위험 단계와 ActionIntent Branch에 연결한다.
5. Investigate EQS를 마지막 확인 위치 Context에 연결한다.
6. 각 EQS 실패 시 C++ 위치 fallback 없이 Selector 다음 Branch로 넘어가는지 확인한다.

## 검증 상태

- C++/UHT/Editor Build: 성공
- 신규 BT Foundation 자동화: 8/8 성공
- 신규 Fire Control 정책 자동화: 5/5 성공
- 기존 Tactical Area Roam 자동화: 5/5 성공
- Blueprint/Behavior Tree asset 수정: 수행하지 않음
- PIE 기반 Branch 전환 검증: 수행하지 않음
- 위치 선택/EQS 구현: 수행하지 않음
