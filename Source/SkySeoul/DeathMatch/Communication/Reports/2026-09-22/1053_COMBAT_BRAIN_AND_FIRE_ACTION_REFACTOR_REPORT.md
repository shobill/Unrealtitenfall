# Combat Brain 및 Fire Action 리팩터링 결과

- 작성일: 2026-09-22 10:53 KST
- 구분: 구현 결과 및 정적 점검
- 작업 범위: Source/SkySeoul/DeathMatch. Content/DeathMatch는 읽기 전용 점검.
- 검증 제한: 빌드/UHT 미실행, Unreal 자동화 테스트 미실행, PIE/BP 런타임 테스트 미실행.

## 결과

CombatTactics의 표시 이름과 역할을 DM Combat Brain으로 정리했다. Brain 소유의 FDMFireCycleRuntime과 Ability 공통 Runner를 통해 Precision, Burst, Automatic 및 Adaptive 선택 패턴을 실행하도록 변경했다. Adaptive Settings는 Adaptive 모드에서만 표시한다.

기존 사용자 변경은 git checkout/reset으로 되돌리지 않았다. 작업 시작 당시 DeathMatch 소스와 에셋은 미추적 상태였으므로 소스 복사본을 기준으로 변경을 비교했다. Config와 DeathMatch 외부 코드를 수정하지 않았으며, BP/맵/몽타주 에셋도 저장·이름 변경하지 않았다. 엄폐/공격 위치 점수, 이동 밸런스, 피해·거리 감쇠, 파츠 시스템과 Notify 시간은 변경하지 않았다.

## 구조 변경

이전: CombatTactics가 이동/사격을 조율하고, Adaptive Action의 런타임 상태에 결합된 Ability 반복 경로만 존재했다. Burst/Automatic은 활성화 사이의 ShotFired 호출에 의존했다.

이후: SituationFramework/Situation Profile이 비전투 Roam과 BT/Profile을 담당한다. Combat Brain이 Context, Action 우선순위·선택, Blackboard, 이동·조준·사격 승인과 FireCycle 상태를 소유한다. Fire Action은 FDMFirePatternSettings를 제공하고, Ability의 StartFireCycleMontage/HandleFireCycle/QueueFireCycle/ResumeFireCycle이 모든 패턴을 실행한다. Adaptive Prepare/SelectPattern만 거리 기반 선택을 담당한다.

Adaptive의 기존 읽기 전용 State 프로퍼티는 BP 참조 보존을 위한 관찰값으로 남겨 두고 Brain 상태를 ObserveCycle로 전달한다. 기존 native ShotFired/BeginCycle API는 호환 목적으로 유지하지만 gameplay 반복 몽타주를 실행하지 않는다. 성공 횟수와 cadence 계산은 FDMFireCycleRuntime::RecordSuccessfulShot을 공유한다.

## 이름과 프로퍼티

실제 UCLASS rename은 하지 않았다. 에셋을 에디터에서 로드·마이그레이션·재저장하지 않는 이번 범위에서는 표시 이름을 우선 변경하는 편이 안전하다.

- UDMCombatTacticsComponent → 표시 이름 DM Combat Brain.
- UDMCombatFireModule → 표시 이름 Fire Action.
- UDMCombatFire_Precision → Precision Single Fire.
- UDMCombatFire_Burst → Burst Fire, UDMCombatFire_Automatic → Automatic Fire.
- UDMCombatFire_Adaptive → Distance Adaptive Fire.
- UDMCombatMovementModule → Movement Action. Movement 파생 native 클래스명 변경은 후속 후보.
- UDMSituationBrain_Combat → DM Situation Profile - Combat. BP_DM_Brain_Combat 경로 유지.
- FireModule → 표시 이름 Fire Action. AdaptiveSettings → Adaptive Fire Settings.
- MovementModule/MovementModulePriority2/3 → Movement Action Priority 1/2/3 표시 이름.
- GetActiveMovementModule → Get Active Movement Action 표시 이름. ActiveMovementModule 저장 이름 유지.
- Automatic에 MaxAutomaticShots(기본 0), AutomaticPause(0.65), bAllowFireWhileMoving(true), MaxFireMoveSpeed(600) 추가. 기존 ShotInterval 보존.
- Ability 내부 Adaptive 전용 함수·포인터 이름은 공통 Cycle 이름으로 변경. 변경된 UPROPERTY는 저장하지 않는 비노출 Transient 런타임 참조다.
- debug native cache는 AdaptiveFireSummary에서 FireCycleSummary로 변경.

## BP 직렬화 및 Redirect

Core Redirect는 추가하지 않았다. 모든 기존 native 클래스, 저장되는 프로퍼티 이름·타입, enum 이름·순서를 보존했기 때문이다. Fire 관련 enum 선언은 DMFireCycle.h로 옮겼지만 /Script/SkySeoul의 반영 이름은 그대로다.

MovementPolicy, FirePolicy, AdaptiveFireSettings 기본 서브오브젝트 이름을 유지했다. BeginPlay의 기존 모드 선택 방식도 유지하여 Use Existing은 원래 객체를 사용하며, 명시 모드에서 저장 객체가 해당 타입이면 값을 그대로 사용한다. 타입이 다르면 해당 모드의 런타임 객체를 생성한다. 에디터 설정값 자체를 초기화하는 PostEditChange 코드는 추가하지 않았다.

AdaptiveSettings에 enum EditCondition과 EditConditionHides를 적용했다. 로컬 UE 5.6 EditConditionParser의 EnumType::EnumValue 처리 코드를 확인했다. 숨김은 표시 조건만 변경하며 객체를 제거하지 않는다. Details에서 즉시 갱신되는지는 에디터에서 확인해야 한다. Precision/Burst/Automatic별 별도 Settings 프로퍼티는 직렬화 구조를 늘리지 않기 위해 추가하지 않았다.

## 실제 사격 동작

- Single: 활성화당 성공한 ExecuteFire 한 번. FireCycle에서 종료, ShotPauseSeconds 적용. AimSeconds/MaxMoveSpeed 유지. FireCycle 없는 기존 Single 몽타주는 완료 시 종료하는 호환 경로 유지.
- Burst: 기본 3회 성공 발사를 같은 활성화에서 반복. 성공만 탄 수에 포함하고 완료한 경우 BurstPause 적용.
- Automatic: 동일 활성화에서 반복. MaxAutomaticShots=0 무제한, 양수이면 해당 성공 횟수에 종료하고 AutomaticPause 적용. 이동 사격 허용과 속도 제한 제공.
- Adaptive: 기존 거리 구간/hysteresis/부분 점사 잠금 유지. 선택 결과를 같은 Runner에 전달.
- Use Existing: 저장된 실제 Action 클래스의 가상 GetCycleSettings를 통해 위 경로를 사용.

Fire만 ExecuteFire를 호출한다. FireCycle은 다음 사이클 판단만 하며 총알·trace·피해를 생성하지 않는다. ExecuteFire의 true 반환만 성공으로 계산한다. BP override 역시 실제 발사를 수행했을 때만 true를 반환해야 한다.

다음 몽타주 예약은 Fire Notify의 시작 오프셋을 고려하고 실제 Fire 직전 cadence를 다시 검사한다. 몽타주의 FireCycle 위치상 설정한 ShotInterval보다 빠르게 돌아갈 수 없는 경우 실제 간격은 더 길 수 있다. 이를 맞추기 위해 Notify 시간이나 애니메이션을 수정하지 않았다.

## 실패와 종료

타깃/사수 사망, 타깃 변경·소실, 무기 변경·소실, Controller/ASC 소실, CanShoot=false, 사거리·속도 위반, 실제 총구 사선 차단·아군 위험, 몽타주 실패/취소/Interrupt를 검사한다. Fire/FireCycle 누락은 경고 및 구체적인 실패 원인으로 종료한다. Watchdog이 발사나 FireCycle을 대신 생성하지 않는다.

실패한 ExecuteFire는 성공 횟수·정상 그룹 pause를 추가하지 않는다. 실패/취소에는 0.075초 anti-spin만 추가하며 이미 성공한 발사의 cadence는 유지한다. cycle ID, montage instance ID, 중복 예약 플래그, EndAbility 재진입 방지 및 ExecuteFire 콜백 중 재활성화 차단을 적용했다. 발사 결과 콜백이 동기적으로 종료를 유발해도 성공한 호출은 기존 cycle에 반영한다.

종료 시 CycleTimer/CycleNotifyTimer/SafetyTimer/CycleWatchdogTimer, Fire notify timer 배열, montage notify delegate, Montage Task delegate/참조를 정리한다.

## 정적 점검 결과

실행한 검사는 텍스트·선언·참조 비교이며 C++ 컴파일러나 Unreal 테스트가 아니다.

- 변경 전 소스와 비교하여 기존 native 클래스명, non-Transient UPROPERTY 이름/타입, 네 fire enum의 순서를 보존함을 확인.
- Source 전체 검색에서 제거한 Adaptive 전용 Runner 심볼 참조가 남지 않음을 확인.
- 공통 FireCycle 경로에 ExecuteFire/ExecutePatternFire 호출이 없음을 확인.
- 네 종류 Timer의 EndAbility 정리, Notify/Task delegate 제거, stale cycle 방어를 소스에서 점검.
- 변경 코드의 괄호/중괄호 균형과 선언 참조를 점검. 이는 컴파일 성공 보장이 아님.
- dm.CombatDebug 2용 요약은 모든 모드의 Brain/Action/패턴/횟수/한도/Notify/cycle/cadence/원인을 표시. Adaptive 거리 정보만 조건부 추가. 새 관찰 문자열은 !UE_BUILD_SHIPPING 영역이며 별도 trace/roll/Actor 검색을 추가하지 않음.
- 기존 테스트 소스의 private Runner 및 debug cache 참조를 새 이름/Brain 상태에 맞게 갱신했지만 실행하지 않음.

## 에셋 연결 확인과 한계

바이너리 내 직렬화 이름·경로 문자열을 읽어 다음 연결을 확인했다. UObject 로드나 BP 컴파일에 의한 검증은 아니다.

- 두 Controller BP에 DMCombatTacticsComponent, AdaptiveSettings/FireModule, AdaptiveFireSettings 및 기존 Fire/Movement 클래스명이 존재.
- BP_DM_AI_Fire에 native DMGameplayAbility_Fire와 AM_DM_AI_AdaptiveFire 참조가 존재.
- AM_DM_AI_AdaptiveFire에 FireCycle 이름이 존재. 실제 notify 타입/순서/시각·실행 여부는 에디터 검증 필요.
- BP_DM_Brain_Combat에 DMSituationBrain_Combat 및 BT_Combat 참조가 존재.
- 원래 Controller 바이너리에는 EDMFireMode::Automatic 문자열, Adaptive Controller에는 EDMFireMode::Adaptive 문자열이 존재. 이름 테이블 관찰만으로 현재 유효 BP 값을 단정하지 않는다. 기존 테스트의 원래 Controller=UseExisting 기대값과 차이가 있을 수 있으며, 테스트 기대에 맞춰 사용자 에셋을 바꾸지 않았다.

## 변경 파일

- Abilities/DMGameplayAbility_Fire.h/.cpp, DMFireAttempt.h
- AI/Combat/DMFireCycle.h (신규)
- AI/Combat/DMCombatModules.h/.cpp, DMCombatFire_Adaptive.h/.cpp
- AI/Combat/DMCombatTacticsComponent.h/.cpp, DMCombatTacticsDebug.cpp
- AI/Combat/DMCombatDebug.h, DMCombatDebugFormat.cpp
- AI/Combat/DMAdaptiveFireTests.cpp, DMCombatDebugTests.cpp (참조 갱신)
- AI/Framework/DMSituationBrain_Combat.h
- plan/01_Architecture.md 및 이 보고서

## 후속 검증 체크리스트

- [ ] UHT/컴파일로 새 헤더와 가상 함수 선언 검증. 이번에는 빌드하지 않았음.
- [ ] 두 Controller BP/SCS를 로드해 native class/subobject와 기존 instanced 설정값 보존, Missing Class/Property 여부 확인.
- [ ] Adaptive → 다른 모드 → Adaptive 전환 시 표시와 거리/cadence 값 유지 확인.
- [ ] 명시 Fire Mode와 Fire Action 클래스가 일치하는지 확인. Use Existing은 기존 객체를 쓰는지 확인.
- [ ] BP_DM_AI_Fire의 몽타주/섹션, Fire 및 FireCycle의 실행 가능한 Notify 타입·순서 확인. 이번 작업은 Notify 시각을 변경하지 않았음.
- [ ] Single 1발, Burst 기본 3발, Automatic 0/양수 한도, Adaptive 세 거리 패턴의 실제 성공 횟수·간격·pause 확인.
- [ ] 누락/중복 Notify, 몽타주 실패/중단, 사망/대상·무기 교체, 사선/아군 위험에서 종료와 Timer 잔존 여부 확인.
- [ ] dm.CombatDebug 2의 모드별 상태·실패 원인 확인.
- [ ] 기존 자동화 테스트의 에셋 기대값은 사용자 설정을 먼저 확인한 뒤 검토. 이번에는 자동화/PIE/BP 테스트를 실행하지 않았음.

컴파일 성공 및 런타임/BP/몽타주 동작 성공을 주장하지 않는다.

## 후속 빌드 로그 수정

사용자가 제공한 빌드 로그의 DMAdaptiveFireTests.cpp C2679는 UAbilitySystemComponent의 완전한 정의 없이 TWeakObjectPtr에 포인터를 대입한 것이 원인이다. 해당 테스트 번역 단위에 AbilitySystemComponent.h를 직접 include했다. 이 대입은 이번 리팩터링에서 추가한 코드다. 로그에 함께 나온 DeathMatch 외부 DynamicAbilityTags 사용 중단 경고는 변경하지 않았다. 수정 후 빌드와 테스트는 실행하지 않았으며 재빌드 성공은 미확인이다.
