# DeathMatch Tactical Area BoxComponent 전환 결과

- 작성일: 2026-10-14 11:45 KST
- 범위: `Unreal/Source/SkySeoul/DeathMatch`의 Tactical Area Volume, Subsystem, Roam Provider, 자동화 테스트
- 구분: 구현 결과

## 요청 또는 증상

`ADMTacticalAreaVolume`이 `AVolume` 기반이어서 에디터에 배치한 직후 조절 가능한 기본 직육면체 영역이 명확하게 보이지 않았다. 복잡한 BSP Brush 대신 하나의 `UBoxComponent`가 영역 표시, Overlap, 내부 판정, Nav 검색 범위와 디버그 Bounds를 일관되게 담당하도록 구조를 변경할 필요가 있었다.

## 결론

`ADMTacticalAreaVolume`을 `AActor` 기반으로 변경하고 `AreaBounds`라는 `UBoxComponent`를 루트 컴포넌트로 추가했다. 기본 Box Extent는 `(500, 500, 250)cm`이며 전체 크기는 약 `1000 × 1000 × 500cm`다.

기존 Area Id, 활성화, Area Type, 팀별 위험도·점유·이동 예정 AI, 교전 참여자 수, Roam 점수 계산과 예약 수명주기는 유지했다. `ADMTacticalAreaVolume*` 외부 인터페이스도 유지하여 Move Goal, Combat Move Director, HUD Debug와 이벤트 연결 코드는 타입 변경 없이 계속 사용할 수 있다.

## 변경 파일

- `Navigation/DMTacticalAreaVolume.h`
- `Navigation/DMTacticalAreaVolume.cpp`
- `Navigation/DMTacticalAreaSubsystem.cpp`
- `AI/Move/DMMoveGoalProvider_Roam.cpp`
- `Tests/DMTacticalAreaRoamTests.cpp`
- `Tests/DMTacticalAreaTestTypes.h` (자동화용 Team Pawn)
- `Communication/Reports/2026-10-14/1145_TACTICAL_AREA_BOX_COMPONENT_REPORT.md`

기존 작업 트리의 다른 변경은 정리하거나 되돌리지 않았다.

## AActor + UBoxComponent 구조

```text
ADMTacticalAreaVolume : AActor
└─ AreaBounds : UBoxComponent (RootComponent)
```

`AreaBounds`는 `VisibleAnywhere`, `BlueprintReadOnly`로 노출되어 배치 인스턴스와 Blueprint Details에서 Box Extent를 편집할 수 있다. 에디터 Shape는 청록색으로 표시하고 `bDrawOnlyIfSelected=false`로 두어 선택하지 않은 상태에서도 영역 위치를 찾을 수 있게 했다. 게임에서는 `Hidden In Game=true`다.

Area Type에는 현재 Roam 점수에서 사용하지 않는 확장용 값이라는 Tooltip을 추가했다. 기존 런타임 Map과 교전 수는 `VisibleInstanceOnly` 읽기 전용 구성을 유지한다.

## Collision 및 Overlap 설정

`AreaBounds` 설정은 다음과 같다.

- Collision Enabled: `Query Only`
- Object Type: `WorldDynamic`
- Generate Overlap Events: `true`
- Can Character Step Up On: `No`
- 모든 채널: `Ignore`
- Pawn 채널: `Overlap`
- Navigation 영향: 비활성

프로젝트의 Player/Robot Character 충돌 프로필을 확인한 결과 Character Mesh와 `CapsuleCompSet`, `NormalCollision` 등이 Pawn Object Type을 사용하므로 별도 커스텀 Object Channel 응답은 추가하지 않았다.

집계 대상은 `APawn`이면서 `DeathMatchTeamUtils::GetTeamIdFromActor` 결과가 `NoTeam(255)`가 아닌 Actor로 제한했다. 따라서 무기, 파츠, 시체용 별도 Actor, 투사체, 디버그 Actor와 Team 미할당 Pawn은 점유 인원으로 집계하지 않는다.

## 중복 Overlap 방지

기존 `AActor/AVolume::NotifyActorBeginOverlap` 및 `NotifyActorEndOverlap` 의존성을 제거하고 `AreaBounds`의 Component Begin/End Overlap Delegate를 연결했다.

볼륨은 Actor별로 현재 겹친 `UPrimitiveComponent`의 weak set을 관리한다. 첫 컴포넌트 Begin에서만 `NotifyVolumeEntered`, 마지막 컴포넌트 End에서만 `NotifyVolumeExited`를 호출한다. Subsystem의 Actor 단위 Presence 레코드와 함께 동작하므로 같은 캐릭터의 Capsule/Mesh 등 여러 컴포넌트가 겹쳐도 한 번만 집계된다. Actor 파괴 시에는 기존 Subsystem 파괴 Delegate가 Presence와 점유 수를 제거한다.

## 내부 위치 판정과 대표 위치

`EncompassesPoint`는 제거했다. 월드 점을 `AreaBounds` Component Transform의 Local Space로 역변환한 뒤 `GetUnscaledBoxExtent()`와 축별 절댓값을 비교한다. Transform 역변환에 Scale이 이미 반영되므로 Scaled Extent를 다시 적용하지 않는다.

이 방식은 Actor/Component 이동, 회전, 비균일 Scale이 있는 경우에도 Box 내부 판정을 유지한다. 대표 위치는 Actor Location 대신 `AreaBounds->GetComponentLocation()`을 반환하며 Roam Provider의 NavMesh 투영 원점과 거리 점수에 그대로 사용된다.

## Search Radius와 Nav/Debug Bounds

Search Radius는 `AreaBounds->GetScaledBoxExtent()`의 XY 대각선 길이로 계산하고 최솟값 `100cm`를 유지한다.

```text
max(100, sqrt(ScaledExtent.X² + ScaledExtent.Y²))
```

Roam Provider의 NavMesh 원점 투영 Extent는 회전까지 포함한 `AreaBounds->Bounds.BoxExtent`를 사용한다. 반경 검색 결과가 Box 밖일 수 있으므로 기존 `IsPointInsideVolume()` 최종 검사는 유지했다.

런타임 Combat Debug Box는 `AreaBounds`의 실제 월드 중심, Scaled Extent와 Component Quaternion으로 그려 회전된 Box와 일치한다. 성공 시 기존 `DebugColor`, 실패 시 Orange를 사용한다.

Subsystem에서 겹친 Tactical Area 중 작은 영역을 고르는 비교도 Actor의 전체 Component Bounds 대신 `AreaBounds`의 Scaled Extent 부피를 사용한다.

## 기존 시스템 호환성

다음 경로를 확인했다.

- `UDMTacticalAreaSubsystem`: 등록, Presence, 위험도, 예약과 Actor 파괴 정리 유지
- `UDMMoveGoalProvider_Roam`: 대표 위치와 검색 반경 사용, 랜덤 Nav 결과의 Box 내부 최종 검사 유지
- `FDMMoveGoal`, `UDMCombatMoveDirector`: `ADMTacticalAreaVolume*` 타입 유지로 인터페이스 변경 없음
- Combat 시작/종료와 Damage/Enemy Seen 이벤트: Subsystem API 변경 없음
- HUD Combat Debug: 기존 Goal/위험도 표시 유지

Tactical Area 관련 파일에서 `AVolume`, `EncompassesPoint`, Actor Notify Overlap, `GetComponentsBoundingBox`, `BrushComponent` 잔존 사용을 정적 검색했으며 발견되지 않았다. 다른 용도의 `ADMTacticalNavRegionVolume`, Deprecated Tactical Analysis Volume은 이번 변경 대상이 아니므로 그대로 유지했다.

## 에디터에서 배치·설정할 항목

레벨에 배치할 클래스는 기존과 동일한 `DeathMatch Tactical Area Volume` (`ADMTacticalAreaVolume`)이다.

1. 액터를 배치한다.
2. Details의 `DeathMatch | Tactical Area > Area Bounds > Box Extent`에서 크기를 조절한다.
3. Actor Scale보다는 Box Extent 편집을 기본 사용법으로 삼는다. Actor Scale이 1이 아니어도 내부 판정과 검색 반경은 대응한다.
4. `Area Id`를 맵 내에서 고유하게 설정한다.
5. `Enabled`와 확장용 `Area Type`을 설정한다.
6. 필요하면 `DeathMatch | Tactical Area | Debug`의 `Debug Display Name`, `Debug Color`를 설정한다.
7. PIE 중 `Runtime`의 Team Danger, Team Occupancy, Team Incoming AI, Current Combat Participant Count를 확인한다.

## 마이그레이션 주의사항

클래스 이름은 유지했지만 부모 클래스가 `AVolume`에서 `AActor`로 바뀌었고 BSP Brush는 `AreaBounds`로 자동 변환되지 않는다. 이미 레벨이나 Blueprint에 배치된 기존 인스턴스는 컴파일 후 `AreaBounds` 생성 여부와 Box Extent를 확인해야 한다.

이전 Brush 크기는 보존된다고 보장할 수 없다. 기존 인스턴스의 Component 구조가 정상 갱신되지 않거나 영역 크기가 달라졌다면 인스턴스를 삭제하고 `DeathMatch Tactical Area Volume`을 다시 배치한 뒤 Box Extent를 설정해야 한다.

## 자동화 테스트

`SkySeoul.DeathMatch.Roam.TacticalArea` 묶음에 다음 검증을 포함했다.

- AreaBounds 생성, RootComponent 지정, 양수 기본 Extent
- 중심/외부 점 판정
- 회전 및 비균일 Scale 적용 시 Local Space 내부/외부 판정
- Box Extent 증가에 따른 Search Radius 증가
- Begin/End Delegate 바인딩
- 유효 TeamId Pawn 필터
- 여러 PrimitiveComponent가 겹칠 때 Actor 한 번만 집계
- 마지막 EndOverlap 시 점유 제거
- Actor 파괴 시 점유 제거
- 기존 팀별 점수와 예약 멱등성/이전/파괴 정리

## 검증 상태

- `SkySeoulEditor Win64 Development` 빌드: 성공
- UnrealHeaderTool (`-WarningsAsErrors`): 성공
- `SkySeoul.DeathMatch.Roam.TacticalArea.Bounds`: 성공
- `SkySeoul.DeathMatch.Roam.TacticalArea.Overlap`: 성공
- `SkySeoul.DeathMatch.Roam.TacticalArea.Reservation`: 성공
- `SkySeoul.DeathMatch.Roam.TacticalArea.Score`: 성공
- 자동화 전체 종료 코드: `0`
- Editor 수동 배치/Blueprint 저장/PIE: 수행하지 않음, 확인 필요

## 남은 PIE 확인 사항

- 선택/비선택 상태에서 AreaBounds가 맵 스케일에 맞게 잘 보이는지
- Box Extent, 회전, Actor Scale 편집 결과가 뷰포트에 즉시 반영되는지
- Player와 Robot의 실제 Capsule/Mesh Overlap에서 한 Actor당 점유가 1만 증가하는지
- 사망, Respawn, UnPossess와 볼륨 경계 이동 시 Runtime 점유 수가 즉시 복구되는지
- 실제 NavMesh에서 랜덤 목표가 회전된 Box 내부에만 확정되는지
- 기존 배치 인스턴스 또는 `BP_DMTacticalArea`가 새 AreaBounds 구조를 정상 상속하는지
