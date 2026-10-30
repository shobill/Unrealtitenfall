# DeathMatch 제거 전 참조 조사 결과

- 조사일: 2026-09-17 KST
- 구분: 실제 제거 전 조사 결과 및 범위 확정
- 기준: 기존 DeathMatch 파일 해시가 2026-09-16 작업 시작 사본과 동일함을 재확인.

## 확인 근거

Unreal Asset Registry 동기 전체 검색: 15,623개 에셋. `/Script/SkySeoul` 의존 패키지 및 DeathMatch 패키지를 합친 359개 패키지를 엔진 LoadPackage로 열고 FLinkerLoad import table 1,963개를 조사했다. 프로젝트 외부 폴더의 BP/맵과 마운트된 플러그인도 Registry 검색 범위에 포함한다. Architecture.AssetImports 테스트 성공, 패키지 로드 실패 없음. C++/Config/플러그인 코드 검색을 병행했다. 문자열 검색만으로 삭제하지 않는다.

원시 증거: `Saved/DMArchitectureAudit/registry-before.json`, `imports-before.tsv`, `Saved/Logs/DMArchitectureImports.log`.

## 제거 확정

- DMSituationSupervisor: 클래스 import 0, 외부 C++ 참조 0.
- AIActionModule_SingleShoot, AIModuleMessage, LocalGameplayMessageManager: 각각 import 0, 외부 C++ 참조 0.
- AIActionModuleBase, AIActionRequest, LocalMessageType, LocalMsg_Actors, LocalMsg_Actor, LocalMsg_FocusChanged, DMSituationCommand: import 0. 구형 계층만의 C++ 사용을 함께 제거한다.
- BTService_DM_UpdateCombatState, BTDecorator_DM_CanShoot, BTDecorator_DM_HasTarget: 각각 import 0, 외부 C++ 참조 0.
- Sense의 LocalMessage 방송, 주입 API, callback 및 FAIModuleTags C++ 구현은 수신 계층과 함께 제거한다. Config/DefaultGameplayTags.ini의 AI.Perception 태그는 범위 밖 사용자 변경이므로 유지한다.

## 유지 확정

- DMBehaviorModule_Attack: `/Game/DeathMatch/AI/ActionModule/BP_DM_AttackModule`의 부모 클래스.
- DMBehaviorModule_Hold, Approach, HuntLost: `/Game/DeathMatch/AI/BrainModule/BP_DM_Brain_Combat`에 저장된 AllowedBehaviorClasses 참조.
- DMMoveGoalProvider_Approach, HuntLost: Precision 및 Adaptive Controller BP에 각각 import. 호환용 클래스와 저장 설정을 유지하며 Director의 현행 선택 경로에서는 제외한다.
- BTTask_DM_SelectMoveLocation, FaceTarget, ActivateAbility: `/Game/DeathMatch/AI/BT/BT_Combat` import 확인. 삭제하지 않는다.
- AIModuleBase는 Sense의 부모이므로 빈 공통 컴포넌트 기반으로 유지한다.
- FDeathMessagePayload는 현행 사망 메시지용이므로 유지한다.
- DMSituationSnapshot, EDMSituationChangeReason 및 직렬화 enum 값은 현행 API/호환성 때문에 유지한다.

제거 후보의 DeathMatch 외부 클래스 참조는 발견되지 않았다. BT_Combat는 범위 밖 BB_MobBB를 참조하므로 해당 Blackboard는 수정하지 않는다. BT_Combat import에 BTTask_MoveTo는 없으며, 현행 Roam 이동 실행은 PathMove의 직접 MoveTo 경로다.

## Controller BP

두 Controller BP의 부모는 모두 `/Script/SkySeoul.DeathMatchRobotAIController`. 기존 Precision은 BP_GM_DeathMatch가 참조하고 Adaptive의 Registry 참조자는 없다. 기존 맵/기본 Controller 및 사용자 설정을 보존한다. 상속 전환은 SCS와 기본값을 추가 확인한 뒤 결정한다.

## 검증 상태

조사 코드 포함 SkySeoulEditor Development 빌드 성공. Registry 스크립트 성공(0 오류/경고). Import 테스트 성공. 아직 런타임 정리 및 변경 후 검증을 실시하지 않은 시점의 보고서다.
