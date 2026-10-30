# DeathMatch 사격 실행 소유권 중복 제거 결과 — 2026-10-15

## 결과

`/Game/DeathMatch/AI/BT/BT_Combat`의 Attack Sequence에서 `DM Start Fire` Task를 제거했다. Attack Branch의 `DM Action Intent = Attack`, `DM Maintain Focus`, `DM Maintain Fire`는 동일한 Attack composite의 Service로 유지되며, 사격 시작·재시도·종료 수명은 `DM Maintain Fire` 하나가 소유한다.

Attack의 런타임 Task 순서는 다음과 같다.

```text
DM Find Combat Position
DM Move To Goal
```

`DM Maintain Fire`의 최초 `StartFire()`가 Action Intent/ADS 적용 전 또는 AimSettle, MoveSpeed, Cadence, 일시적 시야 변화로 실패해도 요청은 제거되지 않는다. Service Tick은 Fire Ability가 비활성일 때만 같은 Request ID를 refresh하므로 조건 회복 후 다음 발사를 다시 시도한다. 활성 Fire Ability는 이미 요청을 만족한 정상 상태로 취급해 GAS 재활성화를 요청하지 않는다.

## 코드 수명 규칙

- 동일 Service 메모리에 Request ID가 이미 있으면 새 요청을 만들지 않는다.
- `OnCeaseRelevant()`는 Service 메모리의 Request ID만 `EndFireRequest()`하고 메모리를 0으로 만든다.
- `EndFireRequest()`는 실제로 제거한 요청이 마지막 요청일 때만 `StopFire()`를 호출한다.
- 다른 유효 요청이 남아 있으면 Fire Ability를 유지한다.
- Fire Ability 한 사이클 종료는 요청 ledger를 제거하지 않는다. Attack Branch가 유지되면 다음 Tick이 같은 요청으로 재시도한다.
- 전투 이탈은 `DM Disengage Combat::ClearFireRequests()`, 사망·빙의 해제·재빙의 초기화는 `DeathMatchRobotAIController::ResetFireControl()` 경로에서 모든 요청과 Fire Ability를 정리한다.
- 단순 전투 위치 선정 또는 Move Task 완료는 Fire Request를 제거하지 않는다.

## 변경 파일

- `Content/DeathMatch/AI/BT/BT_Combat.uasset`
  - Attack Sequence의 `DM Start Fire` 그래프 노드 제거 및 저장
- `Source/SkySeoul/DeathMatch/AI/Fire/DMCombatFireControlComponent.h/.cpp`
  - 활성 Fire Ability를 정상 유지 상태로 처리
  - request 보유/활성 상태에 따른 재시도 조건 명시
  - request 수와 재시도 정책을 자동화 테스트에서 관찰할 수 있게 계약 노출
- `Source/SkySeoul/DeathMatch/AI/BT/DMBTExecutionServices.h/.cpp`
  - Service 메모리당 한 요청만 생성하는 idempotent begin/refresh
  - 활성 Fire Ability 중 Tick 재요청 차단
  - Service 소유 Request ID만 종료
- `Source/SkySeoul/DeathMatch/Tests/DMBTFoundationTests.cpp`
  - Maintain Fire 요청 수명 및 `BT_Combat` 단일 실행자 계약 테스트 추가
- `Source/SkySeoul/DeathMatch/Tests/DMFireControlTestComponent.h`
  - Gameplay Ability 없이 request/stop 계약을 검증하는 결정론적 테스트 probe 추가
- `Source/SkySeoul/DeathMatch/AI/BT/LOGIC_STATUS.md`
  - Maintain Fire 단일 소유권 기록

## 삭제 파일

`/Game/DeathMatch` 전체 패키지 linker import를 검사한 뒤 다음 미사용 클래스를 삭제했다.

- `Source/SkySeoul/DeathMatch/AI/BT/BTTask_DM_StartFire.h/.cpp`
- `Source/SkySeoul/DeathMatch/AI/BT/BTTask_DM_StopFire.h/.cpp`
- `Source/SkySeoul/DeathMatch/AI/BT/BTTask_DM_ActivateAbility.h/.cpp`

최종 Asset Imports 감사에서 세 클래스의 serialized import는 0개였고, `BT_Combat` runtime topology에도 해당 Task가 없다. 마이그레이션 전 에셋 백업은 `Saved/DMAssetBackups/20261015_1405_FireOwnership/Content/DeathMatch/AI/BT/BT_Combat.uasset`에 남겼다.

## 자동화 및 정적 검증

2026-10-15에 다음을 수행했다.

- `SkySeoulEditor Win64 Development` 최종 빌드: 성공
- `SkySeoul.DeathMatch.BTFoundation`: 12/12 성공
  - `Fire.MaintainFireOwnership`: 성공
  - `Fire.BTCombatSingleExecutor`: 성공
- `SkySeoul.DeathMatch.Combat.FireControl`: 5/5 성공
- `SkySeoul.DeathMatch.Architecture.AssetImports`: 1/1 성공
- `/Game/DeathMatch` 최종 linker import 감사: 삭제한 Fire Task 클래스 참조 0개

새 자동화 계약은 다음을 검증한다.

1. Service 진입당 Fire Request 하나만 생성한다.
2. Fire Ability 활성 중 refresh가 중복 Start를 호출하지 않는다.
3. 최초 발사 실패 뒤 조건 회복 시 같은 Request ID로 재시도한다.
4. Service 종료는 자신이 만든 Request를 제거하고 메모리 ID를 비운다.
5. 여러 요청 중 마지막 요청 종료에서만 StopFire가 호출된다.
6. Attack Branch에는 실패 가능한 Fire Task가 없고 Maintain Fire Service만 있으므로, Intent/ADS 준비 전 최초 실패가 Sequence 실패로 전파되지 않는다.

기존 코드 정적 검토로 사망, 빙의 해제, 전투 이탈의 전량 정리 경로도 확인했다. 새로운 Warning 로그나 화면 디버그 출력은 추가하지 않았다. 위치 선정, 사거리, 조준 정확도, 발사 모드, 데미지 로직은 변경하지 않았다.

## 수행하지 않은 검증

Blueprint 수동 테스트와 PIE는 지시대로 수행하지 않았다. 에셋은 headless editor automation으로 그래프 노드를 제거·저장했고, 최종 runtime topology 및 linker import를 자동화로 다시 확인했다.

## 사용자 PIE 확인 항목

- Attack 진입 후 첫 발사가 정상 시작되는지
- 점사/연사 사이클 종료 후 Attack Branch 유지 중 다음 발사가 계속 재개되는지
- 전투 이동 중 허용된 사격이 유지되는지
- Attack Abort 시 Fire Ability가 즉시 정지하는지
- Attack 재진입 시 새 요청으로 정상 재발사하는지
