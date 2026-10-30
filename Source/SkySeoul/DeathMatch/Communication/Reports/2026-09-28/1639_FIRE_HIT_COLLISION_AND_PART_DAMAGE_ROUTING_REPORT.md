# DeathMatch 사격 충돌 및 파츠 데미지 라우팅 결과

- 작성: 2026-09-28 16:39 KST
- 구분: 구현 및 소스 정적 검증. 실제 게임 증상 재현/해결 검증은 미실행.
- 범위: Source/SkySeoul/DeathMatch 및 필요한 PartsCharacter 코드만 변경.
- Editor 실행, 전체 빌드, UHT, BP 컴파일/로드, PIE, Unreal 자동화 실행: 모두 미실행.

## 1. 결론과 사전 확인

PartsComponent가 초기화 시 Capsule Ignore, Owner Skeletal Mesh Query+Block, 부착 시각 파츠 NoCollision을 설정한다. 파괴된 파츠의 남은 Owner PhAT 재명중은 파츠 HP를 건드리지 않고 기존 part-hit body scale로 본체 피해를 적용한다. Bone이 없는 몸통을 가장 가까운 파츠로 바꾸던 경로도 제거했다.

프로젝트 및 DeathMatch 하위, 적용 가능한 상위 경로에서 AGENTS.md를 찾지 못했다. .cursor/rules의 deathmatch-entry, combat-architecture, fire-design, workflow와 plan 4개 문서, Communication/00_README, 최신 1135_HEALTH_BASED_COMBAT_EVALUATION 및 1050_COMBAT_AI_REVIEW_IMPROVEMENTS 보고서를 읽었다. Fire, PartsComponent, PartsDamageManager, PartsCharacterBase, RobotCharacter, WeaponActor, 기존 파츠 분리/사격 테스트와 CharacterBase TakeDamage/OnHitEvent를 확인했다.

기존 사용자 변경이 다수 있으며 DeathMatch/PartsCharacter는 시작부터 untracked다. 기존 변경은 되돌리지 않았다. CharacterBase 등 외부 폴더는 읽기만 했고 수정하지 않았다. 기존 TakeDamage/CurHP API로 해결하여 외부 변경이 필요하지 않았다.

## 2. 기존 총알 충돌 흐름 — 코드에서 확인한 사실

- Fire TraceChannel과 Parts WeaponTraceChannel 기본값은 각각 ECC_GameTraceChannel1. Config의 이름은 HitTrace이고 채널 기본 응답은 Ignore다. Fire bTraceComplex 기본값은 false.
- Parts의 bConfigureOwnerMeshForWeaponTrace=true일 때 Capsule의 해당 채널을 Ignore, 원본 Mesh를 Block으로 설정했다. Mesh가 NoCollision일 때만 QueryAndPhysics로 바꾸므로 PhysicsOnly는 복구하지 못했다.
- Attached Static Mesh는 NoCollision profile로 등록 전후 설정. bAttachedPartsBlockTraces=true면 QueryOnly만 켜고 Ignore 채널 응답은 그대로 남겼다.
- Fire bIgnoreTargetPartMeshes=true면 목표 부착 파츠를 매 발 Ignore 목록에 추가했다. 사수 파츠도 actor ignore와 중복으로 추가했고 FireCycle 안전 ray에도 같은 사수 파츠 열거가 있었다.
- 기본 옵션에서는 원본 Skeletal Mesh의 PhAT가 최종 명중 소유자였다. HideOwnerBones의 HideBoneByName은 PBO_None을 사용하며 파괴 후 개별 PhAT BodyInstance disable/enable은 크래시 방지용 no-op이다.
- 실제 LineTraceSingleByChannel → BuildShotResult → ApplyFireDamage → PartsCharacterBase::ApplyHitDamage → PartsDamageManager → ApplyDamageToPart/ApplyBodyDamage → CharacterBase::TakeDamage 순서다.
- 기존 PartTag 해석은 BoneName → MyBoneName → 원본 Mesh FindClosestBone → Component였다. impact-aware 해석은 미등록 몸통 Bone의 자손 파츠도 후보로 삼을 수 있었다.
- 유효 파츠는 PartDamageScale과 BodyDamageOnPartHitScale(기본 0.5), 미해석 hit은 BodyDamageOnNoPartScale(기본 1)을 사용했다.
- 이미 파괴된 태그는 ApplyDamageToPart에서 즉시 반환하여 PartDamageApplied/BodyDamageApplied 모두 0이었다. 실제 잔존 PhAT가 총알을 막으면 0 피해 방패가 될 수 있는 코드 경로가 확인됐다.

## 3. 확인된 위험한 옵션 조합과 미확인 사항

bConfigureOwnerMeshForWeaponTrace=false + 원본 Mesh NoCollision/PhysicsOnly/Ignore + 부착 NoCollision 또는 QueryOnly/Ignore면 정밀 피격 컴포넌트가 없어진다. Capsule까지 BP 등에서 Ignore이면 실제 피격 대상 자체가 없을 수 있다. Capsule이 Block으로 남으면 반대로 Bone 없는 Capsule이 앞에서 맞을 수 있다. 채널 불일치도 같은 위험이 있다.

bAttachedPartsBlockTraces=true는 이름과 달리 필요한 Block 응답을 만들지 못했으며, bIgnoreTargetPartMeshes=true와도 의도가 충돌했다. 이 구조는 확인했지만 현재 BP가 위험한 조합으로 저장되어 있다는 뜻은 아니다.

Content/DeathMatch의 uasset과 Saved의 copy 파일에서 세 옵션 이름의 UTF-8/UTF-16 문자열을 검사했고 일치 항목이 없었다. C++ 생산 경로 외 부착 파츠 trace 사용 근거도 찾지 못했다. 이는 Asset Registry/BP load 검증을 대체하지 않으며 외부 플러그인/BP 전체의 사용 부재를 증명하지 않는다. 확인된 별도 호환 피격 모드가 없어 요청한 단일 PhAT 정책을 적용했다.

보이는 몸통의 특정 위치만 맞는 실제 현상은 PIE로 재현하지 않았다. 위 정책/라우팅 결함은 수정했지만 모델 PhAT의 공간적 커버리지, 애니메이션/숨김 후 실제 물리 형태가 추가 원인인지는 미확인이다.

## 4. Capsule 최종 정책

PartsComponent::ConfigureOwnerMeshForWeaponTrace가 WeaponTraceChannel만 Ignore로 만든다. 이동/Pawn 응답은 변경하지 않는다. Capsule Block fallback을 도입하지 않았다. 부착 준비를 기다리는 초기화에서도 이 공통 함수를 먼저 호출한다.

## 5. Owner Skeletal Mesh 최종 정책

같은 함수가 채널 Block과 Query 허용을 보장한다. 이미 Query가 있으면 collision mode를 유지한다. PhysicsOnly는 QueryAndPhysics, NoCollision은 QueryOnly로 변경한다. Physics Asset을 생성하거나 개별 body를 수정하지 않는다.

초기화 완료 시 개발 빌드에서 컴포넌트당 한 번 owner, mesh, skeletal asset, Physics Asset, query, channel, mesh/capsule 응답, 충돌이 켜진 시각 파츠 수 및 valid 상태를 출력하고 invalid면 Warning한다. 메시/애니메이션 준비 자체가 실패하면 기존 deferred-init 경고와 재시도 상한 경로를 사용하며 완료 로그는 나오지 않는다.

HideBoneByName(PBO_None), PhAT no-op 안전 주석과 분리 물리는 유지했다. 기존 Static Mesh 등록 시 BodyInstance의 비시뮬레이션 설정은 기존 코드이며 PhAT body 조작을 새로 추가한 것이 아니다.

## 6. Attached Part / Debris 최종 정책

부착 파츠는 등록 전후 NoCollision profile 및 NoCollision enabled를 명시한다. 옛 true 값도 효과가 없다. 분리 파편은 기존 PhysicsActor/QueryAndPhysics 및 응답/수명/임펄스를 유지한다. 생성 Actor에 Parts.DetachedDebris 태그를 추가해 진단과 Fire의 명시적 피해 거부에 사용한다. 기존 SetCanBeDamaged(false)도 유지한다.

Debris가 사선을 물리적으로 막는 기존 동작은 보존했다. 캐릭터 Owner를 가진 잔해라도 HitActor를 원본 캐릭터로 거슬러 올라가 피해를 적용하지 않는다. PartsDamageManager도 자기 Actor/자기 소유 component가 아닌 hit을 거부한다.

## 7. 중복 옵션 및 BP 호환

bAttachedPartsBlockTraces, bConfigureOwnerMeshForWeaponTrace, bIgnoreTargetPartMeshes의 이름, bool 타입, 기존 기본값과 UPROPERTY를 유지했다. AdvancedDisplay + DeprecatedProperty + DeprecationMessage + ToolTip에 무효화와 새 정책을 설명한다. 생산 cpp에서는 세 옵션을 참조하지 않는다.

Fire의 매 발 사수/대상 파츠 열거 및 FireCycle 안전 ray의 사수 파츠 열거를 제거했다. 사수 Actor와 부착 무기 Actor ignore는 유지한다. CollectAttachedPartComponents API는 테스트/도구 호환으로 유지한다. BP 로드 오류가 없음을 실제 검증한 것은 아니다.

## 8. TraceChannel 일치 검증

PartsHitCollision.h의 DefaultWeaponTraceChannel(ECC_GameTraceChannel1)을 양쪽 기본값에 공유한다. BP 저장 채널은 덮어쓰지 않는다.

개발 빌드의 실제 발사 준비 시 사수/명시적 목표, 실제 hit 처리 시 맞은 Actor의 PartsComponent에 Fire Ability의 실제 TraceChannel을 전달한다. 컴포넌트가 Fire/Parts 채널 쌍을 기억하므로 동일 owner와 같은 불일치 쌍은 Warning 한 번만 출력한다. 로그에는 Owner, FireAbility 경로, FireTraceChannel, PartsWeaponTraceChannel이 있다. dm.CombatDiag가 꺼져도 불일치 Warning은 유효하다. 매 Tick 검사나 월드 전체 탐색은 없다.

## 9. 몸통 hit 및 정상 파츠 hit

명시적인 component 매핑을 먼저 확인하고, 원본 Mesh에 맞았을 때만 Hit.BoneName으로 해석한다. MyBoneName과 FindClosestBone 보정은 제거했다. 미등록 몸통에서 자손 팔/머리를 고르지 않도록 impact-aware 후보 탐색 전에 bone 매핑 존재를 요구한다. 등록된 팔 체인의 impact-aware 세분화는 유지한다.

BoneName/PartTag가 없거나 태그에 대응하는 runtime 파츠가 없으면 no-part body scale 경로다. 실제 파츠가 있으면 기존 part + body scale을 유지한다. 직접 ApplyDamageToPart의 없는 태그 호출은 MissingPart로 0을 반환한다.

## 10. 파괴 파츠 재명중 및 실제 피해량

파괴된 태그라도 RoutedHit이 정확히 원본 Actor의 GetMesh()일 때만 본체 경로로 진행한다. PartDamage는 0, part HP는 0으로 유지하고 ApplyPartDamage를 호출하지 않아 OnPartDestroyed를 재발행하지 않는다. 본체는 BodyDamageOnPartHitScale을 사용한다. 직접 태그 API나 파괴된 시각 component hit에는 이 새 우회 경로를 열지 않는다.

BodyDamageApplied는 TakeDamage 반환 요청량이 아니라 실제 BeforeCurHP - AfterCurHP를 사용한다. bApplied도 실제 파츠 또는 본체 HP 감소가 있어야 true다. OnAppliedDamage는 동일 결과를 전달한다. OnHitEvent에는 실제 body 감소량을 전달하며 0이면 호출하지 않는다. Robot의 기존 combined event는 repeat에서 part=0이므로 실제 body만 Damage Awareness에 전달한다. 정상 파츠의 기존 합산 Awareness 의미는 바꾸지 않았다.

기존 FDMShotResult.DamageApplied는 두 HP pool의 합계라는 계약을 유지한다. 따라서 정상 파츠 hit의 total과 CurHP 감소량은 서로 다르며 BodyDamageApplied가 CurHP 감소량이다. 몸통/파괴 파츠 재명중은 part=0이므로 total도 CurHP 감소량과 같다. 파츠 상실로 발생하는 기존 강제 사망/연쇄 파괴는 별도 기존 효과이며 이 구현에서 재설계하지 않았다.

아군 차단 팀 판정은 기존 코드를 유지했다. invalid/non-finite/0 요청, 죽은 대상, 잔해, manager 없음, 외부 hit, 직접 파괴 태그, 없는 파츠 및 scale/실제 피해 거부를 점검하고 진단 사유를 부여했다. Friendly Fire는 Fire 경계에서 유지하며 공용 PartsDamageManager에 DeathMatch 팀 의존성을 추가하지 않았다.

## 11. dm.CombatDiag 1 로그

동일 FireAttempt id의 LineTrace, PartResolution, DamageRouting, ShotResult를 연결한다.

- LineTrace: hit, Actor, Component, Component Class, BoneName, ImpactPoint, start/end, channel, complex.
- PartResolution(피해 적용 전): resolved tag, runtime 존재, 이미 파괴 여부, ownerMesh/attachedPart/detachedDebris, bodyOnly, fallbackTrace=0.
- DamageRouting: requested, partApplied, bodyApplied, totalApplied, friendlyBlocked, zeroReason.
- ShotResult: base, distanceMultiplier, requested, totalApplied, miss 또는 DamageRouting 참조.

Trace miss는 PartResolution이 없고 ShotResult의 TraceMiss가 최종 이유다. 조기 반환은 InvalidTarget, InvalidOrZeroRequestedDamage, DetachedDebris, DeadTarget, FriendlyFireBlocked, MissingDamageManager, ForeignHitOrDebris, MissingPart, DestroyedPartWithoutOwnerMeshHit 및 scale/rejected 사유로 구분한다. 기존 발사 주기에서는 여러 발이 같은 FireAttempt id를 공유할 수 있으며 각 LineTrace→DamageRouting→ShotResult 묶음이 실제 한 발이다.

새 상세 문자열 포맷은 DM_FIRE_LOG의 non-Shipping/diag gate 안에 있다. 파츠 상태 추가 조회도 non-Shipping + diag gate 안이다. 진단은 read-only이며 추가 Trace, 랜덤, 피해 적용, 파츠 상태 변경을 하지 않는다. 불일치 경고의 중복 억제 cache만 관찰용 상태다.

## 12. 변경 파일

PartsCharacter:

- PartsHitCollision.h (신규): 공통 채널 및 debris 식별 태그.
- PartsComponent.h/.cpp: 단일 정책, 레거시 metadata, 초기화/채널 진단, 몸통 매핑 보호, debris 태그.
- PartsDamageManager.h/.cpp: 정확한 hit identity, 몸통/파괴 파츠 라우팅, 실제 피해량과 이유.

DeathMatch:

- Abilities/DMGameplayAbility_Fire.h/.cpp: 공통 기본 채널, 레거시 metadata, ignore 중복 제거, 채널 검사/발사 진단.
- Tests/DMHitRoutingTestTypes.h (신규): 테스트 ability/parts 및 destruction observer.
- Tests/DMHitRoutingTests.cpp (신규): HitRouting/HitCollisionPolicy 자동화 코드.
- Tests/DMPartsSeparationTests.cpp: authored visual NoCollision 검사, 직접 태그 재피해 테스트 의미 명확화.
- Tests/inspect_hit_routing.py (신규): 재실행 가능한 소스 감사.
- 이 보고서.

총 12개 파일. PartsCharacterBase, RobotCharacter, WeaponActor, CharacterBase, Config, Content 에셋은 이번 작업에서 수정하지 않았다.

## 13. 작성한 테스트 — Unreal 실행은 하지 않음

HitRouting: Bone 없음/미등록 몸통의 body-only 및 사용자 scale, 정상 파츠 part/body scale, 실제 파괴 후 owner Mesh 재명중 part=0/body>0, HP 0 유지, destruction 1회, 적용 이벤트 payload, 직접 destroyed-tag API 거부, body scale=0의 bApplied=false, 요청 0, friendly 차단/HP 보존, diag 0/1 동일 결과, 원본을 Owner로 가진 debris/foreign hit 거부, overkill 실제 HP delta, 사망 후 거부.

HitCollisionPolicy: legacy 두 bool의 true/false와 NoCollision/PhysicsOnly/QueryOnly 조합에서 query 복구, mesh Block, capsule Ignore, Pawn 응답 유지. 기본 Fire/Parts 채널 일치도 검사한다.

기존 Parts.AssetAudit에는 실제 authored BP 파츠를 생성한 뒤 legacy true에도 visual NoCollision인지 검사하도록 추가했다. 부착이 deferred 상태면 이 assertion은 실패하므로 실제 실행 시 asset 초기화 타이밍을 함께 점검해야 한다. 테스트가 성공했다고 주장하지 않는다. 실제 Physics Asset 개별 BodyInstance를 수정하는 테스트는 없다.

## 14. 수행한 정적 검증

inspect_hit_routing.py 실행 결과 PASS: 87 source checks. 수정 C++/헤더 10개의 충돌 마커, lexical delimiter 균형, generated include 최종 위치, deprecated 옵션의 필드/metadata 보존 및 생산 참조 없음, 공통 채널, Query 복구, Capsule/Mesh/부착 정책, 중복 Ignore 제거, trace 호출 수, 경고 중복 억제, foreign/파괴 hit guard, 실제 HP delta, body scales, PBO_None/no-op, 로그 필드와 read-only gate를 검사했다.

충돌 setter를 범위 전체에서 검색했다. 최종 살아 있는 캐릭터 정책 setter는 PartsComponent 공통 함수와 부착 생성이고, 나머지는 기존 debris/시각 파괴 및 WeaponActor NoCollision이다. 외부 CharacterBase에는 collision profile 변경 API가 있으나 수정하지 않았다. 이 API나 BP가 초기화 뒤 응답을 바꾸는 경우는 후속 런타임 확인 대상이다.

git diff --check는 오류 없이 종료했다. 대상 폴더가 untracked이므로 이 결과만으로 검증하지 않았고 실제 파일을 읽는 위 스크립트를 실행했다. 본 작업의 쓰기 대상은 위 12개 파일뿐이다. 전체 외부 파일의 작업 전후 hash 비교를 수행했다는 의미는 아니다.

## 15. 미실행 검증

전체 빌드/UHT/링크, Editor 실행, BP 로드·컴파일·재저장, Asset Registry 조회, PIE, Unreal 자동화 실행은 하지 않았다. 정적 검사 통과는 C++/UHT/BP 호환 또는 실제 총알 명중 성공을 증명하지 않는다.

## 16. BP에서 확인할 설정

Fire TraceChannel과 Parts WeaponTraceChannel을 HitTrace로 맞춘다. bTraceComplex 기본 false를 확인한다. 원본 Skeletal Mesh에 유효한 Physics Asset이 있는지, 몸통 및 팔/머리 실제 형상을 덮는지 확인한다. FollowBoneName/HitBoneAliases와 scale 값을 확인한다. legacy 세 옵션의 Deprecated 표시 및 기존 저장값 보존을 확인한다.

Construction Script/BeginPlay/피격 무적/리스폰에서 capsule, owner mesh 또는 part collision을 다시 덮어쓰는 BP가 있는지 확인한다. PartsComponent 이후의 독립 정책 setter를 두지 않는 것이 이 설계의 전제다. 초기화 진단 valid=1과 실제 fired channel 로그를 확인한다.

## 17. PIE에서 확인할 시나리오

1. 같은 몸통의 중앙/가장자리, 여러 자세와 각도에서 사격: 원본 Mesh hit인지, 미등록 Bone은 body-only인지 확인.
2. 정상 팔/머리 hit: 원하는 tag와 두 HP 감소량/기존 scale 확인.
3. 파츠 파괴 후 같은 좌표 반복: part=0, body>0, destruction/분리 FX 재호출 없음 확인.
4. 바닥/공중 debris 사격: 원본 HP 불변, DetachedDebris 사유 확인.
5. 아군, 죽은 대상, 0 요청, body scale=0, overkill의 실제 감소량과 zeroReason 확인.
6. legacy 세 옵션을 서로 반대로 저장해도 최종 정책이 같고 부착 visual이 NoCollision인지 확인.
7. 채널을 일부러 다르게 설정: owner/channel 쌍당 Warning 한 번, BP 값은 덮어쓰지 않는지 확인.
8. dm.CombatDiag 0/1 전환과 동일 입력 비교: 피해/탄퍼짐/발사 주기/랜덤 소비가 같아야 함.
9. Fire/FireCycle, Single/Burst/Automatic/Adaptive, 거리 피해·확률, 사망/리스폰 및 분리 시각 처리 회귀 확인.
10. 실제 모델에서 trace miss가 계속되면 start/end/impact 로그와 PhAT를 대조.

## 18. fallback 판단 및 알려진 제한

새 이중 Trace 또는 Capsule fallback은 넣지 않았다. 기존 Fire의 카메라 조준 ray, 실제 발사 ray, FireCycle safety ray 3개 호출 지점은 그대로다. 정적 분석만으로 실제 모델의 trace hole을 증명하지 못했기 때문이다.

PhAT가 없는 경우 Block 응답만으로 hit body가 생기지 않는다. 개별 PhAT body의 query 상태/기하 커버리지를 컴포넌트 진단으로 검증할 수 없다. 런타임에 BP나 외부 profile setter가 바꾸는 응답도 계속 강제 덮어쓰지 않는다. 목표가 없는 player miss에서는 월드 전체의 다른 캐릭터 채널을 검색하지 않는다. 저장 채널 변경은 초기화 정책을 다시 적용해야 한다.

실제 miss가 확인되면 우선 PhAT/모델 정합을 확인하고, 필요성이 입증된 뒤 첫 trace miss + 명확한 몸통 대상 + 벽 차단 유지 + 정밀 Bone 우선 원칙의 제한적 fallback을 별도 검토한다.
