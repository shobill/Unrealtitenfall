# Adaptive Fire 구현 결과 — 2026-09-16

최종 SkySeoulEditor Win64 Development 빌드 성공. `SkySeoul.DeathMatch.Combat` 자동화 24/24 통과, 종료 코드 0. 원본 Controller의 저장된 Precision 설정을 보존했고 Adaptive는 명시적으로 선택하는 방식이다.

## 적용 방법과 에셋

- 기존 Controller: `/Game/DeathMatch/AI/Controller/BP_DeathMatchAIController` — `UseExistingModule`, 저장된 `DMCombatFire_Precision` 유지.
- 새 Controller: `/Game/DeathMatch/AI/Controller/BP_DeathMatchAIController_Adaptive` — Fire Mode를 Adaptive로 명시 저장한 복사본. 사용하려는 AI에 이 Controller를 지정하거나 기존 Controller의 DMCombatTactics > Fire Mode를 Adaptive로 선택한다. 기존 맵/GameMode의 Controller 선택은 변경하지 않았다.
- 수정한 Ability BP: `/Game/DeathMatch/AI/Abilities/BP_DM_AI_Fire` — Fire Montage를 새 AI 전용 몽타주로 연결, FireCycleNotifyName=FireCycle 저장.
- 새 Montage: `/Game/DeathMatch/AI/Abilities/AM_DM_AI_AdaptiveFire`.
- 원본 Montage: `/Game/SuperBotPackVol2/Etasphera02/Demo/Montage/AM_02_Fire2` — 수정하지 않음.
- 원본 1초 애니메이션과 Fire 시간 유지. DM Named Notify(`UAnimNotify_DMNamed`) Fire=0.0677초, FireCycle=0.1200초. 순서: 시작 → Fire → 반동 → FireCycle → 기존 Recovery/End.
- MontageStartSection=None: Default Section(0초)에서 시작. Slot=DefaultSlot. 원본 Sequence=Etasphera02_Atack3, RootMotion=false.
- 에셋 변경 전 백업: `Saved/DMAssetBackups/20260916_152253_AdaptiveFire`.
- 마이그레이션 스크립트는 기존 FireCycle을 중복 추가하지 않는다. 에셋 자동화 테스트가 기존 Precision, 새 Adaptive, Notify 개수·순서·시간을 검증한다.

## 구조와 설정

`UDMCombatFire_Adaptive`는 기존 FireModule 인터페이스를 구현한다. CurrentPattern, 선택 거리, 성공 탄 수, RemainingBurstShots, NextShotTime, NextBurstAllowedTime, CycleState, CycleId, Notify 수신 여부, 약한 Target/Weapon 참조, enum 실패 원인을 저장한다. 문자열은 진단에만 사용한다.

컴포넌트 Fire Mode: UseExistingModule / Precision / Burst / Automatic / Adaptive. 기본 UseExistingModule은 기존 Automatic 기본 서브오브젝트 및 BP에 저장된 다른 모듈을 보존한다. 명시한 새 모드는 BeginPlay에서 한 번 선택한다. 실행 중 Fire Mode 변경 API는 추가하지 않았다.

컴포넌트의 인스턴스형 Adaptive Settings에서 다음을 편집한다.

- RangeMode: UseWeaponRangeRatios(기본), UseCustomDistances.
- AutomaticRangeRatio=0.40, BurstRangeRatio=0.85.
- AutomaticMaxDistance=800, BurstMaxDistance=1700, SingleMaxDistance=0(무기 최대 사용).
- AutomaticShotInterval=0.12초, MaxAutomaticShots=0(무제한), AutomaticPause=0.65초.
- BurstShotCount=3, BurstShotInterval=0.16초, BurstPause=0.65초.
- SingleShotPause=0.90초.
- PatternDistanceHysteresis=100cm.
- AllowFireWhileMoving=true, MaxFireMoveSpeed=600cm/s.

활성 Weapon Actor의 사거리 데이터를 사용한다. Adaptive 경계에서는 EffectiveRange를 0 이상, 최대를 EffectiveRange 이상으로 보정한다. 비율은 0≤Automatic≤Burst≤1. Effective=2000, Max=3000이면 경계는 800/1700/3000cm. 최대 밖에서는 패턴과 관계없이 차단한다. 기존 Weapon Actor의 확률·피해 감쇠 함수와 기존 getter는 변경하지 않았다.

사용자 지정 거리는 최대를 무기 최대 이하로 제한한 후 Automatic/Burst를 0≤Automatic≤Burst≤Single 관계로 Clamp한다. 잘못된 사용자 지정 설정은 모듈당 경고를 한 번 출력한다. NaN/음수 거리도 안전하게 보정한다.

패턴은 Ability 시작/발사 경계에서 선택한다. 점사 도중에는 그룹을 유지한다. Automatic/Burst/Single 경계에 Hysteresis를 적용한다. 무기·타깃 변경 시 이전 Ability를 종료하고 새 컨텍스트에서 재선택한다. Tactics Tick에서 사거리 한계는 갱신하지만 패턴을 매 Tick 바꾸지 않는다.

## 발사 승인, 반복과 실패

- 첫 사이클과 다음 사이클 시작 시 Tactics의 이동·속도·Aim·settle·facing·sensed·LOS·CanShoot·사거리·cadence·확률 조건을 검사한다.
- 통과한 확률은 해당 한 발 Fire까지 보존한다. Fire에서는 사망, 타깃/무기 변경, 최대 사거리, CanShoot, 실제 muzzle 사선과 아군 위험을 재검사한다. 승인 이후의 일시적인 이동/Aim 변화로 같은 한 발의 확률을 다시 굴리지 않는다.
- Fire Notify만 ExecuteFire와 FDMShotResult/OnShotResolved 경로를 실행한다. Adaptive는 한 사이클에 한 번만 처리한다. 실제 성공에만 FireModule::ShotFired와 bloom/cooldown을 반영한다.
- 기존 scalar FireCooldown(0.2초)은 Adaptive에서 패턴별 cadence로 대체한다. 플레이어/기존 모듈 경로는 그대로다. 선택적 GAS Cooldown GameplayEffect는 첫 성공 발사 시 한 번 적용한다. 현재 AI BP의 해당 Effect는 None이다.
- Single은 한 발 후 종료. Burst는 실제 성공 횟수만 계산하여 3발 후 BurstPause. Automatic은 성공마다 간격을 갱신하고 MaxAutomaticShots=0이면 계속 반복한다. 양수이면 지정 성공 횟수 후 AutomaticPause.
- FireCycle은 실제 탄·피해·발사용 Trace를 실행하지 않는다. 완료 여부를 확인하고 SetTimerForNextTick으로 예약한다. 다음 Tick에서 cadence가 남으면 같은 단일 타이머 핸들로 남은 시간만 기다린다. 이후 기존 Task/Notify/Timer를 정리하고 같은 Montage를 재생한다.
- 계속되는 Burst/Automatic은 하나의 Fire Ability 안에서 반복한다. 매 성공 탄마다 BT 재활성화에 의존하지 않는다. BTTask_DM_ActivateAbility는 변경하지 않았다.
- Fire/FireCycle 둘 다 작성되어 있고 올바른 순서여야 한다. 누락 시 Adaptive를 종료하고 진단한다. 몽타주 종료 시 가짜 FireCycle 또는 가짜 발사를 만들지 않는다.
- 기존 authored Notify 시간 기반 서버 타이머와 런타임 Notify를 지원한다. 타이머에는 CycleId를 캡처하고 런타임 이벤트에는 Montage Instance ID를 검사한다. 이전 사이클 콜백을 무시한다.
- 재생 예약은 bCycleQueued와 단일 CycleTimer로 중복을 방지한다. Notify 구독은 재바인딩 전 해제한다. Task delegates는 제거한 뒤 EndTask한다. 다른 Ability의 몽타주를 중단하지 않도록 ASC의 현재 재생 Ability/몽타주를 확인한다.
- 종료/취소 시 Fire timers, CycleNotifyTimer, CycleTimer, SafetyTimer와 Notify delegate를 정리한다. 약한 Actor 참조를 사용한다.
- 활성 Adaptive에는 0.03초 안전 감시 타이머가 있다. 사망·타깃 소실·사선 차단·아군·사거리·무기 변경·Controller 상실 시 종료한다. 성공 발사 이후에는 이동 정책/속도 상실도 감시한다.
- ExecuteFire 실패는 성공 횟수·그룹 pause·정상 cooldown을 소비하지 않는다. 실패 종료 후 0.075초 anti-spin 간격만 적용한다. 실제 실패 enum과 마지막 원인을 남긴다.

## 이동, Aim, 디버그, 성능

기존 Movement Result.bAllowFire 및 Attack Position 정책과 Adaptive 속도 제한을 모두 만족해야 새 사이클이 시작된다. Cover Hidden/MovingToHide/Escape 등 기존 이동 모듈이 차단하는 사격은 허용하지 않는다. 기존 AI 이동 spread 패널티를 유지한다.

Adaptive에서는 안정 상태로 복귀, 무기 변경, 큰 방향 변경에 AimReadyTime을 갱신한다. 타깃 변경은 기존 재초기화 흐름을 사용한다. 허용 속도 안의 작은 이동은 deadline을 계속 뒤로 밀지 않는다.

dm.CombatDebug 2 상세 HUD에는 모드·패턴·선택 거리·범위 방식·경계·성공 횟수·점사 잔여·사이클 상태/ID·Notify 수신·다음 발사/점사 시간·실패 원인·이동 허용/속도·무기/사거리를 표시한다. 추가된 HUD 부분은 캐시를 읽으며 확률 roll/Trace를 실행하지 않는다. 기존 Debug.Observation 테스트에 Adaptive 캐시 표시와 난수 순서 검증을 연결했다.

매 Tick UObject 생성이나 매 발 전체 Actor 검색은 없다. 모듈은 컴포넌트 인스턴스이며 Montage Task만 사이클별로 생성·정리한다. 안전 검사는 현재 타깃과 무기에 한정한다. 추가된 상세 진단은 Shipping에서 문자열 포맷 인수까지 생성하지 않도록 call-site guard를 적용했다.

## 변경한 C++ 및 스크립트

아래 경로는 `Source/SkySeoul/DeathMatch/` 기준이다.

- 신규 `AI/Combat/DMCombatFire_Adaptive.h`, `.cpp`.
- 수정 `AI/Combat/DMCombatTacticsComponent.h`, `.cpp`.
- 수정 `Abilities/DMGameplayAbility_Fire.h`, `.cpp`.
- 수정 `Abilities/AnimNotify_DMNamed.h`, `.cpp`.
- 수정 `Abilities/DMFireAttempt.h`.
- 수정 `AI/Combat/DMCombatDebug.h`, `DMCombatDebugFormat.cpp`, `DMCombatTacticsDebug.cpp`, `DMCombatDebugTests.cpp`.
- 신규 `AI/Combat/DMAdaptiveFireTests.cpp`.
- 신규 `Tests/DMAdaptiveFirePIETests.cpp`, `Tests/DMAdaptiveFireTestAbility.h`.
- 테스트 fixture 접근용 friend 선언만 추가: `AIModules/AIModule_Sense.h`, `Weapons/DMWeaponLoadoutComponent.h`.
- 신규 `Tests/inspect_adaptive_fire.py`, `Tests/configure_adaptive_fire.py`.

기존 Precision/Burst/Automatic 구현, 플레이어 입력 정책, 사거리별 확률 및 피해 계산, 이동/엄폐 시스템의 동작은 재설계하지 않았다. 기존 보고서와 작업 시작 전에 존재하던 다른 변경은 덮어쓰지 않았다.

## 검증 결과

최종 빌드: SkySeoulEditor Win64 Development, UE 5.6.1, 성공.

최종 실행 명령: UnrealEditor-Cmd.exe SkySeoul.uproject -unattended -nop4 -nosplash -NullRHI "-ExecCmds=Automation RunTests SkySeoul.DeathMatch.Combat; Quit" "-TestExit=Automation Test Queue Empty" -log.

전체 24/24 성공, 테스트 종료 2026-09-16 15:42:32 KST, exit code 0. 최종 로그는 `Saved/Logs/AdaptiveFire_Combat_Final.log`에 보존.

새 Adaptive 테스트 6개:

- AssetMigration: 기존 Precision 보존, 새 Adaptive 선택, Fire/FireCycle 개수와 순서·시간.
- RangeAndHysteresis: 기본/사용자 거리·잘못된 값 보정·3패턴·최대 사거리·경계 흔들림.
- SuccessCadence: Single 종료·Burst 성공 탄 수/휴지·무제한 및 제한 Automatic·간격 제한·이동 속도.
- NotifyLifecycle: 활성 FireCycle은 발사를 만들지 않음, 예약 중복·동기 재진입 방지, delegate 해제, 실제 예약 timer들의 종료 정리.
- RealShotSafetyAndAuthorization: 실제 Fire 실행·중복 금지·승인 유지·ExecuteFire 실패 주입과 비소비·사망·사거리·muzzle 차단·아군·타깃 상실·무기 변경·unpossess.
- PIEPatterns: 실제 Warehouse 맵, 실제 ASC/Montage/Notify, 8개 Controller를 런타임에서 세 패턴으로 순차 설정. 최종 관찰: Automatic 한 그룹 최대 8발, Burst 3발, Single 1발. 맵/BP에 테스트 설정을 저장하지 않음.

기존 18개 Combat 회귀도 성공: Weapon Range/Authorization/FireIntegration, 기존 실제 맵 Fire.PIE, Cadence/Movement, AttackPosition, Cover, Debug, Parts 테스트들.

테스트 시나리오는 여러 assertion으로 묶었으므로 테스트 이름 24개가 요청 체크 항목 30개와 일대일 대응하는 숫자는 아니다. 예약 실행의 실제 Montage 반복은 PIEPatterns에서 관찰했고, 안전 원인들은 별도 결정적 월드 fixture에서 검증했다. 모든 수동 시나리오를 시각적으로 수행했다는 의미는 아니다.

## 미검증 및 남은 한계

- NullRHI 자동 PIE이므로 반동 연결의 자연스러움, 하체 이동/상체 사격 블렌딩, HUD 시각 배치는 수동 PIE 검증 필요.
- 기존 DefaultSlot을 유지했다. 별도 UpperBody Slot/AnimBP 레이어 재작성은 하지 않았다.
- 별도 Client/Dedicated Server 프로세스에서의 네트워크 검증 및 Shipping 빌드는 수행하지 않았다. 서버 권위 경로와 dedicated-server authored Notify timer는 유지한다.
- ShotInterval은 성공한 발사 이후의 최소 준비 간격이다. 다음 Montage 시작부터 Fire까지 약 0.0677초와 프레임 지연이 더해질 수 있어 정확히 0.12초/0.16초당 한 발을 보장하는 발사율 설정은 아니다. 1초 몽타주 전체 종료를 기다리는 지연은 제거했다.
- 원본 Controller를 쓰는 기존 맵은 기존 Precision 동작을 유지한다. Adaptive 적용은 위의 전용 Controller 또는 Fire Mode 선택으로 명시적으로 수행해야 한다.
- 에셋/소스에는 작업 시작 전부터 존재하던 미추적 변경이 많다. 이 작업은 해당 파일들을 git add/commit하지 않았다.
