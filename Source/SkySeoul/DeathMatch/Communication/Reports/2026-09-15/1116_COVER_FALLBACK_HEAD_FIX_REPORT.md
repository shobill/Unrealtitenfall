# 엄폐 실패 대체 행동 및 머리 분리 설정 수정 결과

- 작성일: 2026-09-15 11:16 KST
- 범위: DeathMatch 전투·디버그·캐릭터 코드, DeathMatch BP/Data Asset, 공용 PartsComponent의 비 Shipping 진단 로그
- 구분: 구현 결과 및 런타임 진단
- 근거 지시: 사용자가 첨부한 두 문제 수정 요청과 작업 재개 요청
- 선행 보고서: `Reports/2026-09-11/1701_COVER_AND_HEAD_SEPARATION_REPORT.md`

## 결론

엄폐 실패의 확정 원인은 전체 탐색 실패 예산·쿨다운 부재와 실제 Controller BP의 빈 Priority 2·3 조합이었다. 전체 탐색 2회 실패 후 5초간 Cover Unavailable로 전환하고, 실제 BP의 2순위에 기존 Maintain Distance 모듈을 설정했다. 후보 분할 검사 중에도 이 대체 모듈이 실행될 수 있다. 대체 행동의 발사 허가는 기존 최종 `AllowsFire()`까지 전달되며 기존 사격 조건은 유지된다.

머리는 사용자가 수정한 Data Asset과 실제 BP의 설정 연결이 달랐다. 변경 전 BP의 Pawn.PartsSetupData와 컴포넌트 SetupData는 None이었다. BP를 테스트 월드에 생성해 네이티브 파츠 초기화·파괴 경로를 실행하자 기본 정의가 적용되었고, 머리 행은 1개, Separable=true, Mesh 없음, HP=40, Impair=true였다. Data Asset의 Head.Separable=false가 런타임에 적용되지 않는 원인은 확인했다.

다만 변경 전 재현에서 실제 물리 Debris는 0개였다. 일반 머리 파괴와 사망 폭발 모두 Mesh가 없는 기본 정의에 의해 머리 Bone 숨김 경로를 탔다. 사용자가 목격한 “실제 물리 분리” 전체 증상을 PIE에서 재현한 것은 아니다. 이 점은 설정 불일치의 확정 사실과 구분한다.

Robot BP의 Pawn.PartsSetupData를 DA_Parts_DM_Robot으로 연결했다. 수정 후 실제 파츠 초기화 로그에서 Default=0, HeadRows=1, Separable=0, Mesh=Part_Parts_Head를 확인했다. 일반 파괴에서는 HP 파괴·머리 기능 손상은 발생하고 Debris는 0개였으며, 사망 폭발에서는 비분리 머리를 건너뛰어 Debris가 0개였다. bSeparable=true인 머리·팔의 네이티브 분리 경로도 별도 회귀 테스트로 확인했다.

에디터 빌드 성공, `SkySeoul.DeathMatch.Combat` 자동화 테스트 14개 모두 성공. 실제 PIE 화면·AI 이동·발사·시각 효과는 PIE 검증 필요.

## 확인 근거와 재현 조건

### 변경 전 BP 및 Data Asset

- BP_DeathMatchAIController의 실제 SCS 컴포넌트 템플릿 `DMCombatTactics_GEN_VARIABLE`:
  - Priority 1: DMCombatMovement_CombatCover
  - Priority 2: None
  - Priority 3: None
- BP_DeathMatchAIRobot:
  - Pawn.PartsSetupData=None
  - 컴포넌트 템플릿 SetupData=None
  - 기본 DeathMatch 정의 fallback 옵션=true
- DA_Parts_DM_Robot:
  - Parts.Head 행 1개
  - bSeparable=false
  - PartMesh=/Game/SuperBotPackVol2/Etasphera02/Mesh/Separated/SM_Etasphera02_Head.SM_Etasphera02_Head
  - FollowBoneName=head, ParentTag=None
  - MaxHP=50, bImpairHeadOnDestroy=false
- 저장된 Robot BP 그래프를 Unreal Exporter로 내보내 조사했다. ReceiveBeginPlay/ReceiveActorBeginOverlap/ReceiveTick의 비활성 노드와 빈 Construction Script가 있으며, BP_OnPartDestroyedFX/BP_OnDeathPartsExplosionFX 구현이나 Detach/SimulatePhysics/SpawnActor 호출은 없었다. 부모는 네이티브 DeathMatchRobotCharacter다. 다른 배치 인스턴스나 다른 BP까지 확인했다는 의미는 아니다.
- 읽기 전용 확인 로그: `Saved/Logs/DMAssetInspect2.log`, 그래프 export: `Saved/DM_BP_DeathMatchAIRobot.copy`, `Saved/DM_BP_DeathMatchAIController.copy`.

### 런타임 경로 확인

`Parts.AssetAudit`는 실제 Robot BP를 별도 Game World에 생성하고, APartsCharacterBase::BeginPlay와 같은 Pawn.PartsSetupData 우선 할당을 한 뒤 InitializeParts를 호출한다. 관련 없는 UI·멀티플레이 초기화를 피하기 위해 전체 BeginPlay/PIE 세션을 실행하지는 않는다. 일반 머리 HP 파괴와 남은 파츠 사망 폭발을 새 Pawn별로 분리 실행했다.

- 변경 전 로그: `Saved/Logs/DMCoverPartsBefore.log`, 2026-09-15 11:07 KST.
  - Setup=None, Default=1, HeadRows=1, Index=0, Separable=1, FollowBone=head, Parent=None, PartMesh 없음.
  - 일반 파괴: 실제 DestroyPartInternal Index=0 / Cause=PartDamage / Mesh=None, Destroyed=1, DebrisDelta=0, HeadIntact=0.
  - 사망 폭발: 실제 DestroyPartInternal Index=0 / Cause=DeathExplosion / Mesh=None, Destroyed=1, DebrisDelta=0, HeadIntact=0.
- 변경 후 로그: `Saved/Logs/DMCoverPartsAfter.log`, 2026-09-15 11:13 KST.
  - Setup=/Game/DeathMatch/AI/Character/DA_Parts_DM_Robot.DA_Parts_DM_Robot, Default=0, HeadRows=1.
  - 머리 Index=0, Separable=0, FollowBone=head, Parent=None, Impair=1. InitializeParts DONE definitions=7.
  - 일반 파괴: 실제 DestroyPartInternal / Cause=PartDamage / Mesh=Part_Parts_Head, Destroyed=1, DebrisDelta=0, HeadIntact=0.
  - 사망 폭발: HeadSeparable=0 / SkipDirectExplosion=1, Destroyed=0, DebrisDelta=0, HeadIntact=1.

일반 파괴에서 bSeparable=false인 Mesh를 숨기는 기존 동작은 유지했다. “비분리”는 물리 Debris를 만들지 않는다는 의미이며, 파괴된 머리를 계속 보이게 하는 새 규칙을 추가하지 않았다.

## 엄폐 구현과 기본값

- MaxSearchFailures=2: 개별 후보나 Tick이 아니라 36개 전체 후보 검사를 마친 실패만 누적한다. 검사 중에는 진행 수를 표시한다.
- FailureCooldown=5초: 실패 예산 소진 뒤 전체 탐색을 중단하고 bCanExecute=false를 반환한다. 최적 후보가 완료 시점에 무효화된 경우도 완전 탐색 실패로 센다.
- ThreatResetDistance=300cm: 현재 실패 문맥의 위협 위치에서 이 거리보다 크게 이동하면 재탐색한다. 부분 탐색의 무효화 기준도 같은 값을 사용한다.
- CandidatesPerUpdate=4, SearchInterval=1.5초, 기존 Tactics 기본 Tick 0.1초는 유지했다. 이동하지 않는 위협과 정기 갱신 기준 9번의 업데이트가 한 탐색이고, 두 번 실패는 대략 3.1초의 기회를 준다. 호출 빈도·프레임 지연에 따라 실제 시간은 다를 수 있다.
- 쿨다운 해제: 만료, 타깃 변경, 의미 있는 위협 이동, 명시적 Reset. 단순 Tick이나 하위 모듈 전환은 실패 이력을 지우지 않는다.
- 모듈 선택 변경 때 호출하던 Reset을 Deactivate로 구분했다. 일반 모듈의 Deactivate 기본 동작은 기존 Reset과 같다. Cover는 비활성화 시 실패 예산·부분 탐색·쿨다운을 보존하고, 확보한 엄폐가 있다면 예약만 해제한다.
- 이동 제한시간 초과는 즉시 Unavailable로 전환한다. 기존 Combat Cover의 TravelTimeoutSeconds=12초는 유지한다. 엄폐 무효화·도착 위치 무효화도 그 판단에서 즉시 하위 우선순위에 넘긴다.
- 유효한 엄폐의 Hide → Peek → Exposed → Fire 및 Escape 정상 실행 중 발사 금지 정책은 유지한다. 사거리·발사 확률·피해 감쇠 계산은 수정하지 않았다.
- 기존 HUD/월드 Debug의 공유 스냅샷에 슬롯별 검사 후보 수, 실패 횟수, Unavailable, 쿨다운 잔여시간, 양보 이유를 넣었다. 활성 모듈과 최종 Movement AllowFire도 표시한다. 대체 모듈이 활성화되어도 1순위 엄폐의 실패 상태가 보인다.

## BP 및 Data Asset 변경

- `Content/DeathMatch/AI/Controller/BP_DeathMatchAIController.uasset`
  - Priority 1: 기존 Combat Cover 유지.
  - Priority 2: None → 기존 DMCombatMovement_KeepDistance(Maintain Distance).
  - Priority 3: None 유지.
  - C++ 전역 기본값을 강제로 바꾼 것이 아니라 확인한 BP의 빈 슬롯에 인스턴스 모듈을 저장했다.
- `Content/DeathMatch/AI/Character/BP_DeathMatchAIRobot.uasset`
  - Pawn.PartsSetupData: None → DA_Parts_DM_Robot.
  - APartsCharacterBase::BeginPlay에서 해당 Pawn 필드가 컴포넌트 SetupData에 적용된다. 컴포넌트 템플릿의 SetupData는 재로드 시 None으로 확인되지만 실제 초기화에서는 Pawn 필드가 우선한다.
- `Content/DeathMatch/AI/Character/DA_Parts_DM_Robot.uasset`
  - Head.bImpairHeadOnDestroy: false → true. 기존 fallback 머리 파괴의 기능 손상을 보존한다.
  - Head.bSeparable=false 유지. HP=50, PartMesh, FollowBoneName, ParentTag 및 팔 행의 기존 값은 수정하지 않았다.
  - 연결 수정으로 런타임이 fallback HP=40 대신 사용자가 저장한 HP=50을 사용하게 된다. HP 수치를 임의로 편집한 것은 아니다.
- 변경 전 백업: `Saved/DMAssetBackups/20260915_110918/`. 첫 저장에서 Controller BP를 저장한 뒤 읽기 전용 PartsComponent 포인터를 설정하려다 중단했으며, 해당 포인터 설정을 제거하고 두 번째 실행에서 나머지 변경을 완료했다. 최초 백업에 세 에셋의 수정 전 파일이 보관되어 있다.
- 새 프로세스로 재로드하여 Priority 1/2/3과 Pawn.PartsSetupData 및 머리 행을 확인했다. 근거: `Saved/Logs/DMAssetAfter.log`. 특정 에셋만 저장했으며 Save All은 사용하지 않았다.

## 공용 코드 및 추가 발견

공용 `Source/SkySeoul/PartsCharacter/PartsComponent.cpp`에는 `dm.PartsDiag` 콘솔 변수와 비 Shipping 진단 로그만 추가했다. 일반 파괴·사망 폭발·Detach·HP·피해 규칙은 변경하지 않았다.

실제 DestroyPartInternal 내부가 참조한 Definition Index와 원인, MeshComponent를 확정하려면 그 함수 내부 계측이 필요하므로 DeathMatch 외부 공용 파일 한 곳에 관찰 로그를 넣었다. 기본값 0에서는 출력하지 않고, 1일 때 초기화·머리 파괴·사망 처리 이벤트만 기록한다. 다른 PartsComponent 사용자도 명령이 켜져 있으면 같은 진단 로그를 출력할 수 있으며 게임 동작에는 영향이 없다. Shipping에서는 컴파일 제외한다.

실제 Robot BP 생성 테스트 중 별도의 `Template Mismatch during attachment` 오류도 발견했다. 중첩된 BodySensor/HeadSensor의 부모가 런타임 AimTarget이 아니라 BP 템플릿 AimTarget으로 남는 경우였다. DeathMatch의 DMAimTargetComponent::OnRegister에서 이 잘못된 템플릿 부모만 런타임 부모로 보정했다. 정상 런타임 부모, 사용자 상대 위치와 명시적 Mesh/Bone 연결은 유지한다. 이 오류를 머리 물리 분리의 원인으로 주장하지 않는다. 최종 BP 생성 테스트에서 부모가 템플릿이 아님을 확인했고 기존 ensure도 사라졌다.

## 변경 파일

DeathMatch 코드 경로 기준: `Source/SkySeoul/DeathMatch/`.

- `AI/Combat/DMCombatCoverModule.h`, `DMCombatCoverModule.cpp`: 실패 예산, Unavailable, 쿨다운, 위협 문맥 및 비활성화 처리.
- `AI/Combat/DMCombatModules.h`: Deactivate 수명 훅.
- `AI/Combat/DMCombatTacticsComponent.cpp`: 모듈 전환 시 Deactivate 사용.
- `AI/Combat/DMCombatTacticsComponent.h`, `AIModules/AIModule_Sense.h`: 해당 회귀 테스트를 위한 friend 접근. 런타임 Sense 로직 변경 없음.
- `AI/Combat/DMCombatDebug.h`, `DMCombatTacticsDebug.cpp`, `DMCombatDebugFormat.cpp`: 엄폐 실패 스냅샷과 HUD/월드 공유 표시.
- `AI/Combat/DMCoverFallbackTests.cpp` (신규): 실패·전환·쿨다운·최종 사격 허가 회귀.
- `Character/DMAimTargetComponent.h`, `DMAimTargetComponent.cpp`: BP 센서의 잘못된 템플릿 부모 연결 보정.
- `Tests/DMPartsSeparationTests.cpp` (신규): 실제 BP 설정·파괴 진단 및 머리/팔 분리 회귀.
- `Tests/inspect_cover_parts.py` (신규): Unreal Python 읽기 전용 BP/DA 검사 및 그래프 export.
- `Tests/fix_cover_parts_assets.py` (신규): 확인된 세 에셋의 백업·범위 제한 수정 스크립트.
- 공용 `Source/SkySeoul/PartsCharacter/PartsComponent.cpp`: 비 Shipping 이벤트 진단 로그만 추가.
- 위 BP 2개와 Data Asset 1개.
- 이 신규 결과 보고서. 기존 보고서·지시서·과거 결과 문서를 덮어쓰지 않았다.

## 검증 상태

- `SkySeoulEditor Win64 Development`: 2026-09-15 11:12 KST 빌드 성공, 종료 코드 0.
- `UnrealEditor-Cmd -unattended -nop4 -nosplash -NullRHI`에서 `Automation RunTests SkySeoul.DeathMatch.Combat` 실행: 2026-09-15 11:13 KST, 14개 모두 Success, 종료 코드 0.
- 로그: `Saved/Logs/DMCoverPartsAfter.log`.
- 앞선 실행에서는 새 BP AssetAudit가 센서 템플릿 부모 ensure로 실패했다. 오류를 숨기거나 예상 오류로 처리하지 않았고, 위 런타임 부모 보정 뒤 전체를 재실행해 성공했다.
- 신규 `Cover.Fallback`: 4/36 부분 탐색, 36개 전체 실패 누적, 두 번째 실패 예산 소진, 1.5초 재시도 간격, 쿨다운 중 반복 탐색 금지, Deactivate로 쿨다운 보존, 2·3순위 선택, 만료/타깃 변경/위협 이동/Reset 해제, 이동 timeout 즉시 양보.
- 같은 테스트에서 준비한 Perception 관찰과 실제 전투 context/LOS 경로로 대체 이동의 허가가 최종 AllowsFire=true까지 전달됨을 확인했다. Movement 허가를 false로 바꾸면 최종 사격이 차단됨도 확인했다. 실제 몽타주 발사 시각까지 검증하는 테스트는 아니다.
- 신규 `Parts.AssetAudit`: 실제 저장된 BP 로드·생성, 런타임 센서 부모, 실제 DA 경로, 단일 Head 행, false 분리 설정·기능 손상, 일반 HP 파괴와 사망 폭발의 Debris 0개 확인.
- 신규 `Parts.Separation`: 머리/왼쪽 팔 상부 × bSeparable false/true × 일반 HP 파괴/사망 폭발의 8개 조합. false에서는 Debris 0개·원래 부착 부모 유지·물리 비활성, true에서는 실제 네이티브 Debris Actor 1개 생성. 일반 머리 파괴의 기능 손상은 분리 옵션과 독립적으로 유지됨을 확인했다. 안정적인 물리 회귀를 위해 Cube Mesh와 격리된 파츠 상태 fixture를 사용했다.
- 기존 11개 전투 테스트도 모두 성공: Cover.Phases, Cover.Visibility, Debug.Gates, Debug.Observation, Debug.ShotPayload, Debug.WorldModes, Weapon.Authorization, Weapon.FireIntegration, Weapon.Range, Cadence, Movement.
- 유효 엄폐의 기존 phase 흐름은 기존 Cover.Phases/Visibility 회귀가 담당한다. 실제 NavMesh·개방 공간에서 AI가 움직이며 사격하는 화면은 PIE 검증 필요.
- BP/DA 수정 명령은 최종 종료 코드 0. Python SubobjectData get_object의 deprecation 경고 1개가 있었고 저장·재로드 검증은 성공했다. 최종 C++ 자동화에는 기존 테스트 월드 설정 관련 Warning이 있으나 실패·ensure는 없었다.

## 사용 및 PIE 확인 절차

1. 플레이 전에 `dm.PartsDiag 1`을 켜고 Robot을 새로 생성한다. 로그에서 Owner/Component, Setup 경로, Default=0, HeadRows=1, Separable=0을 확인한다. 완료 후 `dm.PartsDiag 0`으로 끈다.
2. `dm.CombatWorldDebug 2` 또는 `dm.CombatDebug 2`로 실제 활성 모듈과 Cover P1 후보 수/실패 횟수/Unavailable/재시도 잔여시간을 확인한다.
3. 엄폐가 없는 공간에서 2순위 Maintain Distance와 AllowFire YES가 보이고, 다른 Fire Gate도 만족하면 사격하는지 확인한다. 쿨다운 중 P1이 계속 선점하지 않는지 확인한다.
4. 엄폐 가능한 공간에서 Hide → Peek → Exposed → Fire, Escape 정상 실행 중 발사 금지 흐름을 확인한다.
5. Head.bSeparable=false로 일반 머리 HP 파괴와 캐릭터 사망을 각각 새 Pawn에서 확인한다. 일반 파괴에서는 기존처럼 머리 Mesh가 숨겨질 수 있으나 물리 Debris가 없어야 한다.
6. 검수용으로 Head.bSeparable=true를 적용한 새 Pawn에서 일반 파괴/사망 분리를 확인한 후 원하는 설정으로 되돌린다. 이번 저장 결과는 false다.
7. 타깃 교체, 300cm 초과 위협 이동, 쿨다운 만료, 리스폰에서 엄폐 실패 상태가 적절히 초기화되는지 확인한다.

## 남은 위험과 미검증 사항

- PIE 미실행. 실제 화면의 물리 분리·Bone 숨김 구분, NavMesh 이동, 총구·몽타주 및 FX 연출은 PIE 검증 필요.
- 변경 전 실제 BP 생성 경로에서는 물리 분리 자체를 재현하지 못했다. 다른 배치 인스턴스의 SetupData override나 다른 BP를 사용하는 경우는 별도로 확인해야 한다.
- DA의 기존 팔 ArmEffectOnDestroy=None, ParentTag=None 등은 수정하지 않았다. 올바른 DA 연결로 이 저장값도 이제 적용되므로, 기존 fallback의 팔 기능 손상·연쇄 파괴를 의도했다면 DA 설정 검수가 필요하다. 요청 범위를 넘어 팔 정책을 임의로 덮어쓰지 않았다.
- Actor의 PartsComponent 포인터가 None인 BP 상태에 대한 기존 BeginPlay 복구 경로는 유지한다. 이번 테스트는 컴포넌트를 검색해 같은 SetupData 우선 규칙으로 네이티브 초기화를 실행했다.
- 테스트 월드에서는 렌더 프레임·전체 BeginPlay·멀티플레이를 실행하지 않았다. 파츠 fixture는 보호된 런타임 상태를 reflection으로 준비하므로 해당 프로퍼티 구조 변경 시 테스트도 갱신해야 한다.
- 별도 Shipping 패키징, Dedicated Server/Client 실행은 미검증이다. 진단 코드는 비 Shipping 조건으로 격리했다.
