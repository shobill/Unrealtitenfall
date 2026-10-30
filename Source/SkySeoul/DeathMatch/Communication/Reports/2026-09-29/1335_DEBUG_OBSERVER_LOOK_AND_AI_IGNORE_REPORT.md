# DeathMatch Debug Observer Look / AI Ignore 변경 보고서

- 작성 시각: 2026-09-29 13:35 KST
- 수정 범위: `Source/SkySeoul/DeathMatch` 내부
- 검증 방식: 정적 코드 점검 및 관련 참조/에셋 문자열 검색

## 1. 시점 감도 기본값과 축 계산

`ADMDebugObserverPawn`의 단일 `LookSensitivity = 0.12`를 다음 기본값으로 교체했다.

- `LookSensitivityX = 1.0`
- `LookSensitivityY = 1.0`
- `bInvertLookX = false`
- `bInvertLookY = false`

계산식은 다음과 같다.

```text
LookX = MouseX * max(0, LookSensitivityX) * (bInvertLookX ? -1 : 1)
LookY = MouseY * max(0, LookSensitivityY) * (bInvertLookY ? -1 : 1)
Yaw   = PreviousYaw + LookX
Pitch = ClampAngle(PreviousPitch + LookY, -89, 89)
```

`GetInputMouseDelta`가 반환한 프레임 입력량에 `DeltaTime`을 다시 곱하지 않는다. UPROPERTY의 `ClampMin/UIMin = 0.0`과 런타임 `FMath::Max(0.0f, ...)`를 함께 적용하여 BP/직렬화 입력 모두에서 음수 감도를 사용하지 않는다.

## 2. BP 노출 설정

네 설정을 모두 `EditAnywhere`, `BlueprintReadWrite`, 카테고리 `DeathMatch|Debug Observer|Look`로 노출했다. X/Y 반전은 음수 감도 대신 독립 bool로 제공한다.

콘텐츠 전체 문자열 검색에서 `BP_DebugCharacter.uasset`이 `DMDebugObserverPawn` 파생 BP임을 확인했다. 그러나 콘텐츠 전체에 직렬화된 `LookSensitivity` 이름은 없었으므로 기존 단일 값의 BP override는 존재하지 않는 것으로 판단했다. 신규 디버그 클래스의 명확한 X/Y 속성으로 직접 교체했으며 장기 호환용 중복/deprecated 필드는 추가하지 않았다.

## 3. 기존 플레이어 Look 입력과의 비교

- 일반 플레이어: `APlayerControllerBase::Look`가 `LookAxisVector.X`를 `AddControllerYawInput`, `LookAxisVector.Y`를 `AddControllerPitchInput`에 그대로 전달한다.
- 관찰자: 기본 상태에서 `MouseX`와 `MouseY`를 각각 Yaw와 Pitch에 같은 부호로 더한다.
- 따라서 오른쪽 입력은 양의 Yaw, 왼쪽 입력은 음의 Yaw이며, 위/아래 Pitch는 기존 플레이어의 축 값과 동일한 부호 규칙을 사용한다.
- 종전의 고정 `Pitch -= MouseY * 0.12`는 제거했다. 반대 방향이 필요한 개별 BP만 `bInvertLookY`를 켤 수 있다.
- `DefaultInput.ini`의 Mouse X/Y 축 감도는 `0.07`, invert는 false이고, `IMC_Default`에는 Scalar/Negate/Swizzle modifier 참조가 존재한다. 실제 플레이어 코드는 최종 Enhanced Input 액션 값을 추가 배율 없이 적용하므로 관찰자 기본 X/Y 배율도 `1.0`으로 정했다.

## 4. AI Perception 등록 해제

UE 5.6 엔진 소스를 확인한 결과:

- `UAISense_Sight`는 `bAutoRegisterAllPawnsAsSources = true`이다.
- `UAIPerceptionSystem::UnregisterSource(AActor& SourceActor, const TSubclassOf<UAISense> Sense = nullptr)`에서 Sense가 null이면 등록된 모든 Sense와 pending registration에서 Actor를 제거한다.

이에 따라 관찰자의 `BeginPlay`에서 월드 및 `UAIPerceptionSystem` null 여부를 확인한 뒤 `PerceptionSystem->UnregisterSource(*this)`를 호출한다. 이 호출은 최소 Sight를 포함하며, 혹시 명시 등록된 Hearing/Damage/기타 Sense가 있더라도 한 번에 제거한다.

향후 늦은 Sense 인스턴스 생성 등으로 엔진이 Pawn을 다시 source로 등록해도 전투 후보 승인은 아래의 공통 비전투 판정에서 차단된다.

## 5. 커스텀 감지/타깃 경로 제외 규칙

직접적인 `ADMDebugObserverPawn` cast를 AI 모듈마다 반복하지 않고 다음 구조를 추가했다.

1. `IDMNonCombatActor`: BP에서 임의 구현할 수 없는 C++ 비전투 마커 인터페이스.
2. `DeathMatchTeamUtils::IsCombatTargetCandidate`: 유효 Actor이며 비전투 마커가 없는 경우에만 true인 공통 판정.
3. `ADMDebugObserverPawn`이 `IDMNonCombatActor`를 구현.

공통 팀 태도 함수는 비전투 Actor에 항상 Neutral을 반환한다. 이는 `NoTeam` 검사와 별개이므로 관찰자의 팀 값이 바뀌어도 전투 후보가 되지 않으며, 일반 neutral Actor나 실제 플레이어/AI에는 영향이 없다.

`UAIModule_Sense`에는 추가 방어를 적용했다.

- hostile 승인, Awareness 삽입, Awareness 조회에서 공통 후보 판정을 사용한다.
- Perception 갱신에 비전투 Actor가 들어오면 `SeenTargets`, `KnownTargets`, `AwarenessMap`에서 제거하고 `UAIPerceptionComponent::ForgetActor`를 호출한다.
- 제외된 Actor가 Focus였으면 기존 `UpdateFocus` 경로로 대체 타깃/TargetLost 상태를 갱신한다.
- 주기적 Known cleanup도 `IsValidHostileTarget`을 통해 같은 판정을 사용한다.

피해 위협, 위협 평가, combat target 검증은 모두 `DeathMatchTeamUtils::GetAttitudeTowards(...) == Hostile`을 요구하므로 공통 Neutral 처리에 의해 관찰자가 피해 기억/위협 스냅샷/전투 타깃에 들어가지 않는다.

## 6. 전체 Pawn/Character 열거 점검

전 Pawn 열거 중 관찰자가 상태에 영향을 줄 수 있는 경로는 공통 후보 판정으로 제외했다.

- Roam move-point 근접 Pawn 수와 zone occupancy
- legacy BT move-location 근접 Pawn 수와 zone occupancy
- combat position registry occupant snapshot
- spawn point 근접 Pawn occupancy

Cover 가시성의 Pawn 열거는 모든 Pawn을 line-trace ignore 목록에 넣는 용도이며 타깃/점유 후보 수집이 아니다. 관찰자는 원래 actor/mesh collision이 꺼져 있어 이 경로는 수정하지 않았다. Health bar 경로는 `ACharacter`만 열거하므로 `APawn` 기반 관찰자는 후보가 아니다.

## 7. 유지한 관찰자 안전 속성

다음 기존 조건을 유지했다.

- `SetCanBeDamaged(false)`
- `TakeDamage(...) == 0`
- Actor collision off
- Observer mesh `NoCollision`, overlap off, navigation 영향 off
- actor replication off, movement replication off
- `FGenericTeamId::NoTeam`
- `UAIPerceptionStimuliSourceComponent` 미생성

DeathMatch 소스에서 `MakeNoise`, `ReportNoiseEvent`, `ReportDamageEvent`, `SendGameplayEventToActor` 생산 코드를 검색했으며 관찰자 또는 다른 DeathMatch 코드에 해당 호출이 없었다. 관찰자 자체에는 Gameplay Event, noise, damage stimulus를 발생시키는 코드가 없다.

Shipping의 전체 HP 표시 차단(`UE_BUILD_SHIPPING`에서 disable/early return)은 변경하지 않았다. AI 감지 제외는 빌드 구성 조건 없이 적용되며, 클래스의 Shipping 노출 구조나 debug UI 보안 경계를 확대하지 않았다.

## 8. 수정 파일

- `Core/DMNonCombatActor.h` (신규)
- `Core/DeathMatchTeamUtils.h`
- `Core/DeathMatchTeamUtils.cpp`
- `Core/DeathMatchSpawnUtils.cpp`
- `Debug/DMDebugObserverPawn.h`
- `Debug/DMDebugObserverPawn.cpp`
- `AIModules/AIModule_Sense.cpp`
- `AI/Move/DMMoveGoalProvider_Roam.cpp`
- `AI/BT/BTTask_DM_SelectMoveLocation.cpp`
- `AI/Combat/DMCombatPositionRegistry.cpp`
- `Communication/Reports/2026-09-29/1335_DEBUG_OBSERVER_LOOK_AND_AI_IGNORE_REPORT.md`

## 9. 정적 검증 결과

- `LookSensitivity` 단일 필드는 C++ 및 콘텐츠 직렬화 이름 검색에서 제거/미사용임을 확인했다.
- 네 Look 설정의 UPROPERTY specifier, 카테고리, 0 이상 clamp를 확인했다.
- Yaw/Pitch 계산은 일반 플레이어처럼 축 값을 같은 부호로 더하며 Pitch는 계속 `-89~89`로 제한한다.
- 마우스 delta 계산에 `DeltaTime`을 곱하지 않음을 확인했다.
- UE 5.6 `UAIPerceptionSystem::UnregisterSource` 실제 시그니처와 null Sense의 all-senses 동작을 확인했다.
- Sight의 all-Pawn 자동 source 등록을 엔진 코드에서 확인하고 명시 해제를 추가했다.
- Sense/Threat/Damage/Focus/Target 및 전 Pawn 열거 경로를 검색해 관찰자 제외 경로를 확인했다.
- 공통 판정은 오직 `IDMNonCombatActor` 구현 Actor만 제외하므로 일반 플레이어, AI, 일반 neutral Actor의 기존 감지/팀 판정은 유지된다.
- 수정 C++/header 파일의 중괄호 수 일치를 확인했다.
- 관련 없는 사격, 이동 의사결정, 전투 행동, UI 구현은 변경하지 않았다. Pawn 열거 변경은 비전투 마커 Actor를 기존 점유 계산에서 제외하는 조건 추가에 한정했다.

## 10. BP/PIE에서 사용자가 확인할 항목

1. `BP_DebugCharacter` Details에서 Look X/Y 감도와 X/Y Invert가 `DeathMatch|Debug Observer|Look`에 표시되는지 확인.
2. 기본값에서 마우스 오른쪽/왼쪽 및 위/아래 방향이 일반 플레이어와 같은지 확인.
3. 체감 속도가 일반 플레이어와 유사한지 확인하고 필요 시 X/Y를 독립 조정.
4. AI의 시야 안에서 관찰자를 이동시켜도 AI가 조준, Focus/BB Target 설정, 전투 상태 진입, 추적하지 않는지 확인.
5. 관찰자가 AI 근처/스폰 지점/이동 구역에 있어도 AI 점유 판단과 스폰 선택에 영향을 주지 않는지 확인.
6. 일반 플레이어와 AI 캐릭터 상호 감지는 종전대로 유지되는지 확인.
7. Shipping 구성에서 전체 HP 표시가 계속 비활성화되는지 확인.

## 11. 미수행 항목

요청에 따라 전체 빌드, 단위/자동화 테스트, BP 컴파일/테스트, PIE 테스트는 수행하지 않았다. 따라서 실제 BP Details 표시, 에셋 재직렬화, 런타임 입력 체감 및 AI 반응은 위 체크리스트로 최종 확인이 필요하다.
