# DM Combat Brain Movement Action Set 리팩터링 결과

- 작성일: 2026-10-02 KST (정확한 완료 시각 미기록)
- 범위: `Source/SkySeoul/DeathMatch`
- 구분: 구현 결과 및 정적 점검

## 요청 및 결론

DM Combat Brain의 고정 이동 Action 3개 슬롯을 제거하고, 크기 제한이 없는 `MovementActions` 배열과 `FDMCombatMovementActionEntry` 등록 구조로 교체했다. 구 슬롯용 deprecated 프로퍼티, fallback, 런타임 변환, 기본 Fixed Action 생성은 추가하지 않았다. Blueprint 에셋은 수정하지 않았으므로 기존 Controller/Brain BP는 사용자가 새 배열을 직접 구성해야 한다.

Priority는 점수 보너스가 아니라 강한 선택 계층이다. 실행 가능한 가장 높은 Priority를 먼저 정한 다음 그 계층 안에서만 가중 점수와 유지/전환 비용을 비교한다.

## 변경 파일

- `AI/Combat/DMCombatTacticsComponent.h/.cpp`
- `AI/Combat/DMCombatModules.h`, `DMCombatCoverModule.h`, `DMCombatInvestigation.h`
- `AI/Combat/DMCombatMovementDecision.cpp`
- `AI/Combat/DMCombatDamageAwareness.cpp`
- `AI/Combat/DMCombatThreatEvaluation.cpp`
- `AI/Combat/DMCombatThreatMemory.cpp`
- `AI/Combat/DMCombatDebug.h`, `DMCombatTacticsDebug.cpp`, `DMCombatDebugFormat.cpp`, `DMCombatWorldDebugSubsystem.cpp`
- `AI/Combat/DMCoverFallbackTests.cpp`, `DMCombatAttackPositionTests.cpp`, `DMCombatDebugTests.cpp`
- `AI/Combat/DMCombatMovementSelectionTestProbe.h`, `DMCombatMovementSelectionTests.cpp` 신규
- `Tests/DMArchitecturePIETests.cpp`
- `plan/01_Architecture.md` 현행 구조 갱신
- 구 슬롯을 직접 편집하던 `Tests/configure_attack_position.py` 제거
- 본 결과 보고서 신규

## 제거한 기존 구조

다음 프로퍼티와 모든 비보고서 코드 참조를 제거했다.

- `MovementModule`
- `MovementModulePriority2`
- `MovementModulePriority3`
- `bAllowEmergencyHidingWithoutModule`
- `EmergencyHidingModule`
- 고정 `Priorities[3]` 디버그 배열
- 생성자의 `MovementPolicy` Fixed 기본 서브오브젝트
- 내부 `NewObject<UDMCombatMovement_Hiding>()` 긴급 fallback

과거 사실을 보존하는 이전 날짜의 Communication 보고서는 수정하지 않았다. 현행 소스와 테스트에서 구 슬롯 이름이 남아 있지 않음을 별도 검색했다.

## 새 Entry 및 Blueprint 표시

`FDMCombatMovementActionEntry`는 다음 필드를 가진다.

- `bEnabled` (`Enabled`): false이면 평가와 실행에서 제외
- `Action`: `Instanced` inline Movement Action 인스턴스
- `Priority`: 높은 값이 먼저 선택되는 강한 계층
- `ScoreMultiplier`: Action의 context score에 적용하는 Brain별 선호도, 런타임에서 0 이상으로 clamp

DM Combat Brain 컴포넌트의 `Combat|Movement Actions > Movement Actions`에서 배열을 편집한다. 배열 최대 크기는 두지 않았다. 64개를 초과하면 non-Shipping BeginPlay 경고만 출력한다. `Micro Movement Auxiliary`는 같은 카테고리의 별도 프로퍼티로 유지된다.

## 선택 및 점수 규칙

1. 배열을 평가 시작 시 snapshot한다.
2. Disabled, null, 같은 UObject 인스턴스의 후속 중복 entry를 제외한다.
3. 각 Action의 `Evaluate()`와 `Score()`를 호출하고 실행 불가능한 결과를 제외한다.
4. 실행 가능한 후보의 최고 `Priority`를 결정한다.
5. 낮은 Priority 후보는 점수와 무관하게 선택에서 제외한다.
6. 최고 Priority 안에서 다음 최종 점수를 비교한다.

`FinalScore = finite(ActionContextScore) * max(0, finite(ScoreMultiplier)) + CurrentActionRetentionBonus - SamePrioritySwitchCost`

- 현재 Action 유지 보너스와 `MovementSwitchScoreMargin`은 현재 Action이 같은 최고 Priority에서 계속 실행 가능할 때만 적용한다.
- 더 높은 Priority가 실행 가능해지면 낮은 Priority의 유지 보너스와 전환 margin을 무시하고 즉시 교체한다.
- 정확한 동점은 배열 앞 entry가 이긴다.
- NaN/Infinity raw, weighted, final score는 후보에서 제외한다.
- 별도 RecentFailurePenalty 숫자 필드는 추가하지 않았다. 실행 실패·일시 사용 불가 Action은 `bCanExecute=false`로 제외하며 기존 Action 내부 cooldown을 보존한다.

Action 전환 시 이전 Action에 `Deactivate()`를 한 번 호출한다. 같은 Action 유지 시 Deactivate/Reset하지 않는다. 평가 중 실행 가능했지만 선택되지 않은 비현재 Action도 Deactivate하여 예약 위치가 남지 않게 했다. 같은 UObject 중복 entry는 첫 활성 entry만 평가하므로 같은 Tick에 중복 Evaluate/Deactivate되지 않는다. 같은 클래스의 서로 다른 UObject 인스턴스는 허용한다.

## 빈 배열과 Hiding

`MovementActions`가 비어 있으면 Fixed를 자동 생성하지 않는다. 새 주 이동 목적지를 요청하지 않고 현재 위치를 유지하며, 다른 사격 gate가 허용하면 정지 상태에서 발사할 수 있다. 디버그 사유는 `MovementActions array is empty`로 기록한다. 기존 이동 요청은 hold 결과를 처리하는 공통 경로에서 정지된다.

Hiding은 `MovementActions`에 활성 entry로 등록된 경우에만 후보가 된다. 미등록 상태에서 비상 상황이 생기면:

- visible 전투에서는 등록된 나머지 실행 가능 Action을 정상 Priority 규칙으로 평가한다.
- threat-memory 방어에서는 등록된 Cover/Hiding/Retreat/KeepDistance 계열을 같은 공통 Priority 선택기로 평가한다.
- 실행 가능한 대안이 없으면 현재 위치를 유지한다.
- 디버그에 `Emergency active but Hiding is not registered`를 남긴다.
- Hiding 인스턴스를 내부에서 자동 생성하지 않는다.

## Micro Movement

Micro Movement는 Primary `MovementActions`에 통합하지 않았다. `Micro Movement Auxiliary`에서 별도로 설정한다. Primary Action이 이동 중이거나 Cover Action이 활성일 때 Micro가 경로를 덮어쓰지 않는 기존 규칙과 Action 자체의 속도/ADS/사격 정책을 유지했다. 미등록이면 실행되지 않는다.

## 디버그 변경

기본 디버그는 활성/전체 entry 수, 선택 Action, Priority, 최종 점수와 선택 사유를 표시한다. 상세 디버그는 동적 배열을 사용해 각 entry의 index, enabled, Action 이름, Priority, 실행 가능 여부, raw score, multiplier, weighted score, retention bonus, switch cost, final score, 제외 사유와 selected 상태를 표시한다. 상세 후보 배열은 평가마다 Reset/SetNum하여 무한 증가하지 않는다.

## 테스트 소스 갱신

신규 native 자동화 테스트 소스는 다음을 검사하도록 작성했다.

- 4개 초과 Action 등록
- disabled/null/중복 UObject 제외
- 실행 불가능한 높은 Priority 제외
- 실행 가능한 최고 Priority 계층 선택
- 같은 Priority의 Score Multiplier 및 weighted score 비교
- 낮은 Priority의 큰 점수가 높은 Priority를 이기지 못함
- 같은 Priority 동점에서 앞 entry 선택
- 현재 Action retention과 switch cost가 같은 Priority에서만 작동
- 높은 Priority 출현 시 낮은 Priority 현재 Action 즉시 교체 및 이전 Action Deactivate
- 음수 multiplier clamp와 non-finite score 제외
- 빈 배열 hold/fire-safe 동작
- Hiding 미등록 시 자동 생성 없음
- Micro Movement와 Primary 배열 분리

기존 Cover fallback, Attack Position, Combat Debug, Architecture PIE 테스트 소스의 구 슬롯 참조도 배열 API로 갱신했다. 구 Blueprint 슬롯 값을 전제로 하던 AssetPriority 테스트는 제거했다.

## 기존 Blueprint에서 사용자가 다시 구성할 항목

기존 Controller 또는 Brain BP의 **DM Combat Brain > Combat > Movement Actions > Movement Actions** 배열을 새로 만들고, 기존에 사용하던 각 Action 인스턴스 설정을 직접 다시 입력해야 한다. 자동 migration이나 fallback은 없다.

권장 균형형 예시:

- Hiding: Priority 100, Score Multiplier 1.0
- Retreat Fire: Priority 80, Score Multiplier 1.0
- Combat Cover: Priority 70, Score Multiplier 1.0
- Attack Position: Priority 60, Score Multiplier 1.0
- Maintain Distance: Priority 50, Score Multiplier 1.0
- Rush: Priority 40, Score Multiplier 0.8
- Fixed Fire: Priority 0, Score Multiplier 1.0

이는 보고서 예시일 뿐 C++에서 자동 생성하지 않는다. Micro가 필요하면 **Micro Movement Auxiliary**에 별도로 지정한다. 각 inline Action의 기존 세부 설정(이동 속도 배율, ADS/Fire 정책, 거리, Cover 검색 설정 등)도 새 인스턴스에서 확인해야 한다.

## 남은 제한 사항

- 기존 BP의 구 슬롯 직렬화 데이터는 읽거나 변환하지 않는다. BP를 새 배열로 재설정하고 컴파일/저장해야 한다.
- target-switch position reuse용 내부 evaluator와 별도 `InvestigationAction`은 Primary 배열 밖의 기존 Brain 보조 경로로 유지했다.
- Action 감각과 Priority/Multiplier 값은 PIE에서 튜닝이 필요하다.

## 검증 상태

- 변경 파일 중괄호 균형 점검: 완료.
- 충돌 마커 검색: 없음.
- DeathMatch 현행 코드/테스트에서 구 슬롯 프로퍼티, emergency Hiding fallback, 고정 `Priorities[3]` 참조 검색: 없음. 이전 Communication 보고서의 역사 기록은 제외.
- Blueprint 에셋 수정: 하지 않음.
- Blueprint 테스트: 사용자 지시대로 생략.
- 전체 에디터 빌드/UHT/컴파일: 사용자 지시대로 생략.
- Native 자동화 테스트 실행: 생략. 테스트 소스만 추가·갱신.

따라서 정적 구현과 참조 정리는 완료했지만 최종 컴파일, Details 패널 표시, BP 재구성 후 런타임 동작은 후속 확인이 필요하다.
