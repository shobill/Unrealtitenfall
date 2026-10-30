#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "DeathMatch/Navigation/DMNavigationSafety.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDMNavCapsuleDimensionsTest,
	"SkySeoul.DeathMatch.Combat.Navigation.CapsuleDimensions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDMNavCapsuleDimensionsTest::RunTest(const FString& Parameters)
{
	const FDMNavDimensions Size = DMNavigationSafety::FromCapsule(42.0f, 96.0f);
	TestEqual(TEXT("Radius stays scaled capsule radius"), Size.Radius, 42.0f);
	TestEqual(TEXT("Height is twice scaled half height"), Size.Height, 192.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDMNavMismatchTest,
	"SkySeoul.DeathMatch.Combat.Navigation.Mismatch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDMNavMismatchTest::RunTest(const FString& Parameters)
{
	const FDMNavDimensions Capsule{42.0f, 192.0f};
	const FDMNavMismatch TooSmall = DMNavigationSafety::EvaluateMismatch(
		Capsule, Capsule, {10.0f, 70.0f}, 45.0f, 10.0f);
	TestTrue(TEXT("Smaller NavMesh is warned"), TooSmall.bNavMeshTooSmall);
	TestTrue(TEXT("Smaller selected NavData is wrong for agent"), TooSmall.bWrongNavData);
	TestTrue(TEXT("Large step mismatch is warned"), TooSmall.bStepHeightMismatch);

	const FDMNavMismatch Compatible = DMNavigationSafety::EvaluateMismatch(
		Capsule, Capsule, {45.0f, 200.0f}, 45.0f, 45.0f);
	TestFalse(TEXT("Equal or larger NavMesh is compatible"), Compatible.HasWarning());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDMObserverOrbitInputTest,
	"SkySeoul.DeathMatch.Combat.Debug.ObserverOrbitInput",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDMObserverOrbitInputTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Default orbit uses inverse position sign"), DMNavigationSafety::ApplyOrbitInput(0.0f, 2.0f, 1.0f, false) < 0.0f);
	TestTrue(TEXT("Orbit inversion reverses that sign"), DMNavigationSafety::ApplyOrbitInput(0.0f, 2.0f, 1.0f, true) > 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDMNavigationStuckEvidenceTest,
	"SkySeoul.DeathMatch.Combat.Navigation.StuckEvidence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDMNavigationStuckEvidenceTest::RunTest(const FString& Parameters)
{
	FName Reason;
	FDMStuckEvidence Evidence;
	Evidence.SecondsWithoutProgress = 1.0f;
	Evidence.ActualSpeed = 5.0f;
	Evidence.RequestedSpeed = 300.0f;
	TestFalse(TEXT("Evidence does not fire before time threshold"), DMNavigationSafety::IsStuck(Evidence, 2.0f, 0.2f, Reason));
	Evidence.SecondsWithoutProgress = 2.1f;
	TestTrue(TEXT("Low actual/requested speed after threshold is stuck"), DMNavigationSafety::IsStuck(Evidence, 2.0f, 0.2f, Reason));
	TestEqual(TEXT("Low speed reason is retained"), Reason, FName(TEXT("LowRequestedSpeedRatio")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDMNavigationPolicyTest,
	"SkySeoul.DeathMatch.Combat.Navigation.Policy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDMNavigationPolicyTest::RunTest(const FString& Parameters)
{
	TestFalse(TEXT("Crowd and RVO cannot be enabled together"), DMNavigationSafety::IsAvoidanceConfigurationValid(true, true));
	TestTrue(TEXT("RVO-only configuration is valid"), DMNavigationSafety::IsAvoidanceConfigurationValid(false, true));
	TestFalse(TEXT("Partial paths remain rejected"), DMNavigationSafety::IsCandidatePathUsable(true, true, true, true));
	TestFalse(TEXT("Hard passability failure excludes final candidate"), DMNavigationSafety::IsCandidatePathUsable(true, true, false, false));
	TestTrue(TEXT("Projected complete clear path is usable"), DMNavigationSafety::IsCandidatePathUsable(true, true, false, true));
	TestTrue(TEXT("Loose passability failure rejects movement"),
		DMNavigationSafety::ShouldRejectMoveForClearance(false, true));
	TestFalse(TEXT("Full Nav path is not rejected by quality-only clearance failure"),
		DMNavigationSafety::ShouldRejectMoveForClearance(true, false));
	TestTrue(TEXT("Destination static blockage is a hard failure"),
		DMNavigationSafety::IsHardPathFailureReason(TEXT("DestinationStaticBlocked")));
	TestTrue(TEXT("Static path-centre blockage is a hard failure"),
		DMNavigationSafety::IsHardPathFailureReason(TEXT("StaticPassabilityBlocked")));
	TestFalse(TEXT("Quality failure never enters blocked-goal cooldown"),
		DMNavigationSafety::IsHardPathFailureReason(TEXT("WallClearanceQualityLow")));
	TestFalse(TEXT("Dynamic obstacle never enters blocked-goal cooldown"),
		DMNavigationSafety::IsHardPathFailureReason(TEXT("DynamicObstacleDetected")));
	TestFalse(TEXT("Another Pawn is not a hard path blocker"),
		DMNavigationSafety::ShouldHardBlockPathHit(false, true, false, true));
	TestFalse(TEXT("Initial overlap is not a hard path blocker"),
		DMNavigationSafety::ShouldHardBlockPathHit(true, false, true, true));
	TestTrue(TEXT("Confirmed fixed structure at path centre is a hard blocker"),
		DMNavigationSafety::ShouldHardBlockPathHit(true, false, false, true));
	TestTrue(TEXT("First stuck timeout receives one repath"), DMNavigationSafety::ShouldRetryStuckMove(0));
	TestFalse(TEXT("Repeated stuck timeout fails after repath"), DMNavigationSafety::ShouldRetryStuckMove(1));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDMNavigationLooseProbeTest,
	"SkySeoul.DeathMatch.Combat.Navigation.LooseProbe",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDMNavigationLooseProbeTest::RunTest(const FString& Parameters)
{
	float Radius = 0.0f;
	float HalfHeight = 0.0f;
	DMNavigationSafety::CalculateProbeSize(50.0f, 100.0f, 0.70f, 0.80f, Radius, HalfHeight);
	TestEqual(TEXT("Passability radius uses the 0.70 default"), Radius, 35.0f);
	TestEqual(TEXT("Passability half height uses the 0.80 default"), HalfHeight, 80.0f);
	DMNavigationSafety::CalculateProbeSize(100.0f, 60.0f, 0.70f, 0.50f, Radius, HalfHeight);
	TestEqual(TEXT("Probe half height never falls below radius"), HalfHeight, Radius);

	TestTrue(TEXT("Five centimetre vertical change enables raised probe"),
		DMNavigationSafety::ShouldRaiseProbe(FVector::ZeroVector, FVector(100.0f, 0.0f, 5.0f), true, 5.0f));
	TestFalse(TEXT("Flat segment does not enable raised probe"),
		DMNavigationSafety::ShouldRaiseProbe(FVector::ZeroVector, FVector(100.0f, 0.0f, 0.0f), true, 5.0f));
	TestEqual(TEXT("Raised height comes from Character Movement MaxStepHeight"),
		DMNavigationSafety::CalculateRaisedHeight(45.0f, 1.0f), 45.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDMNavigationPathSegmentSweepTest,
	"SkySeoul.DeathMatch.Combat.Navigation.PathSegmentSweeps",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDMNavigationPathSegmentSweepTest::RunTest(const FString& Parameters)
{
	TArray<FDMSweepInterval> Intervals;
	DMNavigationSafety::BuildPathSweepIntervals(
		{FVector::ZeroVector, FVector(100.0f, 0.0f, 0.0f), FVector(100.0f, 600.0f, 0.0f)},
		250.0f, true, 5.0f, Intervals);
	TestEqual(TEXT("Short first segment and 600cm second segment produce four sweeps"), Intervals.Num(), 4);
	if (Intervals.Num() == 4)
	{
		TestEqual(TEXT("First sweep follows path segment zero"), Intervals[0].PathSegment, 0);
		TestEqual(TEXT("Remaining sweeps follow path segment one"), Intervals[1].PathSegment, 1);
		TestEqual(TEXT("Corner is preserved as first segment end"), Intervals[0].End, FVector(100.0f, 0.0f, 0.0f));
		TestEqual(TEXT("Next sweep starts at the same real path corner"), Intervals[1].Start, FVector(100.0f, 0.0f, 0.0f));
		TestFalse(TEXT("Flat path subdivisions do not use raised probes"), Intervals[0].bRaised || Intervals[1].bRaised);
	}

	DMNavigationSafety::BuildPathSweepIntervals(
		{FVector::ZeroVector, FVector(100.0f, 0.0f, 10.0f)}, 250.0f, true, 5.0f, Intervals);
	TestTrue(TEXT("Vertical Nav path segment marks its sweep as raised"), Intervals.Num() == 1 && Intervals[0].bRaised);
	return true;
}

#endif
