# DeathMatch 구조 정리 후 재점검 보고서 — 2026-09-17 16:33

## 최종 판정

구조 정리는 의도대로 적용되었다. 현재 확인 범위에서 즉시 수정해야 할 치명적 연결 오류나 테스트 회귀는 발견되지 않았다.

- SkySeoulEditor Win64 Development 빌드 성공, exit code 0.
- `SkySeoul.DeathMatch.Combat` 자동화 테스트를 이번 점검에서 다시 실행했고 26/26 성공, 실패 0, 프로세스 exit code 0.
- CombatTactics가 교전 판단과 이동·조준·사격 승인을 소유한다.
- SituationFramework는 Roam, 전투 진입/이탈, 안전 정지 fallback으로 제한되었다.
- 구형 Supervisor/LocalMessage/SingleShoot 및 미사용 BT Service/Decorator 구현은 제거되었다.
- 현재 문서는 실제 코드 구조와 일치한다.

## 확인한 정리 항목

### 구형 계층 제거

다음 구현 파일은 제거되었으며 활성 Source/Content에서 런타임 참조가 발견되지 않았다.

- DMSituationSupervisor
- AIActionModule_SingleShoot
- AIModuleMessage
- LocalGameplayMessageManager
- AIModuleTags
- BTService_DM_UpdateCombatState
- BTDecorator_DM_CanShoot
- BTDecorator_DM_HasTarget

`UAIModuleBase`는 Sense의 공통 부모와 직렬화 호환을 위한 빈 추상 컴포넌트로만 남아 있다. 과거 LocalMessage/Action API는 남아 있지 않다.

### 전투 소유권

`UDMSituationFramework::NotifySituationChanged`는 서버 권위에서 먼저 CombatTactics에 실행 기회를 준다. Tactics가 교전 중이면 Framework는 디버그 상태만 반영하고 Behavior 실행이나 Blackboard 덮어쓰기를 하지 않는다.

Framework의 Behavior 생성은 `BehaviorId == Roam`인 클래스만 허용한다. BP에 과거 Attack/Hold/Approach/HuntLost 배열이 저장되어 있어도 AI 인스턴스마다 생성하지 않는다.

Framework의 `ApplyDecision`은 Tactics가 교전 중이면 즉시 반환한다. 비전투에서는 목표 정리를 먼저 수행한 후 Roam을 활성화하므로 새 Roam MoveTo를 곧바로 StopMovement로 취소하던 순서 문제도 없다.

### 이동 소유권

- 비전투: Framework → Roam Behavior → MoveDirector → Roam Provider → 직접 MoveTo.
- 교전: CombatTactics → 선택된 전투 이동 모듈 → 직접 MoveTo.
- MoveDirector는 Roam Provider만 registry에 넣고, Tactics 교전 중 요청을 거부한다.
- BT SelectMoveLocation은 Framework가 있는 정상 경로에서 병행 이동 목표를 만들지 않는다.

자동화 테스트에도 Architecture.Ownership 및 PIEControllers가 추가되어 이 소유권을 회귀 검사한다.

### 사망·리스폰 초기화

Controller의 사망, UnPossess, 리스폰 경로에서 Tactics와 Framework가 초기화된다. Sense의 감지 및 기억도 초기화하도록 연결되어 이전 대상을 기억한 채 즉시 재교전하는 문제가 보완되었다.

### 문서

활성 `plan`과 `LOGIC_STATUS` 문서는 현재 역할 분담을 설명한다. 이전 문서는 `Communication/Archive/Legacy-2026-09-17`에 보관되어 있으며 활성 설계 문서와 구분된다.

## 남은 항목

### P2 — BP 직렬화 호환용 구형 클래스와 컴포넌트

다음 항목은 런타임 주 경로에서는 사용되지 않지만 저장 BP 참조 때문에 유지된다.

- DMBehaviorModule_Attack
- DMBehaviorModule_Hold
- DMBehaviorModule_Approach
- DMBehaviorModule_HuntLost
- DMMoveGoalProvider_Approach
- DMMoveGoalProvider_HuntLost
- BP_DM_AttackModule

이들은 현재 중복 행동을 실행하지 않으므로 기능상 문제는 아니다. 다만 Controller 컴포넌트 목록과 소스 규모를 완전히 줄이려면 별도의 BP migration으로 저장 참조를 제거한 후 삭제해야 한다. 특히 Approach/HuntLost Provider는 Controller의 native subobject로 계속 생성되지만 Director에는 등록되지 않는다.

### P2 — Controller BP 복제 구조

Precision과 Adaptive Controller BP는 모두 native Controller의 직접 자식이며 로컬 DMCombatTactics SCS 구성이 복제되어 있다. 현재 기능은 정상이나 한쪽 설정만 바뀌는 드리프트 위험은 남아 있다. SCS와 instanced module 설정을 보존하는 별도 migration 없이 단순 재부모화하면 안 된다.

### P3 — 자동화 로그 노이즈

테스트는 성공했지만 다음 경고가 반복된다.

- 임시 테스트 World의 `DestroyActor: World has no context`
- fixture에 SkeletalMesh/BehaviorTree/NavMesh가 없어 발생하는 경고
- Parts 초기화 및 CombatDiag를 Warning 레벨로 출력하는 로그
- 테스트 Ability의 invalid AbilitySpecHandle 경고

현재 테스트 실패나 실제 Warehouse PIE 오류를 의미하지는 않는다. 다만 진짜 회귀 경고를 찾기 어렵게 하므로 테스트 fixture 보완 또는 expected warning 처리, 진단 로그 레벨 하향을 후속 정리 후보로 권장한다.

## 이번 재검증 결과

- 빌드 명령: Unreal Build Tool, SkySeoulEditor Win64 Development.
- 결과: Target up to date, Succeeded, exit 0.
- 자동화 명령: UnrealEditor-Cmd NullRHI, `Automation RunTests SkySeoul.DeathMatch.Combat`.
- 결과: 26개 Success, 실패 결과 0, exit 0.
- 실행 로그: `Saved/Logs/SkySeoul.log`.

## 결론 및 다음 단계

현재 상태에서 구조 정리 작업은 완료로 봐도 된다. 곧바로 추가 삭제를 진행할 필요는 없다.

다음 정리를 한다면 기능 개발과 분리하여 다음 순서를 권장한다.

1. 저장 BP의 구형 Behavior/Provider 참조를 안전하게 migration.
2. 호환용 Behavior 4종과 Provider 2종 및 BP_DM_AttackModule 제거.
3. Precision/Adaptive Controller의 공통 설정 상속 또는 데이터화.
4. 테스트 경고와 진단 로그 노이즈 축소.

위 작업은 현재 전투 기능을 고치는 작업이 아니라 컴포넌트 목록과 유지보수 비용을 더 줄이는 2차 정리다.
