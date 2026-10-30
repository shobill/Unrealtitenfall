# DM Has Clear Shot BT Decorator 구현 결과

## 요약

`UBTDecorator_DM_HasClearShot`과 Fire Control의 `HasClearShotToCombatTarget()` / `GetClearShotFailure()` 계약을 추가했다. 판정 대상은 Knowledge가 선택한 현재 `CombatTarget` 하나이며, 실제 무기 muzzle 기반 Fire Origin과 공통 Aim Location, 무기 Trace Channel, 최대 사거리, 월드/아군 차단 판정을 기존 `UDMCombatFireControlComponent::EvaluateLineOfFire()`로 재사용한다.

Decorator 표시 이름은 `DM Has Clear Shot`이고 기본 `Observer Aborts`는 `Both`, `Inverse Condition`은 false, 재평가 간격은 0.1초다. Decorator는 Fire Control 결과만 읽고 별도 전투 기준이나 전투 상태를 보관하지 않는다.

## 추가·수정·삭제 파일

### 추가

- `Source/SkySeoul/DeathMatch/Tests/DMClearShotTests.cpp`
- `Source/SkySeoul/DeathMatch/Communication/Reports/2026-10-14/1705_DM_HAS_CLEAR_SHOT_REPORT.md`

### 수정

- `Source/SkySeoul/DeathMatch/AI/Fire/DMCombatFireControlComponent.h`
- `Source/SkySeoul/DeathMatch/AI/Fire/DMCombatFireControlComponent.cpp`
- `Source/SkySeoul/DeathMatch/AI/BT/DMBTDataNodes.h`
- `Source/SkySeoul/DeathMatch/AI/BT/DMBTDataNodes.cpp`
- `Source/SkySeoul/DeathMatch/Presentation/HUD/DeathMatchHUD_CombatDebug.cpp`

### 삭제

- 없음

## Clear Shot 정확한 판정 항목

아래를 순서대로 모두 만족할 때만 `EDMClearShotFailure::None`, 즉 `true`다.

1. Fire Control owner가 `ADeathMatchRobotAIController`이고 유효한 Pawn을 소유한다.
2. 같은 Controller의 `UDMCombatKnowledgeComponent`가 선택한 유효한 `CombatTarget`이 있다.
3. `Knowledge->IsCombatTargetVisible()`가 true다. 다른 적이 보이는지만 나타내는 `HasVisibleTarget()`는 선택 타겟을 대신하지 않는다.
4. `AI->GetActiveRangeWeapon()`이 유효하다.
5. `ResolveCurrentFireOrigin()`으로 실제 weapon muzzle을 우선하는 Fire Origin을 구한다.
6. `EvaluateLineOfFire()`가 Fire Origin에서 `ResolveIntendedTargetLocation()`의 공통 Aim Location까지의 거리를 활성 무기 `GetMaxFireRange()`와 비교한다.
7. `ResolveWeaponTraceChannel()`의 실제 무기 채널로 trace했을 때 월드 구조물이 막지 않는다.
8. 같은 trace에서 아군이 선행 hit로 사선을 막지 않는다.

실패 결과는 정확히 `None`, `InvalidTarget`, `NotVisible`, `MissingWeapon`, `Range`, `Sight`, `FriendlyFireRisk` 중 하나다. Fire Origin을 만들 수 없는 context와 기타 사선 실패는 `Sight`로 축약한다.

## 기존 DM Can Fire와의 차이

`DM Can Fire`는 기존대로 `CanStartFire()` → `GetFireFailure()`를 사용하며 실제 발사 시점 승인용이다. 따라서 action intent/enable 상태, `AI->CanShoot()`, 이동 정책 및 속도, Fire Module cadence 등을 포함한다.

`DM Has Clear Shot`은 `CanStartFire()`, `GetFireFailure()`, `GetFireSafetyFailure()`를 호출하지 않는다. cadence와 Single/Burst/Automatic/Adaptive 주기, Fire Ability 활성/commit, 몽타주/notify, 현재 속도, 이동 중 사격 정책, ADS 상태 및 전환 완료 여부는 입력에 없고 결과를 바꾸지 않는다.

## 재사용한 Fire Control API

- `ResolveCurrentFireOrigin()` — 활성 무기 muzzle, 실패 시 Pawn View 계약
- `ResolveIntendedTargetLocation()` — `UDMAimTargetComponent`의 공통 전투 Aim Location
- `ResolveWeaponTraceChannel()` — Parts weapon channel 및 공통 fallback
- `EvaluateLineOfFire()` — 최대 사거리, 월드 blocking hit, intended target hit, friendly-fire blocker 분류
- `ClassifyLineOfFireHit()` / `IsFriendlyFireBlocker()` — 기존 실제 사격선 분류

전투 위치 후보 평가도 계속 `ResolveCandidateFireOrigin()`과 같은 `EvaluateLineOfFire()`를 호출하므로 현재 위치와 후보 위치가 Trace Channel/Aim Location/거리/차단 분류 계약을 공유한다.

## 조건 재평가 및 Abort 방식

- 기본 `ReevaluationInterval = 0.1f`; BT 편집기에서는 Advanced 항목이다.
- `bTickIntervals`와 `SetNextTickTime()`을 사용하므로 매 프레임 trace하지 않는다.
- 제한 tick마다 `ConditionalFlowAbort(...ConditionResultChanged)`를 호출한다.
- 기본 `FlowAbortMode = Both`여서 true→false일 때 Attack branch self-abort, false→true일 때 낮은 우선순위 Approach에서 Attack 재활성화 요청이 가능하다.
- `bCreateNodeInstance = true`여서 여러 AI가 Decorator UObject 런타임을 공유하지 않는다. tick 간격 메모리도 각 Behavior Tree component에 귀속된다.
- Decorator에는 거리, 채널, 사거리 수치나 판단 결과 cache를 두지 않는다.

## 디버깅

Fire Control은 마지막으로 제한 재평가된 `EDMClearShotFailure`만 진단 snapshot으로 보관한다. 기존 combat HUD는 추가 trace 없이 다음 compact 줄을 표시한다.

```text
ClearShot=true/false | Failure=None/InvalidTarget/NotVisible/MissingWeapon/Range/Sight/FriendlyFireRisk
```

재평가마다 반복 로그를 추가하지 않았다. HUD는 Decorator/Fire Control이 마지막으로 만든 결과를 읽기만 한다.

## 자동화 테스트

새 `SkySeoul.DeathMatch.Combat.ClearShot` 테스트 소스에 다음 계약을 추가했다.

- 선택 타겟/현재 visibility/활성 원거리 무기 precondition 및 다른 visible 적 비대체
- 공통 line classifier의 열린 사선, 최대 사거리, 월드 구조물, 아군 차단 결과
- cadence cooldown, 이동 중 사격 금지, Fire Ability 및 몽타주 상태 비의존성
- 기본 이름, `Observer Aborts: Both`, inverse false, 0.1초 간격, node instance
- Sight false 전환과 사선 회복 true 전환

2026-10-14 실행 결과(정확한 비-PIE prefix):

- `SkySeoul.DeathMatch.CombatPosition`: 18/18 성공
- `SkySeoul.DeathMatch.Combat.FireControl`: 5/5 성공
- `SkySeoul.DeathMatch.BTFoundation`: 10/10 성공

이번 작업의 제외 범위에 Editor Build가 포함되어 새 C++ 파일은 실행 당시 기존 Editor 바이너리에 아직 링크되지 않았다. 따라서 `SkySeoul.DeathMatch.Combat.ClearShot` 4개 테스트의 실행 결과는 미확인이다. 소스에는 추가됐으며 다음 Editor C++ 빌드 후 해당 prefix를 실행해야 한다.

참고로 최초에 넓은 `SkySeoul.DeathMatch.Combat` prefix를 실행해 제외 대상인 기존 PIE 테스트와 무관한 Parts 테스트까지 함께 발견되었고, 기존 `Combat.Fire.PIE` 및 `Combat.Parts.HitRouting` 실패가 있었다. 이 결과는 본 구현의 회귀 판정에서 제외했으며 이후 위 세 정확한 prefix만 각각 재실행해 모두 성공을 확인했다.

## 사용자가 BT에서 설정할 항목

`Visible Combat > Combat Action > Attack` Sequence의 첫 Decorator로 `DM Has Clear Shot`을 배치한다.

- `Observer Aborts`: `Both` (새 노드 기본값도 Both)
- `Inverse Condition`: false (기본값)
- `Reevaluation Interval`: 기본 0.1초 유지 권장
- Approach branch에는 반전 Decorator를 추가하지 않는다.
- `DM Can Fire`는 Attack/Approach 분기에 사용하지 않고 실제 발사 시점 검사용으로 유지한다.

요청 범위에 따라 BT/EQS/Profile/Blackboard uasset은 생성하거나 수정하지 않았다.

## 남은 미구현·미검증 사항

- 미구현 기능은 없다.
- Editor Build 제외로 새 `SkySeoul.DeathMatch.Combat.ClearShot` 자동화 테스트 실행은 남아 있다.
- BT uasset 편집 제외로 실제 자산에 Decorator를 배치하는 작업은 사용자가 수행해야 한다.
- Blueprint/PIE 검증은 수행하지 않았다(최초 broad prefix에 의해 기존 PIE 테스트가 의도치 않게 발견·실행된 사항은 위 테스트 절에 별도 기록).

