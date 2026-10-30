# 지형 전용 그래플링: GAS 기반 1~3단계

## 이번에 만든 기능

1. 카메라 정면을 최대 30m까지 검사합니다.
2. 가장 먼저 맞힌 물체가 정적인 WorldStatic 지형이면 연결점을 저장합니다.
3. 플레이어와 연결점 사이에 Cable Component로 줄을 표시합니다.

스윙, 견인, 줄 길이에 따른 이동 제한, 날아가는 갈고리 발사체, 멀티플레이 동기화는 아직 없습니다.
줄이 연결되어 있어도 플레이어는 평소대로 이동할 수 있으며 줄의 표시 길이도 거리에 맞춰 변합니다.

## GAS 구조

```text
IA_TDGrapple (Enhanced Input Started / Completed / Canceled)
  → TDPlayerCharacte가 InputTag.TitanDefense.Grapple 전달
  → 기존 PlayerControllerBase.PostProcessInput / ASC.ProcessAbilityInput
  → GA_TDGrapple (UTDGameplayAbility_Grapple 상속)
  → 지형 검사 → CommitAbility → 연결 및 줄 표시
  → WaitInputRelease 또는 사망/기절 태그 감시 → EndAbility
```

- 새로운 ASC는 만들지 않습니다. 기존 AProjectPlayerState의 UCharacterAbilitySystemComponent를 사용합니다.
- PlayerState가 준비되면 서버에서 GrappleAbilitySet을 한 번 부여하며, 조종 해제/제거 시 부여 핸들로 회수합니다.
- 부모가 일반 AbilitySets에 부여하는 능력은 건드리지 않습니다. 그래플링 묶음을 일반 AbilitySets에 중복 추가하지 마세요.
- `Ability.TitanDefense.Grapple`: 능력 분류 태그입니다.
- `State.TitanDefense.Grappling`: GA 활성화 동안 GAS가 소유하는 상태 태그입니다. 실패/취소/정상 종료 시 제거합니다.
- `State.Dead`, `State.Debuff.Stun`: 발동을 막으며, 사용 중 추가되어도 AbilityTask가 취소합니다.
- 입력은 OnInputTriggered 정책입니다. 기절이 풀렸다고 버튼을 누른 채 자동으로 재연결하지 않습니다.
- 컴포넌트는 지형 검사와 줄 표현을 맡습니다. 직접 발동하는 Blueprint TryGrapple 함수는 제거했습니다.
- 외부 ReleaseGrapple이나 대상 삭제는 OnGrappleReleased 델리게이트로 GA를 종료시킵니다.

## 에디터에서 시험하기

- `Content/TitanDefense/BP_TDPlayerCharacter`를 사용합니다. 부모 C++ 클래스에서 GrappleComponent를 상속합니다.
- 테스트 맵의 플레이어로 이 블루프린트를 지정하거나, 맵에 배치하고 Auto Possess Player를 Player 0으로 설정합니다.
- `PlayerControllerBase`를 상속한 컨트롤러(예: BP_PlayerHanController)와 `AProjectPlayerState`를 사용해야 합니다. 기본 언리얼 PlayerController에는 이 프로젝트의 태그 입력 처리기가 없습니다.
- 가운데 마우스 버튼을 누르면 카메라 정면에 즉시 연결하고, 놓으면 해제합니다.
- 대상은 `Mobility = Static`, `Object Type = WorldStatic`, `Visibility = Block`이어야 하며 쿼리 충돌이 켜져 있어야 합니다.
- 타이탄을 포함한 Pawn 및 Movable 물체에는 연결하지 않습니다.
- Visibility를 차단하는 비대상 물체가 앞을 막으면 연결에 실패합니다. Visibility를 무시하는 물체는 이 트레이스에서도 보이지 않습니다.
- 컴포넌트의 `Draw Debug Trace`를 켜면 성공은 초록, 실패는 빨강 선으로 2초간 표시됩니다.

이번 작업은 게임 모드의 Default Pawn Class를 바꾸지 않습니다.

## 에셋 설정 위치

| 에셋 | 위치 / 역할 |
|---|---|
| GA_TDGrapple | Content/TitanDefense/Grapple/GAS: 그래플링 GA 블루프린트 |
| DA_TDGrappleAbilitySet | 같은 폴더: GA와 InputTag를 묶어 기존 AbilitySetData 형식으로 부여 |
| IA_TDGrapple | Content/TitanDefense/Input: Boolean 입력 액션 |
| IMC_TDGrapple | 같은 폴더: 기본 가운데 버튼 매핑, 키 변경은 이 에셋에서 |

BP_TDPlayerCharacter의 Grapple / GAS 및 Input 설정에 이 에셋들을 연결했습니다. 기존 InputConfig나 공용 IMC_Default는 변경하지 않았습니다.
GA 블루프린트의 부모 동작을 그대로 사용하며, 별도 ActivateAbility 그래프를 덮어쓰지 않아도 됩니다.

## 코드 읽는 순서 / 각주

- `TDPlayerCharacte.cpp` [GAS 각주 10~12]: 능력 부여, 입력 태그 전달, 입력/능력 정리입니다.
- `GAS/TDGameplayAbility_Grapple.cpp` [GAS 각주 1~6]: 조준, CommitAbility, 입력/상태 AbilityTask, 단일 종료 경로입니다.
- `TDGrappleComponent.cpp` [각주 7]: 카메라 조준선을 검사합니다. 대상 선별은 [각주 6]에서 합니다.
- [각주 8]: 맞힌 위치를 해당 컴포넌트의 상대 좌표로 저장합니다.
- `BeginPlay`, [각주 9] `UpdateCable`: 줄 생성 및 연결점 좌표 변환입니다. EndLocation에 월드 좌표를 그대로 넣지 않습니다.
- [각주 10]: 대상 삭제 및 조종 해제 시 정리하며, 사망/기절 취소는 GA에서 맡습니다. 게임의 사망/기절 처리는 해당 GAS 태그를 실제로 추가해야 합니다.
- [각주 11]: 이동 속도와 중력은 건드리지 않습니다. 다음 단계에서 스윙을 추가할 위치를 구분하기 위한 설명입니다.

## 조절할 값

`BP_TDPlayerCharacter`에서 GrappleComponent를 선택하면 최대 사거리(Max Range), 줄 시작 위치(Cable Start Offset), 디버그 표시를 조절할 수 있습니다.
케이블은 BeginPlay에 생성되므로 에디터 미리보기에서는 나타나지 않고, 플레이 중 연결에 성공해야 표시됩니다.

## 검증

자동화 테스트 이름: `SkySeoul.TitanDefense.Grapple.TerrainOnly`
정적 지형 연결, 명중 위치 저장, 해제, 사거리 제한, Pawn 차단, 동적 물체 차단, 빗나감 시 초기화를 검사합니다.

- `SkySeoul.TitanDefense.Grapple.GASLifecycle`: 입력 태그 발동/해제, 상태 태그, 사망/기절 차단 및 취소, 외부 취소, 실패 후 재사용, 활성 능력 제거를 검사합니다.
- `SkySeoul.TitanDefense.Grapple.AbilitySet`: 기존 AbilitySetData 부여/회수 경로를 검사합니다.
- `SkySeoul.TitanDefense.Grapple.PlayerStateLifecycle`: 실제 PlayerState ASC 재사용, 중복 부여 방지, 조종 해제/재조종 및 제거 시 회수를 검사합니다.

## 비용 / 쿨다운 / 네트워크 범위

CommitAbility는 지형 검사 성공 후 호출합니다. 현재 기본 비용과 쿨다운은 없으므로 조작감은 이전과 같습니다.
비용/쿨다운은 GAS 표준 확장 지점이지만, 이번 GA는 **LocalOnly** 표시 능력입니다. 서버 권한으로 SP를 소비하거나 쿨다운 효과를 복제하는 완성형 네트워크 능력으로 구현한 것은 아닙니다.
멀티플레이 권한/예측을 추가하기 전에는 로컬 단일 플레이에서만 비용/쿨다운을 설정/검증하세요.
스윙/견인 이동을 추가할 때에는 별도로 서버 타깃 검증, TargetData 전송, 이동 예측 및 케이블 상태 복제를 설계해야 합니다.
