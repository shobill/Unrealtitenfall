#include "Misc/AutomationTest.h"

#include "DeathMatch/AI/BT/BTTask_DM_SelectTacticalArea.h"
#include "DeathMatch/AI/Combat/DMCombatPositionProfile.h"
#include "DeathMatch/Core/DeathMatchTeamUtils.h"
#include "DeathMatch/Navigation/DMTacticalAreaSubsystem.h"
#include "DeathMatch/Navigation/DMTacticalAreaVolume.h"
#include "DeathMatch/Tests/DMTacticalAreaTestTypes.h"
#include "Engine/World.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDMTacticalAreaWeightTest,
	"SkySeoul.DeathMatch.Roam.TacticalArea.Weight",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDMTacticalAreaWeightTest::RunTest(const FString&)
{
	TestEqual(TEXT("Preferred danger has maximum fit"),
		UBTTask_DM_SelectTacticalArea::CalculateDangerFit(6.f, 6.f, 4.f), 1.f);
	TestEqual(TEXT("Danger fit falls linearly within tolerance"),
		UBTTask_DM_SelectTacticalArea::CalculateDangerFit(4.f, 6.f, 4.f), .5f);
	TestEqual(TEXT("Danger outside tolerance has zero fit"),
		UBTTask_DM_SelectTacticalArea::CalculateDangerFit(10.f, 6.f, 4.f), 0.f);

	auto Weight = [](int32 Friendly, int32 Incoming, float Distance, bool bRecent, bool bCurrent)
	{
		return UBTTask_DM_SelectTacticalArea::CalculateSelectionWeight(6.f, 6.f, 4.f,
			Friendly, Incoming, Distance, bRecent, bCurrent,
			1.f, 4.f, .5f, .75f, .15f, 2.f, 2.f, .05f);
	};
	const float Unpenalized = Weight(0, 0, 0.f, false, false);
	TestTrue(TEXT("Friendly crowding lowers weight"), Weight(1, 0, 0.f, false, false) < Unpenalized);
	TestTrue(TEXT("Incoming AI lowers weight"), Weight(0, 1, 0.f, false, false) < Unpenalized);
	TestTrue(TEXT("Distance lowers weight"), Weight(0, 0, 1000.f, false, false) < Unpenalized);
	TestTrue(TEXT("Recent visit lowers weight"), Weight(0, 0, 0.f, true, false) < Unpenalized);
	TestTrue(TEXT("Current area lowers weight"), Weight(0, 0, 0.f, false, true) < Unpenalized);
	const float Minimum = UBTTask_DM_SelectTacticalArea::CalculateSelectionWeight(0.f, 6.f, 4.f,
		100, 100, 100000.f, true, true, 1.f, 4.f, .5f, .75f, .15f, 2.f, 2.f, .05f);
	TestEqual(TEXT("All valid candidates retain minimum selection weight"), Minimum, .05f);

	FRandomStream Stream(1337);
	const TArray<float> WeightedChoices{6.f, 3.f, 1.f};
	int32 Counts[3]{0, 0, 0};
	for (int32 Draw = 0; Draw < 10000; ++Draw)
	{
		const int32 Index = UBTTask_DM_SelectTacticalArea::SelectWeightedIndex(WeightedChoices, Stream);
		if (Index >= 0 && Index < 3) ++Counts[Index];
	}
	TestTrue(TEXT("Higher weight is selected statistically more often"), Counts[0] > Counts[1] && Counts[1] > Counts[2]);
	TestTrue(TEXT("Highest weight is not a guaranteed selection"), Counts[0] < 10000);
	TestTrue(TEXT("Minimum-weight candidate remains selectable"), Counts[2] > 0);

	FRandomStream RetryStream(7);
	TArray<int32> CandidateIds{10, 20, 30};
	TArray<float> RetryWeights{6.f, 3.f, 1.f};
	const int32 FailedIndex = UBTTask_DM_SelectTacticalArea::SelectWeightedIndex(RetryWeights, RetryStream);
	const int32 FailedId = CandidateIds[FailedIndex];
	CandidateIds.RemoveAtSwap(FailedIndex);
	RetryWeights.RemoveAtSwap(FailedIndex);
	const int32 RetryIndex = UBTTask_DM_SelectTacticalArea::SelectWeightedIndex(RetryWeights, RetryStream);
	TestTrue(TEXT("Removing a failed area before re-roll prevents same-request reselection"),
		CandidateIds.IsValidIndex(RetryIndex) && CandidateIds[RetryIndex] != FailedId);

	const float BiasA = UBTTask_DM_SelectTacticalArea::CalculatePersonalDangerBias(4242, 1.f);
	const float BiasB = UBTTask_DM_SelectTacticalArea::CalculatePersonalDangerBias(4242, 1.f);
	TestEqual(TEXT("Per-AI danger bias is stable for a stable seed"), BiasA, BiasB);
	TestTrue(TEXT("Per-AI danger bias stays in the configured range"), FMath::Abs(BiasA) <= 1.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDMTacticalAreaDangerTest,
	"SkySeoul.DeathMatch.Roam.TacticalArea.Danger",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDMTacticalAreaDangerTest::RunTest(const FString&)
{
	UWorld::InitializationValues Values;
	Values.AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("DMTacticalAreaDanger"),
		nullptr, true, ERHIFeatureLevel::Num, &Values);
	UDMTacticalAreaSubsystem* Areas = World ? World->GetSubsystem<UDMTacticalAreaSubsystem>() : nullptr;
	ADMTacticalAreaVolume* SpawnArea = World ? World->SpawnActor<ADMTacticalAreaVolume>() : nullptr;
	ADMTacticalAreaVolume* DynamicArea = World ? World->SpawnActor<ADMTacticalAreaVolume>() : nullptr;
	TestNotNull(TEXT("Danger test creates subsystem"), Areas);
	TestNotNull(TEXT("Danger test creates spawn area"), SpawnArea);
	TestNotNull(TEXT("Danger test creates dynamic area"), DynamicArea);
	if (!Areas || !SpawnArea || !DynamicArea)
	{
		if (World) World->DestroyWorld(false);
		return false;
	}
	SpawnArea->SetActorLocation(FVector::ZeroVector);
	DynamicArea->SetActorLocation(FVector(5000.f, 0.f, 0.f));
	Areas->RegisterVolume(SpawnArea);
	Areas->RegisterVolume(DynamicArea);
	Areas->RegisterSpawnTeamAtLocation(1, SpawnArea->GetActorLocation());
	Areas->RegisterSpawnTeamAtLocation(255, SpawnArea->GetActorLocation());
	TestEqual(TEXT("Team A spawn is base danger 10 for Team B"), SpawnArea->GetTeamBaseDanger(2), 10.f);
	TestEqual(TEXT("Team A spawn adds no base danger for Team A"), SpawnArea->GetTeamBaseDanger(1), 0.f);
	TestFalse(TEXT("NoTeam is never registered as a spawn team"), SpawnArea->HasSpawnTeam(255));

	Areas->ReportEnemySeen(2, SpawnArea->GetActorLocation(), 3.f);
	TestEqual(TEXT("Enemy spawn base danger survives lower dynamic danger"), SpawnArea->GetTeamDanger(2), 10.f);
	TestEqual(TEXT("Dynamic danger remains independently queryable"), SpawnArea->GetTeamDynamicDanger(2), 3.f);
	SpawnArea->AddDanger(2, -3.f);
	TestEqual(TEXT("Enemy spawn base danger remains after dynamic danger decreases"), SpawnArea->GetTeamDanger(2), 10.f);
	Areas->ReportEnemySeen(1, DynamicArea->GetActorLocation(), 100.f);
	Areas->ReportDamageReceived(1, DynamicArea->GetActorLocation(), 100.f);
	TestEqual(TEXT("Dynamic danger is clamped to 10"), DynamicArea->GetTeamDynamicDanger(1), 10.f);
	TestEqual(TEXT("Combat-position danger reads the evaluating team's value"),
		DMCombatPositionEvaluation::ResolveAreaDanger(DynamicArea, 1, 5.f), 10.f);
	TestEqual(TEXT("Combat-position danger does not mix another team's value"),
		DMCombatPositionEvaluation::ResolveAreaDanger(DynamicArea, 2, 5.f), 0.f);
	TestTrue(TEXT("Final danger remains in the 0..10 range"),
		DynamicArea->GetTeamDanger(1) >= 0.f && DynamicArea->GetTeamDanger(1) <= 10.f);
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDMTacticalAreaBoundsTest,
	"SkySeoul.DeathMatch.Roam.TacticalArea.Bounds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDMTacticalAreaBoundsTest::RunTest(const FString&)
{
	UWorld::InitializationValues Values;
	Values.AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("DMTacticalAreaBounds"),
		nullptr, true, ERHIFeatureLevel::Num, &Values);
	ADMTacticalAreaVolume* Volume = World ? World->SpawnActor<ADMTacticalAreaVolume>() : nullptr;
	TestNotNull(TEXT("Tactical area can be spawned"), Volume);
	if (!Volume)
	{
		if (World) World->DestroyWorld(false);
		return false;
	}

	UBoxComponent* Bounds = Volume->GetAreaBounds();
	TestNotNull(TEXT("AreaBounds is created"), Bounds);
	TestTrue(TEXT("AreaBounds is the root component"), Bounds && Volume->GetRootComponent() == Bounds);
	TestTrue(TEXT("Default box extent is positive"), Bounds && Bounds->GetUnscaledBoxExtent().GetMin() > 0.f);
	TestTrue(TEXT("Box center is inside"), Volume->IsPointInsideVolume(Volume->GetRepresentativeLocation()));
	TestFalse(TEXT("Point beyond the default X extent is outside"),
		Volume->IsPointInsideVolume(Volume->GetRepresentativeLocation() + FVector(501.f, 0.f, 0.f)));

	Volume->SetActorTransform(FTransform(FRotator(15.f, 45.f, 10.f), FVector(120.f, -80.f, 40.f), FVector(2.f, .5f, 1.5f)));
	Bounds->SetBoxExtent(FVector(300.f, 200.f, 100.f));
	const FTransform BoundsTransform = Bounds->GetComponentTransform();
	TestTrue(TEXT("Rotated and scaled local point remains inside"),
		Volume->IsPointInsideVolume(BoundsTransform.TransformPosition(FVector(299.f, 199.f, 99.f))));
	TestFalse(TEXT("Rotated and scaled point beyond local extent is outside"),
		Volume->IsPointInsideVolume(BoundsTransform.TransformPosition(FVector(301.f, 0.f, 0.f))));

	Bounds->SetBoxExtent(FVector(100.f, 100.f, 100.f));
	const float SmallRadius = Volume->GetSearchRadius();
	Bounds->SetBoxExtent(FVector(600.f, 400.f, 100.f));
	TestTrue(TEXT("Search radius grows with AreaBounds"), Volume->GetSearchRadius() > SmallRadius);
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDMTacticalAreaOverlapTest,
	"SkySeoul.DeathMatch.Roam.TacticalArea.Overlap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDMTacticalAreaOverlapTest::RunTest(const FString&)
{
	UWorld::InitializationValues Values;
	Values.AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("DMTacticalAreaOverlap"),
		nullptr, true, ERHIFeatureLevel::Num, &Values);
	UDMTacticalAreaSubsystem* Areas = World ? World->GetSubsystem<UDMTacticalAreaSubsystem>() : nullptr;
	ADMTacticalAreaVolume* Volume = World ? World->SpawnActor<ADMTacticalAreaVolume>() : nullptr;
	ADMTacticalAreaTestPawn* Pawn = World ? World->SpawnActor<ADMTacticalAreaTestPawn>() : nullptr;
	TestNotNull(TEXT("Overlap test creates Tactical Area subsystem"), Areas);
	TestNotNull(TEXT("Overlap test creates volume"), Volume);
	TestNotNull(TEXT("Overlap test creates pawn"), Pawn);
	if (!Areas || !Volume || !Pawn)
	{
		if (World) World->DestroyWorld(false);
		return false;
	}

	Areas->RegisterVolume(Volume);
	Pawn->SetGenericTeamId(FGenericTeamId(1));
	UBoxComponent* FirstBody = NewObject<UBoxComponent>(Pawn, TEXT("FirstOverlapBody"));
	UBoxComponent* SecondBody = NewObject<UBoxComponent>(Pawn, TEXT("SecondOverlapBody"));
	UBoxComponent* Bounds = Volume->GetAreaBounds();
	FHitResult SweepResult;
	TestEqual(TEXT("Test Pawn exposes a valid team"), DeathMatchTeamUtils::GetTeamIdFromActor(Pawn).GetId(), uint8(1));
	TestTrue(TEXT("Synthetic overlap components are valid"), IsValid(FirstBody) && IsValid(SecondBody));
	TestTrue(TEXT("AreaBounds begin-overlap delegate is bound"), Bounds->OnComponentBeginOverlap.IsBound());
	TestTrue(TEXT("AreaBounds end-overlap delegate is bound"), Bounds->OnComponentEndOverlap.IsBound());
	TestTrue(TEXT("Begin-overlap delegate targets the spawned volume"),
		Bounds->OnComponentBeginOverlap.Contains(Volume, TEXT("HandleAreaBeginOverlap")));

	Volume->HandleAreaBeginOverlap(Bounds, Pawn, FirstBody, 0, false, SweepResult);
	Volume->HandleAreaBeginOverlap(Bounds, Pawn, SecondBody, 0, false, SweepResult);
	TestEqual(TEXT("Multiple Pawn components count the Actor once"), Volume->GetFriendlyCount(1), 1);
	Volume->HandleAreaEndOverlap(Bounds, Pawn, FirstBody, 0);
	TestEqual(TEXT("One component ending does not remove an Actor still overlapping"), Volume->GetFriendlyCount(1), 1);
	Volume->HandleAreaEndOverlap(Bounds, Pawn, SecondBody, 0);
	TestEqual(TEXT("Last component ending removes occupancy"), Volume->GetFriendlyCount(1), 0);

	Volume->HandleAreaBeginOverlap(Bounds, Pawn, FirstBody, 0, false, SweepResult);
	TestEqual(TEXT("Pawn can enter again"), Volume->GetFriendlyCount(1), 1);
	Pawn->OnDestroyed.Broadcast(Pawn);
	TestEqual(TEXT("Actor destruction removes occupancy"), Volume->GetFriendlyCount(1), 0);
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDMTacticalAreaReservationTest,
	"SkySeoul.DeathMatch.Roam.TacticalArea.Reservation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDMTacticalAreaReservationTest::RunTest(const FString&)
{
	UWorld::InitializationValues Values;
	Values.AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("DMTacticalAreaReservation"),
		nullptr, true, ERHIFeatureLevel::Num, &Values);
	UDMTacticalAreaSubsystem* Areas = World ? World->GetSubsystem<UDMTacticalAreaSubsystem>() : nullptr;
	TestNotNull(TEXT("World creates Tactical Area subsystem"), Areas);
	if (!Areas)
	{
		if (World) World->DestroyWorld(false);
		return false;
	}

	ADMTacticalAreaVolume* First = World->SpawnActor<ADMTacticalAreaVolume>();
	ADMTacticalAreaVolume* Second = World->SpawnActor<ADMTacticalAreaVolume>();
	AActor* AIActor = World->SpawnActor<AActor>();
	Areas->RegisterVolume(First);
	Areas->RegisterVolume(Second);
	First->SetActorLocation(FVector::ZeroVector);
	Second->SetActorLocation(FVector(5000.f, 0.f, 0.f));
	Areas->ReportEnemySeen(1, First->GetActorLocation(), 7.f);
	TestEqual(TEXT("Danger is scoped to the observing team"), First->GetTeamDynamicDanger(1), 7.f);
	TestEqual(TEXT("Other teams do not inherit the danger value"), First->GetTeamDanger(2), 0.f);

	TestTrue(TEXT("First reservation succeeds"), Areas->ReserveIncomingAI(AIActor, 1, First));
	TestEqual(TEXT("Reservation increments incoming count"), First->GetIncomingAICount(1), 1);
	TestTrue(TEXT("Same reservation is idempotent"), Areas->ReserveIncomingAI(AIActor, 1, First));
	TestEqual(TEXT("Idempotent reservation does not double count"), First->GetIncomingAICount(1), 1);
	TestTrue(TEXT("Changing target transfers reservation"), Areas->ReserveIncomingAI(AIActor, 1, Second));
	TestEqual(TEXT("Old target reservation is released"), First->GetIncomingAICount(1), 0);
	TestEqual(TEXT("New target owns one reservation"), Second->GetIncomingAICount(1), 1);

	// Exercise the same lifecycle delegate without requiring an engine world context in this unit world.
	AIActor->OnDestroyed.Broadcast(AIActor);
	TestEqual(TEXT("Actor destruction cannot leak incoming count"), Second->GetIncomingAICount(1), 0);
	World->DestroyWorld(false);
	return true;
}

#endif
