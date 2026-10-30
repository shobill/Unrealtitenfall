# DeathMatch 소유 관계 — 2026-10-02

## 전투 흐름

`AIPerception → AIModule_Sense → SituationFramework → Combat Brain → Blackboard/BT → GameplayAbility_Fire → Weapon Actor`

- Sense: 생존·적대 필터, Seen/Known/Awareness, focus와 기억. Framework를 직접 깨운다. LocalMessage 방송/수신 계층은 제거했다.
- SituationFramework: Situation Profile(기존 Situation Brain) 생성, Roam Behavior만 생성·선택, 전투 진입/이탈, Roam stuck 복구. Tactics가 교전을 처리하면 Brain 평가·Behavior 실행·Blackboard 덮어쓰기를 하지 않는다.
- Combat Brain (`UDMCombatTacticsComponent`, 표시 이름 `DM Combat Brain`): 배열로 등록한 접근·후퇴·엄폐·공격 Action의 강한 Priority 계층과 같은 계층 내 가중 점수 선택, 목적별 EQS 요청, 조준·회전, 이동 중 사격 허용, 최종 사격 승인. 교전 중 자체 Tick으로 갱신한다. 실제 Combat Target/Pending Target 및 Threat Awareness를 소유하며, 시야 소실 기억은 Brain의 deadline으로 제한한다. 기존 Action의 memory 프로퍼티는 직렬화 호환을 위해 남는다.
- BT: WantAttack을 따라 FaceTarget/ActivateAbility 실행. FaceTarget은 교전 중 Tactics의 조준을 덮어쓰지 않는다.
- GameplayAbility_Fire: GAS 활성화·몽타주·Fire/FireCycle Notify·실제 trace·명중·cadence·종료 정리. Single/Burst/Automatic이 하나의 공통 Cycle Runner를 사용한다.
- Weapon Actor: EffectiveRange/MaxFireRange, 거리별 사격 확률, 피해 감쇠 및 spread 데이터의 최종 소유자. 수치와 정책을 변경하지 않았다.

## 비전투 이동

`Framework → Situation Profile(Roam) → Behavior_Move/PathMove → CombatMoveDirector → Roam Provider → Tactical Nav Region Graph → Region stored NavPoint/complete path → CombatMoveDirector MoveTo`

기존 Roam 선택 가중치와 이동 옵션을 보존한다. 현재 BT_Combat에는 엔진 MoveTo가 없다. HasMoveGoal은 false로 유지하여 다른 BT 이동 가지도 병행하지 못하게 한다. SelectMoveLocation은 에셋 참조 때문에 유지하며 Framework가 없는 Controller의 호환 경로에서만 BB 목표를 선택한다.

전투 시작 시 Tactics가 Roam 이동과 Director 목표를 정리하고 BB 이동 목표를 철회한 다음 전투 이동을 요청한다. 종료 시 Framework가 Roam으로 복귀한다. MoveCompleted의 동기 재진입을 차단한다. 사망·unpossess·respawn에서는 Sense의 감지·대상 기억과 Tactics/Framework 상태를 모두 초기화한다.

## Fallback

Situation Profile에서 Combat Brain 컴포넌트가 없거나 disabled/설정 누락 등으로 실행할 수 없고 적이 보이면, 이동 목표와 WantAttack을 지우고 정지한다. 구형 Attack/Hold/Approach/HuntLost로 넘어가지 않는다. 적이 보이지 않고 Tactics 기억도 끝났으면 Roam으로 이탈한다. Pawn이 없거나 사망한 경우 Roam하지 않는다. RoamOnly Brain의 비전투 용도는 유지한다.

## 직렬화 호환

BP가 참조하는 구형 Behavior 4종과 Approach/HuntLost Provider는 삭제하지 않았다. Framework는 저장된 AllowedBehaviorClasses에서도 Roam만 생성한다. Director는 Roam Provider만 등록한다. Provider native subobject 및 BP 값은 호환성 때문에 남는다. enum 값과 기존 클래스명도 유지한다. 참조 0으로 확인한 클래스만 제거했으므로 대체 클래스 Core Redirect는 추가하지 않는다.

조사 근거: Communication/Reports/2026-09-17/1050_DEATHMATCH_ARCHITECTURE_REFERENCE_REPORT.md.


## Brain Module과 Action Module

`DM Combat Brain → Movement Actions[] + Fire Action + Aim/Target coordination`

Combat Brain은 공통 Context, 이동 Action 우선순위/활성 선택, 목적별 EQS 실행·결과 캐시, 조준·회전, 사격 승인, 전투 Blackboard, 진입·종료·초기화와 `FDMFireCycleRuntime`의 최종 소유자다. 실제 `MoveToLocation` 호출은 Combat Move Director만 수행한다. Primary 이동은 `MovementActions`의 `Enabled/Action/Priority/ScoreMultiplier` entry로만 등록한다. 실행 가능한 최고 Priority를 먼저 정하고 같은 Priority에서 `Action Score × ScoreMultiplier + 유지 보너스 - 같은 계층 전환 비용`을 비교한다. 구 고정 3개 슬롯과 자동 migration/fallback은 없으므로 기존 BP는 배열을 직접 다시 구성한다.

Movement Action은 전달된 Context를 평가하고 실행 가능 여부와 Goal/Fire/Aim 요청을 반환한다. 다른 Action이나 Blackboard 전체를 통제하지 않는다. Fire Action은 패턴, 간격, 완료 후 휴지 시간과 이동 속도 정책을 제공한다. Fire Ability가 몽타주, Notify, 실제 ExecuteFire, Timer와 종료 처리를 실행한다. Weapon Actor의 피해·거리·확률 데이터는 변경하지 않았다.

`BP_DM_Brain_Combat`의 native class는 `UDMSituationBrain_Combat`이며 표시 이름은 `DM Situation Profile - Combat`이다. 비전투 Roam/BT Profile과 안전한 fallback 역할이다. 실제 교전 Brain과 구분하며 에셋 경로는 유지한다.

## Fire Mode와 공통 실행 경로

`Fire Action 설정 → Brain 승인 → Fire Ability 활성화 → Montage → Fire → FireCycle → cadence/다음 승인 → Montage 재생 또는 종료`

- Precision: 활성화당 성공한 ExecuteFire 1회. FireCycle에서 종료하고 ShotPauseSeconds로 재활성화를 차단한다. AimSeconds와 MaxMoveSpeed를 유지한다. FireCycle이 없는 기존 Single 몽타주는 완료 시 종료할 수 있지만 가짜 FireCycle이나 발사는 만들지 않는다.
- Burst: 한 활성화 안에서 ShotsPerBurst(기본 3)만큼 성공 발사를 반복한다. ShotInterval을 지키며 마지막 성공 발사에 BurstPause를 적용한다. BT 재활성화로 점사 횟수를 채우지 않는다.
- Automatic: 같은 활성화에서 반복한다. MaxAutomaticShots=0은 조건이 유지되는 동안 무제한, 양수는 성공 횟수 한도에 도달하면 AutomaticPause를 적용한다. bAllowFireWhileMoving/MaxFireMoveSpeed로 이동 사격을 제어한다.
- Adaptive: 기존 거리 구간과 hysteresis로 근거리 Automatic, 중거리 Burst, 원거리 Single을 선택한다. 최대 범위 밖에서는 승인하지 않는다. 부분 점사는 기존처럼 패턴을 잠근다. 선택한 설정을 동일한 Runner에 전달하며 별도 반복 실행 코드는 없다.
- Use Existing Module: 저장된 instanced FireModule 객체와 값을 유지하는 BP 호환 모드다. 실제 Precision/Burst/Automatic/Adaptive 객체의 GetCycleSettings를 사용한다. 신규 설정은 명시적인 Fire Mode를 권장한다. 명시 모드와 Fire Action 클래스가 다르면 기존 BeginPlay 로직이 해당 타입의 런타임 객체를 생성하므로, 사용자 지정 값은 모드와 일치하는 Fire Action에 설정한다.

성공 횟수는 ExecuteFire가 true를 반환한 경우에만 증가한다. BP override도 실제 발사를 수행한 경우에만 true를 반환해야 한다. 실패한 시도는 횟수나 정상 Burst/Automatic pause를 추가하지 않으며, 취소·실패에는 짧은 0.075초 anti-spin 지연을 사용한다. 이미 성공한 발사에서 얻은 cadence/pause는 Reset으로 지우지 않는다.

## Notify와 수명 관리

반복 모드의 재생 구간에는 실행 가능한 Fire Notify 뒤에 FireCycle Notify가 있어야 한다. Fire만 Trace/탄/피해를 실행한다. FireCycle은 다음 사이클 판단만 한다. 기존 authored notify-time timer 옵션은 유지하며 callback과 timer의 중복은 cycle ID, 몽타주 instance ID, shot-attempt/queued 플래그로 막는다.

Fire Notify 오프셋을 재시작 예약 시간에서 빼서 성공 발사 사이 ShotInterval을 맞추며, 실제 발사 직전에도 cadence를 검사한다. 현재 FireCycle보다 먼저 재생을 시작할 수 없으므로 몽타주 구간이 길면 실제 간격은 설정값보다 길어진다. Notify 시간이나 애니메이션 속도를 임의로 바꾸지 않는다.

타깃 소실·변경·사망, 사수 사망, 무기 소실·변경, Controller/ASC 소실, CanShoot=false, 사거리/속도 위반, 사선 차단·아군 위험, 몽타주 실패·취소·Interrupt는 종료 조건이다. Notify 누락은 원인과 경고를 남긴다. Watchdog은 종료만 하며 발사를 합성하지 않는다. 종료는 재진입을 막고 4종 Timer, Fire timer 배열, Notify delegate와 Montage Task를 정리한다.

## Details와 직렬화

기존 UCLASS, enum 이름/순서와 `FirePolicy`/`AdaptiveFireSettings` 서브오브젝트 이름은 유지한다. 이동의 `MovementModule/MovementModulePriority2/MovementModulePriority3` UPROPERTY와 `MovementPolicy` 기본 서브오브젝트는 2026-10-02 배열 전환에서 호환 계층 없이 제거했다. C++ Action 클래스명 rename과 Core Redirect는 적용하지 않았다. 컴포넌트 및 Fire/Movement Action의 DisplayName과 Category를 유지한다.

Primary `MovementActions`가 비어 있으면 Action을 자동 생성하지 않고 Hold한다. Hiding도 배열에 명시적으로 등록된 경우에만 실행한다. `Micro Movement Auxiliary`와 Brain 전용 `InvestigationAction`은 Primary 배열 밖의 별도 보조 경로다.

AdaptiveSettings는 `FireMode == EDMFireMode::Adaptive`와 `EditConditionHides`로 Adaptive일 때만 표시된다. 표시 조건은 값을 삭제하지 않는다. Use Existing에서 저장된 Adaptive Fire Action을 사용하는 경우에는 그 객체 자체의 값으로 실행하며, 별도 Adaptive Settings를 강제로 선택하지 않는다. 나머지 모드별 전용 Settings 객체는 추가하지 않았다.

`dm.CombatDebug 2`는 모든 모드의 Brain/Active Movement Action, Fire Mode/실제 Action 클래스, 패턴, 성공 횟수, 점사 잔여 수, 연사 한도, Cycle ID/State, Notify 수신, 남은 cadence 및 종료/실패 원인을 표시한다. 거리 구간은 Adaptive Action일 때만 추가한다. 표시를 위한 별도 trace, 확률 roll, Actor 전체 검색은 없으며 새 문자열 생성은 non-Shipping 관찰 코드 안에 있다.

## 검증 상태

2026-09-22: 코드·직렬화 식별자와 에셋 참조 문자열만 정적으로 점검했다. 빌드/UHT, 자동화 테스트, PIE, BP 런타임 테스트는 실행하지 않았다. 컴파일 성공이나 BP/몽타주 동작 성공을 의미하지 않는다. 기존 Controller BP/SCS 값과 Details 갱신, 모드별 실제 발사 횟수·간격은 후속 에디터 검증 대상이다.


## Threat Awareness와 타깃 기억 (2026-09-22)

Perception Focus는 Sense의 추천 후보다. Combat Target은 Brain이 확정한 공격 대상이며, Pending Target은 시야 상실 뒤 지연 중인 후보다. Sense Focus 변경만으로 Combat Target을 바꾸지 않는다. Movement Action은 이 참조들을 변경할 수 없고, Fire Ability는 Brain의 확정 타깃을 사용한다.

상태 흐름은 Visible → LostGrace → PendingSwitch 또는 Investigating → WatchingLastKnown → Disengaged다. 어느 기억 상태에서도 원래 타깃이 현재 감지되고 LOS가 확인되면 Pending/deadline을 지우고 즉시 Visible로 돌아간다. 일반 사격 승인에는 기존 AimSettle, CanShoot, 거리, 이동/사격 Action, cadence, 확률 조건도 필요하다.

- Visible: 현재 감지+LOS, 실제 타깃 조준, 마지막 위치/눈 위치/시간 갱신.
- LostGrace: 기본 0.8초, 원래 타깃 유지, 마지막 위치만 사용, Hold, 사격 금지.
- PendingSwitch: 유효한 다른 적을 보류한다. 기본 0.8초 지연은 후보를 처음 확인한 때 시작하며 LostGrace와 병행한다. 둘 다 끝나야 확정한다. 원래 적 재발견이 항상 우선이다. 후보가 사라지면 조사/기존 Watch로 돌아간다. 400cm 이내의 새 적 또는 원래 타깃 사망/invalid이면 긴급 전환하며, 새 타깃에는 최소 0.25초 switch AimSettle을 적용한다.
- Investigating: 별도 `Investigate Last Known Position` Action이 마지막 위치를 관측할 수 있는 가까운 Nav 목적지를 평가한다. 기본 4초 이내이며 grace 종료부터 계산한다. 이동 중 일반 사격 금지. 안전한 후보가 없으면 안전한 현재 위치에서 Watch, 위험하면 사용자 우선순위 안의 Cover/Retreat/KeepDistance를 평가한다.
- WatchingLastKnown: 도착 또는 안전한 현재 위치에서 Hold한다. 기본 3초 후 종료하며 Full Attack Position을 호출하지 않는다. 작은 Peek는 선택 사항이고 이번 기본 동작은 Hold다.
- Disengaged: Fire Ability 취소, FireCycle 정리, 타깃·기억·deadline·예약·소유 MoveTo·BB Target/Focus/WantAttack 정리, Aim OFF, 저장한 회전 설정과 속도 복원. 이후 Framework 판단에서 Roam을 재개한다.

상실 시 전체 기억 상한은 grace + investigation + watch이며 후보 변경이나 fallback으로 연장하지 않는다. 새 상태 Timer를 추가하지 않고 WorldTime deadline을 사용한다. 사망/invalid이고 다른 유효 후보가 없으면 즉시 이탈한다.

Sense의 `bIsHostile`은 현재 감지의 근거가 아니다. 성공한 Sight만 sensed로 처리한다. 상실 이벤트는 이전 LastSensedLocation/Time을 유지한다. 숨은 Actor의 위치/눈 위치/전방 벡터는 조준·위치 점수·사격 보정에 사용하지 않는다. 알려진 다른 적도 Sense의 마지막 기록을 사용한다.

## 현재 위치 유지, 위험도와 fallback

Attack Position은 현재 위치가 사격 가능하고 위험/점유/아군 사선 조건을 통과하면 최초 최소 2초 유지한 뒤 기본 400cm 안에서만 검색한다. 후보는 현재 점수보다 `MinimumScoreImprovementToMove`만큼 더 좋아야 한다. `LocalRepositionCooldown` 기본 2초와 `LocalDirectionHoldSeconds` 기본 3초 동안 반대 방향 후보 제한으로 왕복을 억제한다.

사선/거리/점유/아군 사선/위험으로 현재 위치가 부적절하거나, `NoShotFullRepositionSeconds` 기본 5초 동안 실제 발사하지 못한 경우 Full 검색을 허용한다. 이동 중 현재 위치가 충분히 좋아지면 먼 목적지까지 계속 이동하지 않는다. 기존 BP의 SearchRadius/거리/엄폐/사격 수치는 덮어쓰지 않는다.

공통 `DMPositionRisk`는 적 근접, 전진량, 알려진 적들의 목적지 노출, Nav 경로 길이의 25/50/75/100% 노출, 다수 노출 시 아군 지원 부재, 주변 4방향 Nav projection 여유를 평가한다. 마지막 기록만 있는 다른 적은 8초 이내 기록을 사용한다. 최대 사거리 밖에서는 필요한 접근량만큼 전진 제한을 완화한다. 경로 중 최소 위협 거리 침범, 점유, 아군 사선, HardMaxAdvanceDistance 초과는 거부한다. 위험 상한(기본 65) 또는 최저 점수(기본 0)를 통과하지 못하면 Attack/Investigation은 실행 불가를 반환한다. 기본 bRejectDangerousPositions=true다.

현재/선택 위치의 비싼 Attack 검증은 0.6초 주기, 후보는 기존 incremental search batch, Investigation 재검색 및 기억 기반 방어 경로 검증은 0.75초 주기 캐시를 사용한다. 기존 위치 레지스트리의 전역 스냅샷은 유지하지만 적대 Actor/예약을 위치 점수용 이웃과 예약 충돌 판정에 전달하지 않는다. 아군 점유와 실제 충돌 검사는 유지한다.

Visible은 `MovementActions`의 강한 Priority 계층을 사용한다. 기억 상태는 Brain의 상태 제한을 먼저 적용하며, LostGrace/PendingSwitch는 Hold, Investigating은 전용 조사 후 등록된 Cover/Hiding/Retreat/KeepDistance Action을 같은 Priority 선택기로 평가하고, Watch는 Hold한다. 보이지 않는 대상에 대한 견제·경고 사격은 없다.

## Aim, 속도와 수명주기

`bHoldAimDuringLostGrace`, `bAimWhileInvestigating`, `bAimWhileWatchingLastKnown`은 기본 true다. 조사 Aim을 꺼도 FocalPoint는 마지막 감지 위치에 둘 수 있다. Brain 상태가 Aim/Fire의 최종 허용 범위를 결정하며 Visible 이외에는 WantAttack=false다. Watch 종료와 Reset에서는 Aim/Focus를 해제한다.

`CombatMaxWalkSpeed`와 `InvestigationMaxWalkSpeed`는 실제 CharacterMovement 속도이며 기본 0은 기존 속도를 보존한다. 조사 속도 0이면 전투 속도를 사용한다. 첫 전투 진입에서만 현재 MaxWalkSpeed와 두 회전 옵션을 저장하며 타깃 전환에서는 재저장하지 않는다. 종료 시 정확히 복원하므로 기존 Roam MaxWalkSpeedOverride와 이어진다. Fire Action의 MaxFireMoveSpeed와는 별개다.

Possess/UnPossess, 사망/Respawn, EndPlay, 무기 변경/제거, Profile 재생성/재구성, Framework Reset, Brain Deactivate 및 bEnabled 해제에서 정리한다. 타깃 상실/전환은 활성 Fire Ability를 GAS CancelAbilityHandle로 종료하여 기존 EndAbility의 Timer/delegate/Montage Task 정리를 재사용한다. 이미 획득한 사격 cadence/pause를 앞당기지 않는다.

`dm.CombatDebug 2`는 상태, Combat/Perception/Pending 대상, 마지막 위치/나이, deadline 잔여, 위치 모드, 현재/후보 점수·위험과 전진/목적지/경로 위험, 거부 사유, 설정/실제 속도, Aim/WantAttack/종료 이유를 캐시에서 표시한다. 새 관찰 문자열은 non-Shipping에만 존재하며 관찰 함수에서 Trace/Actor 검색/확률 roll을 수행하지 않는다.

이번 구현은 정적 점검만 수행했다. 전체 빌드/UHT, PIE/BP 로드·런타임, Unreal 자동화 테스트는 미실행이며 컴파일 또는 실제 행동 성공을 주장하지 않는다.

기존 Cover/거리 유지/후퇴 등 일반 이동 Action의 Visible 목적지도 Brain의 호환 안전 gate에서 같은 위험도 캐시를 통과해야 한다. 위험한 Attack 후보 거부 뒤 하위 Action이 검사 없이 위험 구역으로 이동하지 않게 하며 기존 Action 저장값은 유지한다.

## 비전투 Tactical Nav Region 및 Event Field — 2026-10-12

비전투 지역 선택은 `UDMTacticalNavRegionSubsystem`의 실제 Recast polygon/portal 연결 그래프와 팀별 Event Field를
사용한다. PlayerStartByTeam multi-source 도달성, Exploration/Combat/Threat/Presence/SpawnPressure와 실제 그래프
경로 비용으로 어느 Region을 갈지 정한다. Roam Provider는 선택 Region의 미리 저장된 NavPoint와 완전한 NavMesh path로
Region 안의 실제 목적지를 정한다. 실패하면 기존 MovePoint가 최종 fallback이다. 전투 인지가 생기면 기존
CombatTactics가 즉시 제어권을 가진다.

Region은 사각 Cell이나 XY index가 아니라 NavMesh portal 인접성, portal 폭, 높이 연속성, 목표 면적을 기준으로 묶는다.
좁은 portal과 큰 높이 변화는 경계가 되고, 지나치게 작은 Region은 결합 가능한 인접 Region에 병합된다. 로딩 시 실제
NavMesh polygon 위의 분산 NavPoint를 저장하고 실제 캡슐의 0.9 배 정적 검사로 명백한 WorldStatic 차단만 제외한다.
Region 중심은 목적지로 쓰지 않으며, 짧게 독점되는 것은 반경을 가진 실제 NavPoint뿐이다.

전투 위치는 Tactical Nav Region과 분리한다. Action은 AttackPosition/CombatCover/Hide/Retreat/Investigate 목적의 `PositionQuery`를
선택하고 Combat Brain은 한 번에 하나의 EQS만 실행·캐시한다. 새 결과는 설정된 최소 개선량을 넘어야 유효한 기존 위치를
교체한다. EQS 결과도 Controller의 완전 경로 및 실제 캡슐 검증을 다시 통과해야 한다. 명시적 호환 옵션을 켜지 않는 한
EQS 실패나 미지정은 구 C++ 위치 계산 또는 Region/Grid 전투 위치로 자동 fallback하지 않는다. Region의 이벤트 점수는
Brain 상황 판단 정보로만 사용한다.
