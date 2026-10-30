# DM Find Combat Position 후속 수정 결과

## 요약

기존 `Attack`, `Approach`, `Hide` 목적과 범용 `DM Find Combat Position` Task 구조를 유지했다. 위치 프로필 실행 설정은 Blackboard를 거쳐 Branch 수명주기를 소유하는 `DM Action Intent` Service에 적용되며, 후보 축소는 목적별 필수 신호를 먼저 확인한다. 후보 사격 판정과 실제 Fire Control은 무기 Muzzle/Pawn View 기반의 공통 Fire Origin 및 공통 표적 위치·Trace 계약을 사용한다.

## 추가·수정·삭제한 파일

추가:

- `Source/SkySeoul/DeathMatch/Communication/Reports/2026-10-14/DM_Find_Combat_Position_Followup.md`

수정:

- `Source/SkySeoul/DeathMatch/AI/BT/DMBTExecutionServices.h`
- `Source/SkySeoul/DeathMatch/AI/BT/DMBTExecutionServices.cpp`
- `Source/SkySeoul/DeathMatch/AI/BT/BTTask_DM_FindCombatPosition.h`
- `Source/SkySeoul/DeathMatch/AI/BT/BTTask_DM_FindCombatPosition.cpp`
- `Source/SkySeoul/DeathMatch/AI/Combat/DMCombatPositionProfile.h`
- `Source/SkySeoul/DeathMatch/AI/Combat/DMCombatPositionProfile.cpp`
- `Source/SkySeoul/DeathMatch/AI/Execution/DMActionIntentComponent.h`
- `Source/SkySeoul/DeathMatch/AI/Execution/DMActionIntentComponent.cpp`
- `Source/SkySeoul/DeathMatch/AI/Fire/DMCombatFireControlComponent.h`
- `Source/SkySeoul/DeathMatch/AI/Fire/DMCombatFireControlComponent.cpp`
- `Source/SkySeoul/DeathMatch/Abilities/DMGameplayAbility_Fire.cpp`
- `Source/SkySeoul/DeathMatch/Tests/DMCombatPositionTests.cpp`
- `Source/SkySeoul/DeathMatch/Tests/DMBTFoundationTests.cpp`

삭제한 파일은 없다.

## ADS 및 이동 중 사격 설정 전달 경로

최종 흐름은 다음과 같다.

`UDMCombatPositionProfile` → `UBTTask_DM_FindCombatPosition` → Blackboard `ShouldADS` / `FireWhileMoving` → `UBTService_DM_ActionIntent` → `UDMActionIntentComponent` → `UDMAimZoomComponent` / `UDMCombatFireControlComponent`

- Task는 검색 시작 시 `MoveGoal`, `ShouldADS`, `FireWhileMoving`을 각각 unset/false/false로 초기화한다.
- 검색 성공 또는 유효한 캐시 재사용 시에만 프로필 값을 Blackboard에 기록한다.
- Service의 `bUseBlackboardExecutionSettings`가 켜져 있으면 Branch 활성 중 Bool Key를 주기적으로 확인한다.
- Blackboard 값이 실제로 변경된 경우에만 기존 ActionIntent request ID를 갱신한다. request를 재생성하지 않으므로 Branch 우선순위가 흔들리지 않는다.
- Branch 종료/Abort 시 Service가 자신이 만든 request만 제거하며, 아래에 남은 이전 request 정책이 복원된다.
- 우선순위는 `bOverrideExecutionSettings` 고정 override → Blackboard 실행 설정 → Intent 기본 정책이다.
- 프로필 실행 설정은 위치 점수에 사용하지 않는다.

## 목적별 후보 축소 방식

공통:

- EQS 결과 전체에서 좌표 정렬과 farthest-point 방식으로 최대 `MaxCandidateCount`를 공간적으로 다양하게 선택한다. 최초 배열 순서에 의존하지 않는다.
- 제한 후보에는 완전한 Nav 경로 존재 여부만 먼저 검사한다.
- 실제 Nav 경로와 경로 길이는 목적별 shortlist에만 계산한다.
- Criterion은 `bEnabled && bRequiredFilter`일 때만 `AllowedMin/AllowedMax`를 필터로 사용하고, `bEnabled`일 때만 정규화 점수와 `Weight`를 적용한다.
- 완전한 Nav 경로, Attack/Approach 최대 사거리와 포커스 사격 가능성, Hide 기억 위협 존재는 고정 Task 계약이다. 프로필 UI 설명에서 선택 Criterion과 구분했다.

Attack/Approach:

1. 공간 다양화 및 `MaxCandidateCount` 제한
2. 완전한 Nav 경로, 최대 사거리, Range/Friendly 선택 Criterion 검사
3. 제한 후보마다 포커싱 타겟 사격선 1회 검사
4. 사격 가능한 후보만 `ExpensiveCheckCandidateCount` shortlist에 포함
5. shortlist에 실제 Nav 경로 비용과 전체 기억 위협 노출도를 계산
6. 모든 활성 Criterion의 최종 필터/가중 점수로 선택

Hide:

1. 공간 다양화 및 `MaxCandidateCount` 제한
2. 완전한 Nav 경로와 저비용 Criterion 검사
3. 옵션 활성 시 대표 위협에 대한 노출 Trace를 제한 후보마다 1회 수행
4. 대표 위협에 가려진 후보를 우선하고, `HideExposedCandidateFraction`만큼 노출 후보 슬롯을 보존
5. 최대 `HidePrefilterCandidateCount` shortlist에 실제 Nav 경로 비용과 최대 `MaxThreatCount` 전체 위협 노출도를 계산
6. 전체 위협 기준 `MinimumConcealedFraction`과 최종 Criterion 점수로 선택

## Hide 대표 위협 선별

기존 `SelectValidThreats` 정렬 결과의 첫 위협을 사용한다. 우선순위는 포커싱 타겟, 현재 보이는 위협, 최근 감지 위협 순이다. 대표 위협 검사는 shortlist 구성에만 사용하며, 최종 Hide 통과 여부는 전체 유효 기억 위협의 은폐 비율로 다시 판정한다.

프로필 추가 설정:

- `bUseHideRepresentativeThreatPrefilter`
- `HidePrefilterCandidateCount`
- `HideExposedCandidateFraction`

## 공통 Fire Origin 및 사격 계약

- 현재 실제 원점은 활성 무기의 Muzzle을 우선하고, 유효한 무기 원점이 없으면 `Pawn::GetPawnViewLocation()`을 사용한다.
- 후보 원점은 `현재 실제 Fire Origin - Pawn Actor Location` 상대 오프셋을 후보 위치에 투영한다.
- 두 계약을 모두 구할 수 없을 때만 `CandidateEyeHeight`를 fallback으로 사용한다.
- Fire Control의 실시간 안전 검사와 후보 평가는 동일한 원점 해석, 무기 최대 사거리, 무기 Trace Channel, Intended Target 위치, 월드 차단 및 아군 차단 분류를 사용한다.
- 실제 Fire Ability의 AI Trace도 Fire Control이 해석한 무기 Trace Channel을 사용하며, Intended Target 위치는 공통 AimTarget 계약을 사용한다.
- 좌우 Peek, 어깨 Peek, 자세별 Lean은 추가하지 않았다.

## 캐시 무효화 조건

다음 중 하나라도 바뀌면 위치 결과를 재사용하지 않는다.

- Position Purpose
- Combat Target
- Position Profile 객체
- 활성 무기 객체
- 무기 Effective Range 또는 Maximum Range
- 프로필 `bUseADSWhileMoving` 또는 `bAllowFireWhileMoving`
- 유효 기억 위협 snapshot hash(위협 Actor, 10cm 단위 LastKnownLocation, visible 상태)
- `ResultReuseTime` 만료

## 디버깅

기존 compact 1-line 로그에 다음만 추가했다.

- 1차 후보 수
- 포커싱 타겟 사격선 통과 수
- 목적별 정밀 검사 후보 수
- 선택 후보의 예상 Fire Origin
- Hide 대표 위협 이름

개별 후보 Trace 결과는 반복 출력하지 않는다.

## 자동화 테스트 결과

2026-10-14, `UnrealEditor-Cmd -NullRHI` 실행 결과:

- `SkySeoul.DeathMatch.CombatPosition`: 18/18 성공
- `SkySeoul.DeathMatch.Combat.FireControl`: 5/5 성공
- `SkySeoul.DeathMatch.BTFoundation`: 10/10 성공
- `SkySeoulEditor Win64 Development`: 빌드 성공

추가 검증 항목에는 Blackboard 출력 초기화, 실행 설정 우선순위 및 Branch 복원, 사격 불가능 후보의 shortlist 제외, Hide 대표 은폐/노출 혼합, 대표 위협과 최종 전체 위협 판정 분리, 후보 배열 순서 독립성, Criterion 계약, Fire Origin 상대 오프셋, 낮은 엄폐 기준 원점 일치, 후보·위협·정밀 검사 상한이 포함된다.

참고: 초기의 넓은 `SkySeoul.DeathMatch.Combat` 탐색 실행은 작업 제외 대상인 PIE 테스트 `Combat.Fire.PIE`와 기존 `Combat.Parts.HitRouting` 실패도 함께 실행했다. 위 최종 결과는 지시서에서 지정한 세 스위트를 각각 분리 실행한 결과이며 모두 성공했다.

## 에디터에서 사용자가 연결해야 하는 항목

이번 작업에서는 uasset을 수정하지 않았다. 기존 Behavior Tree/Blackboard/Profile asset에서 다음을 확인해야 한다.

1. 위치 이동 Branch의 `DM Action Intent` Service에서 `bUseBlackboardExecutionSettings`를 활성화한다.
2. Service의 `ShouldADSKey`를 Blackboard Bool `ShouldADS`에 연결한다.
3. Service의 `FireWhileMovingKey`를 Blackboard Bool `FireWhileMoving`에 연결한다.
4. 프로필 값을 따르게 할 Branch에서는 `bOverrideExecutionSettings`를 끈다. 이 값이 켜져 있으면 의도적으로 Service 고정값이 우선한다.
5. 각 Position Profile의 Hide shortlist 설정을 맵 밀도에 맞게 조정한다. 기본값은 12개와 노출 대안 25%다.
6. 기존 `CandidateEyeHeight`는 fallback이라는 점을 감안해 유지하거나 조정한다.

## 남은 미구현 사항

지시서의 작업 제외 항목은 그대로 미구현 상태다.

- 새 PositionPurpose, 추격/Investigate
- CombatCover, Retreat, Flank, MicroMove
- Tactical Area Roam 변경
- 위치 예약 시스템 연결
- 체력/위험 상태 판단 변경
- BT/EQS/Profile/Blackboard uasset 생성·수정
- Blueprint/PIE 검증
- Peek/Lean 시스템

