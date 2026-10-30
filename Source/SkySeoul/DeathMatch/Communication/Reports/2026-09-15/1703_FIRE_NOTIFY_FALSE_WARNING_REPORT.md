# Fire Notify 오진 경고 및 발사 재검사 실패 보고서

- 작성일: 2026-09-15 17:03 KST
- 범위: `Source/SkySeoul/DeathMatch/Abilities/DMGameplayAbility_Fire.*`, `Source/SkySeoul/DeathMatch/AI/Combat/`, `Saved/Logs/SkySeoul.log`
- 구분: 진단 및 이전 보고 정정

## 요청 또는 증상

사격 몽타주에 `Fire` Notify가 실제로 존재하지만 다음 경고와 함께 AI가 발사하지 않는다.

```text
DM Fire: ... montage ended with no 'Fire' notify (fallback off)
```

## 결론

이 경고는 Notify 누락을 확정하지 못한다. 현재 `FinishFromMontage()`는 Notify 수신 여부를 저장하지 않고 `bFiredThisActivation=false`만 확인하여 위 문구를 출력한다. 따라서 Notify가 정상 도달했더라도 Notify 시점의 `ExecutePatternFire()` 재검사에서 거절되면 동일한 잘못된 경고가 발생한다.

현재 코드에는 `bFireNotifyReceived` 또는 마지막 실패 원인이 없다. 최신 로그에는 별도의 `montage has no notify matching` 경고가 보이지 않는다. 그러므로 현재 증거상 “Fire Notify가 없다”보다 “Notify는 존재하지만 실제 발사 조건 재검사에서 실패했다”를 우선 조사해야 한다.

## 확인 근거

- `HandleFireFromNotify()`는 Notify 인덱스를 소비한 뒤 `ExecutePatternFire()`를 호출한다.
- `ExecutePatternFire()`가 false이면 `bFiredThisActivation`은 계속 false다.
- `FinishFromMontage()`는 `bFiredThisActivation=false`를 Notify 미수신으로 간주한다.
- `ExecutePatternFire()`는 Notify 시점에 Tactics의 `AllowsFire()`를 다시 호출한다.
- 이 재검사에는 이동 모듈 허가, 시야, 감지, 사거리, 조준 안정시간, 거리 확률, cadence와 Precision 이동 속도 제한이 포함된다.
- Precision의 기본 `MaxMoveSpeed=20cm/s`는 이동 중 재검사 실패 가능성이 높다.
- Ability는 몽타주 시작 시 `몽타주 길이 + FireCooldown`을 미리 잠그고 실제 발사가 없어도 종료 시 쿨다운을 갱신한다.

## 권장 수정

1. Notify 수신, 발사 시도, 실제 발사를 서로 다른 상태로 기록한다.
2. `ExecutePatternFire()`가 구조화된 실패 원인을 반환하거나 별도 진단 결과를 제공하게 한다.
3. 종료 로그를 `NoNotifyReceived`, `NotifyReceivedButBlocked`, `ShotExecuted`로 구분한다.
4. 실제 탄이 발사된 경우에만 정상 무기 cadence와 cooldown을 소비한다.
5. 실패 시에는 긴 무기 쿨다운 대신 필요할 경우 짧은 anti-spin 재평가 간격만 사용한다.
6. 이동 사격을 허용하려면 Movement의 `bAllowFire`와 Precision 이동 속도 제한을 함께 조정한다.
7. 기존 Notify 및 예약 타이머 중복 방지 기능은 보존한다.

## 검증 상태

- C++ 정적 분석: 완료
- 최신 로그에서 종료 경고 반복: 확인
- 실제 Montage 내 Notify 존재: 사용자 확인
- Notify 콜백 수신 여부와 정확한 차단 Gate: 현재 계측 부족으로 미확정
- 코드 및 BP 수정: 수행하지 않음
