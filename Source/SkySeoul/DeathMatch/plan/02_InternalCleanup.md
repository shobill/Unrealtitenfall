# DeathMatch 정리 범위와 잔존 항목 — 2026-09-17

Supervisor, SingleShoot, ActionBase/MessageManager, LocalMessage payload/callback 및 미사용 BT Service/Decorator를 엔진 참조 조사 후 제거했다. Sense의 직접 Framework 통지는 유지했다.

삭제 제외 항목:

- BP_DM_AttackModule의 부모 DMBehaviorModule_Attack.
- BP_DM_Brain_Combat이 참조하는 Hold/Approach/HuntLost Behavior.
- 두 Controller BP가 직렬화한 Approach/HuntLost Provider. Director 등록에서는 제외하지만 native subobject와 저장 설정을 보존한다.
- BT_Combat의 SelectMoveLocation/FaceTarget/ActivateAbility.
- 범위 밖 Config/DefaultGameplayTags.ini의 AI.Perception 태그. 사용자 변경을 보존한다.
- Sense 기반 AIModuleBase, 사망용 FDeathMessagePayload, 현행 Situation enum/snapshot.

후속 후보: 저장된 구형 BP 참조의 명시적 마이그레이션 후 호환 클래스 제거, 두 Controller의 SCS/instanced policy 값을 보존하는 상속 전환, 화면을 보며 하는 장시간 PIE 확인.

기능 추가·Fire Ability 분해·GameInstance 이름 변경·Adaptive 기본화는 이번 범위에 포함하지 않는다.
