# DeathMatch 전술 영역 스폰 위험도 및 가중 랜덤 Roam 구현 결과

- 작성일: 2026-10-14 12:53 KST
- 범위: `Unreal/Source/SkySeoul/DeathMatch`의 Tactical Area Navigation, Roam Move Provider, Move Goal, Combat Debug HUD, 자동화 테스트
- 구분: 구현 결과

## 요청 또는 증상

기존 Tactical Area Roam은 팀 스폰 정보를 위험도에 반영하지 않았고, 동적 위험도를 제한 없이 누적했다. 목표 영역은 위험도가 높을수록 점수가 커지는 방식으로 정렬한 뒤 최고 후보를 확정 선택하므로, 적 스폰에 위험도 10을 부여하면 모든 AI가 같은 영역으로 몰릴 수 있었다.

## 결론

위험도를 `BaseDanger`와 `DynamicDanger`로 분리하고 최종값을 `Clamp(Max(BaseDanger, DynamicDanger), 0, 10)`으로 통일했다. 상대 팀의 스폰이 포함된 영역은 요청 팀 관점에서 항상 `BaseDanger=10`이며, 자기 팀 스폰만 있는 영역은 스폰 정보로 위험도가 증가하지 않는다.

Roam 영역 선택은 최고점 확정 방식에서 AI별 선호 위험도 기반 `SelectionWeight`와 Roulette Wheel 가중 랜덤 방식으로 변경했다. 위치 탐색에 실패한 영역은 해당 AI의 실패 쿨다운에 기록하고 현재 요청 후보에서 제거한 뒤, 남은 가중치로 다시 추첨한다.

## 수정한 파일

- `Navigation/DMTacticalAreaVolume.h/.cpp`
- `Navigation/DMTacticalAreaSubsystem.h/.cpp`
- `AI/Move/DMMoveGoalProvider_Roam.h/.cpp`
- `AI/Move/DMMoveTypes.h`
- `Presentation/HUD/DeathMatchHUD_CombatDebug.cpp`
- `Tests/DMTacticalAreaRoamTests.cpp`

삭제한 파일은 없다.

## 스폰 영역 자동 등록 방식

`UDMTacticalAreaSubsystem::OnWorldBeginPlay`에서 레벨의 `APlayerStartByTeam`을 조회하고, 각 Start의 `TeamId`와 위치를 포함하는 `ADMTacticalAreaVolume`을 연결한다. 스트리밍 또는 늦게 등록되는 볼륨도 처리하도록 볼륨 등록 때 다시 스캔한다.

기존 `ReportCharacterSpawn(Character, TeamId)`은 보조 경로로 실제 Spawn 위치의 팀 정보를 등록한다. 현재 DeathMatch AI는 Possess/Respawn 확정 흐름에서 이 API를 호출한다. `TeamId=255(NoTeam)`은 어느 경로에서도 등록하지 않는다. 한 볼륨은 `TSet<uint8> SpawnTeams`로 여러 팀 스폰을 지원한다.

## BaseDanger와 DynamicDanger

- `BaseDanger`: 별도 누적 맵을 만들지 않고 `SpawnTeams`에서 요청 시 계산한다. 요청 팀과 다른 유효 SpawnTeam이 하나라도 있으면 10, 아니면 0이다.
- `DynamicDanger`: `TeamDynamicDanger`에 관측 팀별로 저장한다. Enemy Seen, Damage Received, Combat Start, Character Death의 기존 가산 정책을 유지하되 매 변경마다 0~10으로 Clamp한다.
- `GetTeamDanger(TeamId)`: 외부 호환 조회 함수이며 `Clamp(Max(BaseDanger, DynamicDanger), 0, 10)`을 반환한다.
- `GetTeamBaseDanger`, `GetTeamDynamicDanger`: 디버그 및 테스트용 분리 조회를 제공한다.

DynamicDanger가 감소하거나 0이 되어도 SpawnTeams에서 계산하는 적 스폰 BaseDanger 10은 유지된다.

## PreferredDanger와 AI별 고정 성향

기본값:

- `PreferredDanger = 6`
- `DangerTolerance = 4`
- `DangerPreferenceWeight = 4`
- `BaseExplorationWeight = 1`
- `MinimumSelectionWeight = 0.05`
- `PersonalDangerBiasRange = 1`

계산식:

```text
EffectivePreferredDanger = Clamp(PreferredDanger + PersonalDangerBias, 0, 10)
DangerFit = Clamp(1 - Abs(FinalDanger - EffectivePreferredDanger)
                      / Max(DangerTolerance, SmallNumber), 0, 1)
```

Provider가 처음 판단할 때 Controller의 안정적인 Path Name 해시로 RandomStream을 초기화한다. 같은 Seed로 `PersonalDangerBias`를 한 번만 `[-Range, +Range]`에서 만들고 생명주기 동안 유지한다. 실제 영역 추첨도 이 AI 전용 RandomStream을 사용하므로 같은 Tick의 AI가 하나의 전역 랜덤 결과를 공유하지 않는다.

## SelectionWeight 계산과 Weighted Random

```text
RawWeight =
    BaseExplorationWeight
  + DangerFit * DangerPreferenceWeight
  - FriendlyCount * FriendlyCrowdingWeight
  - SameTeamIncomingCount * IncomingAIWeight
  - (DistanceCm / 1000) * DistanceWeight
  - RecentVolumePenalty (최근 방문 영역일 때)
  - CurrentVolumePenalty (현재 영역일 때)

SelectionWeight = Max(MinimumSelectionWeight, RawWeight)
```

`CurrentVolumePenalty` 기본값은 2다. 현재 영역과 최근 방문 영역도 완전히 제외하지 않으며, 유효 영역은 최소 가중치로 선택 가능성을 유지한다.

후보의 `SelectionWeight` 합에서 AI별 RandomStream으로 `RandomRoll`을 만들고 누적 가중치가 Roll을 넘는 첫 후보를 선택한다. AreaId 정렬이나 최고점 확정 선택은 제거했다. 위험도 10 영역도 선호 위험도와 차이가 크면 DangerFit이 낮지만, BaseExplorationWeight와 MinimumSelectionWeight 때문에 완전히 금지되지는 않는다.

## 위치 탐색 실패 후 재추첨

한 번 뽑은 후보는 위치 탐색 전에 현재 요청의 후보 배열에서 제거한다. 최대 `MaxRandomLocationAttempts=3`회의 Nav 위치 탐색이 실패하면 그 AI Provider의 `VolumeFailureCooldown=3초`를 적용하고, 남은 후보들의 Weight 합으로 다시 Roulette Wheel 추첨한다. 한 요청에서 실패한 영역은 다시 선택되지 않으며 전체 영역 추첨은 `MaxVolumeSelectionAttempts=3`으로 제한한다.

## 기존 BP 변수 변경

- 제거: `DangerWeight`
- 추가: `PreferredDanger`, `DangerTolerance`, `DangerPreferenceWeight`, `BaseExplorationWeight`, `MinimumSelectionWeight`, `PersonalDangerBiasRange`, `CurrentVolumePenalty`
- 유지: `FriendlyCrowdingWeight`, `IncomingAIWeight`, `DistanceWeight`, `RecentVolumePenalty`, `RecentVolumeCooldown`, 검색/이동 제한 변수
- Move Goal 런타임 메타데이터 `VolumeScore`는 `SelectionWeight`로 변경하고 Base/Dynamic/Final Danger, PreferredDanger, DangerFit, TotalWeight, RandomRoll, CurrentVolume을 추가했다.

초기 개발 단계의 명확성을 우선해 레거시 `DangerWeight` 직렬화 이름은 유지하지 않았다. 기존 Controller BP 인스턴스에서 새 기본값 또는 맵별 튜닝값을 확인해야 한다.

## BP에서 확인할 사항

1. 모든 실제 팀 스폰이 `APlayerStartByTeam`이며 `TeamId`가 올바른지 확인한다.
2. PlayerStart 위치가 의도한 Tactical Area Box 안에 들어가는지 확인한다.
3. AI Controller BP의 `MoveProviderRoam`에서 새 위험도 선호/가중치 기본값을 확인한다.
4. 한 볼륨에 양 팀 스폰이 함께 들어간 경우 두 팀 모두 상대 SpawnTeam을 보므로 양쪽 관점 BaseDanger가 10이 되는 것이 의도인지 확인한다.
5. 기존과 동일하게 각 볼륨 내부에 도달 가능한 NavMesh가 충분한지 확인한다.

## 디버그 확인 방법

`dm.CombatDebug 2`의 Roam 표시는 다음을 제공한다.

```text
Roam: CurrentArea -> SelectedArea | D=Final Pref=EffectivePreferred Fit=DangerFit W=SelectionWeight
Base=BaseDanger Dynamic=DynamicDanger Roll=RandomRoll/TotalWeight
Goal=Location Attempt=RandomNavAttempt
```

목표 선택 로그에는 AreaId, Base/Dynamic/Final Danger, PreferredDanger, DangerFit, SelectionWeight, TotalWeight, RandomRoll, 선택 확률과 목표 위치를 한 번 출력한다. 후보별 값은 Verbose 로그에서 확인한다. Tick 로그는 추가하지 않았다.

## 자동화 테스트 결과

`SkySeoul.DeathMatch.Roam.TacticalArea` 묶음 5개가 모두 성공했다.

- Bounds: 성공
- Danger: 성공
- Overlap: 성공
- Reservation: 성공
- Weight: 성공

Danger 테스트는 적/아군 스폰 BaseDanger, DynamicDanger 감소 후 BaseDanger 유지, 0~10 Clamp, NoTeam 제외를 검증한다. Weight 테스트는 DangerFit, 모든 감점 항목, MinimumSelectionWeight, 고정 Seed 통계 선택, 최고 가중치 비확정, 실패 후보 제거 후 재추첨, 고정 PersonalDangerBias를 검증한다.

## 검증 상태

- `SkySeoulEditor Win64 Development` C++ 빌드: 성공
- UnrealHeaderTool: 성공
- 자동화 `SkySeoul.DeathMatch.Roam.TacticalArea.*`: 5/5 성공, Exit Code 0
- Editor 수동 조작 / Blueprint 저장 / 실제 맵 PIE: 수행하지 않음

## 남은 PIE 확인 사항

1. 실제 맵에서 각 PlayerStartByTeam이 예상 Tactical Area의 SpawnTeams로 등록되는지 확인한다.
2. 양 팀 AI의 Debug에서 상대 스폰 `Base=10`, 자기 스폰 `Base=0`이 표시되는지 확인한다.
3. 여러 AI가 동일 Tick에 판단해도 서로 다른 영역 분포를 보이는지 확인한다.
4. NavMesh가 불완전한 영역을 뽑았을 때 실패 쿨다운 후 남은 영역으로 재추첨되는지 확인한다.
5. BP에 저장된 구형 `DangerWeight` 값이 제거된 뒤 새 기본값들이 의도대로 적용되는지 확인한다.
