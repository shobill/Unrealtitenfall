# DeathMatch 전투 디버그 간략화 / Observer Follow 결과 보고서

- 작성 시각: 2026-09-29 14:30 KST
- 수정 범위: `Source/SkySeoul/DeathMatch` 내부
- 검증 방식: 정적 코드 점검 및 관련 참조 검색

## 1. Debug 레벨별 표시 정보

- `dm.CombatDebug 0`: 전투 디버그 캡처와 표시를 끈다. 레거시 `dm.CombatWorldDebug`가 별도로 켜져 있으면 기존 월드 오버레이 호환 동작은 유지한다.
- `dm.CombatDebug 1`: 범위 안의 모든 AI 위에 한 줄 또는 최대 두 줄의 compact label만 표시한다. 첫 줄은 `HP 72% | FIRE | Bot_3 | 14m`, 타깃이 없으면 `HP 100% | ROAM | NONE` 형식이다. 두 번째 줄은 사격 차단 또는 실제 이동 중일 때만 표시한다. 상세 점수, 타이머, bool 묶음, 좌표, 후보 목록은 표시하지 않는다.
- `dm.CombatDebug 2`: Observer가 AI를 Follow 중이면 해당 AI 한 명만, Follow 선택이 없으면 기존처럼 시점에서 가장 가까운 AI 한 명만 상세 표시한다. Follow 대상이 플레이어이면 임의의 AI 상세 정보를 대신 표시하지 않는다.
- `dm.CombatDebug 3`: 2단계의 단일 AI 상세 정보와 월드 위치 시각화를 viewport-aware debug subsystem에서 함께 표시한다. 유효 공격 후보는 초록, 엄폐 후보는 파랑, Strafe 목적지는 노랑, 거부 후보는 빨강이다. 최종 이동/공격 선택 위치는 AI와 선으로 연결한다.

`dm.CombatDebugDetails`와 `dm.CombatWorldDebug`는 기존 사용자를 위해 레거시 호환 경로로 남겼다. `dm.CombatDebug`가 1 또는 2이면 레거시 월드 오버레이를 중복 출력하지 않으며, 3이면 신규 월드 시각화가 우선한다.

## 2. 사격 차단 원인 축약 매핑

- `NoTarget` -> `NO TARGET` (정상 ROAM/SEARCH에서는 불필요한 두 번째 줄을 생략)
- `NotSensed`, `NotEngaged`, `NoLineOfSight` -> `LOS`
- `OutOfRange` -> `RANGE`
- `Facing`, `AimSettling` -> `AIM`
- `Movement`, `PrecisionSpeed` -> `MOVE`
- `Cadence`, `Probability` -> `COOLDOWN`
- `CannotShoot` -> `PARTS`
- `Inactive`, `FireActive`, `WeaponChanged` -> `ABILITY`

compact 행동명은 `FIRE`, `STRAFE`, `RELOCATE`, `COVER`, `RETREAT`, `SEARCH`, `ROAM`, `DEAD`로만 정규화한다. 위험도는 기존 관찰 스냅샷의 total risk를 반올림하고 `0~100`으로 clamp한다.

## 3. Follow 입력과 BP 설정

Observer Pawn을 local PlayerController가 Possess했을 때만 키 polling이 동작한다.

- `F`: 현재 Follow 중이면 해제, 아니면 crosshair visibility trace에 맞은 Character를 Follow
- `PageUp` 또는 `]`: 다음 Character
- `PageDown` 또는 `[`: 이전 Character
- `R`: Observer에서 가장 가까운 Character
- `WASD`, `E/Q`, `Space/Ctrl`: Follow 중에는 `FollowOffset` 조절, 비 Follow 중에는 기존 자유 비행
- `Shift`: 자유 비행 및 FollowOffset 조절 모두 고속 이동값 사용
- 마우스: 기존 `LookSensitivityX/Y`, `bInvertLookX/Y` 계산을 그대로 적용

Mouse Wheel은 일반 PlayerController의 무기 전환과 충돌할 수 있어 변경하지 않았다.

`DeathMatch|Debug Observer|Follow` BP 카테고리에 다음 기본값/옵션을 노출했다.

- `FollowOffset = (-300, 0, 150)`
- `FollowInterpSpeed = 8`
- `bUseTargetLocalOffset = true`
- `bLookAtFollowTarget = false`
- `FollowLookInterpSpeed = 8`
- `FollowTeleportDistance = 3000cm`
- `bAllowFollowDeadTarget = false`
- `bAutoFollowNextOnTargetLost = true`
- `bManualMovementBreaksFollow = false`
- `MaxFollowOffsetDistance = 5000cm` (`0`이면 제한 없음)
- `FollowSelectionTraceDistance = 100000cm`

요청한 `Set/Clear/Toggle`, next/previous/nearest/crosshair 선택, `GetFollowTarget`, `IsFollowingTarget`을 모두 Blueprint Callable/Pure API로 추가했다.

## 4. 대상 선택 및 대상 상실 처리

- 후보는 `ACharacterBase` 기반의 유효 Character만 허용한다.
- Observer 자신, `IDMNonCombatActor`, detached debris tag Actor를 제외한다.
- 기본값에서는 `IsDead` 또는 유효 Health snapshot의 dead 상태를 제외하며 `bAllowFollowDeadTarget`으로 변경할 수 있다.
- 후보는 선택 명령 또는 대상 상실 시에만 `TActorIterator`로 갱신한다. Tick마다 전체 Actor를 검색하지 않는다.
- 순환 목록은 Actor path name과 unique id로 안정 정렬한다.
- Follow 대상과 캐시는 모두 `TWeakObjectPtr`로 보관한다.
- 파괴/사망/invalid 시 `bAutoFollowNextOnTargetLost`가 true이면 기존 index의 다음 유효 후보로 넘어가고, false이면 Follow를 해제한다.
- EndPlay에서 Follow 대상, 후보 캐시, index를 정리한다.

## 5. 수동 이동과 FollowOffset 관계

Follow는 Actor attachment를 사용하지 않는다. 매 Tick 대상 위치와 local/world `FollowOffset`으로 원하는 위치를 계산하고 `VInterpTo`로 이동한다. 현재 위치와 원하는 위치가 `FollowTeleportDistance`보다 멀면 즉시 이동해 순간이동/respawn 뒤의 긴 추격을 방지한다.

기본값에서는 수동 이동 입력을 view-relative world delta로 계산한 뒤 local offset 모드이면 대상 transform의 역변환으로 `FollowOffset`에 반영한다. 따라서 Follow를 유지한 채 대상 주변 관찰 위치를 바꿀 수 있다. `bManualMovementBreaksFollow=true`일 때만 수동 이동이 Follow를 해제하고 자유 비행으로 전환한다.

`bLookAtFollowTarget=false`에서는 대상 회전과 무관하게 기존 마우스 control rotation을 그대로 유지한다. true에서만 target look rotation으로 `RInterpTo`한 뒤 같은 프레임의 마우스 delta를 계속 적용한다.

## 6. 상세 디버그 선택 연동

신규 native `IDMCombatDebugSelectionProvider`를 Observer가 구현한다. 전투 디버그 코드는 `ADMDebugObserverPawn`을 직접 include/cast하지 않고 local possessed Pawn에서 선택 제공자만 조회한다.

- 제공자가 Follow 중인 AI Pawn을 반환하면 해당 AI Controller의 상세 정보만 표시한다.
- 제공자가 플레이어를 반환하면 AI 상세를 표시하지 않는다.
- 제공자 또는 선택 대상이 없으면 기존 최근접 AI 선택을 유지한다.
- interface가 반환하는 대상은 Observer 내부 weak pointer의 현재 유효 대상뿐이다.

## 7. 수정 파일 목록

신규:

- `Debug/DMCombatDebugSelectionProvider.h`
- 본 보고서

수정:

- `Debug/DMDebugObserverPawn.h/.cpp`
- `AI/Combat/DMCombatDebug.h/.cpp`
- `AI/Combat/DMCombatDebugFormat.cpp`
- `AI/Combat/DMCombatTacticsDebug.cpp`
- `AI/Combat/DMCombatWorldDebugSubsystem.cpp`
- `AI/Combat/DMCombatCoverModule.h/.cpp`
- `AI/Combat/DMCombatDebugTests.cpp`
- `Presentation/HUD/DeathMatchHUD_CombatDebug.cpp`

DeathMatch 밖의 기존 사용자 변경은 수정하거나 되돌리지 않았다.

## 8. 정적 검증 결과

- `dm.CombatDebug` 참조를 검색해 0~3 clamp, 1단계 compact formatter, 2단계 단일 선택, 3단계 world renderer 경로를 확인했다.
- compact formatter는 항상 첫 줄 하나와 선택적인 두 번째 줄 하나만 추가함을 확인했다.
- block/action 출력이 지정된 사용자용 문자열 집합으로 제한되는지 확인했다.
- Health ratio와 risk는 기존 debug snapshot 캡처에만 추가했으며 사격/이동 평가 입력이나 결과를 변경하지 않음을 확인했다.
- 3단계의 공격/엄폐/Strafe/거부 색상과 최종 위치 연결선을 확인했다.
- Follow 구현에 `AttachToActor`, 매 프레임 `GetAllActorsOfClass`, replication 변경, gameplay event/noise/damage 발생이 없음을 확인했다.
- Follow 대상/후보가 weak pointer이고 대상 상실 및 EndPlay 정리 경로가 있음을 확인했다.
- 기존 Observer의 collision off, damage off, replication off, navigation 영향 off, NoTeam, AI perception source unregister 및 `IDMNonCombatActor` 구현을 유지했다.
- 기존 X/Y look sensitivity와 invert 계산이 Follow/비 Follow 공통 후단에서 그대로 실행됨을 확인했다.
- 신규/수정 파일의 단순 중괄호 수 일치와 함수 선언/정의 참조를 확인했다.

정적 검증 중 발견된 프로젝트 전체 `git diff --check` 경고는 DeathMatch 밖 Config 등 기존 변경의 trailing whitespace였다. 이번 변경 파일에서 별도 trailing whitespace 문제는 확인되지 않았다.

## 9. BP/PIE에서 사용자가 확인할 항목

1. `BP_DebugCharacter`의 Follow 카테고리에 설정과 기본값이 노출되고 모든 Callable/Pure 함수가 검색되는지 확인.
2. 기본 Follow에서 대상 뒤/위 offset으로 부드럽게 이동하고 Actor attachment가 생성되지 않는지 확인.
3. Follow 중 마우스 자유 시점, 기존 X/Y 감도와 invert가 유지되는지 확인.
4. WASD/E/Q 및 Shift가 Follow를 해제하지 않고 offset을 조절하며 최대 거리 clamp가 동작하는지 확인.
5. `bManualMovementBreaksFollow`, local/world offset, look-at 옵션을 각각 전환해 동작 확인.
6. 대상 순간이동/respawn 시 3000cm 기준 즉시 이동하고 긴 시간 날아가지 않는지 확인.
7. 대상 사망/파괴 때 auto-next 또는 clear가 설정대로 동작하고 dangling reference가 없는지 확인.
8. 순환 후보에 Observer, 파츠/debris, 비전투 Actor가 없고 dead 허용 옵션이 동작하는지 확인.
9. Observer가 AI Sense/위협/점유/스폰 점유에 계속 영향을 주지 않는지 확인.
10. `dm.CombatDebug 1`이 AI당 최대 두 줄이고 상세 점수/좌표/후보를 표시하지 않는지 확인.
11. `dm.CombatDebug 2`에서 Follow AI 한 명만 상세 표시되고 Follow player에서는 AI 상세가 표시되지 않는지 확인.
12. `dm.CombatDebug 3`에서 후보 색상, 이동 목적지, 최종 선택 연결선 및 F8/Simulate viewport 정렬을 확인.

## 10. 수행하지 않은 검증

요청에 따라 전체 빌드, 개별 C++ 빌드/UHT, BP 컴파일/테스트, Unreal Automation 실행, Editor 실행, PIE 테스트를 수행하지 않았다. 따라서 UHT 노출, 실제 입력 우선순위, BP 직렬화, runtime 카메라 체감과 렌더링 결과는 위 체크리스트로 최종 확인이 필요하다.
