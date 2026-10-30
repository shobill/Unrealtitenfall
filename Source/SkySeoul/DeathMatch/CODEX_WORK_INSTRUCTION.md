# DeathMatch `DM Has Clear Shot` BT Decorator 구현 지시서

## 목표

Visible Combat 내부에서 `Attack`과 `Approach`를 안정적으로 구분할 BT Decorator를 구현한다.

표시 이름은 `DM Has Clear Shot`으로 한다. 이 Decorator는 다음 질문에만 답한다.

```text
현재 위치에서 선택된 CombatTarget을 실제 무기 기준으로 공격할 수 있는가?
```

발사 쿨다운이나 몽타주처럼 순간적으로 변하는 발사 실행 상태는 판단하지 않는다.

## 판단 기준

다음을 모두 만족하면 `true`다.

1. AI Controller와 Pawn이 유효하다.
2. Knowledge의 선택된 `CombatTarget`이 유효하다.
3. 선택된 CombatTarget이 현재 보인다.
4. 유효한 활성 원거리 무기가 있다.
5. 타겟이 무기 최대 사거리 안에 있다.
6. 실제 Fire Origin에서 공통 Aim Location까지 사격선이 열려 있다.
7. 월드 구조물이 사격선을 막지 않는다.
8. 아군이 사격선을 막지 않는다.

하나라도 만족하지 않으면 `false`다.

## 반드시 제외할 판단

다음은 결과에 영향을 주면 안 된다.

- cadence 또는 발사 cooldown
- Single/Burst/Automatic/Adaptive 발사 주기
- Fire Montage와 Notify 상태
- Fire Ability 활성 여부
- 현재 이동 속도
- 이동 중 사격 허용 정책
- ADS 전환 완료 여부

기존 `CanStartFire()` 또는 `DM Can Fire` 결과를 그대로 재사용하지 않는다. 이 기능들은 실제 발사 시점의 일시적인 조건까지 포함하므로 Attack/Approach 상태 분기에 부적합하다.

## 공통 사격 판정 재사용

독립적인 LOS 규칙을 새로 만들지 않는다. 기존 `UDMCombatFireControlComponent`의 다음 계약을 재사용한다.

- 실제 Fire Origin
- Intended Target의 Aim Location
- 무기 Trace Channel
- 무기 최대 사거리
- 월드 차단
- 아군 사선 차단

필요하면 의미가 명확한 경량 API를 추가한다.

```text
HasClearShotToCombatTarget()
GetClearShotFailure()
```

이 API는 cadence와 이동 정책을 검사하지 않는다. 기존 안전 판정 함수가 같은 의미라면 중복 구현하지 말고 재사용한다.

현재 위치 Clear Shot과 후보 위치 EQS의 사격 가능 판정이 서로 다른 Trace 계약을 갖지 않게 한다.

## Decorator 구현

권장 클래스명:

```text
UBTDecorator_DM_HasClearShot
```

표시 이름:

```text
DM Has Clear Shot
```

기존 BT Data Decorator 구조를 따른다. Decorator는 상태를 저장하거나 새로운 기준으로 계산하지 않고 Fire Control과 Knowledge의 현재 사실을 읽는다.

Decorator 내부에 거리, Trace Channel, 사거리 수치를 복제하지 않는다.

## 재평가와 Abort

다음 전환이 자연스럽게 동작해야 한다.

```text
Clear Shot true  → Attack
Clear Shot false → Approach
```

Approach 도중 사선이 확보되면 우선순위가 높은 Attack으로 돌아갈 수 있어야 한다.

Custom Decorator가 `Observer Aborts`를 사용할 수 있게 조건 변경을 관찰한다. 매 프레임 무제한 Trace를 수행하지 않는다. 이벤트 기반 재평가가 어렵다면 제한된 주기로 재평가한다.

권장 기본 재평가 간격:

```text
0.1초
```

필요하면 `AdvancedDisplay` 변수로 제공한다. 여러 AI가 Decorator 런타임 상태를 공유하지 않도록 Node Instance 또는 Node Memory 규칙을 지킨다.

## BT 사용 방식

```text
Sequence: Visible Combat
├─ Decorator: DM Has Visible Target
├─ Task: DM Select Combat Target
└─ Selector: Combat Action
   │
   ├─ Sequence: Attack
   │  ├─ Decorator: DM Has Clear Shot
   │  ├─ Service: DM Action Intent = Attack
   │  ├─ Service: DM Maintain Focus = CombatTarget
   │  ├─ Service: DM Maintain Fire
   │  ├─ Task: DM Find Combat Position = Attack
   │  └─ Task: DM Move To Goal
   │
   └─ Sequence: Approach
      ├─ Service: DM Action Intent = FastReposition
      ├─ Service: DM Maintain Focus = CombatTarget
      ├─ Service: DM Maintain Fire
      ├─ Task: DM Find Combat Position = Approach
      └─ Task: DM Move To Goal
```

권장 Decorator 설정:

```text
Observer Aborts: Both
Inverse Condition: false
```

Approach에는 반전된 `DM Has Clear Shot`을 중복 배치하지 않는다. Attack 조건이 false가 되면 Selector의 다음 Branch가 실행되게 한다.

`DM Can Fire`는 이 상태 분기에 사용하지 않고 실제 발사 시점 검사 용도로 유지한다.

## 디버깅

기존 전투 디버깅에 다음 핵심 결과만 포함할 수 있게 한다.

```text
ClearShot=true/false
Failure=None/InvalidTarget/NotVisible/MissingWeapon/Range/Sight/FriendlyFireRisk
```

매 재평가마다 같은 로그를 반복하지 않는다. 결과나 실패 사유가 변경된 경우에만 출력하거나 기존 compact 상태 표시에 포함한다.

## 자동화 테스트

최소한 다음을 검증한다.

1. 유효하고 보이는 CombatTarget과 열린 사격선이면 true다.
2. 선택된 CombatTarget이 없으면 false다.
3. 다른 적이 보여도 선택된 CombatTarget이 없으면 false다.
4. 타겟이 기억에만 있고 현재 보이지 않으면 false다.
5. 활성 원거리 무기가 없으면 false다.
6. 무기 최대 사거리 밖이면 false다.
7. 월드 구조물이 막으면 false다.
8. 아군이 사선을 막으면 false다.
9. cadence cooldown 중이어도 다른 조건이 정상이면 true다.
10. 이동 중 사격 금지 상태로 이동 중이어도 다른 조건이 정상이면 true다.
11. 몽타주 또는 Fire Ability 상태가 결과를 바꾸지 않는다.
12. Clear Shot 변경 시 Attack에서 Approach로 전환할 수 있다.
13. Approach 중 Clear Shot이 확보되면 Attack으로 복귀할 수 있다.
14. 여러 AI가 Decorator 런타임 상태를 공유하지 않는다.

기존 회귀 테스트:

- `SkySeoul.DeathMatch.CombatPosition`
- `SkySeoul.DeathMatch.Combat.FireControl`
- `SkySeoul.DeathMatch.BTFoundation`

## 작업 제외

- Attack/Approach/Hide 위치 평가 변경
- 새 PositionPurpose 추가
- BT/EQS/Profile/Blackboard uasset 생성 또는 수정
- Tactical Area Roam 변경
- 추격 또는 Investigate 추가
- 발사 cadence 및 몽타주 로직 변경
- 체력·위험 상태 판단 변경
- Blueprint 테스트
- PIE 테스트
- Editor Build

Editor Build 및 Blueprint/PIE 테스트는 수행하지 않아도 된다.

## 완료 조건

- `DM Has Clear Shot`이 BT 편집기에서 배치 가능하다.
- 선택된 CombatTarget에 대한 안정적인 공격 기회만 판단한다.
- cadence, 몽타주, 이동 정책 때문에 결과가 흔들리지 않는다.
- 실제 Fire Control 및 EQS와 같은 Trace 계약을 사용한다.
- `Observer Aborts: Both`로 Attack과 Approach 전환이 가능하다.
- 관련 자동화 테스트가 통과한다.

## 결과 보고서

작업 완료 후 다음 경로에 결과 보고서를 작성한다.

```text
Source/SkySeoul/DeathMatch/Communication/Reports/2026-10-14/
```

보고서에는 다음을 포함한다.

- 추가·수정·삭제한 파일
- Clear Shot의 정확한 판정 항목
- 기존 `DM Can Fire`와의 차이
- 재사용한 Fire Control API
- 조건 재평가 및 Abort 방식
- 자동화 테스트 결과
- 사용자가 BT에서 설정해야 하는 항목
- 남은 미구현 사항
