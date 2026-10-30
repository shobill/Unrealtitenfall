# DeathMatch 전체 코드 정적 점검 보고서 — 2026-09-16 15:53

## 결론

현재 핵심 전투 연결은 동작 가능한 상태다. `Sense → SituationFramework → CombatTactics → Blackboard/BT → Fire Ability → Weapon` 흐름과 파츠 상태의 `CanShoot` 차단, 전투 위치 예약, 거리별 사격 정책은 서로 연결되어 있다. 직전 구현 보고서 기준 빌드 성공 및 Combat 자동화 24/24 통과 기록도 있다.

다만 구조가 두 번 교체되는 동안 구형 판단 계층이 남아 있어, **실제 주 전투 경로와 거의 겹치는 코드가 동시에 컴파일되고 일부는 런타임 생성까지 된다.** 당장 기능을 더 붙이기 전에 전투 의사결정의 단일 소유자를 확정하고 구형 계층을 정리하는 편이 안전하다.

> 이 보고서는 C++ 참조 검색, DeathMatch 에셋 문자열 참조, 현재 보고서와 문서 대조를 통한 정적 감사다. 이번 점검에서 빌드·PIE·자동화 테스트를 다시 실행하지 않았으며 코드는 수정하지 않았다.

## 우선순위별 발견 사항

### P1 — 사실상 미사용인 구형 SituationSupervisor가 통째로 남아 있음

- `AI/Situation/DMSituationSupervisor.h/.cpp`는 현재 Controller에서 생성되지 않는다.
- DeathMatch Content 에셋에서도 해당 클래스 참조가 발견되지 않았다.
- 현재 실제 소유자는 `UDMSituationFramework`이며 Controller도 이것만 기본 서브오브젝트로 생성한다.
- 그런데 `Controller/LOGIC_STATUS.md`, `AI/Move/LOGIC_STATUS.md`, `AI/BT/LOGIC_STATUS.md`는 여전히 Supervisor가 판단과 Blackboard를 소유한다고 설명한다.

판정: **삭제/보관 후보가 매우 확실한 구형 구현**이다. 단, 제거 전에 외부 DeathMatch 밖의 C++/BP 참조까지 Unreal Reference Viewer 또는 Asset Registry로 최종 확인해야 한다.

### P1 — LocalMessage + SingleShoot 구형 액션 계층이 현재 에셋과 연결되지 않음

- `UAIActionModule_SingleShoot`는 자기 파일 외 C++ 사용처가 없고 DeathMatch 에셋 참조도 발견되지 않았다.
- `UAIModuleMessage`도 Controller에서 생성되지 않고 에셋 참조가 없다.
- 따라서 `ULocalGameplayMessageManager`를 만드는 주체가 현재 없다.
- `UAIModule_Sense`에는 LocalMessage 방송 코드가 남아 있지만 Manager가 주입되지 않아 현재 주 경로에서는 실행 효과가 없다.
- 실제 사격은 `BTTask_DM_ActivateAbility`와 `UDMGameplayAbility_Fire`가 담당한다.

판정: `AIActionModule_SingleShoot`, `UAIModuleMessage`, `LocalGameplayMessageManager`, 관련 payload/tag 중 현행 경로에서 쓰지 않는 부분은 **한 묶음의 제거 후보**다. Sense의 LocalMessage 방송 코드도 함께 제거하거나, 이 이벤트 시스템을 정말 사용할 계획이라면 명시적으로 복구해야 한다. 현재처럼 반쪽만 유지하는 상태가 가장 혼란스럽다.

### P1 — Framework 전투 행동과 CombatTactics가 동일 책임을 중복 소유

`UDMSituationBrain_Combat`는 Roam/Approach/HuntLost/Hold/Attack 5개 Behavior를 매 Controller마다 생성한다. 그러나 `UDMSituationFramework::NotifySituationChanged`는 `CombatTactics::TryUpdateCombat()`가 성공하면 즉시 반환하므로, 적과 교전 중에는 이 Behavior 평가·실행 경로를 건너뛴다.

동시에 CombatTactics는 다음을 직접 수행한다.

- 이동 정책 우선순위 평가
- `MoveToLocation` 요청과 정지
- 조준과 회전
- 사격 승인
- Blackboard의 Target/Focus/WantAttack 갱신
- 기존 MoveGoal Blackboard 값 제거

결과적으로 현재 주 경로에서:

- `DMBehaviorModule_Attack`, `Hold`, `Approach`, `HuntLost`는 생성되지만 정상 교전에서는 사실상 우회된다.
- `CombatMoveDirector`의 Approach/HuntLost provider도 정상 교전에서는 Tactics 이동 모듈과 역할이 겹친다.
- Framework/MoveDirector는 주로 비전투 Roam 및 Tactics context 실패 fallback에 남는다.

권장 방향: **CombatTactics를 전투의 단일 소유자로 유지**하고 Framework는 `비전투 Roam + 전투 진입/이탈`만 맡기는 쪽이 현재 구현을 가장 적게 흔든다. 이 경우 Combat Brain의 Attack/Hold/Approach/HuntLost Behavior 및 해당 Move Provider를 정리하고 명칭/문서를 역할에 맞게 축소한다.

### P2 — BT 노드 중 사용 중인 것과 미사용인 것이 섞여 있음

`BT_Combat.uasset`에서 확인된 노드:

- 사용 확인: `BTTask_DM_SelectMoveLocation`, `BTTask_DM_FaceTarget`, `BTTask_DM_ActivateAbility`
- DeathMatch 에셋 참조 미발견: `BTService_DM_UpdateCombatState`, `BTDecorator_DM_CanShoot`, `BTDecorator_DM_HasTarget`

`BTTask_DM_SelectMoveLocation`은 코드 주석상 legacy처럼 보이지만 실제 BT 에셋이 참조하므로 바로 삭제하면 안 된다. 반대로 Service와 두 Decorator는 외부 에셋 참조를 최종 확인한 뒤 제거 후보로 분류할 수 있다.

### P2 — Adaptive 전용 Controller 복제본은 의도적이지만 설정 드리프트 위험

- `BP_DeathMatchAIController`: 기존 Precision 유지
- `BP_DeathMatchAIController_Adaptive`: Adaptive 명시 선택

기능 검증용 분리는 합리적이며 현재로서는 단순 중복 오류가 아니다. 다만 두 BP가 전체 컴포넌트 구성을 각각 저장하므로 앞으로 이동/엄폐/디버그 설정이 한쪽에만 반영될 위험이 있다.

권장: Adaptive BP를 원본 BP의 자식으로 만들고 Fire Mode만 override하거나, 하나의 Controller BP에서 데이터 에셋/설정 enum만 교체한다. 또한 기존 맵은 원본 Precision을 계속 사용하므로 Adaptive가 기본 적용된 상태는 아니다.

### P2 — 문서가 현재 코드와 크게 불일치

- `plan/03_CurrentStatus.md`는 Brain 부재, 빈 Robot/Controller, 점수/타이머 미완이라고 기록한다.
- 실제 코드는 SituationFramework/CombatTactics/AdaptiveFire가 있고 GS에 점수·라운드 타이머·상태 전환이 구현되어 있다.
- 여러 `LOGIC_STATUS.md`는 제거된 Supervisor를 현재 구조처럼 설명하고 BT SelectMoveLocation을 stub이라고 표기하지만 실제 구현과 에셋 참조가 있다.

판정: 현재 문서는 설계 판단을 오도할 가능성이 높다. 최신 구조도 1개만 기준 문서로 남기고 나머지는 날짜가 붙은 Archive로 이동하는 것이 좋다.

### P3 — 테스트와 일회성 에셋 스크립트 위치가 분산됨

- 자동화 테스트가 `AI/Combat/*Tests.cpp`와 `Tests/*.cpp`로 나뉜다.
- Python 에셋 수정/점검 스크립트 8개가 런타임 모듈 Source 하위 `Tests`에 누적되어 있다.
- C++ 테스트는 전처리 가드가 있어 Shipping 런타임에는 들어가지 않는 점은 적절하다.

권장: C++ 테스트 위치를 하나로 통일하고, Python은 `Scripts/DeathMatch/AssetMigration`처럼 목적별 디렉터리로 옮긴 뒤 `configure_*`는 반복 실행 안전 여부와 적용 날짜를 README에 기록한다. 완료된 일회성 스크립트는 삭제보다 Archive가 안전하다.

### P3 — 유지보수성과 이름 정리

- `DMGameplayAbility_Fire.cpp`가 약 1,422줄로 발사 실행, 몽타주/Notify, Adaptive 사이클, 안전 감시까지 한 파일에 모여 있다. 기능은 테스트되고 있으나 다음 변경부터 회귀 범위가 커진다.
- `AIModule_Sense.cpp` 약 693줄, `DMSituationFramework.cpp` 약 620줄도 책임 분리 여지가 있다.
- `DeathMatchGameInstane` 오탈자가 클래스/파일/API에 고착되어 있다. BP/직렬화 영향이 있으므로 Core Redirect를 포함한 별도 리네임 작업으로 처리해야 한다.
- `PlayerStartByTeam.cpp`는 include만 있는 빈 구현 파일이다. 문제는 아니지만 인라인-only 클래스라면 제거 가능하다.
- Tactics와 Brain에 Deprecated UPROPERTY가 남아 있다. 기존 BP 직렬화 호환을 위한 유지라면 제거 버전/시점을 명시해야 한다.

## 연결 상태 평가

### 적절하게 연결된 부분

- Perception과 `AIModule_Sense`가 적대/생존 필터, 기억, focus를 한곳에서 제공한다.
- `CombatTactics`가 이동 정책 결과의 `bAllowFire`를 사격 승인에 연결한다.
- 파츠 상태의 `CanShoot`이 Tactics와 Fire Ability 양쪽 안전 검사에 연결된다.
- Fire Ability가 Notify에서만 실제 `ExecuteFire`를 수행하고 결과를 cadence에 반영한다.
- Attack position/cover가 `DMCombatPositionRegistry`를 사용해 AI 중복 위치를 억제한다.
- 사거리와 피해/발사 확률의 최종 데이터 소유자가 Weapon Actor로 정리되어 있다.
- 디버그와 자동화 테스트가 전투 승인, 엄폐, 파츠, Adaptive fire까지 폭넓게 존재한다.

### 연결을 명확히 해야 하는 부분

- Framework, Tactics, BT 중 누가 **최종 의사결정자**인지 코드 구조로 고정해야 한다. 현재 실질적 최종자는 Tactics지만 Framework가 같은 역할의 모듈들을 생성한다.
- Roam만 기존 `MoveDirector/SelectMoveLocation`을 사용할지, Tactics 이동 정책으로 통합할지 결정이 필요하다.
- Adaptive Controller를 실제 GameMode/맵에 적용할 시점을 명시해야 한다. 현재 기본 맵은 기존 Precision 유지다.
- Sense의 LocalMessage 이벤트를 유지할지 제거할지 결정해야 한다.

## 권장 정리 순서

1. Unreal Reference Viewer/Asset Registry로 DeathMatch 밖까지 구형 클래스 참조 최종 확인.
2. `DMSituationSupervisor` 계층 제거 또는 Archive.
3. `SingleShoot + LocalMessage` 계층을 통째로 제거하거나 명시적으로 복구. 부분 유지 금지.
4. 전투 의사결정 소유자를 CombatTactics로 확정하고 Framework를 Roam/전환 역할로 축소.
5. 미사용 BT Service/Decorator 제거, 사용 중인 SelectMoveLocation은 Roam 정책 확정 전 유지.
6. Controller BP 상속/설정 구조를 정리해 Adaptive/Precision 복제 드리프트 방지.
7. 문서 최신화 후 Fire Ability 파일 분리와 테스트/스크립트 재배치.

## 삭제 전 확인 목록

- DeathMatch 밖 Content 및 플러그인의 BP/맵 참조
- `BP_DeathMatchAIController` SCS와 `BT_Combat` 실제 노드 구성
- 패키징 빌드에서 제거 대상 클래스 redirect 필요 여부
- 저장된 BP의 deprecated property 값이 더 이상 마이그레이션에 필요 없는지
- 기존 Precision Controller와 Adaptive Controller 중 최종 기본값

