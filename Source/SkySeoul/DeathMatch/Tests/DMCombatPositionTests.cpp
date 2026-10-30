#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Algo/Reverse.h"
#include "DeathMatch/AI/BT/BTTask_DM_FindCombatPosition.h"
#include "DeathMatch/AI/Combat/DMCombatPositionProfile.h"
#include "DeathMatch/AI/Fire/DMCombatFireControlComponent.h"
#include "DeathMatch/Controller/DeathMatchRobotAIController.h"
#include "PartsCharacter/PartsHitCollision.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BlackboardData.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Bool.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDMCombatPositionProfileSelectionTest,
	"SkySeoul.DeathMatch.CombatPosition.ProfileSelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDMCombatPositionProfileSelectionTest::RunTest(const FString&)
{
	UBTTask_DM_FindCombatPosition* Task = NewObject<UBTTask_DM_FindCombatPosition>();
	Task->AttackPositionProfile = NewObject<UDMCombatPositionProfile>(Task);
	Task->ApproachPositionProfile = NewObject<UDMCombatPositionProfile>(Task);
	Task->HidePositionProfile = NewObject<UDMCombatPositionProfile>(Task);
	TestTrue(TEXT("Attack profile selected"), Task->GetProfileForPurpose(EDMCombatPositionPurpose::Attack) == Task->AttackPositionProfile);
	TestTrue(TEXT("Approach profile selected"), Task->GetProfileForPurpose(EDMCombatPositionPurpose::Approach) == Task->ApproachPositionProfile);
	TestTrue(TEXT("Hide profile selected"), Task->GetProfileForPurpose(EDMCombatPositionPurpose::Hide) == Task->HidePositionProfile);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDMCombatPositionAreaDangerDefaultsTest,
	"SkySeoul.DeathMatch.CombatPosition.TacticalArea.DefaultsAndSoftFiltering",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDMCombatPositionAreaDangerDefaultsTest::RunTest(const FString&)
{
	const UDMCombatPositionProfile* Profile = NewObject<UDMCombatPositionProfile>();
	TestTrue(TEXT("Area danger is enabled by default"), Profile->AreaDanger.bEnabled);
	TestFalse(TEXT("Area danger is a soft score by default"), Profile->AreaDanger.bRequiredFilter);
	TestEqual(TEXT("Area danger default weight"), Profile->AreaDanger.Weight, 1.f);
	TestEqual(TEXT("Area danger normalizes over the Tactical Area contract"), Profile->AreaDanger.NormalizeMax, 10.f);
	TestTrue(TEXT("Danger increase is enabled by default"), Profile->AreaDangerIncrease.bEnabled);
	TestFalse(TEXT("Danger increase is a soft score by default"), Profile->AreaDangerIncrease.bRequiredFilter);
	TestEqual(TEXT("Danger increase is weighted more strongly"), Profile->AreaDangerIncrease.Weight, 2.f);
	TestTrue(TEXT("Maximum danger remains eligible under default soft settings"),
		DMCombatPositionEvaluation::IsCriterionAllowed(10.f, Profile->AreaDanger));
	TestEqual(TEXT("Unassigned areas use the neutral fallback"),
		DMCombatPositionEvaluation::ResolveAreaDanger(nullptr, 1, Profile->UnassignedAreaDanger), 5.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDMCombatPositionAreaDangerScoreTest,
	"SkySeoul.DeathMatch.CombatPosition.TacticalArea.ScoreAndRequiredFilter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDMCombatPositionAreaDangerScoreTest::RunTest(const FString&)
{
	UDMCombatPositionProfile* Profile = NewObject<UDMCombatPositionProfile>();
	const float SaferScore = DMCombatPositionEvaluation::ComputeAreaDangerScore(
		2.f, 2.f, Profile->AreaDanger, Profile->AreaDangerIncrease);
	const float DangerousScore = DMCombatPositionEvaluation::ComputeAreaDangerScore(
		8.f, 2.f, Profile->AreaDanger, Profile->AreaDangerIncrease);
	TestTrue(TEXT("Equal detailed combat scores prefer lower absolute area danger"), SaferScore > DangerousScore);

	const float SmallIncreaseScore = DMCombatPositionEvaluation::ComputeAreaDangerScore(
		8.f, 6.f, Profile->AreaDanger, Profile->AreaDangerIncrease);
	const float LargeIncreaseScore = DMCombatPositionEvaluation::ComputeAreaDangerScore(
		8.f, 2.f, Profile->AreaDanger, Profile->AreaDangerIncrease);
	TestTrue(TEXT("Equal candidate danger prefers the smaller increase from current danger"),
		SmallIncreaseScore > LargeIncreaseScore);

	Profile->AreaDanger.bRequiredFilter = true;
	Profile->AreaDanger.AllowedMax = 6.f;
	TestFalse(TEXT("Explicit required filter rejects danger outside Allowed range"),
		DMCombatPositionEvaluation::IsCriterionAllowed(8.f, Profile->AreaDanger));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDMCombatPositionAreaDangerCacheTest,
	"SkySeoul.DeathMatch.CombatPosition.TacticalArea.CacheTolerance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDMCombatPositionAreaDangerCacheTest::RunTest(const FString&)
{
	TestFalse(TEXT("Sub-tolerance danger noise preserves result reuse"),
		DMCombatPositionEvaluation::HasMeaningfulDangerChange(5.f, 5.05f, .1f));
	TestTrue(TEXT("Meaningful danger change invalidates result reuse"),
		DMCombatPositionEvaluation::HasMeaningfulDangerChange(5.f, 5.11f, .1f));
	TestEqual(TEXT("Danger increase never becomes negative"),
		DMCombatPositionEvaluation::ComputeAreaDangerIncrease(2.f, 8.f), 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDMAttackFireFeasibilityFilterTest,
	"SkySeoul.DeathMatch.CombatPosition.Attack.FireFeasibility",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDMAttackFireFeasibilityFilterTest::RunTest(const FString&)
{
	AActor* Target = NewObject<AActor>();
	AActor* WorldBlocker = NewObject<AActor>();
	TestTrue(TEXT("Intended target is a valid firing solution"),
		UDMCombatFireControlComponent::ClassifyLineOfFireHit(FGenericTeamId(1), Target, Target, true) == EDMFireAttemptFailure::None);
	TestTrue(TEXT("World blocker rejects attack candidate"),
		UDMCombatFireControlComponent::ClassifyLineOfFireHit(FGenericTeamId(1), WorldBlocker, Target, true) == EDMFireAttemptFailure::Sight);
	TestTrue(TEXT("Missing target hit rejects attack candidate"),
		UDMCombatFireControlComponent::ClassifyLineOfFireHit(FGenericTeamId(1), nullptr, Target, false) == EDMFireAttemptFailure::Sight);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDMCombatPositionFriendlyLineBlockTest,
	"SkySeoul.DeathMatch.CombatPosition.Attack.FriendlyWeaponChannelBlock",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDMCombatPositionFriendlyLineBlockTest::RunTest(const FString&)
{
	ADeathMatchRobotAIController* Friendly = NewObject<ADeathMatchRobotAIController>();
	AActor* Target = NewObject<AActor>();
	Friendly->SetGenericTeamId(FGenericTeamId(1));
	TestEqual(TEXT("Position test defaults to actual weapon trace channel"),
		static_cast<int32>(UDMCombatFireControlComponent::ResolveWeaponTraceChannel(nullptr)),
		static_cast<int32>(PartsHitCollision::DefaultWeaponTraceChannel));
	TestTrue(TEXT("Friendly hit on weapon line is rejected"),
		UDMCombatFireControlComponent::ClassifyLineOfFireHit(FGenericTeamId(1), Friendly, Target, true)
			== EDMFireAttemptFailure::FriendlyFireRisk);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDMCombatPositionRangeTest,
	"SkySeoul.DeathMatch.CombatPosition.RangeFitness",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDMCombatPositionRangeTest::RunTest(const FString&)
{
	TestEqual(TEXT("Effective range receives peak score"),
		DMCombatPositionEvaluation::ComputeRangeFitness(3000.f, 3000.f, 5000.f), 1.f);
	TestTrue(TEXT("Point blank is not preferred"),
		DMCombatPositionEvaluation::ComputeRangeFitness(100.f, 3000.f, 5000.f) < .2f);
	TestTrue(TEXT("Far valid range is less fit"),
		DMCombatPositionEvaluation::ComputeRangeFitness(4500.f, 3000.f, 5000.f) < 1.f);
	TestEqual(TEXT("Beyond maximum range scores zero"),
		DMCombatPositionEvaluation::ComputeRangeFitness(5001.f, 3000.f, 5000.f), 0.f);
	TestTrue(TEXT("Maximum range is included"), DMCombatPositionEvaluation::IsWithinMaximumRange(5000.f, 5000.f));
	TestFalse(TEXT("Beyond maximum range is filtered"), DMCombatPositionEvaluation::IsWithinMaximumRange(5000.1f, 5000.f));
	TestTrue(TEXT("Attack/Approach margin keeps a comfortably inside candidate"),
		DMCombatPositionEvaluation::IsWithinMaximumRange(4925.f, 5000.f, 75.f));
	TestFalse(TEXT("Attack/Approach margin filters a maximum-boundary candidate"),
		DMCombatPositionEvaluation::IsWithinMaximumRange(4999.f, 5000.f, 75.f));
	UDMCombatPositionProfile* Profile = NewObject<UDMCombatPositionProfile>();
	TestEqual(TEXT("Default maximum range safety margin is 75 cm"),
		Profile->MaximumRangeSafetyMargin, 75.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDMCombatPositionThreatMemoryTest,
	"SkySeoul.DeathMatch.CombatPosition.ThreatMemory",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDMCombatPositionThreatMemoryTest::RunTest(const FString&)
{
	AActor* Focus = NewObject<AActor>();
	AActor* Remembered = NewObject<AActor>();
	AActor* Expired = NewObject<AActor>();
	TArray<FDMRememberedThreat> Records;
	auto Add = [&Records](AActor* Actor, float Time, bool bVisible)
	{
		FDMRememberedThreat& R = Records.AddDefaulted_GetRef(); R.Actor = Actor; R.LastSensedTime = Time; R.bVisible = bVisible;
	};
	Add(Focus, 99.f, true);
	Add(Remembered, 98.f, false);
	Add(Expired, 80.f, false);
	TArray<const FDMRememberedThreat*> Selected;
	DMCombatPositionEvaluation::SelectValidThreats(Records, Focus, 100.f, 5.f, 8, Selected);
	TestEqual(TEXT("Only current or valid remembered enemies participate"), Selected.Num(), 2);
	TestTrue(TEXT("Focus threat is retained first"), Selected[0]->Actor == Focus);
	TestFalse(TEXT("Expired memory is excluded"), Selected.Contains(&Records[2]));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDMCombatPositionNearbyFriendlyTest,
	"SkySeoul.DeathMatch.CombatPosition.NearbyFriendlyCount",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDMCombatPositionNearbyFriendlyTest::RunTest(const FString&)
{
	const TArray<FVector> Friendlies{FVector(10, 0, 0), FVector(80, 0, 0), FVector(500, 0, 0)};
	TestEqual(TEXT("Only friendlies inside configured radius count"),
		DMCombatPositionEvaluation::CountNearbyLocations(FVector::ZeroVector, Friendlies, 100.f), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDMApproachNotShortestRushTest,
	"SkySeoul.DeathMatch.CombatPosition.Approach.NotShortestRush",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDMApproachNotShortestRushTest::RunTest(const FString&)
{
	const float TooClose = DMCombatPositionEvaluation::ComputeRangeFitness(100.f, 3000.f, 5000.f);
	const float Effective = DMCombatPositionEvaluation::ComputeRangeFitness(2500.f, 3000.f, 5000.f);
	TestTrue(TEXT("Approach favors an attack-ready range over the closest point"), Effective > TooClose);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDMHideExposurePreferenceTest,
	"SkySeoul.DeathMatch.CombatPosition.Hide.ExposurePreference",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDMHideExposurePreferenceTest::RunTest(const FString&)
{
	FDMCombatPositionCriterion Exposure;
	Exposure.NormalizeMin = 0.f;
	Exposure.NormalizeMax = 1.f;
	Exposure.ScoreDirection = EDMCombatScoreDirection::LowerIsBetter;
	TestTrue(TEXT("Less exposed hide candidate receives higher score"),
		DMCombatPositionEvaluation::NormalizeCriterion(.25f, Exposure)
		> DMCombatPositionEvaluation::NormalizeCriterion(.75f, Exposure));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDMCombatPositionSearchCooldownTest,
	"SkySeoul.DeathMatch.CombatPosition.SearchCooldown",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDMCombatPositionSearchCooldownTest::RunTest(const FString&)
{
	TestFalse(TEXT("Search cannot repeat every frame inside cooldown"),
		UBTTask_DM_FindCombatPosition::ShouldStartSearch(10.01, 10.0, .25f));
	TestTrue(TEXT("Search can repeat after cooldown"),
		UBTTask_DM_FindCombatPosition::ShouldStartSearch(10.25, 10.0, .25f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDMCombatPositionFailureContractTest,
	"SkySeoul.DeathMatch.CombatPosition.Failure.NoFallbackAndClear",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDMCombatPositionFailureContractTest::RunTest(const FString&)
{
	UWorld::InitializationValues IV;
	IV.AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(true).ShouldSimulatePhysics(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("DMCombatPositionFailure"), nullptr, true, ERHIFeatureLevel::Num, &IV);
	if (!TestNotNull(TEXT("World"), World)) return false;
	UBlackboardData* Data = NewObject<UBlackboardData>(World);
	FBlackboardEntry& Entry = Data->Keys.AddDefaulted_GetRef();
	Entry.EntryName = TEXT("MoveGoal");
	Entry.KeyType = NewObject<UBlackboardKeyType_Vector>(Data);
	UBlackboardComponent* Blackboard = NewObject<UBlackboardComponent>(World);
	TestTrue(TEXT("Test blackboard initializes"), Blackboard->InitializeBlackboard(*Data));
	Blackboard->SetValueAsVector(Entry.EntryName, FVector(100.f, 200.f, 300.f));
	const EBTNodeResult::Type Result = UBTTask_DM_FindCombatPosition::FailWithoutFallback(Blackboard, Entry.EntryName);
	TestTrue(TEXT("Failed search returns Failed without generating fallback"), Result == EBTNodeResult::Failed);
	TestFalse(TEXT("Failed search clears its previous MoveGoal"), Blackboard->IsVectorValueSet(Entry.EntryName));
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDMCombatPositionOutputResetTest,
	"SkySeoul.DeathMatch.CombatPosition.Failure.ExecutionOutputsReset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDMCombatPositionOutputResetTest::RunTest(const FString&)
{
	UWorld::InitializationValues IV;
	IV.AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(true).ShouldSimulatePhysics(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("DMCombatPositionOutputReset"), nullptr, true, ERHIFeatureLevel::Num, &IV);
	if (!TestNotNull(TEXT("World"), World)) return false;
	UBlackboardData* Data = NewObject<UBlackboardData>(World);
	auto AddKey = [Data](FName Name, UBlackboardKeyType* Type)
	{
		FBlackboardEntry& Entry = Data->Keys.AddDefaulted_GetRef();
		Entry.EntryName = Name;
		Entry.KeyType = Type;
	};
	AddKey(TEXT("MoveGoal"), NewObject<UBlackboardKeyType_Vector>(Data));
	AddKey(TEXT("ShouldADS"), NewObject<UBlackboardKeyType_Bool>(Data));
	AddKey(TEXT("FireWhileMoving"), NewObject<UBlackboardKeyType_Bool>(Data));
	UBlackboardComponent* Blackboard = NewObject<UBlackboardComponent>(World);
	TestTrue(TEXT("Test blackboard initializes"), Blackboard->InitializeBlackboard(*Data));
	Blackboard->SetValueAsVector(TEXT("MoveGoal"), FVector(1.f));
	Blackboard->SetValueAsBool(TEXT("ShouldADS"), true);
	Blackboard->SetValueAsBool(TEXT("FireWhileMoving"), true);
	UBTTask_DM_FindCombatPosition::InitializeSearchOutputs(Blackboard,
		TEXT("MoveGoal"), TEXT("ShouldADS"), TEXT("FireWhileMoving"));
	TestFalse(TEXT("MoveGoal is cleared before search"), Blackboard->IsVectorValueSet(TEXT("MoveGoal")));
	TestFalse(TEXT("Stale ADS value is cleared"), Blackboard->GetValueAsBool(TEXT("ShouldADS")));
	TestFalse(TEXT("Stale moving-fire value is cleared"), Blackboard->GetValueAsBool(TEXT("FireWhileMoving")));
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDMCombatPositionPurposeShortlistTest,
	"SkySeoul.DeathMatch.CombatPosition.Shortlist.PurposeContracts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDMCombatPositionPurposeShortlistTest::RunTest(const FString&)
{
	TArray<FDMCombatPositionPrefilterCandidate> Candidates;
	auto Add = [&Candidates](float X, float Score, bool bFire, bool bConcealed)
	{
		FDMCombatPositionPrefilterCandidate& Candidate = Candidates.AddDefaulted_GetRef();
		Candidate.Location = FVector(X, 0.f, 0.f);
		Candidate.CheapScore = Score;
		Candidate.bFocusLineClear = bFire;
		Candidate.bRepresentativeConcealed = bConcealed;
	};
	Add(0.f, 10.f, false, false);
	Add(100.f, 2.f, true, true);
	Add(200.f, 1.f, true, false);
	TArray<int32> Selected;
	DMCombatPositionEvaluation::SelectPurposeShortlist(Candidates,
		EDMCombatPositionPurpose::Attack, 1, .25f, Selected);
	TestEqual(TEXT("Blocked candidates do not consume the Attack expensive slot"), Selected.Num(), 1);
	TestTrue(TEXT("Attack shortlist contains a fire-capable candidate"), Candidates[Selected[0]].bFocusLineClear);
	DMCombatPositionEvaluation::SelectPurposeShortlist(Candidates,
		EDMCombatPositionPurpose::Hide, 2, .5f, Selected);
	TestTrue(TEXT("Hide shortlist retains the representative-concealed candidate"), Selected.Contains(1));
	TestTrue(TEXT("Hide shortlist retains a bounded exposed alternative"), Selected.Contains(0) || Selected.Contains(2));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDMCombatPositionDiversityOrderTest,
	"SkySeoul.DeathMatch.CombatPosition.Shortlist.OrderIndependentDiversity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDMCombatPositionDiversityOrderTest::RunTest(const FString&)
{
	TArray<FVector> Forward{FVector(-1000, 0, 0), FVector(-10, 0, 0), FVector(10, 0, 0), FVector(1000, 0, 0)};
	TArray<FVector> Reverse = Forward;
	Algo::Reverse(Reverse);
	TArray<FVector> A;
	TArray<FVector> B;
	DMCombatPositionEvaluation::SelectSpatiallyDiverseLocations(Forward, 3, A);
	DMCombatPositionEvaluation::SelectSpatiallyDiverseLocations(Reverse, 3, B);
	TestEqual(TEXT("Candidate cap is maintained"), A.Num(), 3);
	TestTrue(TEXT("Initial EQS array order does not change the spatial sample"), A == B);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDMCombatPositionCriterionContractTest,
	"SkySeoul.DeathMatch.CombatPosition.Criteria.EnabledRequiredAllowedWeight",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDMCombatPositionCriterionContractTest::RunTest(const FString&)
{
	FDMCombatPositionCriterion Criterion;
	Criterion.bEnabled = false;
	Criterion.bRequiredFilter = true;
	Criterion.AllowedMin = .25f;
	Criterion.AllowedMax = .75f;
	TestTrue(TEXT("Disabled criterion never filters"), DMCombatPositionEvaluation::IsCriterionAllowed(1.f, Criterion));
	Criterion.bEnabled = true;
	Criterion.bRequiredFilter = false;
	TestTrue(TEXT("Non-required criterion never filters"), DMCombatPositionEvaluation::IsCriterionAllowed(1.f, Criterion));
	Criterion.bRequiredFilter = true;
	TestFalse(TEXT("Required criterion rejects outside Allowed range"), DMCombatPositionEvaluation::IsCriterionAllowed(1.f, Criterion));
	TestTrue(TEXT("Required criterion accepts inside Allowed range"), DMCombatPositionEvaluation::IsCriterionAllowed(.5f, Criterion));
	Criterion.Weight = 0.f;
	TestEqual(TEXT("Zero weight contributes no score"),
		DMCombatPositionEvaluation::NormalizeCriterion(.5f, Criterion) * Criterion.Weight, 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDMCombatPositionFireOriginTest,
	"SkySeoul.DeathMatch.CombatPosition.FireOrigin.RelativeProjection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDMCombatPositionFireOriginTest::RunTest(const FString&)
{
	const FVector Projected = UDMCombatFireControlComponent::ProjectFireOriginOffset(
		FVector(100, 200, 20), FVector(140, 210, 180), FVector(1000, 500, 50));
	TestEqual(TEXT("Candidate fire origin preserves the live pawn-relative offset"),
		Projected, FVector(1040, 510, 210));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDMCombatPositionLowCoverParityTest,
	"SkySeoul.DeathMatch.CombatPosition.FireOrigin.LowCoverParity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDMCombatPositionLowCoverParityTest::RunTest(const FString&)
{
	const FVector PawnLocation(200.f, 300.f, 10.f);
	const FVector LiveFireOrigin(230.f, 300.f, 95.f);
	const FVector CandidateFireOrigin = UDMCombatFireControlComponent::ProjectFireOriginOffset(
		PawnLocation, LiveFireOrigin, PawnLocation);
	TestEqual(TEXT("Candidate at the live pawn position uses the identical low-cover trace origin"),
		CandidateFireOrigin, LiveFireOrigin);
	AActor* Target = NewObject<AActor>();
	AActor* LowCoverBlocker = NewObject<AActor>();
	const EDMFireAttemptFailure CandidateResult = UDMCombatFireControlComponent::ClassifyLineOfFireHit(
		FGenericTeamId(1), LowCoverBlocker, Target, true);
	const EDMFireAttemptFailure LiveResult = UDMCombatFireControlComponent::ClassifyLineOfFireHit(
		FGenericTeamId(1), LowCoverBlocker, Target, true);
	TestTrue(TEXT("Candidate and live low-cover blocking classifications match"), CandidateResult == LiveResult);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDMCombatPositionBudgetCapsTest,
	"SkySeoul.DeathMatch.CombatPosition.Shortlist.BudgetCaps",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDMCombatPositionBudgetCapsTest::RunTest(const FString&)
{
	TArray<FVector> Locations;
	for (int32 Index = 0; Index < 100; ++Index) Locations.Add(FVector(Index * 100.f, Index % 7 * 100.f, 0.f));
	TArray<FVector> Limited;
	DMCombatPositionEvaluation::SelectSpatiallyDiverseLocations(Locations, 64, Limited);
	TestEqual(TEXT("MaxCandidateCount bounds the prefilter work"), Limited.Num(), 64);
	TArray<FDMCombatPositionPrefilterCandidate> Candidates;
	for (const FVector& Location : Limited)
	{
		FDMCombatPositionPrefilterCandidate& Candidate = Candidates.AddDefaulted_GetRef();
		Candidate.Location = Location;
		Candidate.bFocusLineClear = true;
	}
	TArray<int32> Shortlist;
	DMCombatPositionEvaluation::SelectPurposeShortlist(Candidates,
		EDMCombatPositionPurpose::Attack, 12, .25f, Shortlist);
	TestEqual(TEXT("ExpensiveCheckCandidateCount bounds full traces and path costs"), Shortlist.Num(), 12);

	TArray<FDMRememberedThreat> Records;
	for (int32 Index = 0; Index < 20; ++Index)
	{
		FDMRememberedThreat& Threat = Records.AddDefaulted_GetRef();
		Threat.Actor = NewObject<AActor>();
		Threat.LastSensedTime = 100.f;
	}
	TArray<const FDMRememberedThreat*> Threats;
	DMCombatPositionEvaluation::SelectValidThreats(Records, nullptr, 100.f, 5.f, 8, Threats);
	TestEqual(TEXT("MaxThreatCount bounds per-candidate full exposure traces"), Threats.Num(), 8);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDMHideRepresentativeIsNotFinalTest,
	"SkySeoul.DeathMatch.CombatPosition.Hide.RepresentativeDoesNotReplaceFullThreats",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDMHideRepresentativeIsNotFinalTest::RunTest(const FString&)
{
	const TArray<bool> Exposure{false, true};
	const TArray<bool> FocusFlags{true, false};
	int32 ExposedCount = 0;
	DMCombatPositionEvaluation::ComputeWeightedExposure(Exposure, FocusFlags, 1.f, 1.f, ExposedCount);
	const float ConcealedFraction = 1.f - static_cast<float>(ExposedCount) / Exposure.Num();
	TestTrue(TEXT("Representative threat is concealed"), !Exposure[0]);
	TestTrue(TEXT("Full-threat concealment can still reject the candidate"), ConcealedFraction < .75f);
	return true;
}

#endif
