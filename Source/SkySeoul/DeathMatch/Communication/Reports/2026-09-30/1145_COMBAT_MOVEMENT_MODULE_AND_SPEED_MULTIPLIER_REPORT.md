# DeathMatch 전투 이동 모듈 및 속도 배율 리팩터링 결과

- 작성일: 2026-09-30 11:45 KST
- 범위: `Source/SkySeoul/DeathMatch` 내부 전투 이동, 비전투 PathMove, 디버그 및 자동화 테스트 소스
- 구분: 구현 결과

## 요청 및 결론

Combat Brain에 직접 들어 있던 마이크로 무빙을 선택적 보조 이동 모듈로 분리했고, 생존 우선 도주를 `Hiding`으로 독립시켰다. `Combat Cover`는 교전을 유지하는 엄폐 행동으로 제한했다. 전투·비전투 이동속도는 DM Combat Brain이 캡처한 `BaseWalkSpeed`와 안정적인 enum 소스별 배율 요청으로 계산하며, 활성 배율 중 최솟값 하나만 최종 적용한다.

작업 전 워크스페이스와 상위 경로에서 `AGENTS.md`를 검색했으나 파일이 존재하지 않았다. `Communication/00_README.md`와 기존 확정 구조 보고서를 읽고 보고서 위치·형식 및 기존 소유권 규칙을 준수했다. 작업 시작 시 `Source/SkySeoul/DeathMatch` 전체가 Git에서 미추적 상태였으므로 Git diff만으로 기존 사용자 변경과 이번 변경을 구분할 수 없었다. DeathMatch 밖 파일과 Blueprint 에셋은 수정하지 않았다.

## 1. 변경 파일

### 전투 이동 및 Brain

- `AI/Combat/DMCombatModules.h`, `.cpp`
- `AI/Combat/DMCombatCoverModule.h`, `.cpp`
- `AI/Combat/DMCombatMicroMovementModule.h`, `.cpp` (신규)
- `AI/Combat/DMCombatTacticsComponent.h`, `.cpp`
- `AI/Combat/DMCombatMovementDecision.cpp`
- `AI/Combat/DMCombatThreatMemory.cpp`
- `AI/Combat/DMCombatDamageAwareness.cpp`
- `AI/Combat/DMCombatAttackPosition.h`, `.cpp`
- `AI/Combat/DMCombatInvestigation.h`, `.cpp`
- `AI/Combat/DMCombatFire_Adaptive.h`

### 디버그

- `AI/Combat/DMCombatDebug.h`
- `AI/Combat/DMCombatTacticsDebug.cpp`
- `AI/Combat/DMCombatDebugFormat.cpp`

### 비전투 이동 배율

- `AI/Framework/DMBehaviorModule_PathMove.h`, `.cpp`
- `AI/Framework/DMBehaviorModule_Move.cpp`
- `AI/Framework/DMBehaviorModule_Approach.cpp`
- `AI/Framework/DMBehaviorModule_HuntLost.cpp`

### 테스트 소스

- `AI/Combat/DMCombatMovementSpeedTests.cpp` (신규)
- `AI/Combat/DMAdaptiveFireTests.cpp`
- `AI/Combat/DMWeaponRangeTests.cpp`

## 2. 추가한 모듈과 클래스

- `UDMCombatMovement_Hiding`: 생존 우선 완전 은폐 위치를 찾는 독립 주 이동 Action.
- `UDMCombatMovement_Micro`: 주 Action이 이동하지 않을 때만 짧은 로컬 이동을 제안하는 선택적 보조 Action.
- `EDMMovementSpeedSource`: `Roam`, `MovementAction`, `ADS`, `Firing`, `ExternalStatus`의 안정적인 속도 요청 키.
- `FDMCombatMoveResult::SelectionScore` 및 Movement Action 공통 `SelectionScore`/정책/배율 설정.

## 3. Hiding과 Combat Cover의 최종 역할

### Hiding

- `C.bEmergencyCover`가 활성화되고 Hiding Action이 등록된 경우에만 실행 가능하다.
- 완전 은폐와 위협 거리 증가를 우선하며 Partial Combat Cover를 허용하지 않는다.
- 시작부터 ADS와 사격을 차단하고, 이동은 `EmergencyCover` intent를 사용한다.
- 도착 후 `ConcealedReevaluationDelay` 동안 은폐를 유지한다. 지연 후 긴급 조건이 끝났을 때만 다른 Action 재평가를 허용한다. 긴급 조건이 계속되면 은폐를 유지한다.

### Combat Cover

- Brain의 emergency 플래그를 내부 Cover phase에 전달하지 않는다. 따라서 Combat Cover가 완전 도주/숨기 행동으로 변하지 않는다.
- ADS, 이동 중 사격, 도착 전 사격 기본 정책이 모두 true다.
- 기존 Hide/Peek 쌍, GoodEnough 조기 선택, 다중 위협/예약/경로 검증을 유지한다.

## 4. EscapeCover / PreparationCover 처리

- `UDMCombatMovement_EscapeCover`는 삭제하지 않았다. `UDMCombatMovement_Hiding`을 상속하는 `Escape Cover (Deprecated)` 호환 래퍼로 유지했다.
- 기존 BP에 EscapeCover 인스턴스가 있으면 자동으로 Hiding 의미와 정책을 사용한다. 즉시 클래스 교체는 필수가 아니지만, 새 BP에서는 Hiding으로 교체하는 것을 권장한다.
- `UDMCombatMovement_PreparationCover`는 준비 위치 도착 후 `OnPreparationWindowStarted`와 명시적 완료 신호를 사용하는 고유 역할이 있어 유지했다. 이동 전/중 사격은 기본 차단한다.

## 5. Micro Movement 모듈화 방식

권장안인 보조 이동 모듈로 구현했다. DM Combat Brain의 `Micro Movement Auxiliary`에 `UDMCombatMovement_Micro` 인스턴스를 명시적으로 지정해야 활성화된다. 기본값은 null이므로 등록하지 않으면 실행되지 않는다.

Brain은 보조 모듈 호출과 결과 합성만 담당한다. 후보 생성, 점수/위험 검증, 예약, 방향 유지, 쿨다운과 속도 배율은 Micro 모듈이 소유한다. 주 Action의 결과가 이미 이동 중이면 Micro는 즉시 비활성화되므로 같은 tick에 별도 PathFollowing 요청으로 주 경로를 덮어쓰지 않는다. Cover의 Hide/Peek 이동 중에도 Micro를 실행하지 않는다.

## 6. 이동속도 배율 관리 소유 클래스

`UDMCombatTacticsComponent`가 단일 소유자다. Pawn이 바뀔 때 원래 `CharacterMovement.MaxWalkSpeed`를 `BaseWalkSpeed`로 한 번 캡처한다. 각 요청은 `EDMMovementSpeedSource` 키로 등록·갱신·해제한다. 사망, 비활성화, 전투 종료 및 Reset 시 요청을 모두 지우고 캡처한 기준 속도로 복원한다.

Roam/Approach/HuntLost의 `UDMBehaviorModule_PathMove`도 `MaxWalkSpeed`를 직접 쓰지 않고 같은 Brain API의 `Roam` 소스를 사용한다. 프로젝트 검색 결과 DeathMatch AI의 직접 `MaxWalkSpeed` 대입은 Brain의 공통 적용 및 원복 두 지점만 남았다.

## 7. 최솟값 계산 방식

매 갱신에서 유효한 활성 요청을 enum 순서로 확인한다.

`FinalMovementMultiplier = Min(ActiveMovementSpeedMultipliers)`

`FinalMaxWalkSpeed = BaseWalkSpeed * FinalMovementMultiplier`

예를 들어 Action 0.80, ADS 0.65, Firing 0.45이면 최종 배율은 0.45, 기준 속도 600이면 최종 속도는 270이다. 배율끼리 곱하지 않는다. 1.0은 제한 없음이며 0.0은 의도적인 완전 정지다. 동일 소스는 `TMap`의 같은 enum 키를 갱신하므로 중복 등록되지 않는다.

## 8. 제거 또는 Deprecated 처리한 고정 속도 설정

다음 직렬화 프로퍼티 이름은 BP 호환을 위해 삭제하지 않고 `Combat|Legacy` + Deprecated로 보존했다. 신규 로직에서는 사용하지 않는다.

- Brain: `CombatMaxWalkSpeed`, `CoveringMoveSpeed`, `StrafeFireMoveSpeed`, `FastRepositionSpeed`, `EmergencyCoverSpeed`, `PeekMoveSpeed`, `InvestigationMaxWalkSpeed`
- Fire Action: `MaxMoveSpeed`, `MaxFireMoveSpeed`
- Brain 내부 구형 Micro 설정 전부

`PathMove.MaxWalkSpeedOverride`도 Deprecated다. 구 BP에 양수 값이 저장되어 있으면 기준 속도 대비 0~1 배율로 변환하여 호환 적용한다. Fire Action의 구 절대 최대 속도는 더 이상 이동속도나 이동 사격 허용 여부를 제한하지 않고, `bAllowFireWhileMoving`만 정지 사격 정책으로 유지된다.

## 9. Blueprint에 새로 노출된 설정

- 모든 Combat Movement Action: `Combat|Movement|Policy`
  - Allow ADS
  - Allow ADS While Moving
  - Allow Fire
  - Allow Fire While Moving
  - Allow Fire Before Arrival
  - Clear ADS On Start
- 모든 Combat Movement Action: `Combat|Movement|Speed Multipliers > Movement Speed Multiplier`
- 모든 Combat Movement Action: `Combat|Movement|Selection > Selection Score`
- DM Combat Brain: `ADS Movement Speed Multiplier`, `Firing Movement Speed Multiplier`
- DM Combat Brain: `Micro Movement Auxiliary`
- DM Combat Brain: `Allow Emergency Hiding Without Module` (기본 false)
- DM Combat Brain: current Action 유지 보너스와 전환 점수 margin
- Hiding: `Concealed Reevaluation Delay`
- PathMove 계열: `Movement Speed Multiplier`

기본 배율은 Roam 0.75, Approach 0.85, HuntLost/Investigation 0.80, Fixed Approach 0.85, KeepDistance/Retreat/AttackPosition/CombatCover 0.80, Strafe/Micro 0.75, Rush/Hiding 1.0, ADS 0.65, Firing 0.45다. BP에 저장된 인스턴스는 에디터에서 실제 표시와 값 상속을 확인해야 한다.

## 10. BP에서 직접 확인하거나 다시 지정할 항목

1. Controller BP의 **DM Combat Brain** 컴포넌트에서 `Movement Action Priority 1/2/3` 중 원하는 슬롯에 **Hiding**을 추가한다.
2. Hiding을 추가하지 않고 `Allow Emergency Hiding Without Module=false`로 두면 AI는 실제 Hiding을 선택하지 않는다. Combat Cover나 Retreat가 Hiding으로 암묵 승격되지 않는다.
3. 기존 Escape Cover가 슬롯에 있으면 이미 Hiding 등록으로 취급되고 자동으로 Hiding 의미를 사용한다. 신규 Hiding 클래스로 교체는 권장하지만 필수 migration은 아니다.
4. Micro Movement가 필요하면 DM Combat Brain의 `Micro Movement Auxiliary`에 **Micro Movement (Auxiliary)**를 직접 지정한다. 구 `bAllowMicroMoveWhileFiring` 값만으로는 활성화되지 않는다.
5. 각 주 Action/보조 Action 인스턴스의 `Movement Speed Multiplier`를 확인한다.
6. ADS/Firing 배율은 DM Combat Brain의 `Combat|Movement|Speed Multipliers`에서 설정한다.
7. Roam/Approach/HuntLost는 해당 PathMove Behavior 컴포넌트의 `Movement Speed Multiplier`에서 설정한다.
8. 기존 Fire Action의 `MaxMoveSpeed`/`MaxFireMoveSpeed`는 Deprecated이므로 새 속도 튜닝에 사용하지 않는다. 정지 사격 여부는 `bAllowFireWhileMoving`으로 설정한다.
9. Hiding/CombatCover/MoveThenFire/Rush/Retreat/Micro의 새 정책 기본값과 저장 BP override 여부를 Details에서 확인한다.

## 11. 선택, 전환 및 실패 처리

등록된 세 주 Action 슬롯만 평가하며 같은 객체가 여러 슬롯에 들어가면 가장 앞 슬롯에서 한 번만 평가한다. 실행 불가 Action은 제외한다. 점수는 Action `SelectionScore`에 슬롯 보너스를 더해 비교하고, 현재 Action에는 유지 보너스를 적용한다. challenger가 `MovementSwitchScoreMargin` 이상 이기지 못하면 현재 Action을 유지하여 프레임 단위 플리핑을 줄인다. 정확한 동점은 앞 슬롯이 이긴다. Hiding은 긴급 상황에서 낮은 슬롯에서도 승리하도록 높은 기본 생존 점수를 갖는다.

`Investigate Last Known Position`은 이미 `UDMCombatMovement_Investigate` Action으로 후보 생성·실행·완료/실패를 소유하고 있어 별도 신규 migration 없이 유지했다. 다만 threat-memory 수명과 진입은 Brain 책임이므로 일반 가시 타깃 슬롯에는 넣지 않았다.

## 12. 디버그 정보

상세 Combat Debug에 다음을 추가/정리했다.

- 선택 Action과 선택 점수
- 슬롯별 점수, 실행 가능 여부, 실패 이유
- Hiding은 compact action에서 `HIDE`, Combat Cover는 `COVER`
- ADS/Fire 허용 상태
- Base/Final MaxWalkSpeed, 최종 배율, 제한 소스
- 활성 배율 소스와 각 값
- Micro 보조 모듈 등록/활성 및 후보 점수

핵심 줄 형식은 `Move=... | ADS=Allowed | Fire=Allowed` 및 `Speed Base=... Final=... Mult=... Limiter=... | Active Mult: ...`다.

## 13. 남아 있는 레거시 및 알려진 제한 사항

- Blueprint 직렬화 보존을 위해 기존 클래스명과 절대 속도/Brain Micro 프로퍼티 이름을 삭제하지 않았다.
- `EscapeCover` 호환 래퍼, PreparationCover, 기존 Fire Action의 절대 속도 필드는 남아 있다. 절대 속도 필드는 신규 계산에는 참여하지 않는다.
- 실제 Blueprint 로드/저장, Details 패널 표시, 기존 인스턴스의 새 기본값 상속 여부는 미확인이다.
- `SelectionScore` 기본값과 슬롯 보너스는 기존 우선순위를 최대한 보존하도록 설정했지만 실제 전투 감각은 PIE 튜닝이 필요하다.
- 발사 종료 후 별도 hold/fade 시간은 추가하지 않았다. Firing 배율은 실제 Fire Ability 활성 상태가 끝나는 다음 Brain 갱신에서 제거된다.
- 외부 상태 이상은 enum 소스와 API만 제공하며 실제 producer는 이번 범위에서 연결하지 않았다.

## 14. 후속 개선 권장사항

- BP에서 Hiding과 Micro를 명시적으로 등록한 뒤 Details 직렬화와 기본값을 확인한다.
- PIE에서 Hiding 이동 중 Ability 취소, ADS 해제, 은폐 도착 후 재평가 지연을 확인한다.
- Action/ADS/Firing 배율을 조합하여 최솟값과 즉시 원복을 확인한다.
- 사망, UnPossess, 전투 이탈, Roam 재진입에서 BaseWalkSpeed 복원과 Roam 배율 재등록을 확인한다.
- 향후 부상/상태 이상은 `ExternalStatus`를 세분화한 enum source로 확장할 수 있다.

## 검증 상태

- 정적 참조 검색: 완료.
- DeathMatch AI의 `MaxWalkSpeed` 직접 대입: DM Combat Brain 공통 배율 적용/기준 속도 원복 2개 지점만 존재함을 확인.
- Hiding 선택 경로: 등록 슬롯 또는 명시적 `bAllowEmergencyHidingWithoutModule` 경로만 존재함을 확인.
- Micro PathFollowing: 주 Action이 `bMove=true`이면 보조 모듈이 실행되지 않고, 실제 `MoveToLocation` 발행은 기존 Brain 공통 경로 하나만 사용하는 것을 확인.
- 제거한 Brain Micro 런타임 필드 참조와 충돌 마커: 없음.
- 중괄호 수 및 generated include 위치의 정적 점검: 이상 없음.
- 신규 native 자동화 테스트 소스는 최소 배율 선택/소스 제거와 정책 기본값을 추가했으나 실행하지 않았다.
- 최초 작업 시 사용자 지시대로 전체 에디터 빌드, UHT/컴파일, Blueprint 테스트, PIE, BP 로드/직렬화 테스트를 생략했다.
- 이후 사용자 제공 빌드 로그에서 56개 액션 중 `DMCombatThreatMemory.cpp`의 C4706(조건식 내 대입) 한 건이 빌드를 중단시킨 것을 확인했다. Hiding 모듈 탐색 시 `Cast` 대입과 null 검사를 별도 문장으로 분리해 수정했다.
- 같은 로그의 나머지 항목은 DeathMatch 외부 코드의 경고였으며 이번 범위에서는 변경하지 않았다.
- 위 수정 이후 전체 에디터 재빌드와 자동화 테스트는 실행하지 않았다. 따라서 최종 컴파일 성공, Blueprint 호환 및 런타임 동작은 후속 빌드/PIE 확인이 필요하다.
