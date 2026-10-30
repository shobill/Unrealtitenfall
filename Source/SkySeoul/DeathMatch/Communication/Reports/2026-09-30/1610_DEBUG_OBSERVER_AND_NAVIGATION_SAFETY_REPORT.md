# DeathMatch 디버그 관찰자 및 AI 내비게이션 안전성 수정 결과

- 작성일: 2026-09-30 16:10 KST
- 범위: `Source/SkySeoul/DeathMatch`, `Config/DefaultEngine.ini`의 Navigation 항목
- 구분: 구현 결과

## 결론

관찰자 Orbit 입력을 자유 시점 입력과 분리하고, Orbit 위치와 LookAt 방향이 서로 반대인 구조에 맞춰 기본 입력 부호를 반전했다. 새 대상 전환은 같은 프레임에 새 Orbit 위치와 LookAt 회전을 적용하므로 이전 대상 방향 보간이 남지 않는다.

DeathMatch Robot은 BeginPlay/Possess 및 명시적 갱신 시 실제 scaled capsule에서 Nav Agent 크기를 동기화한다. AI 이동은 단일 Supported Agent에 맞춘 NavMesh, 제한된 실제 capsule path sweep, 실패 목적지 cooldown, 복합 stuck 판정, DeathMatch Robot 전용 RVO를 사용한다. Crowd Following과 RVO는 동시에 활성화하지 않았다.

기존 설정은 NavMesh `R=10/H=70`으로 기본 캐릭터 capsule `R=42/H=192`보다 작았다. 프로젝트가 단일 Default Supported Agent를 사용하고 일반 캐릭터 기본 capsule도 42/96 half-height이므로 `R=42/H=192`, step height 45로 수정했다. 기존 생성 NavMesh에는 재빌드가 필요할 수 있다.

## 1. 변경 파일

- `Config/DefaultEngine.ini`
- `Debug/DMDebugObserverPawn.h`, `.cpp`
- `Navigation/DMNavigationSafety.h`, `.cpp` (신규)
- `Character/DeathMatchRobotCharacter.h`, `.cpp`
- `Controller/DeathMatchRobotAIController.h`, `.cpp`
- `AI/Framework/DMBehaviorModule_PathMove.cpp`
- `AI/Framework/DMSituationFramework.h`, `.cpp`
- `AI/Move/DMMoveGoalProvider_Roam.cpp`
- `AI/Combat/DMCombatTacticsComponent.h`, `.cpp`
- `AI/Combat/DMCombatAttackPosition.cpp`
- `AI/Combat/DMCombatCoverModule.cpp`
- `AI/Combat/DMCombatDebug.h`, `DMCombatDebugFormat.cpp`, `DMCombatTacticsDebug.cpp`
- `Tests/DMNavigationSafetyTests.cpp` (신규)
- 본 보고서

Blueprint 에셋과 Blueprint 설정은 수정하지 않았다.

## 2. 관찰자 Orbit 원인과 수정 방향

기존 코드는 자유 시점과 Orbit이 같은 `bInvertLookX/Y`와 같은 입력 부호를 사용했다. 그러나 Orbit은 `Pivot - OrbitForward * Distance`로 카메라 위치를 정한 뒤 다시 Pivot을 LookAt한다. 즉 위치 벡터와 실제 시선 방향이 반대라 자유 시점과 같은 부호를 적용하면 체감 방향도 반대로 나타날 수 있다.

기본값에서는 X/Y 모두 Orbit 위치 갱신 부호를 자유 시점과 반대로 적용한다. 오른쪽/왼쪽 이동은 해당 화면 공전 방향으로, raw mouse Y가 위쪽 이동에서 음수인 입력 체계에서는 위쪽 이동이 camera elevation 증가로 이어진다. 자유 시점 `bInvertLookX/Y` 계산은 변경하지 않았다.

새 설정은 `DeathMatch|Debug Observer|Follow Camera`의 `bInvertOrbitX=false`, `bInvertOrbitY=false`다. 기존 자유 시점 설정은 `DeathMatch|Debug Observer|Look`에 그대로 있다.

## 3. 대상 전환 유지

- 다음 대상: Page Up 또는 `]`
- 이전 대상: Page Down 또는 `[`
- 가장 가까운 대상: `R`
- 조준점 아래 대상 흡착/해제: `F`
- 거리: 마우스 휠

`SetFollowTarget`은 새 대상이면 `bResetOrbitOnNewTarget`에 따라 각도를 초기화하고, 즉시 새 Pivot 기준 Orbit 위치로 순간 배치한 뒤 LookAt 회전을 설정한다. Follow 상태는 유지되며 자유 이동 입력을 활성화하지 않는다. 대상 사망/제거 시 기존 weak candidate/index 기반 auto-next를 유지한다.

## 4. 실제 capsule과 Nav Agent 동기화

네이티브 기본 capsule은 radius 42, half-height 96, 전체 height 192다. Blueprint가 scale 또는 capsule 값을 바꾸는 경우 런타임 실제 값은 달라질 수 있다.

`SynchronizeNavAgentWithCapsule()`은 실제 `GetScaledCapsuleRadius()`와 `GetScaledCapsuleHalfHeight()*2`를 사용한다. UE 5.6의 `SetUpdateNavAgentWithOwnersCollisions(true)`와 `UpdateNavAgent(*this)`를 사용한 뒤 AgentRadius/AgentHeight도 명시적으로 같은 값으로 지정한다. 생성자 시점 값을 고정하지 않고 BeginPlay/Possess 후 실행하며 Tick에서는 실행하지 않는다. capsule 자체 크기는 변경하지 않는다. 런타임 capsule을 의도적으로 바꾸는 코드가 생기면 이 공개 함수를 다시 호출할 수 있다.

## 5. NavMesh 설정 변경

변경 전:

- Recast Agent Radius 10
- Recast Agent Height 70
- 전역 Agent Max Step Height 35
- 해상도별 Agent Max Step Height 10
- Supported Agent Default R=10/H=70, step=-1

변경 후:

- Recast Agent Radius 42
- Recast Agent Height 192
- 전역/해상도별 Agent Max Step Height 45
- Supported Agent Default R=42/H=192, step=45
- Agent Max Slope 44 유지
- Crowd Manager class 설정 유지

Config를 변경한 이유는 현재 Navigation System에 단일 Default Supported Agent만 있고, 기존 값이 네이티브 기본 캐릭터 capsule보다 명백히 작아 공유 NavMesh 자체가 통과 불가능한 통로를 허용하기 때문이다. 이 변경은 DeathMatch 밖에서 같은 기본 NavMesh를 사용하는 캐릭터에도 영향을 준다. 다른 크기의 캐릭터가 실제로 필요하면 Default 값을 다시 작게 만드는 대신 별도 Supported Agent/NavData 분리를 권장한다.

## 6. 불일치 진단

비 Shipping에서 실제 capsule, CharacterMovement NavAgent, 선택된 NavData config, Recast default-resolution step height, Character MaxStepHeight를 비교한다. 크기 허용 오차는 1cm, step 차이 경고 기준은 10cm다.

경고 조건은 capsule/NavAgent 불일치, NavMesh가 capsule보다 작음, 선택 NavData가 NavAgent보다 작음, step height 큰 불일치다. Pawn/NavData 조합당 중복 출력을 억제하고 Possess, 선택 NavData 변경이 확인되는 이동 요청, Nav generation 완료 시 재검사한다. 수동 명령은 `dm.Nav.Recheck`다. Shipping에서는 진단 로그, delegate, console command를 컴파일 경로에서 제외한다.

## 7. Clearance 검사

공통 이동 검증 순서는 Nav projection, 완전 경로 확인, 실제 capsule clearance 순서다. 부분 경로는 로밍/전투 모두 거부한다. 최종 capsule 검사는 경로 길이를 따라 균등 분배한 최대 1~8개 구간 sweep만 수행한다. 기본은 4개다.

전투 Attack Position은 기존 cheap 후보와 LOS 후보를 거친 `MaxPathFinalists`에만 clearance를 적용한다. Cover도 path finalist에만 적용한다. 로밍은 가중치로 선택된 최종 MovePoint 경로에 적용한다. 따라서 모든 raw 후보에 고비용 sweep을 실행하지 않는다.

로밍 clearance radius scale은 1.15, 전투는 1.0이다. 실제 capsule보다 작은 공간은 전투에서도 허용하지 않는다. 실패는 `WallClearance`, `AIBlocking`, `InvalidOrPartialPath` 등으로 기록하며 해당 목표 주변 100cm를 기본 1.5초 동안 재요청하지 않는다.

## 8. 벽 비빔 및 Stuck 판정

기존 단순 속도/이동거리 판정에 다음을 결합했다.

- 목표까지 거리의 최소 감소량
- 실제 속도/요청 MaxWalkSpeed 비율
- 목표 방향과 실제 속도 방향의 정렬도
- 실제 capsule의 짧은 전방 sweep에 의한 벽 접촉
- 같은 목표에 대한 반복 repath 횟수
- 설정 시간 동안의 무진행

판정되면 Move를 중단하고 목표 cooldown을 기록하고 활성 이동 모듈에 실패를 통지한다. Attack/Cover 등 기존 모듈의 실패 cooldown/재평가 경로가 이어진다. 같은 목적지로 즉시 무한 재요청하지 않는다.

정적 원인 분석상 기존 문제는 작은 NavMesh agent 크기, 최종 경로 capsule clearance 부재, AI 상호 회피 부재, 거리 중심 stuck 판정이 모두 기여할 수 있었다. PIE를 수행하지 않았으므로 특정 장면의 단일 원인을 확정하지는 않았다. 새 디버그의 `ClearFail`, `AIBlock`, `Stuck`, `Repath` 값으로 목적지/경로 여유/AI 길막을 구분할 수 있다.

## 9. AI 상호 회피 선택

RVO Avoidance를 선택했다. 이 프로젝트의 Controller 상속 체인은 상위 `AAIC_EnemyControllerBase`가 no-argument constructor를 고정하고 있어 권장 방식인 ObjectInitializer의 `SetDefaultSubobjectClass<UCrowdFollowingComponent>`를 DeathMatch 내부만 수정해 안전하게 적용하기 어렵다. 런타임에 기존 PathFollowingComponent를 교체하면 중복 component 수명 위험이 있어 사용하지 않았다.

RVO는 `ADeathMatchRobotCharacter`의 CharacterMovement에만 켠다. 엔진의 RVO radius는 실제 scaled capsule radius를 반환하므로 capsule보다 작지 않다. 로밍 weight 0.70, 전투 weight 0.45, consideration radius 500cm다. 전투는 목표 정밀도를 위해 낮고 로밍은 상호 분리를 위해 높다. Crowd Following은 활성화하지 않았으므로 두 방식 동시 활성화가 없다.

죽은 Robot은 RVO 등록을 해제하고 capsule의 Pawn response를 Ignore로 바꿔 통로를 영구 차단하지 않게 한다. respawn 시 원래 Pawn response와 RVO를 복원한다. Observer는 FloatingPawnMovement이며 collision off라 회피 agent가 아니다. detached parts는 기존 Pawn ignore 정책을 유지한다.

RVO는 NavMesh 경계 자체에 강제 구속되는 방식이 아니므로, 보수적인 roam clearance와 최종 path sweep, 낮은 combat weight, blocked recovery로 위험을 줄였다. 좁은 경계에서 실제 움직임은 PIE 확인이 필요하다.

## 10. 디버그 출력

상세 Combat Debug에 한 줄의 Navigation summary를 추가했다.

- capsule R/H, Character NavAgent R/H
- 선택 NavData 이름과 NavMesh R/H
- Move owner와 목적지
- 최근 clearance 실패와 AI blocking 여부
- stuck 이유와 repath 횟수
- avoidance 방식 `RVO`

compact 기본 출력은 늘리지 않았다. NavMesh 표시는 기존 `P`/`show Navigation`을 사용한다.

## 11. Native 자동화 테스트 소스

`SkySeoul.DeathMatch.Combat.Navigation` 및 Debug 하위에 다음 순수 계산 테스트를 추가했다.

- capsule radius/half-height에서 Agent radius/height 계산
- 작은 NavMesh 경고와 같거나 큰 NavMesh 정상 조건
- Orbit 기본/반전 입력 부호
- stuck 시간과 속도 비율 조건
- Crowd+RVO 동시 선택 거부
- 부분 경로 거부
- clearance 실패 후보 거부

테스트 소스는 Editor build에 컴파일됐지만 Automation 실행은 하지 않았다.

## 12. BP에서 사용자가 확인할 항목

1. Observer BP Details의 Follow Camera에서 `Invert Orbit X/Y=false` 기본 체감을 확인한다.
2. Page Up/Down, 대괄호, R, F, wheel 및 대상 사망 auto-next를 PIE에서 확인한다.
3. Robot BP의 실제 scaled capsule이 42/96과 다르면 `dm.Nav.Recheck` 로그의 실제값을 기준으로 NavMesh Supported Agent 분리 여부를 판단한다.
4. Navigation System이 설정 변경 후 NavMesh를 재생성했는지 `P`로 확인하고 필요하면 Build Paths를 수행한다.
5. 좁은 문/통로가 R=42 NavMesh에서 의도치 않게 끊기지 않는지 확인한다.
6. Controller BP의 Roam/Combat Clearance Scale, RVO Weight, consideration radius, blocked cooldown을 게임 감각에 맞춰 조정한다.
7. Combat Brain과 Situation Framework의 Stuck/Minimum Progress 설정을 확인한다.

## 13. 알려진 제한 및 후속 권장

- Blueprint CDO의 실제 capsule override는 에셋을 열지 않았으므로 미확인이다. 런타임 동기화와 진단은 override를 반영한다.
- Config 변경만으로 이미 저장된 NavMesh 데이터가 즉시 갱신된다고 가정하지 않는다. 에디터에서 NavMesh 재생성 확인이 필요하다.
- RVO는 국소 회피이며 전역 교착 해결기가 아니다. 좁은 일방 통로에서 반복 교착이 남으면 상위 Controller constructor 정리 후 Crowd Following 전환을 별도 작업으로 검토한다.
- 복수 크기 Pawn이 같은 맵에 필요하면 Supported Agent/NavData 분리가 필요하다.
- runtime capsule resize를 수행하는 새 코드에서는 `SynchronizeNavAgentWithCapsule()`을 호출해야 한다.

## 검증 상태

- `SkySeoulEditor Win64 Development` 전체 C++/UHT 빌드: 성공.
- 정적 검색: DeathMatch의 실제 MoveTo 발행 지점은 로밍 PathMove와 Combat Brain 두 곳이며 기존 상호 배제 소유권을 유지함.
- 정적 검색: Crowd Following 활성화 없음, DeathMatch Robot RVO만 활성화됨.
- 정적 검색: Observer 자유 시점은 기존 `bInvertLookX/Y`, Orbit은 새 `bInvertOrbitX/Y`만 사용함.
- native 테스트 소스 컴파일: 성공. Automation 실행은 생략.
- Blueprint 에셋 수정/설정 작업: 수행하지 않음.
- Blueprint 테스트, Unreal Editor 실행, PIE: 수행하지 않음.
- 요청에서는 전체 에디터 빌드를 생략해도 된다고 했으나, C++ API/UHT 검증을 위해 명령행 Editor target 빌드는 수행했다. 에디터 GUI나 Blueprint 로드는 수행하지 않았다.
