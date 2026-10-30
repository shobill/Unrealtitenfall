# 파츠 비분리 표시 및 이동 사격 분석 보고서

- 작성일: 2026-09-15 16:18 KST
- 범위: `Source/SkySeoul/PartsCharacter/`, `Source/SkySeoul/DeathMatch/AI/Combat/`, `Source/SkySeoul/DeathMatch/Abilities/`, 최신 PIE 로그
- 구분: 진단

## 요청 또는 증상

1. `bSeparable=false`인데 머리뿐 아니라 팔도 떨어져 나간 것처럼 보인다.
2. AI가 이동 중에는 사격하지 못하고, 실제 발사에 실패한 뒤에도 쿨다운을 소비하여 다시 사격 가능한 순간을 놓치는 것으로 보인다.

## 결론

파츠는 최신 PIE 로그상 머리와 팔 모두 실제 물리 분리 경로를 타지 않았다. 각 파츠가 `bSeparable=false`인 상태에서 `mesh hide only (no physics)` 경로로 처리되었다. 현재 옵션은 “파괴된 파츠를 붙어 있는 모습으로 유지”가 아니라 “Debris로 분리하지 않고 숨김”을 의미한다. 화면에서 파츠가 사라지는 현상을 분리로 판단했다면 현재 코드의 의도된 표시 방식과 사용자가 원하는 표현이 다른 것이다.

사격 문제는 복합 원인이 확인됐다. Precision Fire는 이동 속도가 기본 `MaxMoveSpeed=20`을 넘으면 Tactics의 안정 조건과 `AllowsFire()` 양쪽에서 사격을 차단한다. `Move Then Fire`와 Cover도 특정 이동 단계에서 `bAllowFire=false`를 반환한다. 반면 새 Attack Position은 `bAllowFireWhileMoving=true`를 지원하지만 Precision의 속도 제한을 다시 통과해야 하므로 이것만 켜서는 이동 사격이 되지 않는다.

실패 후 쿨다운 추정도 코드상 맞다. Fire Ability는 몽타주를 시작하는 순간 `몽타주 전체 길이 + FireCooldown`을 미리 잠그고, 몽타주가 끝나면 실제 발사 성공 여부와 무관하게 다시 `FireCooldown`을 설정한다. 최신 PIE 로그에서는 `BP_DM_AI_Fire` 몽타주에 이름이 `Fire`인 유효 Notify가 없어 실제 발사가 발생하지 않은 채 이 흐름이 반복됐다.

## 확인 근거

### 파츠

최신 `Saved/Logs/SkySeoul.log`에는 다음 결과가 기록됐다.

- `Parts.Head`: `bSeparable=false — mesh hide only (no physics)`
- 좌우 Upper/Lower/Hand: `bSeparable=false — mesh hide only (no physics)`

`DestroyPartInternal`의 동작은 다음과 같다.

- `bSeparable=true`: `DetachPartAsDebris` 호출
- `bSeparable=false`: 원래 파츠 Mesh 숨김 및 충돌 비활성화
- 두 경우 모두 Owner Skeletal Mesh의 해당 Bone 또는 Bone Tree 숨김 처리가 별도로 수행될 수 있음

따라서 현재 확보된 증거로는 `bSeparable` 우회 물리 분리 버그보다 “비분리 파츠도 파괴 시 숨기는 정책”이 화면상 원인일 가능성이 높다.

### 이동 사격

- `FDMCombatMoveResult.bAllowFire` 기본값은 true다.
- `Move Then Fire`는 이동 단계에서 명시적으로 false를 반환한다.
- Combat Cover는 `Exposed` 단계에서만 true다.
- Attack Position은 `bAllowFireWhileMoving=true`일 때 현재 위치의 사선·아군 사선 조건이 유효하면 이동 중에도 true를 반환할 수 있다.
- 하지만 Precision Fire는 Pawn 속도가 `MaxMoveSpeed`보다 높으면 `bStable=false`로 만들고 `AllowsFire()`에서도 다시 거부한다. 기본값 20cm/s는 사실상 정지 사격 정책이다.

### 실패 후 재사격

- Fire Ability 활성화 시 `LockFireUntilMontageAndCooldown()`이 즉시 호출된다.
- 이 함수는 `NextFireAllowedTimeSeconds`를 현재 시각에서 몽타주 전체 길이와 `FireCooldown`만큼 뒤로 설정한다.
- 실제 발사는 몽타주 Notify 시점의 `ExecutePatternFire()` 성공 때만 `NotifyShotFired()`로 인정된다.
- 그러나 `FinishFromMontage()`는 `bFiredThisActivation=false`여도 `SetNextFireAllowedAfter(FireCooldown)`을 호출한다.
- 최신 로그에는 `montage ended with no 'Fire' notify (fallback off)`가 여러 번 반복됐다. 즉 발사하지 않은 몽타주가 재생되고 쿨다운까지 적용된 사례가 확인된다.

## 문제점 및 위험

- `bSeparable`이 물리 분리 여부만 제어하고 파괴 후 가시성은 제어하지 않아 옵션 이름만 보고 기대한 결과와 실제 결과가 다르다.
- 단순히 비분리 파츠를 계속 보이게 바꾸면 파괴 상태인데도 충돌, 피격 판정 또는 본 추종 상태가 남는 문제가 생길 수 있다.
- Precision의 속도 제한만 크게 올리면 달리는 속도에서도 완전한 정지 정확도로 사격할 수 있어 밸런스와 애니메이션이 어색해질 수 있다.
- 몽타주 시작 시 선잠금을 완전히 제거하면서 Ability 활성 상태 검사도 손상하면 같은 몽타주가 중복 실행될 수 있다.
- 유효 Notify 누락을 코드 fallback만으로 덮으면 실제 발사 시점이 애니메이션과 맞지 않을 수 있다.

## 의견 및 권장 설계

### 파츠

`bSeparable`은 그대로 물리 Debris 생성 여부만 담당하게 두는 것이 좋다. 사용자가 원하는 것이 “파괴돼도 몸에 붙은 외형을 유지”라면 bool 하나의 의미를 바꾸기보다 별도 파괴 표시 정책을 추가하는 편이 명확하다.

권장 정책 예:

- `DetachAsDebris`: 분리 후 물리 적용
- `HideInPlace`: 현재 `bSeparable=false` 동작
- `KeepAttachedDisabled`: 외형은 붙여 둔 채 기능·HP·피격만 파괴 상태로 전환

최소 변경을 원하면 `bHideWhenNonSeparableDestroyed` 옵션을 추가할 수도 있지만, 장기적으로는 enum 정책이 설정 실수를 줄인다. `KeepAttachedDisabled`에서는 물리와 Detach를 금지하고, 파괴된 파츠의 재피격 무시 및 기능 손상은 유지하되 Mesh와 필요한 Owner Bone을 숨기지 않아야 한다.

### 이동 사격

이동 모듈의 허가와 Fire Module의 속도 허가를 모두 통과해야 하므로 두 계층을 함께 정리해야 한다.

1. Attack Position, Maintain Distance, Strafe에는 이동 중 사격 허용 정책을 사용할 수 있게 한다.
2. Move Then Fire와 Cover는 행동 의미상 필요한 구간에서는 기존 차단을 유지한다.
3. Precision Fire의 `MaxMoveSpeed`를 “발사 가능 최대 속도”로 명확히 사용하고 BP에서 보행 속도에 맞게 조정한다.
4. 이동 중에는 무조건 정지 사격과 같은 정확도를 주지 말고 Weapon spread 또는 별도 이동 정확도 배율로 불리함을 주는 것이 좋다.
5. 이동 방향과 조준 방향이 달라도 상체 Aim Offset 또는 이동 사격 애니메이션이 대응 가능한지 BP에서 확인한다.

### 실패 후 즉시 재사격

재사용 대기시간은 “사격 Ability를 시도한 시점”이 아니라 “실제 탄이 발사된 시점”부터 소비하는 것이 적절하다.

권장 흐름:

```text
Fire 조건 충족
→ Ability 활성화 및 몽타주 재생
→ Fire Notify 도달
→ Notify 시점 조건 재검사
→ 실제 발사 성공: cadence/cooldown 소비
→ 실제 발사 실패: 무기 cadence/cooldown 미소비, Ability 종료 후 조건이 회복되면 즉시 재시도
```

- 몽타주 중복 방지는 Ability active 상태와 현재 몽타주 재생 여부가 담당한다.
- `NextFireAllowedTimeSeconds`의 장기 잠금은 실제 발사 성공 이후에만 적용한다.
- 실패한 활성화가 기존 `Max` 기반 시간 잠금을 남기지 않도록 성공 쿨다운과 활성 몽타주 재진입 방지를 분리한다.
- 무한 재활성화 방지를 위해 실패 쿨다운을 무기 쿨다운과 동일하게 두지 말고, 필요하면 0.05~0.1초 정도의 짧은 anti-spin 재평가 간격만 별도로 둔다.
- 우선 BP 몽타주에 정확한 `PlayMontageNotify` 또는 `UAnimNotify_DMNamed`를 추가하고 Notify Name을 `Fire`로 맞춰야 한다. 이 설정이 없으면 코드 정책을 고쳐도 정상 발사 타이밍이 존재하지 않는다.

## BP에서 확인할 사항

1. `BP_DM_AI_Fire`가 사용하는 실제 Fire Montage를 연다.
2. 발사 프레임에 `PlayMontageNotify` 또는 `DM Named Notify`가 있는지 확인한다.
3. Notify Name이 정확히 `Fire`인지 확인한다.
4. Precision Fire의 `MaxMoveSpeed` 현재 BP 값을 확인한다.
5. 활성 Movement Priority가 Attack Position, Maintain Distance, Strafe, Move Then Fire 또는 Cover 중 무엇인지 확인한다.
6. Attack Position의 `bAllowFireWhileMoving` 값을 확인한다.
7. 비분리 파츠가 “사라져야 하는지” 또는 “파괴된 채 붙어 있어야 하는지” 원하는 표시 정책을 결정한다.

## 검증 상태

- C++ 정적 분석: 완료
- 최신 PIE 로그 분석: 완료
- 파츠 물리 Debris 미생성 확인: 로그 및 기존 자동화 근거 있음
- 팔·머리 화면 표시가 Bone 숨김인지 실제 Detach인지 시각 확인: 필요
- Fire Notify 누락: 최신 PIE 로그에서 확인
- 이동 사격 및 실패 후 즉시 재시도 구현: 수행하지 않음
- BP/애니메이션 수정: 수행하지 않음
