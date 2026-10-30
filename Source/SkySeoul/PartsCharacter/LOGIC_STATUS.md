# Parts Character — Data-driven

## 구조
```
UPartsSetupData → UPartsComponent / UPartsDamageManager → APartsCharacterBase
```

## 부착
| 항목 | 동작 |
|------|------|
| API | BeginPlay: Register → AttachToComponent |
| DA AttachRelativeTransform | 캐릭터 스켈레탈 Mesh 스페이스 |
| 본 | FollowBoneName에 붙임 |
| ParentTag | 연쇄용 표기만. **부착은 전부 FollowBoneName** (부모 파츠에 안 붙임) |
| 부착 방식 | Mesh-space → `SetWorldTransform` 후 `KeepWorld`로 본에 붙임 (ref-pose bake 안 씀) |

## 분리 물리
- 파괴 시 **월드 액터**에 Box 물리 루트 + **메시 복사** 스폰 후 원본 Destroy
- 로그: `Parts debris COPY ...` (안 보이면 리빌드 안 된 것)
- DebrisLifeSpan 0/-1 → 컴포넌트 기본, **최소 0.5s**
- `DestroyedVisualPolicy`: `DetachAsDebris`는 분리, `HideInPlace`는 숨김, `KeepAttachedDisabled`는 외형과 본 추종을 유지하고 충돌·기능만 비활성화한다.
- `bSeparable=false`는 Debris 생성을 금지한다. 기본 Detach 정책과 함께 설정된 기존 데이터는 안전하게 `KeepAttachedDisabled`로 처리한다. 로봇 DA의 비분리 행은 명시적으로 Keep 정책으로 저장했다.
- 파괴된 파츠의 재피격은 몸통 피해까지 차단하며, 기존 HP·머리 손상·팔 기능 파괴와 중복 파괴 방지는 유지한다.

## 암 연쇄 (의도)
| 타격 | 파괴 |
|------|------|
| Hand | Hand만 |
| Lower | Lower + Hand |
| Upper | Upper + Lower + Hand |

- 연쇄는 `BoneIsChildOf(자식FollowBone, 파괴본)` 만 사용
- ParentTag가 뒤집혀 있어도 **조상(어퍼)은 로우 타격으로 안 죽음**
- PhAT가 전완 히트를 `upperarm_*`로 보고해도 → **Impact 가장 가까운 FollowBone 파츠**로 재해석 (`ResolvePartTagForHit`)

## 손가락
- Hand: `bHideDescendantBonesOnDestroy=true`, HideTreeRootBone=hand_l

## 확인 체크
1. 에디터 종료 후 풀 리빌드 (Live Coding만으로 안 먹을 수 있음)
2. 파괴 시 Log에 `Parts debris COPY` / `Parts hit: bone ... → closest part`
3. DA 각 행 `bSeparable=true`, `FollowBoneName` = upperarm_l / lowerarm_l / hand_l
4. PhAT에 lowerarm 바디가 있으면 더 정확 (없어도 Impact 보정이 동작)
