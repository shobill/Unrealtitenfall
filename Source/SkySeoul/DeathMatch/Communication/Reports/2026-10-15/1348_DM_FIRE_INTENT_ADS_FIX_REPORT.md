# DeathMatch Fire Intent / ADS 수정 결과

작성일: 2026-10-15 (KST)

## 결론

Fire Control을 통과한 뒤 Fire Ability가 `AimZoom`에서 거부되던 원인과, 공격 중 최신 비전투 Intent가 `actionAllowsFire=0`을 만들 수 있던 요청 선택 문제를 수정했다. 실제 `BT_Combat`의 Attack 서비스 수명은 `StartFire`까지 포함하고 있었지만, Task 순서가 `StartFire → FindCombatPosition → MoveToGoal`이어서 `FindCombatPosition`이 Blackboard의 `ShouldADS`와 `FireWhileMoving`을 쓰기 전에 첫 발사가 실행됐다. Attack/FastReposition 서비스는 고정 실행 설정으로 바꿔 이 선행 값 의존을 제거했다.

AI Fire Ability Spec 2개는 의도적인 서로 다른 Ability가 아니라 동일한 `DA_DM_AS_AI → BP_DM_AI_Fire`가 캐릭터 기본 Ability Set과 활성 무기 Ability Set에서 각각 부여된 중복이었다. 캐릭터 기본 부여를 제거하고 무기 활성 소유 경로만 유지했다.

## 원인

1. `UDMActionIntentComponent::ApplyIntent()`가 실행 설정이 있으면 Intent의 기본 ADS 요구를 버리고 `bUseADSWhileMoving`을 현재 ADS 값으로 직접 사용했다.
2. `bUseADSWhileMoving=false`가 이동 중 정책이 아니라 정지 후까지 ADS를 끄는 값으로 동작했다.
3. Intent 요청 승자가 우선순위 없이 최신 Serial만 사용해, 수명이 잠깐 겹친 Roam/FastReposition 요청이 Attack을 가릴 수 있었다.
4. `BT_Combat` Attack Sequence는 첫 자식으로 `DM Start Fire`를 실행하고 그 뒤에 `DM Find Combat Position`을 실행했다. Blackboard 실행 설정은 첫 발사 시점에 아직 확정되지 않았다.
5. `BP_DeathMatchAIRobot.AbilitySets`와 `BP_DM_AIWeapon.AbilitySetWhileActive`가 모두 `DA_DM_AS_AI`를 부여했다.

## 코드 변경

- `UDMActionIntentComponent`
  - ADS 기본 요구와 이동 중 ADS 허용을 분리했다.
  - Attack, AttackMove, CombatCover, Retreat는 정지 상태에서 ADS를 요구한다.
  - 기본 이동 ADS는 Attack, AttackMove, CombatCover에서 허용하고 Retreat에서는 허용하지 않는다.
  - 실행 설정의 `bUseADSWhileMoving=false`는 이동 중에만 ADS를 끄며, 정지하면 해당 Intent의 기본 ADS가 복원된다.
  - Pawn 속도 변화에 맞춰 ADS를 갱신한다.
  - 요청 선택은 `Retreat > Hide > CombatCover > Attack/AttackMove > FastReposition > Observe > Investigate > Roam > None` 우선순위를 사용하고, 같은 우선순위 안에서만 최신 Serial로 결정한다.
- `UDMAimZoomComponent`
  - AI Intent를 해석하던 정책 함수를 제거했다. 이제 이 컴포넌트는 요청된 ADS 상태의 저장·표현만 담당한다.
- `UDMCombatFireControlComponent`
  - Attack/AttackMove/CombatCover만 기본 이동 사격을 허용한다. Retreat와 FastReposition은 기본 이동 사격을 허용하지 않는다.
  - Fire Ability를 호출하기 전에 AimZoom이 준비되지 않았으면 `AimSettle`로 대기한다. Ability의 ADS 검사는 우회하지 않았다.
- `FDMActionIntentExecutionSettings`, BT Service, Combat Position Profile
  - `bUseADSWhileMoving`가 이동 중 전용 정책임을 주석과 Details 툴팁에 명시했다.

ADS 상태 소유권은 ActionIntent 정책 → AimZoom 상태 적용 → Fire Control/Fire Ability 검증 순서로 정리했다. Fire Ability나 `StartFire()`가 Attack Intent 또는 ADS를 임의로 강제하지 않는다.

## BT_Combat 확인 및 설정

에셋: `/Game/DeathMatch/AI/BT/BT_Combat`

Attack Sequence의 `DM Action Intent`와 `DM Maintain Fire`, `DM Start Fire`가 같은 `BTComposite_Sequence_0`에 붙어 있어 Attack 브랜치가 끝나거나 Abort될 때까지 요청 ID가 유지되고, 종료 시 서비스의 `OnCeaseRelevant()`에서 해당 요청만 제거된다.

저장한 Attack 서비스 Details:

- Action Intent: `Attack`
- Override Execution Settings: `true`
- Use ADS While Moving: `true`
- Allow Fire While Moving: `true`
- Use Blackboard Execution Settings: `false`

저장한 Approach/FastReposition 서비스 Details:

- Action Intent: `FastReposition`
- Override Execution Settings: `true`
- Use ADS While Moving: `false`
- Allow Fire While Moving: `false`
- Use Blackboard Execution Settings: `false`

향후 Blackboard 방식으로 되돌릴 경우 `ShouldADS`와 `FireWhileMoving`은 Attack Sequence 진입 전, 최소한 `DM Start Fire`보다 먼저 기록해야 한다. 현재 Task 순서에서는 Blackboard 방식을 다시 켜면 같은 선행 값 문제가 재발한다.

## Fire Ability 부여 경로

수정 전:

- `BP_DeathMatchAIRobot.AbilitySets[0] = DA_DM_AS_AI`
- `BP_DM_AIWeapon.AbilitySetWhileActive = DA_DM_AS_AI`
- 두 경로 모두 `BP_DM_AI_Fire`를 부여

수정 후:

- `BP_DeathMatchAIRobot.AbilitySets`는 비움
- `BP_DM_AIWeapon.AbilitySetWhileActive = DA_DM_AS_AI` 유지
- 무기 선택 시 `RefreshActiveWeaponAbilities()`가 부여하고, 교체/해제 시 추적된 `ActiveWeaponAbilityHandles`로 회수하는 기존 수명 계약 유지

수정 전 에셋 백업:

- `Saved/DMAssetBackups/20261015_134716_FireIntentADS/`

## 자동화 및 정적 검증

- `SkySeoul.DeathMatch.BTFoundation.ActionIntent.BranchLifecycle`
  - 최신 Roam/FastReposition이 활성 Attack을 가리지 않는지 추가 검증
  - Hide처럼 더 높은 전투 안전 우선순위는 Attack을 선점하고, 종료 후 Attack이 복원되는지 검증
- `SkySeoul.DeathMatch.BTFoundation.ActionIntent.ADSPolicy` 추가
  - 이동 ADS override가 정지 ADS 요구를 제거하지 않는지 검증
  - Attack/AttackMove/CombatCover/Retreat의 기본 ADS와 이동 ADS 구분 검증
  - Attack과 FastReposition/Retreat의 이동 사격 기본 정책 구분 검증
- `inspect_fire_intent_ads.py`로 에셋을 읽기 전용 재감사했다.
  - AI 무기 쪽 Fire Ability Set 한 경로만 남음
  - Attack 고정 설정 `true/true`, FastReposition `false/false`, Blackboard override 비활성 확인
- `configure_fire_intent_ads.py`는 대상과 기존 중복 상태를 assert하고 백업 후 수정하도록 작성했다.

요청 범위에 따라 Unreal Editor 빌드, C++ 자동화 실행, Blueprint 테스트, PIE는 수행하지 않았다. 새 C++ 자동화는 다음 Editor 빌드 후 실행해야 한다. 에셋 감사 commandlet은 정상 종료했다.

## 변경 파일

코드/테스트/문서:

- `AI/Execution/DMActionIntentComponent.h`
- `AI/Execution/DMActionIntentComponent.cpp`
- `AI/Execution/DMActionIntentTypes.h`
- `AI/BT/DMBTExecutionServices.h`
- `AI/Combat/DMCombatPositionProfile.h`
- `AI/Fire/DMCombatFireControlComponent.cpp`
- `Weapons/DMAimZoomComponent.h`
- `Weapons/DMAimZoomComponent.cpp`
- `Tests/DMBTFoundationTests.cpp`
- `Tests/inspect_fire_intent_ads.py`
- `Tests/configure_fire_intent_ads.py`
- 이 보고서

DeathMatch 에셋:

- `Content/DeathMatch/AI/BT/BT_Combat.uasset`
- `Content/DeathMatch/AI/Character/BP_DeathMatchAIRobot.uasset`

삭제한 파일은 없다. 다른 게임 모드, Roam/EQS 위치 선정, 사거리·가시성·아군 사선·파츠·cadence 정책은 변경하지 않았다.

## 남은 PIE 확인 항목

1. Attack 첫 `DM Start Fire`에서 ADS가 켜지고 `gate=AimZoom`이 발생하지 않는지 확인한다.
2. Attack Sequence 실행 중 `actionAllowsFire=1`이 유지되고, Abort/종료 후 ADS와 해당 Intent 요청이 해제되는지 확인한다.
3. FastReposition 중 ADS/이동 사격이 꺼지고 Attack 재진입 시 다시 켜지는지 확인한다.
4. 런타임 ASC에서 `Ability.DeathMatch.Fire`에 해당하는 AI Fire Spec이 1개인지 확인한다.
5. 무기 교체/해제/재선택 후 Fire Spec이 각각 회수·재부여되며 1개를 넘지 않는지 확인한다.
