# DeathMatch 사격 쿨다운 단일화 결과

## 결과

AI 사격의 정상 발사 주기 단일 권한자를 `UDMCombatFireControlComponent::FireCycle.NextShotTime`으로 통일했다. Fire Ability의 별도 시간 게이트는 AI Fire Control 경로에서 사용하지 않으며, Fire Module의 `NextShotTime`은 Fire Control이 동기화하는 호환 관찰값으로만 남겼다.

정상 발사 주기와 실패 재시도는 다음처럼 분리했다.

- 성공 사격 주기: `FireCycle.NextShotTime`
- 실패 재시도: `UDMCombatFireControlComponent::RetryAfterTime`
- 플레이어/Fire Control 없는 레거시 경로: `UDMGameplayAbility_Fire::LegacyNextFireAllowedTimeSeconds`

## 구현 요약

### Fire Control 공용 상태

`UDMCombatFireControlComponent`에 다음 공용 읽기/판정 API를 추가했다.

- `GetNextShotAllowedTime()`
- `GetRetryAfterTime()`
- `GetTimeUntilFireReady()`
- `IsFireCycleReady()` / `IsFireCycleReadyAt()`
- `IsFireTimingReadyAt()`

`GetFireFailure()`, Fire Request 갱신, MaintainFire의 활성화 시도 여부가 이 상태를 사용한다. 정상 cadence 대기는 예상 상태로 취급하여 Warning을 남기지 않는다.

`RefreshFireRequest()`는 요청을 유지하되 Fire Ability가 비활성이고 공용 시간 게이트가 준비된 경우에만 `StartFire()`를 호출한다. 따라서 0.05초 서비스 Tick은 의도만 유지하며 cooldown 동안 GAS 활성화를 호출하지 않는다.

### Ability 이중 쿨다운 제거

기존 `NextFireAllowedTimeSeconds`는 `LegacyNextFireAllowedTimeSeconds`로 역할을 명확히 바꿨다.

- AI Fire Control 경로: `FireControl->AllowsFire()`의 공용 상태만 사용한다.
- 플레이어/Fire Control 없는 경로: 기존 `FireCooldown`과 레거시 시간을 유지한다.
- AI Fire Control 경로에서는 성공 사격 뒤 GAS Ability cooldown도 적용하지 않는다. BP에 Cooldown Gameplay Effect가 설정되어 있어도 공용 `FireCycle.NextShotTime`과 경쟁하지 않는다.
- Ability 시작 시 Fire Module 캐시를 `ActiveCycle->NextShotTime`에 다시 복사하던 쓰기를 제거했다.
- AI 실패 시 Ability의 레거시 시간을 갱신하지 않고 Fire Control의 `RetryAfterTime`을 갱신한다.

### 성공/실패 커밋

정상 cadence는 다음 순서에서만 커밋된다.

`FireAbility Activated -> FireNotify Received -> ExecuteFire 성공 -> NotifyShotFired -> RecordSuccessfulShot -> FireCycle.NextShotTime`

Ability 활성화, Montage 시작, Notify 대기만으로는 `NextShotTime`이 바뀌지 않는다. Notify 전 취소, Montage/Notify 오류, 사선/이동 조건 실패, `ExecuteFire` 실패는 정상 ShotInterval/Pause를 소비하지 않는다.

실패 재시도 정책은 Fire Control에 집중했다.

- Movement/MoveSpeed/AimSettle: 별도 지연 없이 다음 서비스 Tick에서 재평가
- Sight/FriendlyFireRisk: 0.1초 재평가 지연
- 일반 일시 실패: 0.075초 재평가 지연
- MissingWeapon/InvalidTarget/TargetDead/AvatarDead: 관련 조건 변화까지 정지
- Montage/Fire Notify/FireCycle Notify 구성 오류: Fire Control reset까지 정지

타깃 교체와 무기/Avatar 조건 회복은 해당 조건 기반 정지를 해제한다.

### Fire Module 호환 상태

Fire Module의 `NextShotTime`, `IsReady()`, `ShotFired()`는 기존 native 호환을 위해 남겼다. AI 실행 경로의 판정 원본으로는 읽지 않는다.

Fire Control은 다음 시점에 호환 복사본을 원본과 동기화한다.

- 새 Ability cycle 승인
- 실제 성공 사격 커밋
- Fire Control reset

Adaptive의 Blueprint 읽기 전용 상태도 `ObserveCycle(FireCycle)`로 같은 값을 관찰한다.

## 모드별 흐름

- Single: 성공 한 발 뒤 `max(ShotInterval, SinglePause)`를 공용 `NextShotTime`에 기록한다. 기존 FireCycle Notify 생략 호환을 유지한다.
- Burst: 한 Ability 안에서 Fire/FireCycle Notify를 반복한다. 중간 탄은 `ShotInterval`, 마지막 탄은 `max(ShotInterval, BurstPause)`를 기록한다.
- Automatic: 한 Ability 안에서 반복하며 중간 탄은 `ShotInterval`, 제한 탄 수가 있는 그룹 마지막 탄은 `max(ShotInterval, AutomaticPause)`를 기록한다. 무제한 설정은 계속 ShotInterval을 사용한다.
- Adaptive: 거리로 Single/Burst/Automatic 설정을 선택한 뒤 동일한 `FDMFireCycleRuntime::RecordSuccessfulShot()` 경로를 사용한다.

Burst/Automatic 내부 다음 Montage는 Fire Notify가 `NextShotTime`에 도달하도록 미리 시작한다. 내부 cycle 승인에서는 새 Ability 활성화 cadence를 다시 소비하지 않으며, Notify 직전에는 동일한 `ActiveCycle->NextShotTime`을 재확인한다.

## 로그

일반 `LogDM`에는 다음 사건을 AttemptId/CycleId와 함께 남긴다.

- `FireCycle Ready`
- `FireAbility Activated`
- `FireNotify Received`
- `ShotExecuted result=true/false reason=...`
- `FireCooldown Committed next=...`

기존 `DM Fire Start ... AbilityRejected` 일반 로그와 Ability의 `gate=ShotCooldown` Warning을 제거했다. 상세 Ability gate 로그는 combat diagnostic gate 아래에만 남는다.

## 자동화 및 정적 검증

- `SkySeoulEditor Win64 Development` 빌드: 성공
- `SkySeoul.DeathMatch.Combat.FireControl`: 7/7 성공
  - 새 `CadenceAuthority`: Single/Burst/Automatic 성공 시점과 간격/휴지 계약 검증
  - 새 `RetryTimingIndependent`: 성공 cadence와 실패 retry 독립성 및 AI별 상태 독립성 검증
  - 기존 AdaptiveRange/FriendlyFire/MovingFire/Patterns/WeaponRange 성공
- `SkySeoul.DeathMatch.BTFoundation.Fire.MaintainFireOwnership`: 성공
  - cooldown 중 여러 refresh에도 Ability 시작 호출 0회
  - cooldown 종료 뒤 같은 Fire Request로 다음 시작
- AI Fire Control 경로의 `ApplyCooldown()` 정적 검증: GAS Ability cooldown 미적용, 플레이어/레거시 경로만 유지
- 쓰기 지점 정적 검색: AI Ability의 `ActiveCycle->NextShotTime` 직접 쓰기와 AI 레거시 cooldown 쓰기 없음

전체 `SkySeoul.DeathMatch.Combat`도 참고 실행했다. 이번 관련 테스트 외 별도 영역 실패가 두 개 있었다.

- `SkySeoul.DeathMatch.Combat.Fire.PIE`: 테스트 환경에서 실제 AI shot 수 0
- `SkySeoul.DeathMatch.Combat.Parts.HitRouting`: 기존 damage routing 기대값 불일치

두 실패는 이번 FireControl 단위/소유권 테스트와 별도이며, 지시 범위에 따라 수정하지 않았다.

## 남은 BP 확인 사항

Editor/PIE 수동 검증은 수행하지 않았다. 다음은 BP에서 확인이 필요하다.

- `BP_DM_AI_Fire` Montage에 `Fire` Notify가 있고 반복 모드에는 그 뒤 `FireCycle` Notify가 있는지
- Single Montage의 FireCycle 생략 호환이 의도된 자산에만 사용되는지
- Adaptive 거리별 선택값과 ShotInterval/BurstPause/AutomaticPause가 기존 BP 값과 같은지
- 실제 PIE 로그에서 `fireFailure=None` 직후 Ability `ShotCooldown` 거절이 사라졌는지
- 지속 Fire Request에서 cooldown 종료 후 다음 `FireCycle Ready`가 한 번만 발생하는지
