# TacticalGrid 최근 Roam 이력 컴파일 수정 결과

- 작성일: 2026-10-08 14:52 KST
- 범위: `Source/SkySeoul/DeathMatch/Navigation/DMTacticalGridSubsystem.cpp`
- 구분: 빌드 오류 수정 및 Editor 빌드 검증

## 요청 또는 증상

`SkySeoulEditor Win64 Development` 빌드에서 `DMTacticalGridSubsystem.cpp`의 최근 Roam 이력 감점 계산이
컴파일되지 않았다.

오류 위치는 `FMath::Min(Index, UE_ARRAY_COUNT(RankScale) - 1)`이었다. `Index`는 `int32`지만 UE 5.6의
`UE_ARRAY_COUNT` 결과는 `SIZE_T`이므로, 서로 다른 signed/unsigned 정수 타입을 받는 `FMath::Min`의 제약 조건이
성립하지 않았다. 이어진 `Scale` 초기화 오류는 이 템플릿 해석 실패의 후속 오류였다.

## 결론

배열 길이를 명시적으로 `int32`로 변환해 `RankIndex`를 먼저 계산하고, 그 인덱스로 RankScale을 읽도록 수정했다.
동작과 감점값 100/60/30/15/8은 변경하지 않았다.

## 수정 파일

- `Navigation/DMTacticalGridSubsystem.cpp`
- 본 보고서

## 검증 상태

- 실행: Unreal Engine 5.6 `Build.bat SkySeoulEditor Win64 Development ... -WaitMutex -NoHotReloadFromIDE`
- `DMTacticalGridSubsystem.cpp` 컴파일: 성공
- `UnrealEditor-SkySeoul.lib` 링크: 성공
- `UnrealEditor-SkySeoul.dll` 링크: 성공
- `SkySeoulEditor.target` 메타데이터 생성: 성공
- 최종 결과: `Result: Succeeded`
- Unreal 자동화 테스트 및 PIE: 이번 빌드 수정에서는 실행하지 않음

## BP에서 확인할 사항

이번 수정은 C++ 정수 타입 정합성만 바로잡았으므로 BP 설정 변경은 없다. 후속 PIE 확인 항목은
`1326_FRONTIER_PATH_AND_SPAWN_CONTROL_REPORT.md`의 목록을 따른다.
