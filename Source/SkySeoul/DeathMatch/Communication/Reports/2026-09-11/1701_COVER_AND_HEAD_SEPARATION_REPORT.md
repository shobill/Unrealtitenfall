# 엄폐 탐색 정체 및 머리 파츠 분리 점검 보고서

- 작성일: 2026-09-11 17:01 KST
- 범위: `Source/SkySeoul/DeathMatch/AI/Combat/`, `Source/SkySeoul/DeathMatch/Character/`, `Source/SkySeoul/PartsCharacter/`, 관련 DeathMatch BP·Data Asset 경로
- 구분: 진단

## 요청 또는 증상

1. AI가 엄폐해야 하는 상황에서 숨을 곳을 찾지 못하면 엄폐 탐색을 반복하며 공격하지 않는다.
2. 팔은 `bSeparable=false`에서 분리되지 않지만 머리는 같은 옵션을 꺼도 분리된다.

## 결론

엄폐 문제는 실패 후 대체 행동 정책이 부족한 것이 직접 원인이다. 엄폐 모듈에는 전체 탐색 실패 횟수나 총 탐색 시간 제한이 없으며, 유효 엄폐가 없으면 기본 1.5초 간격으로 계속 재탐색한다. 탐색 중 엄폐 모듈은 발사를 허용하지 않는다. Priority 2·3에 실행 가능한 교전 이동 모듈이 없으면 AI는 공격하지 않는 상태를 반복할 수 있다.

머리 분리는 현재 확인한 C++ 정상 파괴 경로만으로는 설명되지 않는다. `UPartsComponent`는 일반 파괴와 사망 폭발 모두 `bSeparable=false`를 확인한다. 실제 런타임에서 다른 Setup Data가 사용되거나 기본 DeathMatch 정의가 대신 사용되는 경우, `Parts.Head` 행이 중복된 경우, 또는 BP 이벤트가 별도로 머리 Mesh를 분리하는 경우를 확인해야 한다. 기본 DeathMatch 머리 정의의 `bSeparable` 값은 `true`다.

## 확인 근거

### 엄폐

- 한 번의 전체 엄폐 탐색은 36개 후보를 `CandidatesPerUpdate` 단위로 나누어 검사한다.
- 전체 후보에서 엄폐를 찾지 못하면 `SearchInterval` 뒤 다시 시작한다.
- 실패 횟수 제한, 총 탐색 제한 시간 및 실패 후 장기 쿨다운이 없다.
- 엄폐가 없을 때 `bCanExecute=false`, `bAllowFire=false`를 반환한다.
- Combat Cover는 엄폐를 확보한 뒤에도 `Exposed` 단계에서만 발사를 허용한다.
- 전투 Tactics는 Priority 1부터 3까지 실행 가능한 첫 모듈을 선택한다. 모두 실행 불가능하면 최종 결과의 발사를 금지한다.

### 머리 파츠

- `DestroyPartInternal`은 실제 Definition의 `bSeparable`이 true일 때만 Debris 분리 함수를 호출한다.
- false이면 해당 파츠 Mesh를 숨기고 충돌만 끈다.
- 사망 시 파츠 폭발 처리도 `bSeparable=false`인 Definition을 건너뛴다.
- Setup Data가 없거나 비어 있고 기본 파츠 사용 옵션이 켜져 있으면 기본 DeathMatch 정의가 주입된다.
- 기본 정의 생성 코드는 머리를 포함한 파츠의 `bSeparable`을 true로 설정한다.

## 문제점 및 위험

- 엄폐 실패와 아직 후보를 나누어 검사 중인 상태가 외부 결과에서는 모두 실행 불가로 보인다.
- 엄폐 모듈이 높은 우선순위에 고정된 상태에서 재탐색하면 대체 행동을 반복적으로 선점할 수 있다.
- 머리 증상이 실제 Static Mesh 분리인지, Skeletal Mesh 머리 Bone 숨김인지 영상만으로 혼동될 수 있다.
- 에디터에서 수정한 Data Asset과 런타임 `PartsComponent.SetupData`가 다르면 편집 값이 적용되지 않는다.
- BP의 파츠 파괴·사망 FX 이벤트가 Detach, 물리 활성화 또는 별도 Actor 생성을 수행하면 C++ 옵션과 무관하게 분리처럼 보일 수 있다.

## BP에서 확인할 사항

1. AI Controller의 Movement Priority 1~3 실제 설정을 확인한다.
2. 엄폐 모듈 아래에 `Maintain Distance` 또는 `Strafe`처럼 발사 가능한 대체 모듈이 있는지 확인한다.
3. `BP_DeathMatchAIRobot`의 Parts Component가 실제로 `DA_Parts_DM_Robot`을 Setup Data로 사용하는지 확인한다.
4. Data Asset에서 `Parts.Head` 행이 하나인지, 그 행의 `bSeparable`이 false인지 확인한다.
5. `BP_OnPartDestroyedFX`, `BP_OnDeathPartsExplosionFX`에서 머리 Mesh Detach, Simulate Physics 또는 별도 머리 Actor 생성이 있는지 확인한다.
6. 머리 파괴와 캐릭터 사망을 별도 상황으로 재현한다.

## 권장 후속 작업

- 엄폐에 전체 탐색 실패 예산과 실패 쿨다운을 추가하고, 그동안 하위 우선순위의 교전 모듈로 넘긴다.
- 엄폐 실패 자체는 사격의 영구 차단 조건으로 사용하지 않는다. 대체 모듈과 기존 Fire Gate가 허용하면 사격한다.
- 머리 파츠 초기화와 파괴 시 실제 Setup Data 경로, `Parts.Head` 행 개수, `bSeparable`, 파괴 원인을 비 Shipping 진단 로그로 확인한다.
- 로그로 원인을 확정한 뒤 Data Asset, BP 이벤트 또는 C++ 중 실제 원인 경로만 수정한다.

## 검증 상태

- C++ 정적 점검: 완료
- BP 내부 그래프와 Data Asset 런타임 값: 미확인
- PIE 재현: 미실행
- 코드 또는 BP 수정: 수행하지 않음
