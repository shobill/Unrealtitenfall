# DM Combat Position - Tactical Area 위험도 연결 결과

## 결과 요약

`DM Find Combat Position`의 기존 EQS 후보 후처리 구조에 AI 팀 기준 Tactical Area 위험도를 연결했다. 기존 위치별 `ThreatExposure` 시야 트레이스는 그대로 유지하고, `AreaDanger`와 `AreaDangerIncrease`를 별도 소프트 점수로 cheap evaluation 단계와 최종 점수에 반영한다.

- 후보 및 현재 위치 Area 조회: `UDMTacticalAreaSubsystem::FindVolumeAtLocation`
- 위험도 원본: `ADMTacticalAreaVolume::GetTeamDanger(AI TeamId)`
- 미배치 위치: `UnassignedAreaDanger` 기본값 5.0
- 후보별 Actor 전체 탐색, Overlap, 추가 EQS 없음
- 동일 Area 후보는 로컬 Area별 위험도 캐시를 사용
- Roam 선택 점수 및 상태 전환 로직 변경 없음

## 변경 파일

- `Source/SkySeoul/DeathMatch/AI/Combat/DMCombatPositionProfile.h`
- `Source/SkySeoul/DeathMatch/AI/Combat/DMCombatPositionProfile.cpp`
- `Source/SkySeoul/DeathMatch/AI/BT/BTTask_DM_FindCombatPosition.h`
- `Source/SkySeoul/DeathMatch/AI/BT/BTTask_DM_FindCombatPosition.cpp`
- `Source/SkySeoul/DeathMatch/Navigation/DMTacticalAreaSubsystem.cpp`
- `Source/SkySeoul/DeathMatch/Tests/DMCombatPositionTests.cpp`
- `Source/SkySeoul/DeathMatch/Tests/DMTacticalAreaRoamTests.cpp`

## 점수식과 기본값

```text
DangerIncrease = Max(0, CandidateAreaDanger - CurrentAreaDanger)

FinalScore =
    FireFeasibilityScore
  + RangeFitnessScore
  + ThreatExposureScore
  + NearbyFriendlyScore
  + PathCostScore
  + AreaDangerScore
  + AreaDangerIncreaseScore
```

신규 Criterion은 기존 `FDMCombatPositionCriterion`의 `bEnabled`, `bRequiredFilter`, `Weight`, `AllowedMin/Max`, `NormalizeMin/Max`, `ScoreDirection` 계약을 그대로 사용한다.

- `AreaDanger`
  - enabled: true
  - required filter: false
  - weight: 1.0
  - direction: `LowerIsBetter`
  - allowed: 0..10
  - normalize: 0..10
- `AreaDangerIncrease`
  - enabled: true
  - required filter: false
  - weight: 2.0
  - direction: `LowerIsBetter`
  - allowed: 0..10
  - normalize: 0..10
- `UnassignedAreaDanger`: 5.0
- `Search.AreaDangerChangeTolerance`: 0.1

기본 설정에서는 위험도 10인 후보도 필터 탈락하지 않고 낮은 점수만 받는다. 디자이너가 해당 Criterion의 `bRequiredFilter`를 켠 경우에만 Allowed 범위가 강제 필터로 동작한다.

## 평가 및 성능 구조

1. 제한된 EQS 후보의 완전 Nav 경로 가능 여부를 기존 방식으로 확인한다.
2. 후보 Area를 Subsystem 등록 Volume 목록을 통해 조회한다.
3. AI의 `GenericTeamId`로 Area 위험도를 읽고, 현재 Area 대비 증가량을 계산한다.
4. 두 위험도 Criterion의 필터와 점수를 cheap score에 반영한다.
5. 기존 목적별 shortlist를 유지한다.
6. shortlist에만 기존 전체 경로 길이와 개별 위협 시야 트레이스를 수행한다.
7. 최종 점수에 위험도 두 항목을 각각 더한다.

같은 Area에 속한 여러 후보는 `TMap<const ADMTacticalAreaVolume*, float>`에 저장한 팀별 위험도를 재사용한다. Area 조회는 기존 등록 Volume 목록만 순회한다. 겹치는 Area는 기존의 작은 Bounds Volume 우선 정책을 유지하고, 크기가 같은 경우 Actor path name 순으로 결정해 등록 순서와 무관하게 선택한다.

## 캐시 무효화

선택 결과 캐시에 다음 스냅샷을 함께 저장한다.

- 평가 팀 ID
- 현재 Area / 후보 Area 객체
- 현재 Area 위험도 / 후보 Area 위험도

재사용 시 AI 현재 위치와 캐시 위치의 Area를 다시 조회한다. 팀 ID 또는 Area 객체가 달라지거나, 현재/후보 위험도 중 하나가 `AreaDangerChangeTolerance`보다 크게 변하면 `ResultReuseTime` 안이어도 캐시 위치를 재사용하지 않는다. 0.1 이하의 부동소수점 변화는 재사용을 유지한다. 기존 타겟, 무기, 사거리, 위협 스냅샷, 실행 플래그 조건은 그대로 유지했다.

## 디버그 출력

기존 `dm.CombatDebug 2` 선택 성공 로그에만 다음 값을 추가했다.

```text
Area=<candidate actor> CurrentArea=<current actor> AreaDanger=<value> DangerIncrease=<value>
```

화면 디버그와 매 프레임 Warning 로그는 추가하지 않았다.

## 에디터 조절 항목

아래 세 Data Asset은 서로 다른 `UDMCombatPositionProfile` 인스턴스이므로 신규 항목을 독립적으로 저장하고 조절할 수 있다.

- `DA_DM_AttackPositionProfile`
- `DA_DM_ApproachPositionProfile`
- `DA_DM_HidePositionProfile`

각 Asset의 `Evaluation > Tactical Area`에서 다음을 조절한다.

- `AreaDanger`: 절대 위험도 선호/필터
- `AreaDangerIncrease`: 현재 위치 대비 위험 증가 선호/필터
- `UnassignedAreaDanger`: Tactical Area 미배치 위치의 중립값

`Search`에서 `AreaDangerChangeTolerance`를 조절한다. 기본값은 세 목적 모두 AreaDanger 1.0, AreaDangerIncrease 2.0이며, Approach도 강제 필터 없이 위험 증가 감점만 적용한다. Hide는 필요에 따라 두 Weight를 더 높이거나 Allowed 범위와 required filter를 별도로 설정할 수 있다. 목적별 임의 배율은 코드에 추가하지 않았다.

기존 `.uasset` 바이너리는 수정하지 않았다. 신규 직렬화 필드는 C++ 생성자 기본값을 가지며 각 Asset을 에디터에서 저장하면 목적별 값이 해당 Asset에 독립 저장된다.

## 자동화 검증

### 빌드

- `SkySeoulEditor Win64 Development`: 성공
- UnrealHeaderTool: 성공

### 관련 테스트

- `SkySeoul.DeathMatch.CombatPosition`: 21개 전부 성공
- `SkySeoul.DeathMatch.Roam.TacticalArea.Danger`: 성공

신규/보강 검증 내용:

- 낮은 절대 Area 위험도의 점수가 더 높음
- 절대 위험도가 같을 때 현재 Area 대비 증가량이 작은 후보의 점수가 더 높음
- 기본 설정의 고위험 후보는 강제 탈락하지 않음
- `bRequiredFilter` 활성화 시 Allowed 범위 밖 후보 탈락
- Area 미배치 위치는 5.0을 사용
- 같은 Area도 AI TeamId별 위험도를 분리해 읽음
- 0.1 초과 위험도 변화는 캐시 무효화, 미세 변화는 허용
- 음수 위험 증가량은 0으로 제한
- 기존 Attack 사선/최대 사거리, Approach shortlist, Hide 전체 위협 노출 계약 테스트 유지 및 성공

참고로 더 넓은 `SkySeoul.DeathMatch.Combat` 묶음에서는 이번 변경과 무관한 기존 테스트 두 개가 실패했다.

- `SkySeoul.DeathMatch.Combat.Fire.PIE`: 실제 발사 관측 0회
- `SkySeoul.DeathMatch.Combat.Parts.HitRouting`

변경 범위 전용 테스트는 별도 실행에서 종료 코드 0으로 모두 성공했다.
