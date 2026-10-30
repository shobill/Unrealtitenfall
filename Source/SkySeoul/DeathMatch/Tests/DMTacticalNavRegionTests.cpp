#include "Misc/AutomationTest.h"

#include "DeathMatch/Navigation/DMTacticalNavRegionSubsystem.h"
#include "Engine/World.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDMTacticalNavRegionCoreTest,
	"SkySeoul.DeathMatch.TacticalNavRegion.Core",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDMTacticalNavRegionCoreTest::RunTest(const FString&)
{
	TestEqual(TEXT("Origin event keeps full strength"),
		UDMTacticalNavRegionSubsystem::CalculateEventStrength(1.f, 0, .65f, 0.f, 10.f, 1.f), 1.f);
	TestTrue(TEXT("First neighbor is distance-faded"),
		FMath::IsNearlyEqual(UDMTacticalNavRegionSubsystem::CalculateEventStrength(1.f, 1, .65f, 0.f, 10.f, 1.f), .65f));
	TestTrue(TEXT("Time decay reduces a live event"),
		UDMTacticalNavRegionSubsystem::CalculateEventStrength(1.f, 0, .65f, 5.f, 10.f, 1.f) < 1.f);
	TestEqual(TEXT("Expired event contributes nothing"),
		UDMTacticalNavRegionSubsystem::CalculateEventStrength(1.f, 0, .65f, 10.f, 10.f, 1.f), 0.f);
	TestTrue(TEXT("Narrow connection modifier attenuates propagation"),
		UDMTacticalNavRegionSubsystem::CalculateEventStrength(1.f, 1, .65f, 0.f, 10.f, 1.f, .5f) < .65f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDMTacticalNavRegionRuntimeTest,
	"SkySeoul.DeathMatch.TacticalNavRegion.Runtime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDMTacticalNavRegionRuntimeTest::RunTest(const FString&)
{
	FWorldInitializationValues IV;
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("DMTacticalNavRegionRuntime"), nullptr, true, ERHIFeatureLevel::Num, &IV);
	UDMTacticalNavRegionSubsystem* Subsystem = World ? World->GetSubsystem<UDMTacticalNavRegionSubsystem>() : nullptr;
	TestNotNull(TEXT("World creates Tactical Nav Region subsystem"), Subsystem);
	if (!Subsystem) { if (World) World->DestroyWorld(false); return false; }

	Subsystem->BuildState = EDMTacticalNavRegionBuildState::Ready;
	Subsystem->Regions.SetNum(3);
	for (int32 Index = 0; Index < 3; ++Index)
	{
		Subsystem->Regions[Index].RegionId = Index;
		Subsystem->Regions[Index].RepresentativeLocation = FVector(Index * 1000.f, 0.f, 0.f);
		Subsystem->Regions[Index].ReachableTeams.Add(0);
		Subsystem->RegionIndexById.Add(Index, Index);
	}
	auto AddEdge = [&](int32 From, int32 To, float Width)
	{
		FDMTacticalNavRegionConnection Edge;
		Edge.NeighborRegionId = To; Edge.TravelCost = 1000.f; Edge.PortalWidth = Width; Edge.bPassable = true;
		Subsystem->Regions[From].Neighbors.Add(Edge);
	};
	AddEdge(0, 1, 300.f); AddEdge(1, 0, 300.f); AddEdge(1, 2, 80.f); AddEdge(2, 1, 80.f);

	TArray<int32> Path; float Cost = 0.f;
	TestTrue(TEXT("Region path follows explicit NavMesh-derived edges"), Subsystem->FindRegionPath(0, 2, Path, Cost));
	TestEqual(TEXT("Region path contains all connected regions"), Path.Num(), 3);
	TestTrue(TEXT("Path cost accumulates connection travel cost"), FMath::IsNearlyEqual(Cost, 2000.f));

	FDMTacticalRegionEvent Event;
	Event.EventType = EDMTacticalRegionEventType::Threat; Event.SourceTeam = 0; Event.OriginRegionId = 0;
	Event.BaseStrength = 1.f; Event.PropagationDistance = 2; Event.DistanceFalloff = .65f;
	Event.Lifetime = 10.f; Event.TimeDecay = 1.f; Event.Timestamp = World->GetTimeSeconds();
	Subsystem->ActiveEvents.Add(Event);
	Subsystem->RebuildRuntimeScores(World->GetTimeSeconds());
	FDMTacticalNavRegionRuntimeData Origin, Neighbor, Far;
	Subsystem->GetRegionRuntimeData(0, 0, Origin);
	Subsystem->GetRegionRuntimeData(1, 0, Neighbor);
	Subsystem->GetRegionRuntimeData(2, 0, Far);
	TestTrue(TEXT("Origin receives maximum threat"), Origin.ThreatScore > Neighbor.ThreatScore);
	TestTrue(TEXT("Narrow second edge further attenuates threat"), Neighbor.ThreatScore > Far.ThreatScore);
	FDMTacticalNavRegionRuntimeData OtherTeam;
	TestFalse(TEXT("Threat event remains team-scoped"), Subsystem->GetRegionRuntimeData(0, 1, OtherTeam));

	Subsystem->ReportNavPointFailure(1, FVector(1000.f, 0.f, 0.f), EDMTacticalNavFailureReason::Stuck);
	TestTrue(TEXT("Runtime movement failure creates temporary point cooldown"),
		Subsystem->IsNavPointCoolingDown(1, FVector(1000.f, 0.f, 0.f), World->GetTimeSeconds()));

	World->DestroyWorld(false);
	return true;
}

#endif
