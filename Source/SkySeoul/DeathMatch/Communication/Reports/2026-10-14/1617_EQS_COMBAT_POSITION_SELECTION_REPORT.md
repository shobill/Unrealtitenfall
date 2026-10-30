# DeathMatch EQS 전투 위치 선택 구현 결과

- 작성일: 2026-10-14 16:17 KST
- 범위: 범용 전투 위치 BT Task, 목적별 평가 프로필, 공통 사격선 판정, ActionIntent 실행 설정, 자동화 테스트
- 구분: C++ 구현 결과

## 결론

`DM Find Combat Position` 하나로 Attack, Approach, Hide 세 목적을 처리하도록 구현했다. 하나의 공통 EQS가 NavMesh 후보를 공급하고, Task가 선택된 `UDMCombatPositionProfile`로 필수 필터와 점수를 적용한다. 실패 시 임의 위치 또는 다른 목적의 위치를 만들지 않고 `MoveGoal`을 비운 뒤 Failed를 반환한다.

EQS/BT/Profile 에셋은 이번 범위에 따라 만들지 않았다. C++ 빌드 및 자동화 테스트는 완료했다.

## 추가한 파일

- `AI/Combat/DMCombatPositionProfile.h/.cpp`
  - `EDMCombatPositionPurpose`: Attack, Approach, Hide
  - `UDMCombatPositionProfile`: 목적별 Data Asset
  - 공통 criterion, 검색 제한, 정규화 및 순수 평가 함수
- `AI/BT/BTTask_DM_FindCombatPosition.h/.cpp`
  - 표시 이름 `DM Find Combat Position`
  - 공통 EQS 비동기 실행, 목적별 프로필 선택, 필터/점수/캐시, Blackboard 출력
- `Tests/DMCombatPositionTests.cpp`
  - 전투 위치 선택 전용 C++ 자동화 테스트 10개
- 본 결과 보고서

## 수정한 파일

- `AI/Fire/DMCombatFireControlComponent.h/.cpp`
  - 실제 Fire Control과 위치 평가가 공유하는 무기 채널 사격선 판정 추가
  - Attack 기본 이동 중 사격 허용
- `AI/Execution/DMActionIntentTypes.h`
- `AI/Execution/DMActionIntentComponent.h/.cpp`
  - branch별 ADS/이동 사격 실행 override 추가
- `AI/BT/DMBTExecutionServices.h/.cpp`
  - `bUseADSWhileMoving`, `bAllowFireWhileMoving` 에디터 설정 추가
- `AI/Combat/DMCombatPositionRegistry.h/.cpp`
  - 예약을 제외한 현재 아군 위치 snapshot 조회 추가
- `AI/BT/BTTask_DM_SelectMoveLocation.h`
  - 에셋 호환을 위해 유지하되 DeprecatedNode로 표시
- `Tests/DMFireControlPolicyTests.cpp`
  - Attack/Hide 이동 사격 기본 정책 회귀 검증 추가

삭제한 파일은 없다.

## PositionPurpose와 프로필 구조

Task의 `PositionPurpose`는 Attack, Approach, Hide만 제공한다. 목적별 Task 클래스를 복제하지 않는다.

Task에는 다음 프로필 슬롯이 있다.

- Attack Position Profile
- Approach Position Profile
- Hide Position Profile

프로필은 `UDMCombatPositionProfile` Data Asset이다. 각 평가 criterion은 다음 값을 제공한다.

- 사용 여부
- 필수 필터 여부
- 점수 가중치
- 허용 최소/최대
- 정규화 최소/최대
- Higher/Lower/Peak 점수 방향
- 검사 반경

공통 검색 설정은 프로필 안에서 다음을 조절한다.

- 최대 후보 수
- 최대 기억 위협 수
- 고비용 검사 후보 수
- 검색 cooldown
- 결과 재사용 시간
- 위협 기억 최대 나이
- 후보 사격/노출 검사 높이

Purpose를 에디터에서 바꾸면 Attack은 ADS On/Moving Fire On, Approach와 Hide는 Off/Off 기본값으로 맞춘다. 필요하면 Data Asset 또는 `DM Action Intent` Service 실행 override에서 바꿀 수 있다.

## 공통 후보 생성과 비용 제한

`CommonCandidateQuery` 하나를 모든 목적이 공유한다. 에디터 EQS에서는 NavMesh 기반 point generator만 구성하고 All Matching 결과를 Task에 넘긴다.

Task 처리 순서는 다음과 같다.

1. EQS 결과를 프로필의 `MaxCandidateCount`로 제한한다.
2. 각 후보에 complete/non-partial Nav 경로가 있는지 필수 확인한다.
3. 사거리 적합도와 설정 반경 내 아군 수로 저비용 1차 순위를 만든다.
4. 상위 `ExpensiveCheckCandidateCount`개만 남긴다.
5. 남은 후보에 실제 사격선, 기억 위협 노출 trace, 최종 path cost 점수를 적용한다.
6. 필수 필터를 통과한 최고 점수 위치 하나만 반환한다.

Nav 완전성 검사는 모든 채택 가능 후보에 필수다. 실제 경로 길이를 점수로 쓰는 작업, LOS와 다중 위협 trace는 축소 후보에만 최종 반영한다. 검색은 cooldown 안에서 재실행되지 않으며, 같은 목적/타겟의 성공 결과는 짧은 reuse 시간 동안 재사용한다.

## 목적별 필터와 점수

### Attack

필수 조건:

- 유효하고 현재 보이는 focusing target
- complete/non-partial Nav 경로
- 무기 최대 사거리 이내
- 공통 사격선 판정 성공

점수:

- 기억 중인 위협 노출도
- 무기 유효 사거리 적합도
- 설정 반경 내 현재 아군 수
- 선택적 Nav 경로 비용

별도 엄폐, 공격 각도, 현재 위치 보너스, 예약, 체력/위험, 추격 점수는 넣지 않았다.

### Approach

Attack과 동일하게 유효하고 현재 보이는 focusing target, 완전 경로, 최대 사거리, 후보 위치의 사격 가능성을 필수로 요구한다. 점수도 사거리 적합도, 위협 노출, 주변 아군 수, 경로 비용만 사용한다.

타겟과 단순히 가까울수록 점수가 오르지 않는다. `ComputeRangeFitness`는 지나치게 가까운 위치를 낮게 평가하고 유효 사거리 대역을 최고로 평가하므로 직선 최단거리 돌진을 방지한다.

### Hide

필수 조건:

- complete/non-partial Nav 경로
- 유효 기간 안의 기억 위협 최소 1명
- `MinimumConcealedFraction` 이상 은폐

점수:

- 위협 노출 감소
- 가려진 위협 수에 반영되는 weighted exposure
- 설정 반경 내 현재 아군 수
- 이동 경로 비용

사격 가능성과 무기 사거리는 Hide에서 평가하지 않는다. 완전 은폐만 요구하지 않고 최소 은폐 기준과 점수 순위를 분리했다.

## 기억 위협과 아군 평가

노출 평가는 `UDMCombatKnowledgeComponent::GetThreatRecords()`만 읽는다. 전역 적 검색이나 숨은 적의 실시간 transform 조회는 하지 않는다.

- 현재 보이거나 유효 기억 안에 있는 record만 포함
- `ThreatMemoryMaxAge` 초과 record 제외
- `MaxThreatCount` 상한 적용
- focusing target 우선, visible/최근 감지 순으로 제한
- 각 record의 `LastKnownLocation` 사용
- focusing target과 다른 위협의 노출 가중치 별도 설정

아군 밀집도는 `DMCombatPositionRegistry`의 프레임 공유 current-occupant snapshot을 사용하지만 예약 claim은 명시적으로 제외한다. 후보별로 설정 반경 안의 수만 센다.

## 실제 사격과 EQS 사격 판정 공유

`UDMCombatFireControlComponent::EvaluateLineOfFire`를 공통 함수로 만들었다. 실제 Fire Control과 전투 위치 Task가 모두 다음 계약을 사용한다.

- `UPartsComponent::WeaponTraceChannel`, 없으면 `PartsHitCollision::DefaultWeaponTraceChannel`
- 무기 최대 사거리
- intended target 실제 hit
- 월드/다른 actor 차단
- 같은 팀 actor의 사선 차단을 `FriendlyFireRisk`로 분류

Fire Ability도 Parts weapon channel과 일치 여부를 기존 `ValidateWeaponTraceChannel`로 검증하므로 위치 평가와 실제 발사 경로의 채널 계약이 하나로 맞춰진다.

## Blackboard 입출력

입력:

- 직접 Blackboard 상태를 재판정하지 않는다.
- Knowledge의 focusing target 및 기억 snapshot, Controller의 현재 무기 정보를 읽는다.

출력:

- 성공: 기존 Vector Key `MoveGoal`
- 실행 프로필 전달: `ShouldADS`, `FireWhileMoving`
- 실패: Task가 소유하는 `MoveGoal`만 clear하고 Failed

Knowledge, CombatTarget, 다른 branch의 이동 요청, Tactical Area 상태는 삭제하지 않는다. EQS 실패, 설정 누락, cooldown, 필터 전멸 모두 fallback 위치를 생성하지 않는다.

## ActionIntent 실행 설정

위치 점수에는 ADS와 이동 사격 여부를 넣지 않았다. `DM Action Intent` Service가 branch 수명 동안 실행 override를 적용한다.

- `bUseADSWhileMoving`
- `bAllowFireWhileMoving`

기본 정책은 Attack ADS On/Moving Fire On, Hide Off/Off다. Approach는 BT에서 FastReposition 등 원하는 intent와 override를 조합해 Off/Off 기본 또는 신중 이동 변형을 만들 수 있다. branch 종료/abort 시 기존 request stack 정책대로 이전 intent가 복구된다.

## 디버깅

`dm.CombatDebug 2`일 때 검색 1회당 compact log 하나만 남긴다.

- Position Purpose
- EQS 후보 수
- 최종 필터 통과 수
- 선택 위치와 총점
- Fire Feasibility
- Threat Exposure
- Range Fitness
- Nearby Friendly Count
- Path Cost
- 실패 이유

모든 후보의 세부 점수를 화면에 출력하지 않는다.

## 기존 시스템과 책임 경계

- BT: Attack/Approach/Hide 목적 선택, Task 실행, 성공 시 Move, 실패 시 Selector fallback
- EQS: 공통 NavMesh 후보 생성
- Find Combat Position Task/Profile: 목적별 위치 필터와 순위
- Knowledge: 보이는/기억 위협과 focusing target snapshot
- Combat Status/Decorator: 체력·위험·행동 전환 판단
- ActionIntent/AimZoom/Fire Control: ADS와 이동 사격 실행
- Move Director: BT가 제출한 이동 실행
- Tactical Area: 기존 Roam 위치 선택 그대로 유지

Task 안에서 체력, 위험도, Combat Status, 추격/조사 상태를 다시 계산하지 않는다. 위치 예약도 전투 위치 평가에 연결하지 않았다.

## 자동화 테스트 결과

최종 직접 실행 결과:

- UnrealHeaderTool: 성공
- `SkySeoulEditor Win64 Development`: 성공
- `SkySeoul.DeathMatch.CombatPosition`: 10/10 성공
- `SkySeoul.DeathMatch.Combat.FireControl`: 5/5 성공
- `SkySeoul.DeathMatch.BTFoundation`: 8/8 성공

CombatPosition 테스트 항목:

- 목적별 프로필 선택
- Attack 사격 가능 필터
- 실제 무기 채널과 아군 사선 차단
- 최대 사거리 필터
- 유효 사거리 적합도
- 현재/유효 기억 위협만 포함
- 만료 기억 제외
- 설정 반경 내 아군 수
- Approach 최단거리 돌진 방지
- Hide 저노출 우선
- 실패 시 fallback 없음 및 이전 MoveGoal clear
- 검색 cooldown으로 매 프레임 반복 방지

참고로 전체 `SkySeoul.DeathMatch.Combat` 필터도 실행했으나 이번 작업 제외 범위인 기존 PIE Fire 테스트와 기존 Parts HitRouting 테스트가 실패했다. PIE는 Controller BP의 제거된 `DMCombatTactics` 참조가 남은 상태이고, HitRouting은 기존 damage fixture 결과가 0인 문제다. 새 CombatPosition 10개와 관련 FireControl 5개는 같은 실행에서도 모두 성공했다.

## 사용자가 에디터에서 생성·연결할 에셋

1. 공통 EQS `EQS_DM_CombatPositionCandidates` 하나를 만든다.
   - NavMesh 기반 point generator 사용
   - AI가 이동 가능한 후보만 생성
   - 목적별 점수 Test는 넣지 않고 공통 후보 공급 역할만 담당
2. `UDMCombatPositionProfile` Data Asset 세 개를 만든다.
   - `DA_DM_AttackPositionProfile`, Purpose=Attack
   - `DA_DM_ApproachPositionProfile`, Purpose=Approach
   - `DA_DM_HidePositionProfile`, Purpose=Hide
3. Root BT의 각 전투 branch에 같은 `DM Find Combat Position` Task 클래스를 배치한다.
   - 각 instance의 `PositionPurpose`만 Attack/Approach/Hide로 선택
   - 공통 EQS와 세 Profile 슬롯 연결
4. Blackboard에 `MoveGoal` Vector를 연결한다.
   - 실행 설정을 Blackboard에서도 사용할 경우 `ShouldADS`, `FireWhileMoving` Bool 추가
5. 성공 뒤 기존 `DM Move To Goal`을 실행하고, 실패는 Selector 다음 branch로 흐르게 한다.
6. branch의 `DM Action Intent` Service에서 필요하면 실행 override를 설정한다.
   - Attack 권장: ADS On / Moving Fire On
   - Approach 권장: ADS Off / Moving Fire Off
   - Hide 권장: ADS Off / Moving Fire Off
7. 기존 `DM Select Move Location`은 새 전투 branch에 연결하지 않는다.

## 아직 구현하지 않은 항목

- 추격 시스템
- Investigate 위치 검색
- 마지막 목격 위치 자동 이동
- CombatCover 전용 위치/노드
- Retreat 전용 위치/노드
- Flank 전용 위치/노드
- MicroMove 전용 위치/노드
- 전투 위치 예약
- 체력/위험 재계산
- Tactical Area Roam 변경
- 실제 BT/EQS/Profile/Blackboard uasset 생성 및 연결
- Blueprint 수정/컴파일
- PIE 전투 검증

