# DeathMatch Robot AI Controller — 2026-10-14

Controller 기본 구성은 Perception, Sense, Combat Knowledge, Combat Fire Control, Movement Speed Policy, Combat Move Director다. Controller는 Possess/UnPossess, 팀·파츠 정보, Perception/피해 전달, Navigation Safety와 단일 RootBehaviorTree 실행만 담당한다.

RootBehaviorTree 미지정 시 Warning을 남기고 안전하게 idle 상태를 유지한다. Brain/Framework/Provider/Tactics 컴포넌트는 생성하지 않는다. 기존 loose navigation passability와 quality probe 정책은 유지한다.
