# DeathMatch BT 기본 행동 기반 — 2026-10-15

Root Behavior Tree는 내부 상태를 읽고 행동 의도와 실행 시점만 선택한다. 감지/기억/현재 타겟은 Combat Knowledge, 체력·사망·위험·행동 조건은 Combat Status, 발사 가능성과 cadence는 Fire Control이 소유한다. BT Service는 이 값을 Blackboard에 복사하거나 Branch 수명 동안 실행 요청을 유지할 뿐 상태를 재계산하지 않는다.

- Task: SelectCombatTarget, DisengageCombat, SelectTacticalArea, SelectRoamLocation, MoveToGoal
- Branch Service: ActionIntent, MaintainFocus, MaintainFire
- Blackboard 동기화 Service: UpdateAIState, UpdateCombatBlackboard, UpdateHealthRisk, UpdateFireState
- Decorator: IsDead, HasVisibleTarget, HasRememberedTarget, HasValidCombatTarget, IsEmergency, ShouldRetreat, NeedsCombatCover, CanFire, ShouldInvestigate, ShouldDisengage

ActionIntent는 None/Roam/Observe/Attack/AttackMove/FastReposition/CombatCover/Hide/Retreat/Investigate다. BT에는 enum만 노출되고 ADS는 AimZoom, 실제 속도 배율은 Movement Speed Policy, 사격/이동 사격 허용은 Fire Control이 해석한다. Branch 종료/Abort 시 요청 ID로 해당 Branch가 만든 Intent, Focus, Fire 효과만 제거하며 이전 요청이 있으면 복원한다.

Attack Branch의 사격 실행과 수명은 `DM Maintain Fire` Service 하나가 소유한다. Service는 Branch 진입 시 요청 하나를 만들고, Fire Ability가 비활성인 동안 같은 요청으로 재시도하며, Branch 종료/Abort 시 자신이 만든 요청만 해제한다. `DM Start Fire`, `DM Stop Fire`, `DM Activate Ability` Task는 에셋 참조 제거 후 삭제했다.

Combat Knowledge는 후보와 감지 기록만 갱신하고 자동으로 CombatTarget을 선택하지 않는다. SelectCombatTarget Task가 Target Selection Policy를 명시적으로 실행한다. DisengageCombat은 Fire/ADS/Focus/전투 이동/위치 예약/CombatTarget만 정리하고 적 기억과 피해 기록은 보존한다.

전투 위치 선택과 EQS는 이번 단계에서 확장하지 않았다. Tactical Area 기반 Roam Task는 유지한다. 실제 Root BT와 Blackboard uasset 구성은 에디터 후속 작업이다.
