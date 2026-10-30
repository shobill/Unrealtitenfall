# 현재 체력 기반 Combat Evaluation 구현 결과

- 작성일: 2026-09-28 11:35 KST
- 구분: 구현 및 정적 검증 결과
- 수정 범위: Source/SkySeoul/DeathMatch 내부만
- 빌드, UHT, BP 컴파일/로드, PIE, Unreal 자동화 테스트: 모두 미실행

## 결론

Combat Brain이 공식 Health API를 읽어 공통 Context의 HealthRatio/HealthRisk와 정책 계수를 만든다. Position Risk는 기존 기하 캐시에서 노출을 재사용하고 현재 체력 비용을 한 번만 가산한다. Attack Position, Cover 후보와 Brain의 이동 제안 검증, Micro Move가 이를 소비한다. 저체력 전용 Brain, Action, Tick, 상태 머신은 추가하지 않았다.

기존 Emergency 프로퍼티 및 Movement 우선순위 슬롯을 유지했다. Health Evaluation을 끄거나 Weight를 0으로 두면 새 연속형 비용/보상은 0, Micro 배율은 1이다. 기존 저체력+감지 노출 Emergency는 독립적으로 유지한다. 실제 컴파일 및 게임 행동 성공을 보증하는 결과는 아니다.

## 1. 먼저 확인한 규칙과 기존 흐름

DeathMatch 내부와 적용 가능한 상위 Source 경로에서 AGENTS.md를 찾지 못했다. 실제 지속 규칙인 .cursor/rules/deathmatch-entry.mdc, deathmatch-combat-architecture.mdc, deathmatch-fire-design.mdc, deathmatch-workflow.mdc를 읽었다. plan의 4개 문서, Communication 운영 규칙 및 최신 날짜 보고서 2026-09-28/1050_COMBAT_AI_REVIEW_IMPROVEMENTS_REPORT.md를 확인했다. 규칙 파일은 수정하지 않았다.

- 공식 API는 ACharacterBase::GetHealth()/GetMaxHealth()다. AttributeSetBase가 유효하면 GAS 값을 반환하고, 없으면 둘 다 0을 반환한다. TakeDamage는 CurHP를 감소시키고 AttributeSet이 있으면 SetHealth(CurHP)로 동기화한다. 외부 CharacterBase는 읽기만 했다.
- 기존 EmergencyLowHealthRatio는 Brain의 UpdateVisibleRisk에서 자기 캐릭터 API를 직접 여러 번 호출하여 bool 임계값을 만들었다. MaxHealth>0을 확인했지만 NaN/Infinity에 대한 명시적 방어는 없었다. 새 공통 계산이 이 자기 체력 판단을 대체한다.
- 기존 BuildContext의 Robot IsDead 검사와 ResetCombat/컨트롤러 수명 관리가 사망을 처리한다. HP=0 계산은 사망 처리를 대신하지 않는다.
- Target Evaluator에도 GetHealth/GetMaxHealth가 있지만 이는 공격 후보인 **적의 체력**을 사용하는 기존 FinishTargetBonus다. 자기 체력 중복 계산이 아니므로 변경하지 않았다.
- Damage Awareness는 최근 피해량/MaxHP 또는 API MaxHealth를 이용해 RecentDamageRatio를 보존한다. 현재 잔여 체력과 다른 사건 이력이며 변경하지 않았다. 신뢰할 수 없는 환경/낙하/아군/출처 불명 피해는 계속 공격 타깃을 생성하지 않는다.
- Position Risk의 기존 Total은 Proximity + Advance + DestinationExposure + PathExposure + AllySupport + Escape + FlankRear이며, Soft Risk Limit 초과 페널티를 마지막에 가산한다. Cheap 단계의 군중/방향 비용과 LOS 검증의 weighted exposure를 구분한다.
- Attack 점수는 LineOfFire/WeaponRange/Cover/Flank/Separation 보상에서 PathCost/Exposure/FriendlyFire/Occupancy/Cohesion/SameLane 및 Risk.Total을 뺀다. 후보 사선, Nav/Capsule, 사거리, 점유, 아군 위험 등의 기존 거부 조건을 유지한다.
- Cover는 cheap 후보 정렬 → LOS/Hide/Peek 검증 → 제한된 경로 finalist 순서다. 낮은 Cost가 좋은 위치다. Attack은 높은 Score가 좋은 위치다.
- Visible Movement는 기본 슬롯 1/2/3의 첫 실행 가능 제안을 사용한다. 기존 Emergency일 때만 Cover를 먼저 평가한다. Attack의 최소 유지/점수 개선, Cover의 phase/예약, Retreat의 MaintainDistance, Rush의 명시적 빠른 접근, Brain Micro Move의 별도 작은 후보 검색을 유지했다.

## 2. 변경 파일

AI/Combat 아래:

- DMCombatHealthEvaluation.h: 신규 설정 구조체, 공유 결과, 순수 계산 및 Emergency helper.
- DMCombatHealthEvaluationTests.cpp: 신규 Unreal 계산 테스트 코드. 실행하지 않음.
- DMCombatModules.h: Context에 공유 Health와 CurrentExposure 추가.
- DMCombatTacticsComponent.h/.cpp: BP 설정, BuildContext 단일 체력 조회, 기존 순서 안의 이동 제안 보정.
- DMCombatMovementDecision.cpp: Emergency 공유 입력, 체력 변화 재검증, Micro 반경, FastReposition 개선 기준.
- DMCombatPositionRisk.h/.cpp: raw 경로 노출과 체력 비용, 캐시 이후 1회 가산.
- DMCombatAttackPosition.h/.cpp: 체력 점수/보호 보상/노출 전진 페널티와 관찰 필드.
- DMCombatCoverModule.h/.cpp: 최종 후보 체력 비용, 보호/안전 탈출 보상과 선택 당시 관찰값.
- DMCombatThreatMemory.cpp: 피해 기억 방어 Context의 현재 노출 공유.
- DMCombatTacticsDebug.cpp: 기존 cached debug summary에 체력과 실제 보정값 추가.

그 밖의 변경: Tests/inspect_health_evaluation.ps1 및 이 보고서. 총 16개 파일(코드 14개, 정적 검사 1개, 보고서 1개).

## 3. HealthRatio / HealthRisk 계산

HealthRatio = Clamp(CurrentHealth / MaxHealth, 0, 1).
MaxHealth가 유한하지 않거나 0 이하이면 1. CurrentHealth가 유한하지 않은 경우도 중립값 1로 방어한다. 나눗셈은 double 중간값으로 수행한다. 정상 MaxHealth에서 음수 체력은 0, 초과 회복은 1로 Clamp한다.

HealthRisk = Clamp(1 - HealthRatio, 0, 1).
WeightedRisk = HealthRisk × (Enable ? nonnegative finite Weight : 0).

Caution/Critical은 각각 0~1 Clamp 후 높은 값을 Caution, 낮은 값을 Critical로 사용한다. 저장값을 다시 쓰지 않는다. Preference = WeightedRisk × Clamp((Caution - HealthRatio) / max(SMALL_NUMBER, Caution - Critical), 0, 1). 동일 경계에서도 분모가 0이 되지 않는다. 이 Preference는 Cover/Retreat/Aggression 계수에 사용한다. 노출 비용과 Micro는 선형 HealthRisk를 사용한다.

각 Action이 HP를 읽지 않는다. BuildContext 호출당 한 번 계산하고 복사된 LOS/Peek Context도 동일 결과를 공유한다. 사격 승인 경로가 별도로 Context를 구축하는 기존 구조는 유지하므로, 전체 프레임당 전역 1회라는 의미는 아니다.

## 4. Position Risk와 점수

- HealthExposureRisk = WeightedExposure × WeightedRisk × LowHealthExposurePenalty.
- HealthPathRisk = WeightedPathExposure × WeightedRisk × LowHealthPathExposurePenalty.
- WeightedPathExposure는 기존 Nav polyline 샘플의 weighted exposure 평균이다. 기존 PathExposureWeight가 0이어도 새 경로 체력 비용은 독립적으로 작동한다.
- Cheap 단계에서는 실제 노출을 모르므로 blanket health cost를 추가하지 않는다. LOS/Path 평가 이후 현재 위치와 후보에 같은 공식을 사용한다.
- 기하 캐시 저장은 체력 가산 **이전**이다. Cache hit/miss 공통 끝부분에서 Total을 기하 항목으로 다시 구성하고 체력 비용을 1회 가산한 다음 기존 ExcessRiskPenalty를 적용한다.
- TotalHealthRiskContribution은 직접 체력 비용 + 체력으로 늘어난 초과 위험 페널티다. 직접 비용과 확대 후 기여도를 디버그에서 구분한다. Risk Tier는 최종 Total을 사용한다.
- 노출과 경로 노출이 모두 0이면 직접 체력 비용과 추가 초과 위험 기여도도 0이다. 기존 근접/군중/탈출 위험 자체가 0이 된다는 의미는 아니다.
- Attack은 Risk.Total을 한 번 빼고, 현재 실제 노출 아래의 부분 보호 정도(Blocked/2)에 Cover 보상을 준다. 사거리 확보에 필요한 접근을 제외한 전진 비율과 노출이 함께 있을 때만 Aggression 페널티를 준다. Minimum Base Position Score 판정에서는 새 보상/페널티를 되돌려 기존 하한 의미를 유지한다.
- Cover는 최종 경로 후보에서 체력 기여도를 한 번 Cost에 더한다. 현재보다 노출이 줄어드는 보호 정도에 Cover 보상, Escape 목적이며 경로 평균 노출이 현재 이하일 때 Retreat 보상을 Cost에서 뺀다. LOS 단계에서 체력 비용을 미리 더하지 않는다.

## 5. Emergency와 연속형 평가의 역할

EmergencyLowHealthRatio, bLowHealthExposureTriggersEmergency, EmergencyWeightedExposureThreshold, bCloseEnemyTriggersEmergency, bMultiExposureTriggersEmergency의 이름과 기존 기본값을 유지했다.

저체력 Emergency는 shared HealthRatio < 기존 임계값 AND SensedWeightedExposure > 0 AND 기존 토글이다. 단순 저체력만으로 발생하지 않는다. 기억만 남은 적의 노출은 sensed 노출에 포함되지 않는다. 기존 Critical Risk Tier의 별도 Emergency 승격은 유지했다. 따라서 낮은 신뢰도 기억은 저체력 토글을 직접 켜지 않지만, 기존 공통 Critical 판단 전체를 제거한 것은 아니다.

## 6. 행동 선택과 공격성

저체력은 Action을 새로 생성하거나 슬롯을 교체하지 않는다. 기존 Emergency Cover 순서 외의 새 우선순위 재정렬은 없다.

Brain은 legacy 이동 제안의 기존 경로 안전 검증 뒤 CurrentRisk - DestinationRisk + CoverReward + RetreatReward - AggressionPenalty를 계산한다. 실제 체력 보정이 있는 제안이 개선을 만들지 못하거나, Emergency 탈출 경로가 현재보다 더 노출되고 총 위험도도 높으면 그 제안을 거부하고 기존 다음 슬롯을 평가한다. 안전한 후퇴 보상은 실제 거리 증가 및 안전한 경로 조건에서만 적용한다. 모든 슬롯이 실패하면 기존 정지/사격 허용 fallback을 유지한다.

Attack의 위치 점수 개선 요건을 보존하고 FastReposition의 최소 개선량에 선택 후보 AggressionPenalty를 더한다. Rush 등 노출된 불필요한 전진도 위 Brain 검증을 통과해야 한다. 무기 피해, 확률/명중률, FireCycle, 발사 모드, Target Score를 자기 체력으로 조정하지 않는다. 안전한 저체력 AI의 기본 사격 권한도 바꾸지 않는다.

## 7. Micro Move

EffectiveScale = Lerp(1, Clamp(LowHealthMicroMoveScale,0,1), Clamp(WeightedRisk,0,1)). 기본 Weight=1이면 요청한 HealthRisk 기반 공식과 같다.

최소/최대 검색 반경 모두 같은 배율을 적용한다. 진행 중 Micro Goal도 기존 재검증 때 줄어든 반경 안인지 확인한다. 축소된 최대 반경이 실제 최소 이동 거리보다 작으면 현재 자리에서 사격을 유지한다. 1이면 기존 반경, 0이면 체력 위험 1에서 반경 0이다. 체력 10%와 설정 0에서는 배율 0.1이다. Cover 배제와 Precision 안정화 대기, 명시적 Strafe Action은 그대로다.

## 8. BP 설정과 기본값

Brain의 Combat|Health Evaluation → HealthEvaluation 구조체에 노출한다.

- bEnableHealthEvaluation=true, HealthEvaluationWeight=1.
- CautionHealthRatio=0.65, CriticalHealthRatio=0.30.
- LowHealthExposurePenalty=20, LowHealthPathExposurePenalty=10.
- LowHealthCoverReward=20, LowHealthRetreatReward=15, LowHealthAggressionPenalty=15.
- LowHealthMicroMoveScale=0.5 (Advanced).
- HealthRatioReevaluationThreshold=0.05 (Advanced): 누적 5%p 변화 시 기존 점수를 조기 재검증한다. 0이면 정상 주기를 사용한다. 기존 Emergency 경계 통과와 기능 on/off 전환은 별도 갱신한다.

음수/비유한 가중치는 0, 비율은 0~1로 처리한다. 모든 신규 튜닝 값에 production 사용처와 단위/0의 의미/방향 Tooltip이 있다. 기존 프로퍼티 삭제·rename·이동이나 subobject 이름 변경은 없다. 에셋을 로드하지 않았으므로 실제 BP 직렬화 호환 성공은 미검증이다.

## 9. 캐시, 피해 기록 및 디버그

기존 피해 이벤트의 snapshot dirty와 position reevaluation 흐름은 그대로다. HealthRatio 변화는 현재/방어/Attack/Cover/Micro/재사용 점수의 다음 검증 시간을 앞당기며 예약, phase, 최소 유지 시간 및 기하 캐시는 유지한다. Snapshot/Target 점수 갱신을 직접 요청하지 않는다. 작은 변화는 기존 검증 주기에 반영된다.

dm.CombatDebug 2 + dm.CombatDebugDetails 1의 기존 MovementDecisionSummary에 current/max, ratio/risk, 목적지/경로/확대 후 체력 기여도, Micro 배율/실행 여부, Brain Cover/Retreat/Aggression 보정, Attack 현재/선택 점수 변화, Cover 최종 선택 보상을 추가했다. 기존 Risk Tier와 저체력 Emergency 결과도 함께 표시된다. Cover finalist는 선택 당시 값이라는 표시가 있다. 디버그는 계산된 Context/점수만 읽으며 새 Trace/Nav/Random/Evaluate/타깃 선택 호출을 추가하지 않았다.

## 10. 수행한 정적 검증

Tests/inspect_health_evaluation.ps1을 실행하여 소스/독립 계산식 검사 82개 통과. 충돌 마커, lexical 괄호 균형, generated include 마지막 위치, 11개 튜닝 설정의 production 참조/Tooltip, Context 계산 위치, Movement 직접 API 조회 제거, 기하 캐시와 체력 가산 순서, 단일 Total 가산, 경로 노출 독립성, Emergency 연결, Base Score 호환, 디버그에 새 평가 호출 없음 등을 확인했다.

기본값에서 Exposure=1, PathExposure=1인 독립 산술 확인:

- 100/100: Ratio=1, Risk=0, 목적지 비용=0, 경로 비용=0, Micro=1.
- 50/100: Ratio=0.5, Risk=0.5, 목적지 비용=10, 경로 비용=5, Micro=0.75.
- 10/100: Ratio=0.1, Risk=0.9, 목적지 비용=18, 경로 비용=9, Micro=0.55.

MaxHealth 0/음수/NaN/Infinity, 음수/초과 Current, 비활성/Weight=0, 역전된 Caution/Critical도 독립 산술로 확인했다. 이는 실제 C++ 실행 결과가 아니다.

Unreal 테스트 코드에는 Clamp, HealthRisk, 비활성/Weight=0, 노출 고/저체력 차이, 안전 위치 0 비용, 저체력 단독 및 실제 sensed 노출 Emergency, Micro, 역전/동일 경계와 비정상 수치 사례를 추가했다. **작성만 했으며 실행하지 않았다.**

git diff --check -- Source/SkySeoul/DeathMatch는 오류 없이 종료했다. 다만 DeathMatch 폴더 전체가 시작부터 untracked이므로 이것만으로 변경을 검증하지 않았다. 실제 수정 파일 직접 읽기와 위 스크립트를 사용했다. 이번 도구 쓰기 대상 16개는 모두 DeathMatch 내부다. 시작 시 이미 존재한 외부 Config/Content/Character 등의 변경은 수정하거나 되돌리지 않았다. 외부 파일 전체의 작업 전후 hash 비교를 수행했다는 의미는 아니다.

## 11. 미실행 검증, BP 확인 및 PIE 시나리오

Editor 실행, 전체 빌드, UHT, BP 컴파일/로드/재저장, PIE, Unreal 자동화 실행을 하지 않았다. C++ 타입/링크/UHT 성공과 실제 행동 품질은 미검증이다.

이후 BP에서 기존 Movement 슬롯 및 instanced 설정, Emergency 값 보존, 새 구조체 기본값/Advanced/Tooltip을 확인한다. Enable=false와 Weight=0에서 새 점수 보정이 사라지고 기존 Emergency만 남는지 확인한다. 비율 역전 및 0 가중치, Micro Scale 1/0을 확인한다.

PIE 확인 시나리오:

1. 동일한 적/위치에서 100%, 50%, 10%: 노출 비용이 증가하고 안전 위치 비용은 증가하지 않는지 비교.
2. 저체력 안전 사격 위치: Cover 반복 탐색 없이 사격 유지, Micro 축소 확인.
3. 저체력 실제 노출: 기존 Emergency 조건과 안전 Cover/Retreat 선택 확인.
4. 저체력 근접 적과 위험한 탈출 경로: 무조건 이동하지 않고 정지 대응 또는 기존 긴급 정책을 유지하는지 확인.
5. 같은 위치에서 위험한 Rush와 안전 Attack Position 비교, 현재 점수 개선이 없는 이동 억제 확인.
6. 여러 적의 목적지/경로 노출, 아군 사선/점유/Nav 실패의 기존 하드 거부 확인.
7. 기억만 남은 적, 환경/출처 불명 피해: sensed Emergency/새 공격 타깃을 만들지 않는지 확인.
8. 작은 연속 회복/피해, 큰 피해와 Emergency 경계 통과: 캐시 주기 및 조기 검증 확인.
9. Cover Hide/Peek, 명시적 Strafe, Precision 안정화, 각 Fire Mode와 실제 발사 주기 보존 확인.
10. 사망/리스폰/UnPossess/Brain 비활성화 후 이전 Micro/체력 관찰 상태가 남지 않는지 확인.
11. debug on/off가 행동/랜덤/타깃 변경 결과에 영향을 주지 않는지 비교.

## 12. 알려진 제한 및 후속 후보

- GAS AttributeSet이 없으면 공식 API의 MaxHealth=0에 따라 중립 Ratio=1이다. Body CurHP/MaxHP로 임의 fallback하지 않았다. 초기화 시점은 PIE 확인 대상이다.
- Cheap 후보 단계는 새 노출 비용을 추정하지 않는다. 기존 LOS/Path finalist 예산 밖 후보가 실제로 더 안전할 가능성은 남는다.
- 현재 위치는 경로 비용 0, 이동 후보는 실제 경로 평균 노출을 부담한다. 최대 순간 노출/노출 지속시간은 별도 모델링하지 않는다.
- 작은 HP 변화와 runtime 세부 가중치 수정은 기존 검증 주기까지 지연될 수 있다. Enable/Weight=0 전환, 기본 5%p 변화 및 Emergency 경계는 조기 갱신한다. 조사 전용 Action의 내부 재검색 주기도 유지한다.
- Cover 선택 보상과 Attack 점수 단위가 기존 Cost/Score 구조마다 다르므로 기본 수치의 실제 밸런스는 PIE 조정이 필요하다.
- Health는 현재 타깃을 직접 바꾸지 않는다. 위치 이동/LOS가 달라져 이후 기존 타깃 평가가 다른 결과를 내는 간접 효과까지 차단하는 것은 아니다.
- 우선순위를 존중하므로 상위 Action이 안전한 Hold를 반환하면 하위 Retreat를 건강도만으로 강제 선택하지 않는다. 전역 Action utility 정렬은 추가하지 않았다.
