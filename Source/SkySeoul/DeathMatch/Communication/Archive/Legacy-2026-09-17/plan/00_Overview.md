# DeathMatch Plan Overview

## 역할 분담
- **뼈대(아키텍처):** 시스템 경계, 클래스 책임, 데이터 흐름, 구현 순서
- **구현:** BP·폴리시 수치·맵 세팅, 발사/UI/밸런스 등 구체 로직

## 전제
- SkySeoul **근간은 수정하지 않는다** (`CharacterBase`, ASC 위치, 무기 GAS, 인벤토리, Photon 본편 등).
- 모드 진입점은 **현행 유지**: 맵 World Settings에서 GameMode를 `GM_DeathMatch`(또는 BP 자식)로 교체.
- “플러그”는 UE Plugin이 아니라 **모드 레이어를 근안 위에 얹는 아키텍처**를 뜻한다.
- 손댈 범위는 **`Source/SkySeoul/DeathMatch/` 내부**뿐이다.

## 연결 방식 (입구는 그대로)
```
맵 → GM_DeathMatch
      ├─ GS_DeathMatch (점수/라운드) ← Presentation UI 구독
      ├─ DeathMatchHUD (위젯 생성·화면 전환)
      ├─ DeathMatchPlayerState (팀 + Dead → Event.Death)
      ├─ DefaultPawn / PC (GI Payload 또는 GM BP)
      └─ AI → DeathMatchAIController + Modules → DeathMatchRobotCharacter
```

근안과 맞물리는 지점 3개만 사용:
1. **스폰** — 근안 캐릭터/Robot 파생 Possess
2. **사망** — `State.Dead` → `Event.Death`
3. **팀** — `GenericTeamId` / Attitude

## 문서 목록
| 파일 | 내용 |
|------|------|
| `00_Overview.md` | 이 문서 (전제·연결·역할) |
| `01_Architecture.md` | 모듈식 큰 틀, 레이어, 계약 |
| `02_InternalCleanup.md` | 내부 정리 백로그 (P0~P3) |
| `03_CurrentStatus.md` | 현 코드 구현 범위 요약 |
| `Presentation/LOGIC_STATUS.md` | UI 뼈대·BP 작업 가이드 |
