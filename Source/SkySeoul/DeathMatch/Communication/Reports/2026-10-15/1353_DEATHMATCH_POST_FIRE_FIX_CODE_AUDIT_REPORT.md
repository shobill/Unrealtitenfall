# DeathMatch Fire 수정 후 전체 코드 점검 보고서

- 작성일: 2026-10-15 13:53 KST
- 범위: `Source/SkySeoul/DeathMatch`, `Content/DeathMatch/AI/BT`, 최신 Fire Intent/ADS 결과 보고서
- 구분: 점검

## 결론

최신 Fire Intent/ADS 수정 코드는 에디터 빌드에 성공했고, Fire Ability 중복 부여도 결과 보고서와 에셋 감사 로그상 한 경로로 정리되었다. 다만 현재 실제 BT에는 **지속 사격 Service와 즉시 사격 Task가 동시에 존재하는 중복 실행 구조**가 남아 있다. 이것은 단순 정리 대상이 아니라 사격 실패를 다시 만들 수 있는 최우선 수정 사항이다.

전체 코드에서는 현재 사용 중인 Tactical Area 방식 이전에 만들었던 Tactical Grid와 Tactical Nav Region 체계가 다량 남아 있다. Grid는 런타임 CVar 기본값으로 정지하지만, Nav Region은 현재 소비자가 없는데도 World Subsystem으로 자동 생성되어 NavMesh 분석과 주기 갱신을 수행한다. 이전 실험 체계와 관련 BT 노드·Blackboard 별칭도 컴파일 대상에 남아 있어 정리 가치가 크다.

## 주요 발견

### P1. `DM Maintain Fire`와 `DM Start Fire`가 같은 공격 브랜치에서 중복 실행됨

- 최신 결과 보고서가 실제 `BT_Combat` Attack Sequence에 `DM Maintain Fire` Service와 첫 자식 `DM Start Fire` Task가 함께 있다고 확인했다.
- `DM Maintain Fire::OnBecomeRelevant()`는 `BeginFireRequest()`를 호출하고, 이 함수는 즉시 `RefreshFireRequest()`를 거쳐 `StartFire()`까지 호출한다.
- 직후 첫 Task인 `DM Start Fire`가 같은 `StartFire()`를 다시 호출한다.
- 첫 호출이 성공해 Fire Ability가 활성 상태라면 두 번째 호출은 `TryActivateDeathMatchAbility()`의 `FireAlreadyActive` 조기 반환으로 실패한다.
- `DM Start Fire` Task는 false를 BT `Failed`로 반환하므로 Sequence가 중단되고 Service 종료가 Fire 요청을 해제·취소할 수 있다.
- 두 호출의 실행 순서나 첫 호출 당시 Intent/ADS 준비 여부에 따라 성공과 실패가 바뀔 수 있어, 사용자가 겪은 “가끔 쏘고 대부분 안 쏨”과 일치하는 경쟁 구조다.

권장: Attack Sequence에서는 지속 소유권을 가진 `DM Maintain Fire` Service만 남기고 `DM Start Fire` Task를 제거한다. 반대로 Task 기반 단발 구조를 선택할 경우 Service를 제거해야 하며 둘을 함께 두면 안 된다. 현재 자동/점사/연사와 브랜치 수명 설계에는 Service 단일 소유가 더 적합하다.

### P1. 사용되지 않는 Tactical Nav Region Subsystem이 런타임에서 실제 작업함

- `UDMTacticalNavRegionSubsystem`을 사용하는 비테스트 런타임 코드는 검색되지 않았다.
- 관련 Volume의 DeathMatch 에셋 참조도 검색되지 않았다.
- 하지만 `UTickableWorldSubsystem`이므로 World마다 자동 생성된다.
- 초기화 시 Navigation Generation delegate를 등록하고, Tick에서 Recast polygon 수집, 영역 분할·병합, 연결 생성, Spawn reachability 계산을 수행한다.
- Ready 이후에도 주기적으로 transient data와 runtime score를 다시 계산한다.
- 현재 Roam은 `UDMTacticalAreaSubsystem`과 `ADMTacticalAreaVolume`을 사용하므로 이 계산 결과는 소비되지 않는다.

권장: Tactical Nav Region을 폐기하기로 확정했다면 Subsystem, Types, Volume, Event Profile 및 전용 테스트를 함께 제거한다. 보존할 계획이라면 최소한 명시적 enable 설정과 `ShouldCreateSubsystem()` 또는 Tick guard를 두어 기본 상태에서는 생성·빌드·Tick하지 않게 한다.

### P2. 폐기된 Tactical Grid 코드와 Move Goal 메타데이터가 남아 있음

- `UDMTacticalGridSubsystem`은 Deprecated이고 `dm.LegacyTacticalGridRuntime=0`으로 기본 Tick이 차단되어 있어 현재 직접 런타임 비용은 낮다.
- 비테스트 런타임 소비자는 없고, Grid/Analysis Volume 에셋 참조도 검색되지 않았다.
- 그런데 `DMMoveTypes.h`는 여전히 `DMTacticalGridTypes.h`를 include하며 다음 필드를 유지한다.
  - `bUsesTacticalGrid`
  - `TacticalCell`
  - `TacticalPurpose`
  - `TacticalPath`
  - `ControlledApproachDirection`
  - `MakeTacticalRoam()`
- 이 때문에 폐기된 대형 타입 체계가 Move 계층에 계속 전파되고 빌드·유지보수 범위를 키운다.

권장: Grid를 복구할 계획이 없다면 Subsystem/Types/AnalysisVolume/전용 테스트와 `FDMMoveGoal`의 Grid 필드를 한 번에 제거한다. Tactical Area 필드는 현재 실제 Roam 경로에서 사용되므로 유지한다.

### P2. 현재 BT 에셋에서 참조되지 않는 구형 BT 노드가 다수 존재함

에셋 문자열 참조와 소스 연결을 함께 검사했을 때 다음 클래스는 현재 DeathMatch BT 에셋에서 사용되지 않는다.

- Fire 중복 계열: `UBTTask_DM_ActivateAbility`, `UBTTask_DM_StopFire`, `UBTDecorator_DM_CanFire`
- 이동 구형 계열: `UBTTask_DM_SelectMoveLocation`
- 위치 예약 수동 계열: `UBTTask_DM_ReserveCombatPosition`, `UBTTask_DM_ReleaseCombatPosition`
- 단발 유틸리티: `UBTTask_DM_ClearCombatTarget`, `UBTTask_DM_FaceTarget`
- 사용하지 않는 상태 동기화 Service: `UpdateAIState`, `UpdateCombatBlackboard`, `UpdateHealthRisk`, `UpdateFireState`
- 미사용 Decorator: `HasValidCombatTarget`, `IsEmergency`, `NeedsCombatCover`, `ShouldInvestigate`, `ShouldDisengage`

이 중 Update Service 계열은 “BT가 상태를 갱신하지 않고 런타임 컴포넌트의 상태를 읽는다”는 현재 설계와도 맞지 않는다. 다만 UCLASS 제거는 저장된 외부 테스트 에셋이나 향후 사용 계획을 먼저 확인한 뒤 단계적으로 수행해야 한다.

### P2. Blackboard 키 정의에 현재 사용하지 않는 과거 별칭이 다수 남음

현재 BT 에셋에서 확인되지 않거나 해당 노드가 미사용인 키:

- `AIState`, `LastDamageOrigin`, `ThreatAwareness`, `HealthRatio`, `RiskTier`
- `EQSResultLocation`, `ShouldFire`
- legacy alias인 `TargetActor`, `HasTarget`, `CanShoot`, `FocusLocation`, `IsFocusSeen`, `MoveGoalLocation`, `HasMoveGoal`, `WantAttack`, `MoveIntent`

`TargetLocation`은 현재 Lost Target 계열 에셋 참조가 있어 바로 제거하면 안 된다. `ShouldADS`와 `FireWhileMoving`도 현재 Action Intent Service의 직렬화 필드에 남아 있으므로, 고정 override만 사용할지 결정하기 전에는 유지해야 한다.

### P3. ADS 정책 갱신을 위해 AI마다 매 프레임 Tick함

- `UDMActionIntentComponent`가 Pawn 속도만 확인하기 위해 매 프레임 Tick한다.
- 기능상 오류는 아니며 현재 AI 수에서는 부담이 작다.
- 다만 `UDMAimZoomComponent`도 별도로 매 프레임 Tick하므로 ADS 하나에 두 개의 Tick 경로가 있다.
- 이동 시작/정지 이벤트 또는 낮은 주기 갱신으로 바꿀 수 있지만 지금 당장 우선순위는 낮다.

### P3. Debug Observer 생성자 경고

실행 로그에 `Default__DMDebugObserverPawn`에서 초기화 전 `SetReplicates` 호출 경고가 발생한다. 생성자에서 `SetReplicates(false)`와 `SetReplicateMovement(false)`를 호출하기 때문이다. 기능 장애는 아니지만 반복 경고를 없애려면 생성자에서 `bReplicates` 및 복제 이동 기본값을 올바른 초기화 방식으로 지정해야 한다.

### 도구 상태. 현재 Git 명령이 정상 작동하지 않음

`Unreal` 폴더에서 `git status` 실행 시 Git이 공백이 포함된 `C:/Users/aldkz/Desktop/SkySeoul` 경로를 잘못 처리하며 실패했다. 코드 문제는 아니지만 변경 추적과 안전한 정리에 방해가 된다. 대규모 삭제 전에 저장소의 worktree/core.worktree 설정을 먼저 정상화할 필요가 있다.

## 최신 Fire Intent/ADS 작업 검토

확인된 정상 사항:

- `bUseADSWhileMoving=false`가 정지 ADS까지 끄지 않도록 `IntentRequiresADS`와 이동 ADS 정책이 분리되었다.
- Intent 요청은 단순 최신 요청이 아니라 정책 우선순위 후 동일 우선순위 Serial 비교로 변경되었다.
- Fire Control은 Ability 호출 전 `AimSettle`을 반환하여 Fire Ability 내부 `AimZoom` 거부까지 들어가지 않게 했다.
- AI Fire Ability Set은 캐릭터 기본 부여가 제거되고 활성 무기 소유 경로만 남았다는 에셋 감사 로그를 확인했다.
- Attack Service는 고정 `Attack / ADS 이동 허용 / 이동 사격 허용`, FastReposition은 고정 false/false로 저장된 로그를 확인했다.

주의 사항:

- 하드코딩된 Intent 우선순위는 현재 정책으로는 일관되지만 에디터 조절값은 아니다.
- `Retreat`는 ADS와 정지 사격을 허용하지만 이동 사격은 기본 금지다. 의도한 후퇴 방식인지 PIE에서 확인해야 한다.
- 위 정상 수정과 별개로 `Maintain Fire + Start Fire` 중복은 반드시 정리해야 한다.

## 검증 상태

- `SkySeoulEditor Win64 Development` 빌드: 성공.
- UnrealHeaderTool: 성공.
- 최신 에셋 감사 로그: AI Fire Ability Set 단일 무기 경로, Attack/FastReposition 고정 실행 설정 확인.
- 자동화 테스트 실행: 확인 실패. 실행 시 최신 로그가 `inspect_fire_intent_ads.py` 감사 프로세스의 `QUIT_EDITOR` 기록으로 끝나 테스트 결과가 생성되지 않았다. 따라서 새 `BTFoundation.ActionIntent` 테스트는 빌드만 되었고 실행 성공으로 간주하지 않는다.
- BP/PIE: 미실행.

## 권장 정리 순서

1. `BT_Combat`에서 `DM Start Fire`를 제거하고 `DM Maintain Fire` 하나만 발사 수명 소유자로 유지한다.
2. PIE에서 실제 사격, Attack Abort 시 정지, 재진입 시 재발사를 확인한다.
3. Tactical Nav Region을 기본 비활성화하거나 완전히 제거한다.
4. Tactical Grid와 `FDMMoveGoal`의 Grid 잔재를 함께 제거한다.
5. 미사용 BT 노드와 Blackboard 키를 에셋 감사 후 묶음별로 삭제한다.
6. Debug Observer 경고와 Git worktree 설정을 별도 소규모 정리로 처리한다.

