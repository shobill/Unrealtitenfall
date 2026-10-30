# DeathMatch AI 사격 거리 판정 단일화 결과 보고

## 결과

AI Fire Control 경로의 사거리 판정을 Fire Control 공용 계약 하나로 통일했다. 이제 Ability 활성화와 실제 Fire Notify 실행은 활성 Fire Control + Fire Module 경로에서 별도의 Actor 원점 거리 공식을 실행하지 않으며, 실제 발사 순간 `GetFireSafetyFailure()`가 동일한 총구·조준점·무기 최대 사거리 계약을 다시 평가한다.

## 최종 공용 계약

- Fire Origin: 활성 무기 `GetMuzzleLocation()`, 실패 시 Pawn `GetPawnViewLocation()`
- Aim Location: `ResolveIntendedTargetLocation(Shooter, IntendedTarget, FireOrigin)`
- Maximum Range: 활성 무기 `ADMWeaponActor::GetMaxFireRange()`
- 허용 조건: `Distance <= MaximumRange + 2cm`
- 거리 결과: `FDMFireRangeEvaluation`에 Start, End, Distance, MaximumRange, bAllowed 저장
- 사선: 공용 거리 결과가 허용된 경우에만 무기 Trace Channel로 평가

`MaxFireRange == 0`이면 `GetMaxFireRange()`가 `GetEffectiveRange()` 이상을 반환하는 기존 계약을 유지한다. AI/Ability 코드에서 `MaxFireRange` 원본 필드를 직접 읽는 호출부는 제거했다. `FallbackFireRange`는 활성 무기를 찾지 못한 Ability 레거시 경로에서만 `ResolveMaxFireRange()`를 통해 사용된다.

## 제거·분리한 중복 판정

### `UDMGameplayAbility_Fire::ActivateAbility()`

기존 `Avatar ActorLocation -> Target ActorLocation` 검사를 제거했다. 활성 Fire Control + Fire Module은 직전 Fire Control 승인을 신뢰하고, 플레이어/비활성 Fire Control 경로만 `EvaluateLegacyRangeFailure()`를 사용한다. 레거시 검사도 Ability의 Fire Start와 공용 Aim Location/거리 평가 함수를 사용한다.

### `UDMGameplayAbility_Fire::ExecutePatternFire()`

기존 `Avatar ActorLocation -> Target ActorLocation` 검사를 제거했다. AI 경로는 Fire Notify 순간 `CycleSafetyFailure()` 또는 `GetFireFailure()`로 Fire Control을 재평가한다. 레거시 경로만 동일 공용 거리 평가를 사용한다.

### `UDMGameplayAbility_Fire::CycleSafetyFailure()`

Fire Control 검사 뒤 다시 실행하던 Adaptive `InRange(ActorLocation distance)`와 Ability Trace Channel 기반 별도 사선 트레이스를 제거했다. Fire Control이 실제 무기 총구, 공용 Aim Location, `GetMaxFireRange()`, 무기 Trace Channel을 한 번에 소유한다. 타겟이 Notify 전에 이동하면 이 재평가에서 실제 사거리 밖인 경우에만 Range로 거절되고 기존 `RecordFailedFireAttempt()` RetryAfter 흐름으로 들어간다.

## 진단 로그

Fire Control의 실제 Range 실패에만 다음 형식으로 기록한다.

```text
ShotRangeRejected attempt=... start=... end=... distance=... max=... weapon=... fireControl=1
```

동일 AI/타겟은 1초 동안 반복 출력하지 않는다. 정상 발사에는 거리 상세 로그를 추가하지 않았다.

## 전투 위치 평가

`DM Find Combat Position`의 Attack/Approach 후보는 후보 Fire Origin에서 공용 Aim Location까지의 거리를 사용한다. 절대 발사 가능 여부는 계속 무기 `GetMaxFireRange()`가 결정한다.

`UDMCombatPositionProfile::MaximumRangeSafetyMargin`을 추가했으며 기본값은 75cm다. Attack/Approach 후보 필터에만 적용하고 Hide에는 적용하지 않는다. 기존 `RangeFitness`는 Effective Range 안쪽의 편안한 구간을 최고점으로 유지하고 Maximum Range 경계로 갈수록 0점에 가까워진다.

설정 위치:

- `DA_DM_AttackPositionProfile` > Evaluation > Range > Maximum Range Safety Margin
- `DA_DM_ApproachPositionProfile` > Evaluation > Range > Maximum Range Safety Margin

새 속성의 클래스 기본값 75cm가 두 기존 Data Asset에 적용되며, 각 Asset에서 목적별로 조절할 수 있다.

## 자동화 및 정적 검증

- `SkySeoulEditor Win64 Development`: 성공 (UHT 포함)
- `SkySeoul.DeathMatch.Combat.FireControl`: 9/9 성공
  - 새 `RangeSingleAuthority`, `AbilityRangeRouting` 포함
- `SkySeoul.DeathMatch.CombatPosition`: 21/21 성공
  - Maximum Range 경계 점수/필터와 75cm 여유 포함
- `SkySeoul.DeathMatch.Combat.ClearShot`: 4/4 성공
  - Sight/FriendlyFireRisk 분류 유지
- Fire Ability Actor 원점 기반 Range 검사 잔존 여부: 없음
- AI/Ability의 raw `MaxFireRange` 필드 읽기: 없음
- `FallbackFireRange` 읽기: `ResolveMaxFireRange()`의 무기 없는 fallback 한 곳만 존재

참고로 넓은 `SkySeoul.DeathMatch.Combat` 회귀 묶음에서는 이번 범위의 테스트가 모두 성공했지만, 기존 `SkySeoul.DeathMatch.Combat.Parts.HitRouting` 1건이 데미지 기대값 0 문제로 실패했다. 이번 변경 파일 및 사거리 판정과 무관하며, 별도 범위로 남겼다.

## PIE 확인 항목

1. Attack 상태에서 `FireCycle Ready -> FireAbility Activated -> ShotExecuted result=true` 흐름이 실제 사거리 안에서 반복되는지 확인한다.
2. `ShotExecuted result=false reason=Range` 발생 시 `ShotRangeRejected`의 start/end/distance/max가 실제 최대 사거리 밖인지 확인한다.
3. 최대 사거리 경계에서 Attack/Approach 위치가 75cm 안쪽 후보를 우선/선택하는지 확인한다.
4. 타겟이 Fire Notify 직전에 최대 사거리 밖으로 이동하면 발사가 거절되고 Approach 또는 위치 재선정으로 전환되는지 확인한다.
5. 벽과 아군이 사선을 가릴 때 기존 Sight/FriendlyFireRisk 거절이 유지되는지 확인한다.

Unreal Editor 수동 실행과 BP 수동 편집은 수행하지 않았다.
