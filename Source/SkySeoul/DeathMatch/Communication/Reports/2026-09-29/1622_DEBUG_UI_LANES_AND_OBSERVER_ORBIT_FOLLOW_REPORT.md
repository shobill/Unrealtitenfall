# DeathMatch Debug UI Lanes / Observer Orbit Follow 결과 보고서

- 작성 시각: 2026-09-29 16:22 KST
- 수정 범위: `Source/SkySeoul/DeathMatch` 내부
- 검증 방식: 정적 코드 점검 및 관련 참조 검색

## 1. HP 바, 이름, Combat Debug 최종 배치

화면의 세로 배치 순서는 위에서 아래로 다음과 같다.

1. Combat Debug 라벨
2. 캐릭터 이름
3. HP 진행 바와 숫자 HP

Health Bar 자체의 월드 기준점은 기존 `HealthBar` 소켓 우선, Character Capsule top, Actor Bounds top 순서를 유지한다. 기본 C++ Health Bar 위젯은 이름을 별도 상단 행에 두고, 하단 행에 진행 바와 숫자 HP를 둔다. Debug All Health Bar의 draw size는 이름 행을 포함하도록 `160 x 38`로 확장했다. 실제 사용자용 `UDMVisibleHealthBarsComponent`는 기존처럼 `bShowActorName=false`가 기본이므로 이름을 추가하지 않는다.

Combat Debug는 동일한 기본 Health Bar anchor에서 `dm.CombatDebugVerticalOffset`만큼 위로 올린 위치를 사용한다. 기본값은 `100cm`다. 두 anchor를 화면에 각각 투영한 뒤 Debug 라벨 하단이 `HealthBarScreenY - dm.CombatDebugHealthBarScreenHeight - dm.CombatDebugMinimumScreenGap`보다 내려가지 않게 한다. 기본 예약 높이는 Debug All draw size와 같은 `38px`, 최소 간격은 `20px`다. 라벨 박스의 하단이 이 투영점보다 내려가지 않도록 패딩을 포함한 전체 박스 높이를 먼저 빼므로, 원거리에서 100cm의 화면상 높이가 작아지거나 상세 라벨이 길어져도 이름/HP 영역 방향으로 시작점이 내려오지 않는다. 커스텀 BP widget 높이가 다르면 `dm.CombatDebugHealthBarScreenHeight`를 조정한다.

`ADeathMatchHUD` 경로와 F8/Simulate용 `UDMCombatWorldDebugSubsystem` 경로가 같은 anchor, 월드 오프셋, 화면 간격 계산을 사용한다.

## 2. 중복 이름 처리

캐릭터 자신의 표시 이름은 Debug All Health Bar가 소유한다. 상세 Combat Debug formatter에서 기존 첫 줄의 `PawnName` 출력을 제거했다. Compact 라벨의 이름 필드는 관찰 중인 Pawn 자신의 이름이 아니라 전투 대상 이름이므로 유지했다. 예를 들어 `Robot_02`의 Debug All 이름 아래에서 compact 라벨에 보이는 `Bot_3`는 Robot_02가 조준 중인 대상이다.

`UDMVisibleHealthBarsComponent`는 사용자용 기본값에서 이름을 숨기므로 플레이어 UI에 새 중복 이름을 만들지 않는다.

## 3. Follow 중 입력

- `F`: crosshair 아래 Character Follow 시작 / 현재 Follow 해제
- `R`: 가장 가까운 유효 Character Follow
- `PageUp` 또는 `]`: 다음 대상
- `PageDown` 또는 `[`: 이전 대상
- 마우스 X/Y: 제한 범위 안에서 Orbit yaw/pitch 변경
- Mouse Wheel Up: 거리 감소
- Mouse Wheel Down: 거리 증가
- `W/A/S/D`, `E/Q`, `Space/Ctrl`, `Shift`: Follow 카메라 위치에 사용하지 않음

Follow가 아닐 때는 기존 자유 비행의 `W/A/S/D`, `E/Q`, `Space/Ctrl`, `Shift`, 자유 look 처리를 그대로 유지한다.

## 4. Orbit 위치와 LookAt 계산

매 Tick 대상 pivot을 다시 구한다.

1. Skeletal Mesh의 `CameraFollow` socket
2. 없으면 `DebugFollow` socket
3. 없으면 Character Capsule의 상체 높이
4. Character가 아닌 경우 Actor Bounds center
5. 위 결과에 `FollowPivotHeightOffset`을 추가

카메라 위치는 `OrbitRotation(-OrbitPitch, BaseYaw + OrbitYaw, 0)`과 다음 식으로 계산한다.

`DesiredLocation = TargetPivot - OrbitRotation.ForwardVector * CurrentFollowDistance`

여기서 양수 OrbitPitch는 카메라가 대상보다 위로 올라가는 의미가 되도록 UE view pitch 부호를 위치 계산에서 반대로 사용한다. 위치는 `VInterpTo`, zoom 거리는 `FInterpTo`, 회전은 `RInterpTo`를 사용한다. 회전 목표는 매 Tick 현재 카메라 위치에서 TargetPivot을 향하는 LookAt 회전이다.

대상 pivot이 화면 중앙 safe zone 밖으로 나가거나 투영할 수 없으면 해당 Tick에 LookAt을 즉시 적용한다. Follow 시작 또는 `FollowTeleportDistance`보다 먼 대상 이동/전환도 위치와 회전을 즉시 보정한다. 따라서 일반 이동은 부드럽고, 대상 순간이동·respawn·먼 대상 전환은 느리게 추격하지 않는다.

## 5. 기본 거리와 Orbit 제한

`DeathMatch|Debug Observer|Follow Camera` BP 카테고리에 다음 값을 노출했다.

- `DefaultFollowDistance = 500cm`
- `MinimumFollowDistance = 150cm`
- `MaximumFollowDistance = 1500cm`
- `FollowZoomStep = 75cm`
- `FollowZoomInterpSpeed = 10`
- `FollowPositionInterpSpeed = 8`
- `FollowRotationInterpSpeed = 10`
- `OrbitSensitivityX/Y = 1.0`
- `MaximumOrbitYaw = 75도`
- `MinimumOrbitPitch = -45도`
- `MaximumOrbitPitch = 60도`
- `DefaultOrbitYaw = 0도`
- `DefaultOrbitPitch = 15도`
- `bResetOrbitOnNewTarget = true`
- `bUseTargetRotationAsOrbitBase = false`
- `FollowPivotHeightOffset = 20cm`
- `TargetScreenSafeZone = 0.25`
- `FollowTeleportDistance = 3000cm` 유지

Yaw는 `-MaximumOrbitYaw ~ +MaximumOrbitYaw`, pitch는 설정된 min/max로 매 Tick clamp한다. 기존 `bInvertLookX/Y`는 Orbit 입력에도 적용한다.

## 6. Mouse Wheel Zoom과 일반 플레이어 보호

Observer가 local PlayerController에 Possess되고 실제 Follow 중일 때만 `MouseWheelAxis`와 wheel up/down pulse를 읽는다. Up은 목표 거리를 줄이고 Down은 늘리며, 목표 거리와 현재 거리를 분리해 보간한다. 빠른 반복 입력도 min/max 범위에서 clamp한다.

`ADeathMatchPlayerController`의 무기 순환 경로에는 현재 Pawn이 Follow 중인 `ADMDebugObserverPawn`일 때만 wheel tag를 조기 반환하는 방어를 추가했다. 일반 `ADeathMatchPlayerCharacter`에서는 기존 `CycleWeaponNext/Prev` 경로가 그대로 실행된다. free-flight Observer에서는 새 조기 반환을 하지 않으며 Observer 자체도 wheel zoom을 읽지 않는다.

## 7. 대상 회전을 기본 Orbit 기준으로 사용하지 않는 이유

`bUseTargetRotationAsOrbitBase=false`가 기본이다. AI가 빠르게 방향을 바꿀 때 카메라 yaw가 AI 회전에 끌려가며 휙 도는 현상을 막고, 대상 위치만 따라가는 안정적인 월드 기준 Orbit을 제공하기 위해서다. BP에서 true로 바꾸면 target yaw를 base로 사용하므로 대상 뒤를 따라가는 형태로 전환할 수 있다.

## 8. 기존 Follow 설정 호환 처리

공개 Follow API는 모두 유지했다.

- `SetFollowTarget`
- `ClearFollowTarget`
- `ToggleFollowTarget`
- `FollowNextCharacter`
- `FollowPreviousCharacter`
- `FollowNearestCharacter`
- `FollowCharacterUnderCrosshair`
- `GetFollowTarget`
- `IsFollowingTarget`

기존 BP/asset 직렬화 호환을 위해 `FollowOffset`, `FollowInterpSpeed`, `bUseTargetLocalOffset`, `bLookAtFollowTarget`, `FollowLookInterpSpeed`, `bManualMovementBreaksFollow`, `MaxFollowOffsetDistance` 필드는 제거하지 않았다. 대신 `Follow|Legacy` 고급 카테고리와 `(Legacy, Ignored)` 표시명으로 옮겼고 새 Orbit 구현에서는 읽지 않는다. 의미가 다른 새 Orbit 설정으로 기존 값을 억지로 재사용하지 않았다. Follow 상태는 항상 pivot을 바라본다.

## 9. 대상 상실과 전환

- 대상과 후보 목록은 계속 `TWeakObjectPtr`로 보관한다.
- 매 프레임 Actor 전체 검색을 하지 않는다. 후보 검색은 명시적 선택/순환 또는 대상 상실 때만 수행한다.
- 기본값에서는 죽은 대상을 제외하고 `bAllowFollowDeadTarget`을 유지한다.
- Observer 자신, `IDMNonCombatActor`, detached debris를 제외한다.
- 대상 사망/파괴 시 `bAutoFollowNextOnTargetLost`가 true이면 기존 index 기준의 다음 유효 대상으로 전환하고, false이면 Follow를 끝낸다.
- 새 대상이 멀면 `FollowTeleportDistance` 기준으로 즉시 Orbit 위치에 배치한다. Follow 최초 시작과 상실 후 auto-next도 즉시 배치한다.
- EndPlay에서 대상, 후보, index를 정리한다.

## 10. 수정 파일 목록

- `Debug/DMDebugObserverPawn.h`
- `Debug/DMDebugObserverPawn.cpp`
- `Controller/DeathMatchPlayerController.cpp`
- `Presentation/HealthBars/DMHealthBarWidget.cpp`
- `Presentation/HealthBars/DMDebugAllHealthBarsComponent.cpp`
- `Presentation/HUD/DeathMatchHUD_CombatDebug.cpp`
- `AI/Combat/DMCombatDebug.h`
- `AI/Combat/DMCombatDebug.cpp`
- `AI/Combat/DMCombatDebugFormat.cpp`
- `AI/Combat/DMCombatWorldDebugSubsystem.cpp`
- 본 보고서

## 11. 정적 검증 결과

- Health Bar anchor 계산은 수정하지 않았고 Combat Debug만 동일 기준점 위 `100cm`에 배치한 뒤 화면에서 Debug All 높이 `38px + 20px` 간격을 최소값으로 강제한 것을 확인했다.
- 기본 Health Bar widget tree가 이름 행과 HP 행을 분리하며 Debug All만 이름 표시가 true이고 Visible Health Bar는 false임을 확인했다.
- 상세 formatter의 own `PawnName` 줄이 제거됐고 compact 이름은 target name임을 확인했다.
- `FormatCompact`는 필수 첫 줄 하나와 조건부 둘째 줄 하나만 추가하므로 `dm.CombatDebug 1` 최대 두 줄 규칙을 유지한다.
- Follow 분기에서는 이동 키 값을 위치 계산에 사용하지 않고, free-flight 분기에서만 `AddMovementInput`을 호출함을 확인했다.
- Orbit yaw/pitch와 zoom 목표 거리가 매 Tick 설정 범위로 clamp됨을 확인했다.
- Follow 위치가 매 Tick 새 pivot으로 계산되고 LookAt 회전이 항상 같은 pivot을 향함을 확인했다.
- wheel zoom은 Follow 분기 안에서만 실행되고 PlayerController 방어도 Follow 중 Observer에만 적용됨을 확인했다.
- 일반 플레이어 무기 순환의 `CycleWeaponNext/Prev` 코드는 변경하지 않았다.
- 후보 검색은 선택/상실 시 `TActorIterator`를 사용하며 Tick 전체 검색은 추가하지 않았다.
- 기존 Observer의 local-only replication off, collision off, damage off, NoTeam, navigation 비영향, AI perception unregister, `IDMNonCombatActor` 구현을 유지했다.
- `DeathMatchTeamUtils::IsCombatTargetCandidate`가 `IDMNonCombatActor`를 거부하고 spawn 점유와 combat position registry가 이 필터를 사용하므로 Observer가 두 점유 목록에서 제외되는 기존 경로를 확인했다.
- AI 판단, 사격, 이동 모듈의 의사결정 코드는 수정하지 않았다.

## 12. BP/PIE에서 사용자가 확인해야 할 항목

1. Debug All에서 `Combat Debug / 이름 / HP 바` 순서와 실제 간격을 확인하고 필요하면 두 CVar를 조정한다.
2. 커스텀 BP Health Bar widget이 있으면 이름 행을 위쪽에 배치하고 `DisplayOptions.bShowActorName`을 존중하는지 확인한다.
3. `dm.CombatDebug 1`의 한두 줄 compact 표시와 `2/3` 상세 박스가 HP lane과 겹치지 않는지 확인한다.
4. F8/Simulate와 일반 PIE viewport에서 같은 anchor 기준으로 보이는지 확인한다.
5. Follow 시작 시 대상이 즉시 중앙에 들어오고 마우스가 시선을 떼는 대신 대상 주위를 도는지 확인한다.
6. yaw/pitch clamp, invert X/Y, wheel up/down 방향과 거리 보간 체감을 확인한다.
7. Follow 중 이동/상승/하강/Shift 키가 위치를 바꾸지 않고 free-flight에서는 기존대로 동작하는지 확인한다.
8. 일반 플레이어 Pawn에서 wheel 무기 전환이 그대로 동작하는지 확인한다.
9. 대상 사망, 파괴, respawn, 순간이동, 먼 대상 순환 시 auto-next/teleport 동작을 확인한다.
10. `bUseTargetRotationAsOrbitBase=true`가 필요한 BP에서만 target-relative 추적 감각을 확인한다.
11. Observer가 AI Sense, 위협, 위치 점유, spawn 점유에 포함되지 않는지 런타임에서 재확인한다.

## 13. 수행하지 않은 검증

요청에 따라 전체 빌드, 개별 C++/UHT 빌드, BP 컴파일/테스트, Unreal Editor 실행, PIE 테스트, Automation 테스트를 수행하지 않았다. 실제 BP 직렬화, custom widget layout, Enhanced Input 우선순위, 카메라 체감과 화면 렌더링은 위 체크리스트로 최종 확인해야 한다.
