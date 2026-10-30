# DeathMatch Fire Ability — 2026-09-17

UDMGameplayAbilityBase는 CanShoot와 대상 조회를 제공한다. UDMGameplayAbility_Fire는 실제 발사 사이클의 소유자다.

CombatTactics가 사격을 승인하고 Blackboard/BT ActivateAbility가 GAS를 활성화한다. Fire는 몽타주 및 Fire/FireCycle Notify에 맞춰 발사 trace·명중·파츠 피해·cadence를 처리한다. Adaptive의 반복/묶음/단발 경계도 기존 Notify 경로를 사용한다. 거리·확률·피해·spread 데이터는 활성 Weapon Actor에서 읽는다.

Supervisor/SingleShoot/LocalMessage callback에 의존하지 않는다. 이번 구조 정리에서 Fire Ability 구현·무기 밸런스를 변경하지 않았다. 기존 정적 FireRange 중심 설명은 Archive에 보존했다.


2026-09-22: 활성 Combat Brain의 확정 Combat Target을 대상 조회와 CycleSafety 비교에 사용한다. Sense Focus 변경은 진행 중인 기존 타깃 사격을 교체하지 않는다. Brain이 Visible이 아니거나 감지/LOS가 없으면 숨은 타깃 조준/거리 읽기 전에 사격을 차단한다. 타깃 상실은 GAS 취소를 통해 기존 EndAbility의 모든 Timer/Notify delegate/Montage Task를 정리한다. 공통 Runner/Notify 시점/무기 수치는 그대로이며 견제 사격은 구현하지 않았다. 빌드/PIE/BP/자동화 미실행.
