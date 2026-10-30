# Debug Observer와 모듈형 Health Bar 구현 결과

- 작성일: 2026-09-29 12:52 KST
- 범위: `Source/SkySeoul/DeathMatch`
- 구분: 구현 및 정적 검증, Editor 타깃 빌드/UHT 검증
- Content/BP/맵 에셋 변경: 없음

## 결론

개발자용 X-ray 표시와 실제 사용자용 가시 표시를 서로 다른 컴포넌트로 구현했다. 두 컴포넌트가 공유하는 것은 Health Snapshot, 위치 계산, C++ 기본 위젯, 동적 Widget Component 풀과 Entry 수명 관리뿐이다. 전체 캐릭터 검색과 raw health 직접 조회는 Debug 컴포넌트가 소유하고, Visible 컴포넌트는 화면/거리/LOS/정책 승인 뒤 Public Health Provider를 먼저 요청한다. 원격 네트워크 클라이언트는 Provider가 없을 때 다른 Pawn의 raw health로 fallback하지 않는다.

`SkySeoulEditor Win64 Development` 빌드와 UHT는 최종 성공했다. 자동화 테스트 실행, BP 컴파일/로드, PIE, Shipping 패키징 및 실제 네트워크 검증은 수행하지 않았다.

## 1. 최종 클래스 구조

```text
ADMDebugObserverPawn
└─ UDMDebugAllHealthBarsComponent
   └─ UDMHealthBarsRuntimeComponent (neutral runtime base)

Player Pawn 또는 PlayerController
└─ UDMVisibleHealthBarsComponent
   └─ UDMHealthBarsRuntimeComponent (neutral runtime base)

공통
├─ FDMHealthSnapshot / EDMHealthSource
├─ DMHealth::Resolve / UDMHealthFunctionLibrary
├─ UDMHealthBarWidget
├─ IDMPublicHealthProvider
└─ IDMHealthBarAuthorizationProvider
```

`UDMVisibleHealthBarsComponent`에는 AllCharacters, X-ray, 벽 뒤 표시 또는 Debug 컴포넌트 전환 API가 없다.

## 2. 공통 Health Snapshot 및 Resolver

`FDMHealthSnapshot`은 CurrentHealth, MaxHealth, HealthRatio, bAlive, bValid, HealthSource를 제공한다. HealthSource는 Invalid, BodyHP, GAS, PublicData를 구분한다. `DMHealth::MakeSnapshot`이 finite/MaxHealth>0 검증, CurrentHealth와 Ratio Clamp, 기존 사망 상태 결합을 한 곳에서 수행한다.

`DMHealth::Resolve`와 BP용 `UDMHealthFunctionLibrary::GetHealthSnapshot`은 읽기 전용이다. UI Widget은 Actor 체력을 직접 조회하지 않고 전달된 Snapshot만 표시한다.

## 3. DeathMatch 체력 소스 우선순위

1. `APartsCharacterBase`와 `ADeathMatchPlayerCharacter`: 유효한 CurHP/MaxHP BodyHP 우선.
2. 그 밖의 `ACharacterBase`: 유효한 GAS GetHealth/GetMaxHealth 우선.
3. GAS가 없거나 MaxHealth가 0 이하/비유한 값이면 유효한 CurHP/MaxHP fallback.
4. 어느 쪽도 유효하지 않으면 bValid=false, HealthRatio=1, HealthSource=Invalid.

bAlive는 `ACharacterBase::IsDead`가 false이고 Clamp된 CurrentHealth가 0보다 큰 경우에만 true다. 기존 GetHealth/GetMaxHealth의 전역 의미는 변경하지 않았다.

## 4. Combat Brain 체력 조회 변경

- `UDMCombatTacticsComponent::BuildContext`: 자기 체력을 Resolver로 한 번 읽고 기존 `DMCombatHealth::Evaluate` 계산식과 설정에 전달한다.
- `DMCombatThreatEvaluation`: FinishTargetBonus가 Resolver의 Target HealthRatio를 사용한다.
- `DMCombatDamageAwareness`: 최근 피해 비율의 분모가 Resolver의 MaxHealth를 사용한다.

기존 Health Evaluation 수식, 임계값, 가중치와 동작 정책은 변경하지 않았다.

## 5. Debug Observer Pawn 구성

`ADMDebugObserverPawn`은 APawn 기반이며 Scene Root, Camera, 선택적 Engine Sphere Mesh, FloatingPawnMovement, `UDMDebugAllHealthBarsComponent`를 가진다. 기본적으로 non-replicated, movement replication off, actor collision off, mesh NoCollision/overlap off/navigation 영향 off, CanBeDamaged=false, TakeDamage=0, GenericTeamId=NoTeam이며 Perception Stimuli 컴포넌트를 만들지 않는다.

로컬 Possess 상태에서 W/S/A/D, E 또는 Space 상승, Q 또는 LeftControl 하강, Shift 고속 이동, 마우스 시점을 C++로 읽는다. GameMode 기본 Pawn과 기존 입력 에셋은 변경하지 않았다.

## 6. Debug All Health Bars 대상 검색

0.5초 기본 주기의 `TActorIterator<ACharacter>`로 후보를 갱신한다. 매 프레임 GetAllActorsOfClass를 호출하지 않는다. Actor/자기 자신 유효성, Resolver 유효성, 선택적 거리, 사망 표시 옵션을 적용한다. 체력은 후보별 기본 0.1초 주기로 갱신한다. 파츠 Debris는 ACharacter 후보가 아니므로 제외된다. MaximumDisplayDistance=0은 무제한이다.

진단값은 TrackedTargetCount, ActiveWidgetCount, PooledWidgetCount, InvalidHealthCount다.

## 7. Visible Health Bars 화면/LOS 판정

후보 존재 수집은 0.25초 주기의 Character 열거이며 이 단계에서는 HP를 읽지 않는다. 기본 0.1초 visibility pass에서 다음 순서를 사용한다.

1. Actor와 표시 정책 유효성
2. 최대 6000cm 거리
3. PlayerCameraManager forward dot
4. 머리 위 anchor의 ProjectWorldLocationToScreen
5. player viewport rect
6. 위 조건을 통과한 대상만 Visibility channel LOS trace
7. 공개 health 승인 및 Snapshot 요청

LOS는 카메라에서 캐릭터 Capsule 절반 높이의 가슴 위치를 향한다. viewer/owner와 viewer 부착 Actor를 ignore한다. 무충돌, Target 자체 또는 Target이 소유한 Actor hit는 visible로 처리한다. LOS 실패에만 최대 0.2초의 마지막 Snapshot grace를 적용하며 거리/후방/화면 밖/정책 실패에는 즉시 숨긴다.

진단값은 CandidateCount, OnScreenCount, LOSVisibleCount, AuthorizedHealthCount, ActiveWidgetCount, PooledWidgetCount다.

## 8. 공유 코드와 공유하지 않는 정책

공유 항목:

- Snapshot 값 검증과 Resolver
- C++ Health Bar Widget의 표시 갱신
- Socket/Capsule/Bounds anchor 계산
- Screen Space Widget Component 생성, 풀링, 위치 갱신, 파괴 안전성
- Target Entry의 weak target, widget, 마지막 Snapshot/visible time/display state/next health time

공유하지 않는 항목:

- Debug의 전체 Character 검색 권한과 raw Resolver 접근
- Visible의 팀/외부 정책, 거리, 카메라, 화면, LOS 판정
- Visible의 Public Health Provider와 네트워크 fallback 제한
- Shipping Debug gate

중립 Base에는 AllCharacters/VisibleOnly 모드나 검색 함수가 없다.

## 9. Shipping Debug 차단

별도 Developer 모듈은 현재 단일 게임 모듈 구조에 비해 범위가 커 추가하지 않았다. UCLASS는 UHT 안전을 위해 항상 선언한다. 대신 `UDMDebugAllHealthBarsComponent`의 BeginPlay, Tick, enable API, availability API와 Scan 함수에 compile-time `UE_BUILD_SHIPPING` gate를 적용했다.

Shipping에서는 Tick을 끄고 bEnabled를 false로 강제하며 후보/위젯을 비운다. Tick은 즉시 return하고 Scan도 즉시 return하므로 대상 열거, Resolver 조회, Widget 생성이 실행되지 않는다. BP에서 enable 함수를 호출해도 다시 활성화되지 않는다. Observer Pawn 클래스 자체는 패키지에 남고 자유 카메라 코드는 존재하지만 전체 체력 기능은 활성화할 수 없다. 별도 Developer 모듈만큼의 물리적 코드 제외는 아니라는 한계가 있다.

## 10. 실제 사용자용 공개 체력 데이터 경계

`IDMPublicHealthProvider::GetPublicHealthSnapshot(Viewer, Target, OutSnapshot)`과 `IDMHealthBarAuthorizationProvider::{CanDisplayHealthBar, CanRevealHealth}`를 추가했다. 명시 Provider가 없으면 Owner, local PlayerController, Pawn, GameState 순서로 인터페이스 구현자를 찾는다.

Visible 컴포넌트는 최종 화면/LOS 통과 뒤에만 Provider를 호출한다. Provider 결과는 PublicData source로 다시 검증/정규화한다. Provider가 있되 값을 거부하거나 invalid를 반환하면 raw health로 우회하지 않는다.

## 11. 현재 네트워크 보안 수준과 한계

원격 네트워크 클라이언트는 Provider가 없을 때 다른 Pawn의 CurHP/GAS로 fallback하지 않는다. 선택적으로 자기 Pawn만 raw fallback할 수 있다. Standalone과 Listen Server 로컬 권한 환경만 `bAllowStandaloneOrAuthorityResolverFallback`으로 visible 대상의 Resolver fallback을 허용한다.

이번 작업은 서버 권한 replicated public-health feed 자체를 구현하지 않았다. 따라서 네트워크 클라이언트에서 적 HP를 표시하려면 서버가 승인/만료를 관리하는 Provider 구현이 후속으로 필요하다. 기존 게임 코드가 정확한 적 HP를 이미 모든 클라이언트에 복제한다면 이 UI 경계만으로 메모리 치트를 막을 수 없다. ExpirationTime을 포함하는 별도 replicated feed 구조도 후속 범위다.

## 12. Widget Pool 및 업데이트 주기

동적 `UWidgetComponent`는 owner의 local instance component로 만들고 replication/collision/overlap/navigation 영향을 끈다. Screen Space, 기본 140x18, pivot 0.5/1.0이다. 숨길 때 Destroy하지 않고 pool로 반환하며 다음 Target이 재사용한다. Actor 파괴/invalid weak pointer는 정리하고 EndPlay에서 component를 파괴한다.

활성 Widget 위치만 매 Tick 갱신한다. Debug scan 0.5초/health 0.1초, Visible candidate scan 0.25초/visibility+LOS 0.1초/health 0.1초가 기본이다. 동일 Snapshot이면 Progress Bar와 Text를 다시 설정하지 않는다.

## 13. HP 바 위치 계산

1. Character mesh에 설정 Socket이 실제 존재하면 Socket world location.
2. Character면 ActorLocation + ActorUp × (scaled CapsuleHalfHeight + HeightOffset).
3. 그 외 Actor면 Bounds origin + world Z extent + HeightOffset.

기본 Socket 이름은 `HealthBar`, HeightOffset은 30cm다. LOS는 HP 바 머리 위치가 아니라 가슴 위치를 사용한다. Screen Space Widget이 활성 PlayerCameraManager/viewport를 기준으로 렌더링되며 Observer actor rotation을 billboard 기준으로 사용하지 않는다.

## 14. 변경 파일 목록

신규:

- `Core/DMHealthTypes.h`
- `Core/DMHealthResolver.h/.cpp`
- `Debug/DMDebugObserverPawn.h/.cpp`
- `Presentation/HealthBars/DMHealthBarWidget.h/.cpp`
- `Presentation/HealthBars/DMHealthBarsRuntimeComponent.h/.cpp`
- `Presentation/HealthBars/DMDebugAllHealthBarsComponent.h/.cpp`
- `Presentation/HealthBars/DMVisibleHealthBarsComponent.h/.cpp`
- `Presentation/HealthBars/DMHealthBarProviders.h`
- `Tests/DMHealthBarTestTypes.h`
- `Tests/DMHealthResolverTests.cpp`
- 본 보고서

수정:

- `AI/Combat/DMCombatTacticsComponent.cpp`
- `AI/Combat/DMCombatThreatEvaluation.cpp`
- `AI/Combat/DMCombatDamageAwareness.cpp`

기존 Build.cs에 Engine/UMG/Slate/SlateCore/EnhancedInput/AIModule가 이미 있어 모듈 파일은 변경하지 않았다. DeathMatch 밖 Source와 Config/Content의 기존 사용자 변경은 수정하거나 되돌리지 않았다.

## 15. 작성한 테스트

`DMHealthResolverTests.cpp`에 다음 Editor automation 테스트 코드를 작성했다.

- Snapshot invalid max/current/source, Clamp, Ratio, 기존 사망 상태 결합
- DeathMatch Robot의 BodyHP 우선
- 일반 Character의 유효 GAS 우선과 GAS MaxHealth=0 body fallback
- Capsule top anchor 계산
- Observer의 CanBeDamaged=false, TakeDamage=0, replication/collision off, NoTeam

테스트 코드는 UHT/컴파일됐지만 Unreal Automation으로 실행하지 않았다. Widget Pool 재사용, 실제 Socket, 화면/LOS와 네트워크 fallback은 PIE/통합 테스트가 필요하다.

## 16. 수행한 정적 검증

- 신규/수정 헤더의 generated include가 마지막 include인지 확인.
- 충돌 마커 없음 확인.
- 신규 UPROPERTY의 production 참조 검색.
- Visible 컴포넌트에 AllCharacters/X-ray/Debug dependency와 GetAllActorsOfClass가 없음 확인.
- UI Widget이 CurHP/GetHealth를 직접 선택하지 않음 확인.
- Combat Brain, Threat Evaluation, Damage Awareness와 Debug/Visible 제한 fallback이 공통 Resolver를 사용함 확인.
- Shipping Debug의 Tick/Scan/enable compile-time gate 확인.
- scan/health/visibility 주기와 LOS가 visibility pass에서만 호출되는지 확인.
- Widget pool 반환, weak target, Actor invalid/EndPlay 정리 확인.
- Pawn/Component/Widget replication 비활성 확인.
- 적용한 patch 경로가 `Source/SkySeoul/DeathMatch` 내부임을 확인.

## 17. 빌드/UHT 및 미수행 검증

수행:

- `Build.bat SkySeoulEditor Win64 Development ... -WaitMutex -NoHotReloadFromIDE`
- 최종 결과: UHT 성공, C++ compile/link 성공, `Result: Succeeded`.

미수행:

- Unreal Automation 테스트 실행
- BP 클래스 생성/컴파일/로드/저장
- Editor UI 실행과 PIE
- Shipping 타깃 빌드/패키징
- Dedicated/Listen/Remote client 네트워크 런타임
- 장시간 성능/메모리/Slate profiling

따라서 최종 Development Editor 빌드 성공은 확인했지만 BP/PIE/Shipping/네트워크 동작 성공을 의미하지 않는다.

## 18. BP에서 해야 할 설정

Debug:

1. `ADMDebugObserverPawn`을 직접 배치하거나 BP 파생 클래스를 만든다.
2. 원하는 시점에 local PlayerController로 Possess한다.
3. 기본 C++ Widget으로 즉시 표시 가능하며 필요하면 WidgetClass만 BP 파생 `UDMHealthBarWidget`으로 교체한다.
4. GameMode DefaultPawn은 자동 변경되지 않는다.

Visible:

1. local player Pawn 또는 PlayerController에 `UDMVisibleHealthBarsComponent`를 추가한다.
2. 거리, LOS, 팀 표시, grace와 WidgetClass를 조정한다.
3. 네트워크 게임은 PlayerController/Pawn/GameState 또는 명시 객체에 `IDMPublicHealthProvider`를 구현한다.
4. 게임 모드별 승인 규칙이 필요하면 `IDMHealthBarAuthorizationProvider`를 구현하거나 `bRequireAuthorizationProvider`를 켠다.

## 19. PIE 테스트 시나리오

1. Debug Observer 배치/Possess 후 자유 비행, 벽 뒤/화면 밖/원거리 Character 표시와 mesh hidden 기본값 확인.
2. Observer 충돌/피해/AI perception/팀/공격 타깃/Nav 영향이 없는지 확인.
3. Robot/Player/GAS Character의 100/60/30/0 HP에서 숫자, 색상, alive/dead 옵션 확인.
4. Socket 존재/부재, Capsule 크기, non-Character Bounds의 anchor 확인.
5. Visible 컴포넌트에서 거리 밖, 카메라 뒤, viewport 밖, 벽 뒤가 각각 숨는지 확인.
6. 얇은 기둥/모서리에서 LOS grace가 0.2초만 유지되고 장기 벽 추적이 되지 않는지 확인.
7. team/neutral/hostile 옵션과 외부 authorization allow/deny 확인.
8. Provider valid/invalid/deny 및 Provider 존재 시 raw fallback 우회가 없는지 확인.
9. Standalone/Listen Server fallback과 Remote Client의 enemy raw fallback 금지를 분리 확인.
10. 반복 표시/숨김에서 Widget 수가 계속 증가하지 않고 pool이 재사용되는지 확인.
11. Target 파괴/사망/respawn/level transition/owner EndPlay에서 dangling widget이 없는지 확인.
12. F8, view target/camera 전환과 split-screen 가능 환경에서 현재 local camera 기준 표시 확인.

## 20. 알려진 제한과 후속 작업

- 서버 권한 replicated public-health feed와 expiration은 인터페이스 경계만 만들었고 실제 전송 시스템은 없다.
- Debug 클래스는 별도 Developer 모듈이 아니라 게임 모듈에 남으며 Shipping에서 실행 경로만 compile-time 차단한다.
- `TActorIterator<ACharacter>`는 10~30명 목표의 초기 구현이다. 대규모 게임은 registry/spatial candidate source로 교체할 수 있다.
- Widget pool, Screen Space 화면 밖 실제 Slate 비용, split-screen owner player, LOS grace 품질은 PIE/profiling 확인이 필요하다.
- C++ 기본 위젯은 140x18의 최소 표시다. 긴 Actor 이름/HealthSource는 BP 파생 Widget에서 레이아웃 조정이 필요하다.
- 기본 Observer 조작은 키 직접 polling이다. 프로젝트 전용 Enhanced Input Action/IMC 통합은 기존 입력 에셋을 수정하지 않기 위해 제외했다.
- 현재 팀 fallback은 GenericTeam attitude와 Friendly/Hostile/Neutral 토글이다. 다른 게임 모드의 세부 규칙은 authorization provider로 확장한다.

