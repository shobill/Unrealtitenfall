# Threat memory, target switch and position risk 결과 보고서

작성: 2026-09-22 16:32 (Asia/Seoul)

## 작업 범위와 검증 한계

DeathMatch C++ 및 설계 문서만 수정했다. Content/DeathMatch, Saved BP export, BP_WORK 문서는 참조 목적으로 읽었으며 BP 에셋을 저장하거나 자동 마이그레이션하지 않았다. 기존 사용자 변경·BP 우선순위·Controller 선택·맵 기본 Controller·Weapon 거리/피해/확률·파츠 CanShoot·Notify 시점은 덮어쓰지 않았다.

전체 빌드/UHT 미실행. PIE/BP 로드 및 런타임 테스트 미실행. Unreal 자동화 테스트 미실행. 아래 결과는 소스 정적 점검이며 컴파일 성공, BP 로드 성공 또는 실제 플레이 행동 성공을 의미하지 않는다. DeathMatch 디렉터리는 작업 시작 당시 Git untracked 상태였으므로 Git diff를 기존 사용자 파일 대비 검증 근거로 사용하지 않았다.

## 변경 파일

아래 경로는 Source/SkySeoul/DeathMatch/ 기준이다.

- `AI/Combat/DMCombatModules.h`
- `AI/Combat/DMCombatModules.cpp`
- `AI/Combat/DMCombatTacticsComponent.h`
- `AI/Combat/DMCombatTacticsComponent.cpp`
- `AI/Combat/DMCombatThreatMemory.cpp`
- `AI/Combat/DMCombatPositionRisk.h`
- `AI/Combat/DMCombatPositionRisk.cpp`
- `AI/Combat/DMCombatInvestigation.h`
- `AI/Combat/DMCombatInvestigation.cpp`
- `AI/Combat/DMCombatAttackPosition.h`
- `AI/Combat/DMCombatAttackPosition.cpp`
- `AI/Combat/DMCombatPositionRegistry.cpp`
- `AI/Combat/DMCombatDebug.h`
- `AI/Combat/DMCombatDebugFormat.cpp`
- `AI/Combat/DMCombatTacticsDebug.cpp`
- `AI/Framework/DMSituationFramework.cpp`
- `Abilities/DMGameplayAbilityBase.cpp`
- `Abilities/DMGameplayAbility_Fire.cpp`
- `Controller/DeathMatchRobotAIController.cpp`
- `Weapons/DMWeaponLoadoutComponent.cpp`
- `AIModules/AIModule_Sense.cpp`

설계 문서:

- `plan/01_Architecture.md`
- `AIModules/LOGIC_PERCEPTION.md`
- `AI/Framework/LOGIC_STATUS.md`
- `Abilities/LOGIC_STATUS.md`
- 본 결과 보고서.

## 전후 상태 흐름과 소유권

변경 전: BuildContext가 Sense Focus를 실제 Target으로 사용했다. Focus가 바뀌면 전투를 재시작하며, 시야 소실 기억은 Movement Action의 memory 시간에 의존했다. 공격 위치는 큰 반경에서도 최고 점수 후보를 찾았다.

변경 후: Sense는 현재/기억 후보와 LastSensedLocation/Time만 제공한다. Brain이 약한 Combat Target과 Pending Target을 소유하고 확정/지연/재발견/이탈을 결정한다. Movement Action은 Context와 위치 평가만 수행한다. Fire Ability 대상 조회와 CycleSafety 대상 비교도 Brain의 Combat Target을 기준으로 한다.

Visible → LostGrace → PendingSwitch 또는 Investigating → WatchingLastKnown → Disengaged. 원래 타깃의 현재 감지+LOS 확인은 후보 선택보다 먼저 실행하며 모든 Pending/deadline을 지우고 Visible로 복귀한다. 새 타깃은 살아 있고 현재 감지+LOS가 유효할 때만 확정한다.

- Visible: 실제 위치/눈 위치를 읽고 기억을 갱신한다. Aim ON. 기존 사격 승인 조건을 모두 유지한다.
- LostGrace: 기본 0.8초 동안 원래 타깃을 유지하고 마지막 위치를 향해 Hold한다. 다른 적은 Pending으로만 저장한다.
- PendingSwitch: 기본 0.8초는 후보를 처음 확인한 때부터 계산하며 grace와 병행한다. 두 시간이 끝나야 전환한다. 후보 사망/소실 시 Pending/deadline을 지우고 조사 또는 기존 Watch로 돌아간다.
- 긴급 전환: 새 적이 400cm 이내이거나 원래 타깃이 사망/invalid인 경우 일반 지연을 생략한다. 새 타깃은 switch AimSettle 기본 0.25초와 기존 사격 settle을 적용한다.
- Investigating: 별도 `Investigate Last Known Position` Action으로 기본 4초 동안 안전한 관측 위치를 평가한다. 기본 400cm 주변 8방향을 검색하며 목적지와 경로를 검증한다.
- WatchingLastKnown: 도착하거나 안전한 현재 위치에서 기본 3초 Hold한다. 일반 Attack Position, 큰 전진, 일반 사격은 호출하지 않는다. 기본 Watch는 Hold이며 별도 Local Peek 동작을 추가하지 않았다.
- Disengaged: 모든 전투 소유 상태를 해제하고 다음 Framework 판단에서 Roam을 재개한다.

Investigation deadline은 grace 종료 기준이며 전체 상실 기억 상한은 grace+investigation+watch다. 후보 변경과 실패 재검색으로 연장하지 않는다. 원래 타깃이 사망/invalid이고 다른 유효 후보도 없으면 즉시 종료한다. 개별 상태 Timer 대신 WorldTime deadline을 사용한다.

## Sense의 기억 안전성 수정

기존 `bSuccessfullySensed || Info->bIsHostile`는 적대성만으로 시야 상실 대상을 sensed로 남길 수 있어 성공 Sight만 사용하도록 바꿨다. 상실 이벤트에서 현재 ActorLocation을 읽어 기록을 덮어쓰지 않는다. 감지 중에만 마지막 위치/시간을 갱신한다. FindBestHostile 기억 후보와 숨은 Perception Focus의 위치도 AwarenessMap 기록을 사용한다.

Brain/Investigation/위험도는 숨은 타깃의 현재 transform을 점수·조준에 사용하지 않는다. 현재 Sense 감지가 있는 후보에 대한 LOS 검사는 재발견 확인을 위해 필요하며, 일반 사격의 거리·정밀 Aim/Trace 보정은 감지+LOS gate 뒤에서만 수행한다. 기존 레지스트리 스냅샷은 세계 점유 정보를 수집하지만, 적대 Actor 및 예약은 점수 이웃과 예약 충돌 판정에서 제외한다.

## 현재 위치 안정성과 Local/Full Reposition

기존 Attack Position 설정은 유지한다. 현재 위치가 사격 가능하고 위험/점유/아군 사선 검사를 통과하면 최초 최소 2초 Hold한다. 이후 기본 400cm 안에서 Local 검색만 수행하고 `Best.TotalScore > Current.TotalScore + MinimumScoreImprovementToMove`를 만족해야 이동한다.

LocalRepositionCooldown 기본 2초, LocalDirectionHoldSeconds 기본 3초이며 유지 시간 중 반대 방향 후보를 배제한다. 현재 사선/사거리/위험/점유/아군 사선 문제가 있거나 실제 사격이 기본 5초 동안 없을 때만 Full 검색을 허용한다. 이동 중 현재 위치가 충분히 좋아지면 먼 목적지로 계속 이동하지 않는다.

## 위험도와 후보 거부

공통 FDMPositionRiskSettings / DMPositionRisk::Evaluate를 Attack와 Investigation, 기억 기반 방어 fallback이 사용한다.

- Proximity: 기본 최소 400cm 이하 거부, 주변은 점진적 위험 증가. Nav 경로 샘플의 최소 거리 침범도 거부.
- Advance: 현재 위치 대비 적 방향 전진량. 기본 MaxAdvanceDistance=400, HardMaxAdvanceDistance=800, AdvanceRiskWeight=25. 최대 사거리 밖에서는 필요한 접근량만큼 제한 완화.
- Destination Exposure: 알려진 유효 적들의 기억 위치에서 목적지 노출을 계산한다. 기본 가중치 12. 다른 숨은 적은 8초 이내 감지 기록을 사용한다.
- Path Exposure: 실제 Nav polyline 길이의 25/50/75/100% 지점을 검사한다. 기본 가중치 20.
- Ally Support: 1200cm 내 아군 지원이 없고 다수 적에게 노출되면 추가 위험 15.
- Escape: 주변 4방향 200cm 지점의 Nav projection 성공 수가 2개 미만이면 위험 10. 막다른길의 정밀 연결성 검사는 아니다.
- Occupancy/Friendly lane: 기존 점유·예약 및 아군 사선 검사를 유지하고, 공통 위험도에서도 위험한 점유/아군 lane 진입을 거부한다.

FinalScore는 기존 점수에서 공통 위험 총합을 추가로 차감한다. 기본 MaxAcceptableRisk=65, MinimumAcceptablePositionScore=0, bRejectDangerousPositions=true. 위험 상한 또는 점수 하한을 통과하지 못하면 최고 후보라도 실행 불가다. 근접/경로/Hard advance/점유 거부는 위험 감점만으로 무효화하지 않는다.

Attack 현재/선택 목적지의 비싼 재검증은 0.6초, 후보는 기존 incremental batch, Investigation 및 기억 기반 방어 경로는 0.75초 캐시다. 목적지가 크게 바뀌면 방어 위험 캐시를 재검증한다. Debug는 계산된 값만 읽으며 추가 Trace/Actor 전체 검색/확률 roll이 없다. 새 ThreatSummary와 상태 보고 문자열은 non-Shipping 조건 안에 있다. 후보 배열은 기존 검색 자체가 사용하는 데이터이며 새 디버그 전용 후보 배열을 Shipping에서 생성하지 않는다.

## Movement Action fallback

BP의 Priority 1/2/3 객체나 순서를 변경하지 않았다. Attack/Investigation이 실행 불가이면 다음 정책을 평가한다. Visible은 일반 기존 우선순위, LostGrace/PendingSwitch는 Hold, Investigating은 전용 조사 후 안전한 현재 Watch 또는 기존 우선순위 중 Cover/Retreat/KeepDistance를 사용한다. Watch는 Hold다. 위험한 방어 목적지/경로도 거부하고 다음 Action으로 넘어간다. Visible의 기존 Cover/거리 유지/후퇴 등 Action도 Brain의 호환 안전 gate에서 같은 위험도 캐시를 통과해야 하므로 Attack 거부 뒤 하위 Action이 위험한 목표를 무검증 실행하지 않는다. 실패 cooldown을 priority 재평가가 지우지 않도록 유지한다. 모든 경로가 불가능해도 기억 시간 종료는 유지된다.

## Aim/Focus/Blackboard와 속도

Visible 이외에는 WantAttack=false이며 상태가 Action의 Aim 요청을 제한한다. bHoldAimDuringLostGrace/bAimWhileInvestigating/bAimWhileWatchingLastKnown 기본 true. 조사 Aim을 끄면 FocalPoint는 마지막 위치에 둘 수 있다. BB TargetActor는 Visible에만 게시하고, 기억 상태는 위치 값만 게시한다.

전투 진입 때 MaxWalkSpeed와 bOrientRotationToMovement/bUseControllerDesiredRotation을 한 번 저장한다. CombatMaxWalkSpeed>0을 Visible/LostGrace/PendingSwitch에 적용한다. 조사/Watch는 InvestigationMaxWalkSpeed>0이면 사용하고 아니면 Combat 값으로 돌아간다. 두 속도 기본 0은 기존 값을 유지한다. 타깃 전환 시 원본 속도를 다시 저장하지 않으며 종료 때 저장값을 정확히 복원한다. 기존 Roam MaxWalkSpeedOverride와 Fire Action의 MaxFireMoveSpeed는 별개다.

Reset은 Combat/Pending/기억/모든 deadline/조사 목적지/Watch/예약/소유 MoveTo/FireCycle/Aim/Gameplay Focus/BB WantAttack·TargetActor·TargetLocation·HasTarget·FocusLocation·IsFocusSeen 및 소유 이동 키를 정리한다. 회전 옵션과 속도를 복원한다. 전투 소유 MoveTo만 Reset에서 중지한다.

## FireCycle과 lifecycle

타깃 상실/교체/Reset에서 GAS CancelAbilityHandle로 활성 Fire Ability를 종료한다. 기존 EndAbility의 CycleTimer/CycleNotifyTimer/CycleWatchdogTimer/SafetyTimer, Fire notify timer 배열, Notify delegate, Montage Task 정리를 재사용한다. 확정 타깃이 현재 감지되지 않거나 LOS가 없으면 숨은 위치를 읽는 사격 보정 전에 차단한다. 성공한 발사의 cadence/pause를 reset으로 앞당기지 않는다. 공통 Runner 및 Fire Mode 순서/Notify 시각은 재설계하지 않았다.

AI 사망/Respawn, Possess/UnPossess, EndPlay, 무기 변경·제거·전체 제거, Profile 재생성/재구성, Framework ResetSituation, Brain Deactivate, bEnabled=false, invalid/pending-kill 타깃 경로를 정적으로 확인했다. Reset 중 동기 콜백의 재진입은 bUpdating guard로 차단한다. 추가 Timer/delegate 등록은 없다.

## BP 직렬화와 정적 점검 결과

- 기존 UCLASS 이름과 MovementModule/MovementModulePriority2/MovementModulePriority3/FireModule/AdaptiveSettings 타입·이름 유지.
- MovementPolicy/FirePolicy/AdaptiveFireSettings 서브오브젝트 이름 유지. ThreatInvestigation이라는 새 전용 subobject만 추가하고 priority slot을 덮어쓰지 않음.
- 기존 FireMode `UseExistingModule, Precision, Burst, Automatic, Adaptive` 순서 유지. 기존 PositionReject 순서 유지, 새 Risk/Score 값만 뒤에 추가.
- 새 ThreatAwareness는 transient. BP 설정을 초기화하는 저장/마이그레이션 없음. class/property rename이 없어 Core Redirect 없음.
- 기본/Adaptive Controller uasset을 읽기 전용 문자열 검색하여 DMCombatTacticsComponent, DMCombatTactics_GEN_VARIABLE, 세 Movement property, FireModule, AdaptiveSettings 이름의 존재 확인. 이는 BP 로드나 값 해석 검증이 아님.
- 21개 코드 파일에 UTF-8/괄호·중괄호/전처리 균형/generated include 순서 정적 검사 수행.
- 새 Brain 함수 8개의 선언·구현 대응, 새 소스의 로컬 include 참조 확인. 엔진 소스에서 CancelAbilityHandle 및 Pawn Nav API 선언 확인.
- 상태 분기, 원래 타깃 우선 복귀, Pending 상실/사망, 타임아웃, 숨은 위치 gate, Visible 외 WantAttack 차단, 사격 취소, 속도 단일 저장/복원, 위험 후보 fallback, 캐시 주기 및 Shipping guard 수동 검토.
- 전체 빌드/UHT: 미실행. PIE/BP 로드/런타임: 미실행. Unreal 자동화: 미실행. 기존 자동화 테스트도 실행하지 않음.

## 사용자가 BP에서 확인/설정할 값

필수 BP 재저장이나 우선순위 변경은 없다. 신규 기본값을 사용할 수 있다. 에디터에서 다음 값의 노출과 상속 상태를 확인한다.

- DM Combat Brain / Threat Memory: grace 0.8, switch 0.8, switch settle 0.25, emergency distance 400, investigation 4, watch 3 및 Aim bool 3종.
- CombatMaxWalkSpeed/InvestigationMaxWalkSpeed: 원하는 실제 cm/s를 지정한다. 기본 0은 기존 속도를 유지한다.
- Attack Position: LocalRepositionRadius 400, 기존 MinimumPositionHoldSeconds 2, MinimumScoreImprovementToMove 15, LocalRepositionCooldown 2, LocalDirectionHoldSeconds 3, NoShotFullRepositionSeconds 5, FullRepositionRiskThreshold 55.
- Attack/Investigation의 RiskSettings: 위 기본값과 맵/무기 거리 기준을 비교한다. MinThreatDistance는 기존 Action의 동명 설정이 위치 평가에 우선하므로 해당 값을 설정한다.
- Movement Priority는 기존 사용자 설정을 유지한다. 안전 fallback을 원하면 기존 Cover 및 Retreat/KeepDistance 객체가 해당 순위에 지정되어 있는지 확인한다.
- bAimWhileInvestigating=false로 조사 이동 ADS를 끌 수 있다. Fire MaxFireMoveSpeed는 실제 이동 속도와 별도로 유지한다.

## 이후 사용자 수동 테스트 체크리스트

1. 에디터에서 C++ 빌드/UHT 후 기존 기본/Adaptive Controller BP를 열어 SCS와 priority/Fire/Adaptive instanced 값이 유지되는지 확인.
2. 좋은 현재 사격 위치에서 최소 Hold, 400cm local 범위, 점수 개선 임계값과 방향 유지 확인.
3. 원래 적 상실 직후 0.8초 이내 재발견: 원래 Target 복귀, Pending 초기화, 새 승인 후 발사 확인.
4. 다른 적이 동시에 보일 때 지연 후 전환, 지연 중 후보 소실·사망·교체, 가까운 적 및 원래 타깃 사망 긴급 전환 확인.
5. 적을 벽 뒤로 이동시켜 기억 위치/눈 위치가 따라가지 않는지 확인. WantAttack과 실제 Burst/Automatic 반복 발사가 끊기는지 확인.
6. 안전한 조사 Nav 목적지, 경로 중 다수 노출, 근접 위험, 아군 lane/예약 충돌을 각각 만들어 후보 거부와 다음 priority fallback 확인.
7. 조사 도착/실패/타임아웃 및 Watch 타임아웃 뒤 Aim/Focus/BB/Move/예약이 정리되고 Roam으로 돌아오는지 확인.
8. Combat/Investigation 속도와 원래 Roam 속도·두 회전 옵션 복원 확인. 반복 전투 진입 및 타깃 전환도 확인.
9. 사망/Respawn/UnPossess/무기 변경/무기 전체 제거/Profile 재구성/Brain 비활성화/강제 Reset 중 몽타주·Timer·delegate·예약 잔류 여부 확인.
10. dm.CombatDebug 0과 2에서 행동 차이가 없는지, debug 상태/잔여 시간/점수/위험/거부 사유가 실제 행동과 맞는지 확인.

## 후속 후보

Suppressive Fire Action은 별도 후속 후보이며 이번에는 보이지 않는 적을 향한 견제·경고 사격을 구현하지 않았다. 최근 피해/직접 공격 기반 긴급 전환은 신뢰 가능한 공격자 이력 시스템을 추가하지 않고 후속 후보로 남긴다. Escape의 정밀 경로 연결성, 위협 수가 많을 때 공간 인덱스 및 Nav/Trace 비용 측정도 런타임 검증 후 확장할 수 있다.


## 후속 빌드 로그 수정

사용자 제공 빌드 로그에서 다음 컴파일 오류를 확인하여 수정했다.

- DMCombatInvestigation.cpp: 지역 Registry 3곳을 PositionRegistry로 변경하여 상속 멤버 가림(C4458) 제거. 저장 프로퍼티 이름은 변경하지 않았다.
- DMCombatPositionRisk.cpp: GetPawn()의 TObjectPtr 반환값을 auto*로 추론하지 않고 APawn*에 명시적으로 대입(C3535 및 후속 C2440/C2665/C2664 오류 원인 제거).
- DMCombatThreatMemory.cpp: StopCombatFire의 Controller를 비const로 받아 Blackboard 수정 가능한 overload 사용(C2662). Pawn 선택을 명시적 APawn* 대입과 if로 분리하여 중첩 조건식의 raw pointer/TObjectPtr 모호성(C2445) 제거.

로컬 UE 5.6 Controller/AIController 헤더의 GetPawn 반환 타입과 GetBlackboardComponent const/nonconst overload 및 수정 코드 참조를 정적으로 확인했다. 재빌드/UHT/런타임 테스트는 실행하지 않았으므로 빌드 성공을 주장하지 않는다. 사용자 로그의 DeathMatch 외부 C4996 deprecated 경고는 작업 범위 밖이므로 변경하지 않았다.
