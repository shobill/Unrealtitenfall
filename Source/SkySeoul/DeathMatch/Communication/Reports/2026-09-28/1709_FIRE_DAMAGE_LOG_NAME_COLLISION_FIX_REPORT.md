# Fire 피해 진단 람다 이름 충돌 수정

- 작성: 2026-09-28 17:09 KST
- 근거: 사용자가 제공한 빌드 로그의 DMGameplayAbility_Fire.cpp:1074 C4459.

## 원인 및 수정

앞선 작업에서 추가한 지역 람다 `LogDamage`가 엔진 Pawn.h의 전역 로그 카테고리 `LogDamage`를 가려 컴파일이 실패했다. DMGameplayAbility_Fire.cpp의 람다 선언과 호출 8곳을 `RecordFireDamageResult`로 변경했다. 피해 계산과 진단 출력 내용은 유지했다.

첨부 로그의 MSB3073/종료 코드 6은 빌드 실패 결과다. 다른 파일의 C4996은 deprecated API 경고이며 이번 수정 범위에 포함하지 않았다.

## 검증 상태

이 파일에서 기존 이름이 남지 않고 선언/호출 이름이 일치함을 검색으로 확인했다. inspect_hit_routing.py를 다시 실행하여 소스 정적 검사 87개가 통과했다. 전체 빌드, UHT, BP, PIE 및 Unreal 자동화 테스트는 이번 수정 후 실행하지 않았다. 재빌드 성공은 미검증이다.

BP 설정 변경은 필요하지 않다. 후속 재빌드에서 C4459 해소와 링크 완료를 확인해야 한다.
