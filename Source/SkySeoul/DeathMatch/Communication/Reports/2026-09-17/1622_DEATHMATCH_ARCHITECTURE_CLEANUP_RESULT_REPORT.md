# DeathMatch 구조 정리 결과 보고서

- 작성일: 2026-09-17 16:22 KST (최종 검증 반영 16:26)
- 범위: Source/SkySeoul/DeathMatch, Content/DeathMatch
- 구분: 구현 및 검증 결과

## 결론

CombatTactics가 교전 이동·엄폐·공격 위치·조준·사격 승인의 최종 소유자다. SituationFramework는 Roam, 전투 진입/이탈 및 명확한 정지 fallback으로 축소했다. 저장 BP가 참조하는 구형 Behavior는 유지하되 AI마다 생성하지 않는다. 참조 없는 구형 메시지/단발 사격/Supervisor 계층과 BT 노드 3종을 함께 제거했다.

기존 사용자 변경을 기준 사본으로 보존하여 비교했다. Content/DeathMatch의 27개 파일은 모두 작업 시작 사본과 동일하다. 맵 기본 Controller, Precision/Adaptive BP 값, BT 에셋을 변경하지 않았다. Fire Ability, 무기 데이터, 전투 위치 선정 점수, 엄폐 정책 및 파츠 구현도 변경하지 않았다.

## 제거 전 참조 조사

실제 제거 전에 1050_DEATHMATCH_ARCHITECTURE_REFERENCE_REPORT.md로 범위를 확정했다. Unreal Asset Registry 전체 검색은 15,623개 에셋을 대상으로 했다. DeathMatch 패키지와 /Script/SkySeoul 의존 패키지의 합집합 359개를 Unreal LoadPackage로 열어 FLinkerLoad import 1,963개를 조사했다. 변경 후 객체 직렬화의 soft reference 502개도 조사했다. C++/Config/플러그인 참조 검색을 병행했으며 문자열 검색만으로 삭제하지 않았다.

- UDMSituationSupervisor, UAIActionModule_SingleShoot, UAIModuleMessage, ULocalGameplayMessageManager: 각각 에셋 class import 0, DeathMatch 외부 C++ 사용 0.
- UBTService_DM_UpdateCombatState, UBTDecorator_DM_CanShoot, UBTDecorator_DM_HasTarget: 각각 에셋 class import 0, 외부 C++ 사용 0.
- UDMBehaviorModule_Attack: BP_DM_AttackModule 부모이므로 유지.
- UDMBehaviorModule_Hold, Approach, HuntLost: BP_DM_Brain_Combat의 AllowedBehaviorClasses에 저장되어 있으므로 유지.
- UAIActionModuleBase, FAIActionRequest, LocalMessageType/LocalMsg_Actors/LocalMsg_Actor/LocalMsg_FocusChanged, DMSituationCommand: 저장 import 0이며 구형 계층 내부 사용뿐이어서 함께 제거.
- Approach/HuntLost MoveProvider: 두 Controller BP의 import에 존재하므로 native subobject와 직렬화 설정을 유지. Director의 현재 registry에는 Roam만 등록한다.
- SelectMoveLocation/FaceTarget/ActivateAbility: 실제 BT_Combat에 모두 존재하므로 유지.
- 외부 연결: BT_Combat가 /Game/A_DevFolder/Characters/Enemy/Mob/AI/BB_MobBB를 사용한다. 외부 Blackboard는 열람만 했다. Config/DefaultGameplayTags.ini의 AI.Perception 태그 5개도 범위 밖이라 유지했다.

제거 대상의 변경 후 soft reference는 0이다. 359개 패키지 로드에 실패·Missing Class 오류가 없었다. 참조가 있는 클래스 자체를 유지했으므로 근거 없는 대체 클래스 Core Redirect는 추가하지 않았다.

근거 파일: Saved/DMArchitectureAudit/registry-before.json, imports-before.tsv, registry.json, imports.tsv, soft-references.tsv, topology.txt. Audit 자동화 코드는 Tests/DMArchitectureAssetAuditTests.cpp 및 inspect_architecture_registry.py에 남겼다.

## 실제 삭제한 파일과 타입

다음 경로는 Source/SkySeoul/DeathMatch 기준이며 총 15개 파일이다.

- AI/Situation/DMSituationSupervisor.h, .cpp
- AIModules/AIActionModule_SingleShoot.h, .cpp
- AIModules/LocalGameplayMessageManager.h, .cpp
- AIModules/AIModuleTags.h, .cpp
- AIModules/AIModuleBase.cpp
- AI/BT/BTService_DM_UpdateCombatState.h, .cpp
- AI/BT/BTDecorator_DM_CanShoot.h, .cpp
- AI/BT/BTDecorator_DM_HasTarget.h, .cpp

남은 AIModuleBase.h에서 UAIModuleMessage, UAIActionModuleBase, FAIActionRequest/EAIActionState와 구형 채널 API를 제거했다. Sense 부모 UAIModuleBase는 유지한다. DeathMatchMessageStructure.h에서 구형 LocalMessage payload만 제거하고 현행 FDeathMessagePayload는 유지했다. DMSituationTypes.h에서 Supervisor 전용 Command와 사용되지 않는 공격 경과 시간을 제거했다. Sense의 LocalMessage 방송, 구형 수신 callback/tag 코드를 같은 변경으로 제거했다.

변경 파일 전체 목록은 Saved/DMArchitectureAudit/change-manifest.json에 작업 시작 사본 대비로 기록했다. Git에서 기존 DeathMatch 파일이 untracked였으므로 git diff만으로 사용자 변경 여부를 판단하지 않았다.

## 변경 전후 런타임 흐름

이전에는 Sense 직접 알림과 LocalMessage 계층이 공존하고, Framework의 Combat Brain/Behavior 메뉴가 전투 행동을 별도로 판단·생성했다. PathMove의 직접 MoveTo와 Blackboard 목표 발행도 같은 이동을 표현했다.

현재 소유 관계는 다음과 같다.

1. Sense: 대상 감지·적대 필터·Seen/Known/Awareness·focus 기억과 Framework 알림.
2. SituationFramework: Roam Behavior 하나 생성, 비전투 판단, Tactics 실행에 따른 진입/이탈. Tactics 실행 불가 상태에서 보이는 적이 있으면 정지·목표 취소·사격 금지 fallback.
3. CombatTactics: 교전 정책 선택, 위치 예약, MoveTo, 엄폐/공격 위치, 조준·회전, 이동 중 사격 허용 및 최종 승인.
4. BT: 승인에 따라 행동 실행. FaceTarget은 교전 중 Tactics 조준을 덮어쓰지 않는다.
5. GameplayAbility_Fire: 몽타주·Notify·실제 발사/명중·cadence와 종료 처리.
6. Weapon Actor: 사거리·확률·피해·spread 데이터.

Roam은 Framework → Behavior_Move/PathMove → Director → Roam Provider → 직접 MoveTo다. 전투 진입 시 Tactics가 Roam 목표 및 Blackboard HasMoveGoal을 철회한다. Framework는 교전 중 전투 Behavior/BB를 다시 적용하지 않는다. MoveCompleted 동기 재진입을 막고, 새 Roam MoveTo를 발행한 뒤 정지시키던 순서를 바로잡았다.

SelectMoveLocation은 유지하며 Framework가 존재하는 경로에서는 병행 BB 목표를 만들지 않는다. Framework 없는 호환 경로의 기존 구현은 남겼다. 실제 BT_Combat는 Selector 아래 FaceTarget/ActivateAbility Sequence와 SelectMoveLocation Sequence이며 엔진 BT MoveTo 노드는 없다.

사망·소유 해제·리스폰 시 Sense의 perception 및 대상 기억, Tactics 전투 상태, Framework 결정/목표를 함께 초기화한다. 강화한 PIE 테스트가 이전 대상 기억으로 리스폰 즉시 재교전하는 문제를 발견하여 ResetAwareness를 연결했다.

## BP 및 BT 변경 사항

에셋 수정 없음. 두 Controller BP는 모두 native DeathMatchRobotAIController의 직접 자식이다. 둘의 로컬 SCS DMCombatTactics는 같은 복제 GUID 594056F54411A50917BB5CAC7A770582를 갖는다. 단순 재부모화는 상속 컴포넌트와 로컬 컴포넌트의 중복/식별 충돌을 만들 수 있고, 기존 instanced movement/fire 설정의 명시적 이전이 필요하다. 사용자 설정을 추측해 합치지 않고 이번에는 복제 구조를 유지했다.

기존 BP_GM_DeathMatch는 Precision Controller를 참조한다. Adaptive의 Registry 참조자는 0이다. 기본값 선택이나 기존 맵의 Adaptive 전환은 하지 않았다. 자동 PIE에서 Adaptive 검증을 위해 GameMode CDO를 메모리에서만 임시 설정하고 PIE 초기화 직후 원래 클래스로 복구한다. 저장하지 않는다.

## 문서

plan 4개와 Framework/Situation/Move/BT/Controller/Abilities LOGIC_STATUS 및 AIModules/LOGIC_PERCEPTION을 현재 구조로 갱신했다. 이전 11개 문서는 Communication/Archive/Legacy-2026-09-17/에 원문 보관했다.

## 검증 상태

- SkySeoulEditor Win64 Development: 16:23 빌드 성공, exit 0. UHT/compile/link 성공. Saved/DMArchitectureAudit/build-final.txt에 출력 보관.
- Combat 전체 자동화: 최종 26/26 성공, 실패 0, exit 0 (16:25 종료). 기존 24개를 유지하고 Architecture.Ownership/PIEControllers 2개를 추가하여 총 26개다. 제거한 테스트는 없다.
- 최종 로그: Saved/Logs/DMArchitectureCombat-final26.log. 집계: Saved/DMArchitectureAudit/validation-final.json. Adaptive 패턴 최대 연속 성공은 2/3/1회였고, Precision 실제 맵은 로봇 8개 및 실제 발사를 관찰했다.
- 최종 Architecture PIE 11,850개 관찰 표본: Roam 이동, Combat 진입, 접근/후퇴/공격 위치/엄폐 이동, 같은 발사 tick의 이동 중 사격 모두 확인. 각 AI의 Behavior는 Roam 하나이며 HasMoveGoal 충돌 표본 0. 대상 소실 이탈 및 사망·리스폰 assertions도 성공.
- 진단 로그의 실제 MoveOwner 발행 274건(Roam 55, Tactics 219)을 AI/전체 frame 번호로 묶어 같은 AI·같은 frame의 중복 요청 0을 확인했다. AutomationController가 재출력한 로그는 집계에서 제외했다. 관찰 구간에서 Framework/Tactics 동시 BB 이동 소유 충돌 없음. 이는 모든 미래 입력에서의 충돌 부재를 수학적으로 보장하는 것은 아니다.
- 별도 Architecture.AssetImports: 성공. 변경 후 359개 패키지 로드와 import/soft reference 조사, Missing Class 또는 삭제 타입 로드 오류 없음. Saved/Logs/DMArchitectureImports-final.log.
- 실제 Warehouse 맵의 자동 PIE는 NullRHI로 실행했다. 원래 Precision Controller에서 실제 ASC/몽타주 Notify/발사를 확인하고, 별도 실행에서는 실제 Adaptive BP를 스폰하여 발사를 확인했다.
- Adaptive 패턴 검사는 Automatic 한 그룹 2회 이상, Burst 정확히 3회, Single 정확히 1회라는 기존 조건을 유지했다. 처음 12초 고정 구간에서 감지 시작 지연으로 Automatic을 놓치는 실패가 있어 단계별 성공 또는 30초 timeout 후 전환하도록 검사 시간을 고쳤다. 마지막 Single은 12초 동안 중복 발사가 없는지도 검사한다.
- 새 PIE 검사는 첫 30초 동안 원래 Adaptive 설정을 관찰하고, 30초 이후 일회성 PIE 인스턴스의 첫 movement priority에 기존 PreparationCover를 배치해 엄폐 이동을 검증한다. 엄폐 점수/검색/사격 수치 및 저장 BP는 바꾸지 않는다.
- 25초에는 기존 사망 알림 및 팀 리스폰 경로로 상태 초기화를 검증한다. 강화 과정에서 Sense 잔존 기억으로 즉시 재교전하는 실패를 발견해 production 코드를 수정했고, 이후 검사가 통과했다.
- 대상 소실 검사는 일회성 PIE에서 추적 대상을 제거하고 남은 Sense 기록을 비워 전투 이탈/Roam 복귀를 검사한다. 이는 가림막 뒤에서 자연적으로 기억 시간이 만료되는 모든 Hunt 상황을 검증하는 것은 아니다.
- 초기 FireIntegration의 구형 Brain 사격 승인 기대값은 새 소유 관계에 맞게 수정했다. Brain이 승인하지 않는 것과 Tactics 이동 사거리 경계를 함께 검증한다.
- 기존 CommonUI viewport 설정 오류는 원래 PIE 테스트와 동일하게 expected error로 처리한다. 해당 외부 설정은 이번 작업에서 변경하지 않았다.

## 미검증 및 후속 후보

- NullRHI 자동 PIE이므로 렌더링·애니메이션의 육안 품질과 실제 플레이 감각은 미검증이다.
- 네트워크/Photon, 패키징/Shipping, 모든 맵과 모든 사용자 BP 조합을 검증한 것은 아니다.
- Adaptive의 안전한 상속 전환은 SCS/instanced 설정을 보존하는 별도 migration 검증 후 진행할 후보이다.
- 호환용 Attack/Hold/Approach/HuntLost와 두 MoveProvider는 저장 BP 참조를 명시적으로 이관한 뒤 제거할 수 있다.
- 범위 밖 Config의 잔여 AI.Perception 태그는 별도 범위에서 정리할 후보이다.
