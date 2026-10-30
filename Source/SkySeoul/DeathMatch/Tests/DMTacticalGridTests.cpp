#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "GameFramework/PlayerStart.h"
#include "Components/SceneComponent.h"
#include "DeathMatch/Navigation/DMTacticalGridSubsystem.h"
#include "DeathMatch/PlayerStart/PlayerStartByTeam.h"

namespace
{
FDMTacticalCellStaticData MakeCell(int32 X, int32 Region, const FVector& Location)
{
    FDMTacticalCellStaticData Cell;
    Cell.Id = {X, 0, 0, Region}; Cell.CellType = EDMTacticalCellType::Normal;
    Cell.RepresentativeLocation = Location; Cell.Height = Location.Z; Cell.WalkableRatio = 1.f;
    Cell.Openness = .5f; Cell.bReachableFromAnySpawn = true; Cell.ReachableTeamMask = -1;
    Cell.bTraversable = true; Cell.ValidNavSampleCount = 1; Cell.ValidSamples.Add(Location);
    Cell.bNavMeshProjectionSucceeded = true; Cell.bHasValidGridConnection = true;
    Cell.bReachableFromTeamARepresentativeSpawn = true;
    Cell.bReachableFromTeamBRepresentativeSpawn = true; Cell.bPlayableStrategicCell = true;
    Cell.Bounds = FBox(Location - FVector(290, 290, 120), Location + FVector(290, 290, 120));
    return Cell;
}

void Connect(FDMTacticalCellStaticData& A, FDMTacticalCellStaticData& B, float Cost = 600.f)
{
    FDMTacticalConnection AB; AB.Neighbor = B.Id; AB.TravelCost = AB.ActualPathLength = Cost;
    AB.bHasValidNavPath = true; AB.bAgentCapsulePassable = true; A.Connections.Add(AB);
    FDMTacticalConnection BA = AB; BA.Neighbor = A.Id; B.Connections.Add(BA);
    A.ConnectionCount = A.EscapeRouteCount = A.Connections.Num();
    B.ConnectionCount = B.EscapeRouteCount = B.Connections.Num();
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDMTacticalGridCoreTest, "SkySeoul.DeathMatch.TacticalGrid.Core",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDMTacticalGridCoreTest::RunTest(const FString&)
{
    const FDMTacticalCellId A{10, 5, 0, 3};
    const FDMTacticalCellId Same{10, 5, 0, 3};
    const FDMTacticalCellId OtherLayer{10, 5, 1, 3};
    const FDMTacticalCellId OtherRegion{10, 5, 0, 4};
    TestTrue(TEXT("Cell ID equality is stable"), A == Same);
    TestEqual(TEXT("Equal Cell IDs hash equally"), GetTypeHash(A), GetTypeHash(Same));
    TestTrue(TEXT("Same XY on another layer is a different cell"), A != OtherLayer);
    TestTrue(TEXT("Disconnected region is part of identity"), A != OtherRegion);

    TestEqual(TEXT("Ratio below .20 is blocked"), UDMTacticalGridSubsystem::ClassifyCellType(.19f, 1.f), EDMTacticalCellType::Blocked);
    TestEqual(TEXT("Ratio .20-.50 is narrow"), UDMTacticalGridSubsystem::ClassifyCellType(.35f, .5f), EDMTacticalCellType::Narrow);
    TestEqual(TEXT("Ratio .50-.80 is partial"), UDMTacticalGridSubsystem::ClassifyCellType(.65f, .5f), EDMTacticalCellType::Partial);
    TestEqual(TEXT("High ratio with limited clearance is normal"), UDMTacticalGridSubsystem::ClassifyCellType(.9f, .4f), EDMTacticalCellType::Normal);
    TestEqual(TEXT("High ratio with high openness is open"), UDMTacticalGridSubsystem::ClassifyCellType(.9f, .9f), EDMTacticalCellType::Open);
    TestEqual(TEXT("Observation confidence decays by time"), UDMTacticalGridSubsystem::CalculateDecayedObservation(.8f, 5.f, .1f), .3f);
    TestEqual(TEXT("Observation confidence clamps at zero"), UDMTacticalGridSubsystem::CalculateDecayedObservation(.2f, 5.f, .1f), 0.f);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDMTacticalGridRuntimeTest, "SkySeoul.DeathMatch.TacticalGrid.Runtime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDMTacticalGridRuntimeTest::RunTest(const FString&)
{
    UWorld::InitializationValues IV;
    IV.AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("DMTacticalGridRuntime"), nullptr, true, ERHIFeatureLevel::Num, &IV);
    UDMTacticalGridSubsystem* Grid = World ? World->GetSubsystem<UDMTacticalGridSubsystem>() : nullptr;
    if (!TestNotNull(TEXT("World owns one tactical grid subsystem"), Grid)) { if (World) World->DestroyWorld(false); return false; }

    Grid->BuildState = EDMTacticalGridBuildState::Ready;
    Grid->Cells = {MakeCell(0, 0, FVector::ZeroVector), MakeCell(1, 0, FVector(600, 0, 0)),
        MakeCell(2, 0, FVector(1200, 0, 0)), MakeCell(3, 1, FVector(1800, 0, 0))};
    Connect(Grid->Cells[0], Grid->Cells[1]); Connect(Grid->Cells[1], Grid->Cells[2]); Grid->RebuildCellIndex();
    Grid->PendingConnections.SetNum(Grid->Cells.Num());
    UDMTacticalGridSubsystem::FPendingConnection Edge01; Edge01.NeighborIndex = 1; Edge01.Cost = 600.f;
    UDMTacticalGridSubsystem::FPendingConnection Edge10 = Edge01; Edge10.NeighborIndex = 0;
    UDMTacticalGridSubsystem::FPendingConnection Edge12 = Edge01; Edge12.NeighborIndex = 2;
    UDMTacticalGridSubsystem::FPendingConnection Edge21 = Edge01; Edge21.NeighborIndex = 1;
    Grid->PendingConnections[0].Add(Edge01); Grid->PendingConnections[1].Add(Edge10);
    Grid->PendingConnections[1].Add(Edge12); Grid->PendingConnections[2].Add(Edge21);

    const TArray<FDMTacticalCellStaticData> AvailabilityFixture = Grid->Cells;
    Grid->StrategicTeamA = 1; Grid->StrategicTeamB = 2;
    for (FDMTacticalCellStaticData& Cell : Grid->Cells) Cell.ReachableTeamMask = (1 << 1) | (1 << 2);
    Grid->Cells[1].ReachableTeamMask = (1 << 1);
    Grid->Cells[2].ValidNavSampleCount = 0; Grid->Cells[2].ValidSamples.Reset();
    Grid->FinalizeStrategicCellAvailability();
    TestTrue(TEXT("Cell reachable from both representative spawns is strategically playable"), Grid->Cells[0].bPlayableStrategicCell);
    TestFalse(TEXT("Cell reachable from only one team is excluded by the default policy"), Grid->Cells[1].bPlayableStrategicCell);
    TestTrue(TEXT("One-team cell records the missing team reason"),
        Grid->Cells[1].StrategicExclusionReasons.Contains(EDMTacticalStrategicCellExclusionReason::UnreachableFromTeamB));
    TestFalse(TEXT("NavMesh projection failure excludes a strategic cell"), Grid->Cells[2].bPlayableStrategicCell);
    TestTrue(TEXT("NavMesh projection failure reason is retained"),
        Grid->Cells[2].StrategicExclusionReasons.Contains(EDMTacticalStrategicCellExclusionReason::NoNavProjection));
    TestFalse(TEXT("Disconnected region is not a strategic destination"), Grid->Cells[3].bPlayableStrategicCell);
    TestTrue(TEXT("Disconnected region records missing connection"),
        Grid->Cells[3].StrategicExclusionReasons.Contains(EDMTacticalStrategicCellExclusionReason::NoValidConnection));
    Grid->StrategicTeamB = 255; Grid->FinalizeStrategicCellAvailability();
    TestTrue(TEXT("Missing second representative spawn is diagnosable"),
        Grid->Cells[0].StrategicExclusionReasons.Contains(EDMTacticalStrategicCellExclusionReason::InvalidSpawnReference));
    Grid->Cells = AvailabilityFixture; Grid->RebuildCellIndex();

    for (FDMTacticalCellStaticData& Cell : Grid->Cells) { Cell.bReachableFromAnySpawn = false; Cell.ReachableTeamMask = 0; }
    Grid->StrategicTeamA = 1; Grid->StrategicTeamB = 2; Grid->bHadSpawn = true;
    Grid->RepresentativeSpawnCellByTeam.Add(1, 0); Grid->RepresentativeSpawnCellByTeam.Add(2, 0);
    Grid->ReachabilityVisited.Add((static_cast<uint64>(1) << 32) | 0u);
    Grid->ReachabilityVisited.Add((static_cast<uint64>(2) << 32) | 0u);
    Grid->ReachabilityQueue.Add({0, 1}); Grid->ReachabilityQueue.Add({0, 2});
    Grid->BuildState = EDMTacticalGridBuildState::MarkingReachability;
    while (Grid->BuildState == EDMTacticalGridBuildState::MarkingReachability) Grid->MarkReachability();
    TestTrue(TEXT("Spawn flood fill marks connected start region"), Grid->Cells[2].bReachableFromAnySpawn);
    TestFalse(TEXT("Distance alone does not connect a separate wall/island region"), Grid->Cells[3].bReachableFromAnySpawn);
    Grid->BuildState = EDMTacticalGridBuildState::Ready;

    auto SpawnTracked = [World](const FVector& Location)
    {
        AActor* Actor = World->SpawnActor<AActor>();
        USceneComponent* Root = NewObject<USceneComponent>(Actor); Actor->SetRootComponent(Root); Root->RegisterComponent();
        Actor->SetActorLocation(Location); return Actor;
    };

    AActor* AllyA = SpawnTracked(FVector::ZeroVector);
    AActor* AllyB = SpawnTracked(FVector(50, 0, 0));
    AActor* Enemy = SpawnTracked(FVector(1200, 0, 0));
    Grid->RegisterActor(AllyA, 1, true); Grid->RegisterActor(AllyB, 1, false); Grid->RegisterActor(Enemy, 2, true);
    Grid->RebuildDynamicCounts(0.f);
    FDMTacticalCellDynamicData TeamOneEnemyCell;
    TestTrue(TEXT("Enemy cell has team runtime data"), Grid->GetTeamCellData(1, Grid->Cells[2].Id, TeamOneEnemyCell));
    TestEqual(TEXT("Physical enemy count is debug-only spatial truth"), TeamOneEnemyCell.PhysicalEnemyCount, 1);
    TestEqual(TEXT("Physical presence does not become AI knowledge"), TeamOneEnemyCell.KnownThreatCount, 0);

    Grid->ReportThreat(1, AllyA, Enemy, Enemy->GetActorLocation(), true, false, true, 1.f);
    Grid->ReportThreat(1, AllyB, Enemy, Enemy->GetActorLocation(), true, false, true, 1.f);
    Grid->RebuildDynamicCounts(0.f);
    Grid->GetTeamCellData(1, Grid->Cells[2].Id, TeamOneEnemyCell);
    TestEqual(TEXT("Two allies knowing the same actor produce one unique known threat"), TeamOneEnemyCell.KnownThreatCount, 1);
    TestEqual(TEXT("Unique visible threat is tracked separately"), TeamOneEnemyCell.VisibleThreatCount, 1);
    TestEqual(TEXT("Engagement threat stays unique"), TeamOneEnemyCell.EngagementThreatCount, 1);

    Grid->Settings.Runtime.MaximumReservationsPerCell = 1;
    TestTrue(TEXT("First exploration reservation succeeds"), Grid->ReserveCell(1, AllyA, Grid->Cells[1].Id, EDMTacticalPurpose::Explore, 5.f));
    TestFalse(TEXT("Exclusive exploration reservation rejects duplicate actor"), Grid->ReserveCell(1, AllyB, Grid->Cells[1].Id, EDMTacticalPurpose::Explore, 5.f));
    Grid->ReleaseReservations(AllyA);
    TestTrue(TEXT("Released tactical reservation becomes available"), Grid->ReserveCell(1, AllyB, Grid->Cells[1].Id, EDMTacticalPurpose::Explore, 5.f));
    TestTrue(TEXT("Actor can retain a separate exploration reservation"), Grid->ReserveCell(1, AllyA, Grid->Cells[0].Id, EDMTacticalPurpose::Explore, 5.f));
    TestFalse(TEXT("Changing reservation cannot bypass exclusive-cell capacity"),
        Grid->ReserveCell(1, AllyA, Grid->Cells[1].Id, EDMTacticalPurpose::Explore, 5.f));
    TestTrue(TEXT("Rejected reservation change preserves the previous cell"),
        Grid->Reservations.ContainsByPredicate([&](const FDMTacticalCellReservation& R)
            { return R.Actor.Get() == AllyA && R.CellId == Grid->Cells[0].Id; }));

    Grid->RecordFailure(AllyA, 1, Grid->Cells[1].Id, TEXT("AttackPosition"), EDMTacticalPurpose::Attack, EDMTacticalFailureReason::NoLineOfSight, 5.f);
    TestTrue(TEXT("Failure applies to matching cell/action/purpose"), Grid->IsFailureCoolingDown(AllyA, 1, Grid->Cells[1].Id, TEXT("AttackPosition"), EDMTacticalPurpose::Attack));
    TestFalse(TEXT("Failure does not disable another action in the same cell"), Grid->IsFailureCoolingDown(AllyA, 1, Grid->Cells[1].Id, TEXT("CombatCover"), EDMTacticalPurpose::Attack));
    TestFalse(TEXT("Failure does not disable another purpose in the same cell"), Grid->IsFailureCoolingDown(AllyA, 1, Grid->Cells[1].Id, TEXT("AttackPosition"), EDMTacticalPurpose::Hide));

    Grid->ReleaseReservations(AllyA); Grid->ReleaseReservations(AllyB);
    FDMTacticalCellDynamicData& RecentZero = Grid->FindOrAddDynamic(1, Grid->Cells[0].Id);
    RecentZero.FriendlyActorCount = 3; RecentZero.ObservationConfidence = 1.f;
    RecentZero.LastFriendlyVisitTime = RecentZero.LastFriendlyObservationTime = 60.f;
    FDMTacticalCellDynamicData& RecentOne = Grid->FindOrAddDynamic(1, Grid->Cells[1].Id);
    RecentOne.FriendlyCombatantCount = 1; RecentOne.ObservationConfidence = 1.f;
    RecentOne.LastFriendlyVisitTime = RecentOne.LastFriendlyObservationTime = 60.f;
    FDMTacticalCellDynamicData& Unvisited = Grid->FindOrAddDynamic(1, Grid->Cells[2].Id);
    Unvisited.ObservationConfidence = 0.f; Unvisited.LastFriendlyVisitTime = -1.f; Unvisited.LastFriendlyObservationTime = -1.f;

    FDMTacticalEvaluationContext Visible;
    Visible.State = EDMTacticalEvaluationState::Visible; Visible.bInCombat = true; Visible.bHasVisibleEnemy = true;
    Visible.Now = 60.f; Visible.Personality.ExplorationPreference = 1.f; Visible.Personality.CombatPreference = -1.f;
    TArray<FDMTacticalCellScore> VisibleScores;
    TestTrue(TEXT("Visible state produces bounded graph candidates"), Grid->FindTacticalCandidates(AllyA, 1, Visible, 2, 3, VisibleScores));
    TestTrue(TEXT("Candidate search never exceeds requested top count"), VisibleScores.Num() <= 3);
    for (const FDMTacticalCellScore& Score : VisibleScores)
    {
        TestEqual(TEXT("Extreme exploration cannot enable Visible exploration weight"), Score.AppliedWeights.ExplorationWeight, 0.f);
        TestTrue(TEXT("Visible purpose remains combat-family"), Score.Purpose == EDMTacticalPurpose::Attack
            || Score.Purpose == EDMTacticalPurpose::Join || Score.Purpose == EDMTacticalPurpose::Ambush);
    }

    FDMTacticalEvaluationContext DefensiveVisible = Visible;
    DefensiveVisible.HealthRisk = 1.f; DefensiveVisible.RecentDamagePressure = 1.f;
    TArray<FDMTacticalCellScore> DefensiveScores;
    TestTrue(TEXT("Low-health Visible state still produces candidates"),
        Grid->FindTacticalCandidates(AllyA, 1, DefensiveVisible, 2, 3, DefensiveScores));
    const FDMTacticalCellScore* NormalAdvance = VisibleScores.FindByPredicate([&](const FDMTacticalCellScore& Score)
        { return Score.CellId == Grid->Cells[2].Id; });
    const FDMTacticalCellScore* DefensiveAdvance = DefensiveScores.FindByPredicate([&](const FDMTacticalCellScore& Score)
        { return Score.CellId == Grid->Cells[2].Id; });
    if (NormalAdvance && DefensiveAdvance)
    {
        TestTrue(TEXT("Low health and damage suppress supported advance"),
            DefensiveAdvance->SupportedAdvanceScore < NormalAdvance->SupportedAdvanceScore);
        TestTrue(TEXT("Low health and damage preserve defensive exposure correction"),
            DefensiveAdvance->KnownEnemyExposure > NormalAdvance->KnownEnemyExposure);
    }

    FDMTacticalEvaluationContext Lost;
    Lost.State = EDMTacticalEvaluationState::LostGrace; Lost.Now = 60.f;
    Lost.bHasRememberedEnemy = true; Lost.bHasLastKnownThreatLocation = true;
    Lost.LastKnownThreatLocation = Grid->Cells[2].RepresentativeLocation;
    Lost.Personality.ExplorationPreference = 1.f;
    TArray<FDMTacticalCellScore> LostScores;
    TestTrue(TEXT("LostGrace produces candidates"), Grid->FindTacticalCandidates(AllyA, 1, Lost, 2, 3, LostScores));
    for (const FDMTacticalCellScore& Score : LostScores)
    {
        TestTrue(TEXT("LostGrace never chooses unrelated Explore purpose"), Score.Purpose != EDMTacticalPurpose::Explore);
        TestTrue(TEXT("LostGrace keeps exploration weight low under extreme personality"),
            Score.AppliedWeights.ExplorationWeight <= .135f + KINDA_SMALL_NUMBER);
    }

    FDMTacticalEvaluationContext Pending = Lost;
    Pending.State = EDMTacticalEvaluationState::PendingSwitch;
    Pending.bHasCurrentCell = Pending.bHasCurrentSelection = true;
    Pending.CurrentCell = Pending.CurrentSelectedCell = Grid->Cells[0].Id;
    Pending.CurrentPurpose = EDMTacticalPurpose::Attack;
    TArray<FDMTacticalCellScore> PendingScores;
    TestTrue(TEXT("PendingSwitch produces candidates"), Grid->FindTacticalCandidates(AllyA, 1, Pending, 2, 3, PendingScores));
    const FDMTacticalCellScore* Retained = PendingScores.FindByPredicate([&](const FDMTacticalCellScore& Score)
        { return Score.CellId == Grid->Cells[0].Id; });
    if (Retained) TestTrue(TEXT("PendingSwitch rewards current-cell retention"), Retained->CurrentCellRetentionScore > 0.f);

    FDMTacticalEvaluationContext Roam;
    Roam.State = EDMTacticalEvaluationState::DisengagedRoam; Roam.Now = 60.f;
    Roam.Personality.CombatPreference = 1.f; Roam.Personality.ExplorationPreference = -1.f;
    TArray<FDMTacticalCellScore> RoamScores;
    TestTrue(TEXT("Roam state produces candidates"), Grid->FindTacticalCandidates(AllyA, 1, Roam, 2, 3, RoamScores));
    if (!RoamScores.IsEmpty())
    {
        TestEqual(TEXT("Extreme combat cannot enable Roam combat weight"), RoamScores[0].AppliedWeights.CombatOpportunityWeight, 0.f);
        TestTrue(TEXT("Roam keeps a strategic noncombat purpose"), RoamScores[0].Purpose == EDMTacticalPurpose::ExploreFrontier
            || RoamScores[0].Purpose == EDMTacticalPurpose::SpawnControl);
        TestTrue(TEXT("Roam prioritizes the low-confidence never-visited cell"), RoamScores[0].CellId == Grid->Cells[2].Id);
        const FDMTacticalCellScore* UnvisitedScore = RoamScores.FindByPredicate([&](const FDMTacticalCellScore& Score)
            { return Score.CellId == Grid->Cells[2].Id; });
        if (UnvisitedScore) TestTrue(TEXT("Never-visited Roam cell receives stale-visit value"), UnvisitedScore->StaleVisitScore > 0.f);
    }

    FDMTacticalEvaluationContext Investigating;
    Investigating.State = EDMTacticalEvaluationState::Investigating; Investigating.Now = 60.f;
    Investigating.bHasRememberedEnemy = true; Investigating.bHasLastKnownThreatLocation = true;
    Investigating.LastKnownThreatLocation = Grid->Cells[2].RepresentativeLocation;
    TArray<FDMTacticalCellScore> InvestigationScores;
    TestTrue(TEXT("Investigating state produces candidates"), Grid->FindTacticalCandidates(AllyA, 1, Investigating, 2, 3, InvestigationScores));
    const FDMTacticalCellScore* LastKnownScore = InvestigationScores.FindByPredicate([&](const FDMTacticalCellScore& Score)
        { return Score.CellId == Grid->Cells[2].Id; });
    if (LastKnownScore) TestTrue(TEXT("Investigating rewards the last-known cell"), LastKnownScore->LastKnownThreatScore > 0.f);

    FDMTacticalEvaluationContext Watching = Investigating;
    Watching.State = EDMTacticalEvaluationState::WatchingLastKnown;
    TArray<FDMTacticalCellScore> WatchScores;
    TestTrue(TEXT("Watching-last-known state produces candidates"), Grid->FindTacticalCandidates(AllyA, 1, Watching, 2, 3, WatchScores));
    for (const FDMTacticalCellScore& Score : WatchScores)
    {
        TestEqual(TEXT("Watching suppresses supported advance"), Score.AppliedWeights.SupportedAdvanceWeight, 0.f);
        TestTrue(TEXT("Watching never chooses direct Attack"), Score.Purpose != EDMTacticalPurpose::Attack);
    }

    const FDMTacticalScoreWeights Corrected = UDMTacticalGridSubsystem::ApplyPersonalityCorrection(
        Grid->Settings.StateProfiles.Visible, Visible.Personality);
    TestTrue(TEXT("Personality correction stays within lower 35 percent bound"),
        Corrected.CombatOpportunityWeight >= Grid->Settings.StateProfiles.Visible.CombatOpportunityWeight * .65f);
    TestTrue(TEXT("Personality correction stays within upper 35 percent bound"),
        Corrected.AmbushWeight <= Grid->Settings.StateProfiles.Visible.AmbushWeight * 1.35f + KINDA_SMALL_NUMBER);

    AllyB->SetActorLocation(FVector(600, 0, 0));
    TestTrue(TEXT("Registered actor can transfer tactical cells"), Grid->UpdateActorCell(AllyB, 1, false));
    TestTrue(TEXT("Actor transfer updates stored cell"), Grid->RegisteredActors.FindChecked(AllyB).CellId == Grid->Cells[1].Id);
    Grid->UnregisterActor(AllyB);
    TestFalse(TEXT("Destroyed/unpossessed actor removal clears registry"), Grid->RegisteredActors.Contains(AllyB));

    World->DestroyWorld(false);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDMTacticalGridStrategicTest, "SkySeoul.DeathMatch.TacticalGrid.StrategicRoam",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDMTacticalGridStrategicTest::RunTest(const FString&)
{
    UWorld::InitializationValues IV;
    IV.AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("DMTacticalGridStrategic"), nullptr, true, ERHIFeatureLevel::Num, &IV);
    UDMTacticalGridSubsystem* Grid = World ? World->GetSubsystem<UDMTacticalGridSubsystem>() : nullptr;
    if (!TestNotNull(TEXT("Strategic test owns tactical grid"), Grid)) { if (World) World->DestroyWorld(false); return false; }
    auto SpawnTracked = [World](const FVector& Location)
    {
        AActor* Actor = World->SpawnActor<AActor>();
        USceneComponent* Root = NewObject<USceneComponent>(Actor); Actor->SetRootComponent(Root); Root->RegisterComponent();
        Actor->SetActorLocation(Location); return Actor;
    };

    Grid->BuildState = EDMTacticalGridBuildState::Ready;
    Grid->Cells = {
        MakeCell(0, 0, FVector(0, 0, 0)), MakeCell(1, 0, FVector(600, 0, 0)),
        MakeCell(2, 0, FVector(1200, 0, 0)), MakeCell(3, 0, FVector(1800, 0, 0)),
        MakeCell(4, 0, FVector(600, 600, 0)), MakeCell(5, 0, FVector(1200, 600, 0)),
        MakeCell(6, 1, FVector(0, 1800, 0))};
    Grid->StrategicTeamA = 1; Grid->StrategicTeamB = 2;
    Connect(Grid->Cells[0], Grid->Cells[1]); Connect(Grid->Cells[1], Grid->Cells[2]);
    Connect(Grid->Cells[2], Grid->Cells[3]); Connect(Grid->Cells[1], Grid->Cells[4]);
    Connect(Grid->Cells[4], Grid->Cells[5]); Grid->RebuildCellIndex();
    AActor* Scout = SpawnTracked(Grid->Cells[0].RepresentativeLocation);
    Grid->RegisterActor(Scout, 1, false);
    Grid->Settings.Runtime.FriendlyObservationRadiusHops = 2;
    Grid->Settings.Runtime.FriendlyObservationConfidenceAtSelf = 1.f;
    Grid->Settings.Runtime.FriendlyObservationConfidencePerHop = .2f;
    Grid->RebuildDynamicCounts(10.f); Grid->ApplyFriendlyGraphObservation(10.f); Grid->RebuildExplorationStates(1, 10.f);
    FDMTacticalCellDynamicData Cell0, Cell1, Cell2, Cell3, Cell4, Cell5, Isolated;
    Grid->GetTeamCellData(1, Grid->Cells[0].Id, Cell0); Grid->GetTeamCellData(1, Grid->Cells[1].Id, Cell1);
    Grid->GetTeamCellData(1, Grid->Cells[2].Id, Cell2); Grid->GetTeamCellData(1, Grid->Cells[3].Id, Cell3);
    Grid->GetTeamCellData(1, Grid->Cells[4].Id, Cell4); Grid->GetTeamCellData(1, Grid->Cells[5].Id, Cell5);
    Grid->GetTeamCellData(1, Grid->Cells[6].Id, Isolated);
    TestEqual(TEXT("Friendly self cell gets full graph observation"), Cell0.ObservationConfidence, 1.f);
    TestEqual(TEXT("One-hop cell gets recommended graph observation"), Cell1.ObservationConfidence, .8f);
    TestEqual(TEXT("Two-hop cell gets recommended graph observation"), Cell2.ObservationConfidence, .5f);
    TestTrue(TEXT("Known cell adjacent to unknown region becomes frontier"), Cell2.bFrontier);
    TestTrue(TEXT("Second connected branch also creates a frontier"), Cell4.bFrontier);
    TestTrue(TEXT("Unconnected wall/region cell stays unknown"), Isolated.bUnknown);
    TestFalse(TEXT("Unknown disconnected cell is not itself a frontier"), Isolated.bFrontier);

    FDMTacticalEvaluationContext Roam; Roam.State = EDMTacticalEvaluationState::DisengagedRoam; Roam.Now = 10.f;
    Roam.RecentRoamCells.Add(Grid->Cells[2].Id); Roam.RecentRoamTimes.Add(10.f);
    TArray<FDMTacticalCellScore> FrontierCandidates;
    TestTrue(TEXT("Frontier roam candidates are available"), Grid->FindTacticalCandidates(Scout, 1, Roam, 6, 8, FrontierCandidates));
    for (const FDMTacticalCellScore& Score : FrontierCandidates)
        TestTrue(TEXT("Current and one-hop cells are excluded from normal roam"), Score.Path.CellHopCount >= 2);
    if (!FrontierCandidates.IsEmpty())
    {
        TestEqual(TEXT("Roam selects explicit frontier purpose"), FrontierCandidates[0].Purpose, EDMTacticalPurpose::ExploreFrontier);
        TestTrue(TEXT("Recent roam history penalizes repeated frontier"),
            FrontierCandidates.ContainsByPredicate([&](const FDMTacticalCellScore& S)
                { return S.CellId == Grid->Cells[2].Id && S.RecentRoamCellPenalty > 0.f; }));
        TestTrue(TEXT("Unvisited non-recent frontier outranks repeated frontier"), FrontierCandidates[0].CellId == Grid->Cells[4].Id);
    }

    for (int32 Index : {2, 4})
    {
        FDMTacticalCellDynamicData& D = Grid->FindOrAddDynamic(1, Grid->Cells[Index].Id);
        D.bFrontier = false; D.AdjacentUnknownCellCount = 0; D.ConnectedUnknownRegionSize = 0;
    }
    TArray<FDMTacticalCellScore> NearFallback;
    TestTrue(TEXT("Near-cell fallback is available when no strategic candidate exists"),
        Grid->FindTacticalCandidates(Scout, 1, Roam, 6, 8, NearFallback));
    TestTrue(TEXT("Fallback may relax the two-hop minimum"), NearFallback.ContainsByPredicate([](const FDMTacticalCellScore& S)
        { return S.Path.CellHopCount < 2; }));

    Grid->PathCache.Reset();
    Connect(Grid->Cells[0], Grid->Cells[2], 2000.f);
    Grid->RebuildCellIndex();
    FDMTacticalPathResult WeightedPath;
    TestTrue(TEXT("Weighted graph destination remains reachable"),
        Grid->AnalyzeTacticalPath(Scout, 1, Grid->Cells[0].Id, Grid->Cells[2].Id, WeightedPath));
    TestEqual(TEXT("Dijkstra chooses two cheap edges instead of expensive direct edge"), WeightedPath.CellHopCount, 2);
    TestEqual(TEXT("Dijkstra reports weighted travel distance"), WeightedPath.TotalTravelDistance, 1200.f);
    FDMTacticalCellDynamicData& RiskCell = Grid->FindOrAddDynamic(1, Grid->Cells[1].Id);
    RiskCell.bContested = true; RiskCell.CombatIntensity = 2.f; ++Grid->TeamRuntime.FindOrAdd(1).Revision; Grid->PathCache.Reset();
    FDMTacticalPathResult RiskPath;
    Grid->AnalyzeTacticalPath(Scout, 1, Grid->Cells[0].Id, Grid->Cells[2].Id, RiskPath);
    TestTrue(TEXT("Path analysis counts contested transit cells"), RiskPath.ContestedCellCount > 0);
    TestTrue(TEXT("Path analysis exposes nonzero route risk"), RiskPath.TotalPathCost > WeightedPath.TotalPathCost);
    FDMTacticalPathResult DisconnectedPath;
    TestFalse(TEXT("Disconnected region is rejected as unreachable"),
        Grid->AnalyzeTacticalPath(Scout, 1, Grid->Cells[0].Id, Grid->Cells[6].Id, DisconnectedPath));
    Connect(Grid->Cells[5], Grid->Cells[6]);
    Grid->Cells[5].Connections.Last().bPartialPath = true;
    Grid->Cells[6].Connections.Last().bPartialPath = true;
    Grid->PathCache.Reset();
    TestFalse(TEXT("Partial-only tactical connection is excluded from path graph"),
        Grid->AnalyzeTacticalPath(Scout, 1, Grid->Cells[0].Id, Grid->Cells[6].Id, DisconnectedPath));

    Grid->UnregisterActor(Scout);
    Grid->Cells.Reset();
    for (int32 Index = 0; Index < 10; ++Index) Grid->Cells.Add(MakeCell(Index, 0, FVector(Index * 600.f, 0, 0)));
    for (int32 Index = 0; Index < 9; ++Index) Connect(Grid->Cells[Index], Grid->Cells[Index + 1]);
    Grid->Cells[5].bJunction = true; Grid->Cells[5].EscapeRouteCount = 3;
    Grid->Cells[4].bDeadEnd = true; Grid->Cells[4].EscapeRouteCount = 1;
    Grid->RebuildCellIndex();
    Grid->PendingConnections.SetNum(Grid->Cells.Num());
    for (int32 Index = 0; Index < Grid->Cells.Num(); ++Index)
        for (const FDMTacticalConnection& Edge : Grid->Cells[Index].Connections)
            if (const int32* Neighbor = Grid->CellIndexById.Find(Edge.Neighbor))
            {
                UDMTacticalGridSubsystem::FPendingConnection Pending; Pending.NeighborIndex = *Neighbor;
                Pending.Cost = Edge.TravelCost; Pending.Clearance = 84.f; Grid->PendingConnections[Index].Add(Pending);
            }
    for (FDMTacticalCellStaticData& Cell : Grid->Cells)
    {
        Cell.bReachableFromAnySpawn = false;
        Cell.ReachableTeamMask = 0;
    }
    Grid->SpawnCellIndicesByTeam.FindOrAdd(1).Add(0); Grid->SpawnCellIndicesByTeam.FindOrAdd(2).Add(9);
    Grid->RepresentativeSpawnCellByTeam.Add(1, 0); Grid->RepresentativeSpawnCellByTeam.Add(2, 9);
    Grid->StrategicTeamA = 1; Grid->StrategicTeamB = 2; Grid->bHadSpawn = true;
    Grid->ReachabilityVisited.Reset(); Grid->ReachabilityQueue.Reset(); Grid->ReachabilityCursor = 0;
    Grid->ReachabilityVisited.Add((static_cast<uint64>(1) << 32) | 0u);
    Grid->ReachabilityVisited.Add((static_cast<uint64>(2) << 32) | 9u);
    Grid->ReachabilityQueue.Add({0, 1}); Grid->ReachabilityQueue.Add({9, 2});
    Grid->BuildState = EDMTacticalGridBuildState::MarkingReachability;
    TestTrue(TEXT("Team one spawn-cell set is available to the static field"), Grid->SpawnCellIndicesByTeam.FindRef(1).Contains(0));
    TestTrue(TEXT("Team two spawn-cell set is available to the static field"), Grid->SpawnCellIndicesByTeam.FindRef(2).Contains(9));
    while (Grid->BuildState == EDMTacticalGridBuildState::MarkingReachability) Grid->MarkReachability();
    Grid->FinalizeStrategicCellAvailability();
    TestTrue(TEXT("Shared graph cells are reachable from both representative spawns"),
        Grid->Cells[5].bReachableFromTeamARepresentativeSpawn && Grid->Cells[5].bReachableFromTeamBRepresentativeSpawn);
    Grid->PrepareSpawnFields();
    for (int32 Guard = 0; Guard < 100 && Grid->BuildState == EDMTacticalGridBuildState::CalculatingSpawnFields; ++Guard)
        Grid->CalculateSpawnFields();
    TestEqual(TEXT("Multi-source field reports enemy-spawn graph hops"), Grid->Cells[5].SpawnFieldsByTeam.FindChecked(2).Hops, 4);
    TestEqual(TEXT("Multi-source field reports enemy-spawn path distance"), Grid->Cells[5].SpawnFieldsByTeam.FindChecked(2).PathDistance, 2400.f);
    while (Grid->BuildState == EDMTacticalGridBuildState::CalculatingSpawnControl) Grid->CalculateSpawnControl();
    const FDMTacticalSpawnControlData& Excluded = Grid->Cells[7].SpawnControlByEnemyTeam.FindChecked(2);
    const FDMTacticalSpawnControlData& Junction = Grid->Cells[5].SpawnControlByEnemyTeam.FindChecked(2);
    const FDMTacticalSpawnControlData& DeadEnd = Grid->Cells[4].SpawnControlByEnemyTeam.FindChecked(2);
    TestFalse(TEXT("Enemy spawn exclusion cells cannot become spawn-control candidates"), Excluded.bCandidate);
    TestTrue(TEXT("Preferred-range junction becomes spawn-control candidate"), Junction.bCandidate);
    TestTrue(TEXT("Main-approach junction scores above dead-end cell"), Junction.Score > DeadEnd.Score);
    TestFalse(TEXT("Insufficient escape routes exclude dead-end control cell"), DeadEnd.bCandidate);
    TestTrue(TEXT("Front progress increases toward enemy spawn"),
        Grid->Cells[7].SpawnFieldsByTeam.FindChecked(1).FrontProgress
            > Grid->Cells[2].SpawnFieldsByTeam.FindChecked(1).FrontProgress);

    AActor* RoamerA = SpawnTracked(Grid->Cells[0].RepresentativeLocation);
    AActor* RoamerB = SpawnTracked(Grid->Cells[0].RepresentativeLocation + FVector(0, 25, 0));
    AActor* RoamerC = SpawnTracked(Grid->Cells[0].RepresentativeLocation + FVector(0, 50, 0));
    AActor* RoamerD = SpawnTracked(Grid->Cells[0].RepresentativeLocation + FVector(0, 75, 0));
    Grid->RegisterActor(RoamerA, 1, false); Grid->RegisterActor(RoamerB, 1, false);
    Grid->RegisterActor(RoamerC, 1, false); Grid->RegisterActor(RoamerD, 1, false);
    for (FDMTacticalCellStaticData& Cell : Grid->Cells)
    {
        FDMTacticalCellDynamicData& D = Grid->FindOrAddDynamic(1, Cell.Id);
        D.bUnknown = false; D.bKnown = true; D.bRecentlyObserved = false; D.bStale = false; D.bFrontier = false;
        D.LastFriendlyVisitTime = D.LastFriendlyObservationTime = 100.f;
    }
    FDMTacticalCellDynamicData& ValuableFrontier = Grid->FindOrAddDynamic(1, Grid->Cells[3].Id);
    ValuableFrontier.bFrontier = true; ValuableFrontier.AdjacentUnknownCellCount = 2; ValuableFrontier.ConnectedUnknownRegionSize = 8;
    FDMTacticalEvaluationContext StrategicRoam; StrategicRoam.State = EDMTacticalEvaluationState::DisengagedRoam; StrategicRoam.Now = 100.f;
    TArray<FDMTacticalCellScore> StrategicCandidates;
    TestTrue(TEXT("Strategic roam produces frontier/control candidates"),
        Grid->FindTacticalCandidates(RoamerA, 1, StrategicRoam, 1, 16, StrategicCandidates));
    TestTrue(TEXT("Strategic roam ignores the old local search radius and reaches a distant frontier"),
        StrategicCandidates.ContainsByPredicate([&](const FDMTacticalCellScore& S)
            { return S.CellId == Grid->Cells[3].Id && S.Path.CellHopCount > 1; }));
    TestTrue(TEXT("Valuable frontier takes priority over spawn control"), !StrategicCandidates.IsEmpty()
        && StrategicCandidates[0].Purpose == EDMTacticalPurpose::ExploreFrontier);
    TestTrue(TEXT("Frontier preference keeps SpawnControl as a lower-ranked alternative"),
        StrategicCandidates.ContainsByPredicate([](const FDMTacticalCellScore& S)
            { return S.Purpose == EDMTacticalPurpose::SpawnControl; }));
    TArray<FDMTacticalStrategicRegionSelection> StrategicRegions;
    TestTrue(TEXT("Noncombat API returns strategic regions instead of exact movement destinations"),
        Grid->FindStrategicRegions(RoamerA, 1, StrategicRoam, 16, StrategicRegions));
    TestTrue(TEXT("Strategic region carries layer and disconnected NavMesh region identity"),
        !StrategicRegions.IsEmpty() && StrategicRegions[0].LayerId == StrategicRegions[0].CellId.Layer
        && StrategicRegions[0].NavRegionId == StrategicRegions[0].CellId.Region);
    FDMTacticalEvaluationContext CombatRequest = StrategicRoam;
    CombatRequest.State = EDMTacticalEvaluationState::Visible;
    CombatRequest.bInCombat = true;
    StrategicRegions.Reset();
    TestFalse(TEXT("Tactical Grid strategic API refuses combat destination requests"),
        Grid->FindStrategicRegions(RoamerA, 1, CombatRequest, 16, StrategicRegions));
    ValuableFrontier.AdjacentUnknownCellCount = 1; ValuableFrontier.ConnectedUnknownRegionSize = 1;
    StrategicCandidates.Reset();
    TestTrue(TEXT("Low exploration value falls back to spawn control"),
        Grid->FindTacticalCandidates(RoamerA, 1, StrategicRoam, 10, 16, StrategicCandidates));
    const FDMTacticalCellScore* SpawnControlScore = StrategicCandidates.FindByPredicate([](const FDMTacticalCellScore& S)
        { return S.Purpose == EDMTacticalPurpose::SpawnControl; });
    TestNotNull(TEXT("Spawn-control purpose is selected when frontier value is low"), SpawnControlScore);
    if (SpawnControlScore)
    {
        Grid->Settings.Strategic.StrategicCellCapacity = 2;
        const FDMTacticalCellStaticData* ReservedCell = Grid->FindCell(SpawnControlScore->CellId);
        const FVector ExactA = ReservedCell->RepresentativeLocation + FVector(-180.f, -120.f, 0.f);
        const FVector ExactB = ReservedCell->RepresentativeLocation + FVector(180.f, 120.f, 0.f);
        const FVector ExactC = ReservedCell->RepresentativeLocation + FVector(-180.f, 140.f, 0.f);
        TestTrue(TEXT("First strategic reservation in a cell succeeds"), Grid->ReserveCell(1, RoamerA,
            SpawnControlScore->CellId, EDMTacticalPurpose::ExploreFrontier, 5.f, SpawnControlScore, ExactA, 80.f));
        TArray<FDMTacticalCellScore> PenalizedCandidates;
        Grid->FindTacticalCandidates(RoamerB, 1, StrategicRoam, 1, 16, PenalizedCandidates);
        TestTrue(TEXT("A partially occupied strategic cell remains a candidate with a reservation penalty"),
            PenalizedCandidates.ContainsByPredicate([&](const FDMTacticalCellScore& S)
                { return S.CellId == SpawnControlScore->CellId && S.StrategicReservationPenalty > 0.f; }));
        TestTrue(TEXT("Second strategic reservation in the same cell succeeds"), Grid->ReserveCell(1, RoamerB,
            SpawnControlScore->CellId, EDMTacticalPurpose::ExploreFrontier, 5.f, SpawnControlScore, ExactB, 80.f));
        TestTrue(TEXT("Strategic cell capacity is soft; a separated third exact goal succeeds"), Grid->ReserveCell(1, RoamerC,
            SpawnControlScore->CellId, EDMTacticalPurpose::ExploreFrontier, 5.f, SpawnControlScore, ExactC, 80.f));
        TestFalse(TEXT("Overlapping exact NavMesh goal reservation is rejected"), Grid->ReserveCell(1, RoamerD,
            SpawnControlScore->CellId, EDMTacticalPurpose::ExploreFrontier, 5.f, SpawnControlScore, ExactA + FVector(10.f, 0.f, 0.f), 80.f));
        TArray<FDMTacticalCellScore> CapacityFilteredCandidates;
        Grid->FindTacticalCandidates(RoamerC, 1, StrategicRoam, 1, 16, CapacityFilteredCandidates);
        TestTrue(TEXT("A crowded strategic region remains available with a soft penalty"),
            CapacityFilteredCandidates.ContainsByPredicate([&](const FDMTacticalCellScore& S)
                { return S.CellId == SpawnControlScore->CellId; }));
        Grid->ReleaseReservations(RoamerA);
        Grid->ReleaseReservations(RoamerB);
        Grid->ReleaseReservations(RoamerC);

        FDMTacticalCellScore SharedRoute = *SpawnControlScore;
        SharedRoute.Purpose = EDMTacticalPurpose::SpawnControl; SharedRoute.ApproachRouteId = 77;
        SharedRoute.ApproachRouteCapacity = 3; SharedRoute.bApproachRouteCapacityRelaxed = false;
        Grid->Settings.Strategic.MaximumControllersPerApproachRoute = 3;
        TestTrue(TEXT("First controller may use an approach route"), Grid->ReserveCell(1, RoamerA,
            Grid->Cells[3].Id, SharedRoute.Purpose, 5.f, &SharedRoute));
        TestTrue(TEXT("Second controller may share an approach route"), Grid->ReserveCell(1, RoamerB,
            Grid->Cells[4].Id, SharedRoute.Purpose, 5.f, &SharedRoute));
        TestTrue(TEXT("Third controller may share an approach route"), Grid->ReserveCell(1, RoamerC,
            Grid->Cells[5].Id, SharedRoute.Purpose, 5.f, &SharedRoute));
        for (FDMTacticalCellStaticData& Cell : Grid->Cells)
            for (auto& ControlPair : Cell.SpawnControlByEnemyTeam) ControlPair.Value.ApproachRouteId = 77;
        TArray<FDMTacticalCellScore> CongestedOnlyCandidates;
        TestTrue(TEXT("Congestion does not remove every strategic candidate"),
            Grid->FindTacticalCandidates(RoamerD, 1, StrategicRoam, 1, 16, CongestedOnlyCandidates));
        TestTrue(TEXT("All-routes-congested evaluation marks a least-congested SpawnControl candidate as relaxed"),
            CongestedOnlyCandidates.ContainsByPredicate([](const FDMTacticalCellScore& S)
                { return S.Purpose == EDMTacticalPurpose::SpawnControl && S.bApproachRouteCapacityRelaxed; }));
        TestTrue(TEXT("Approach-route capacity is a soft score and never blocks an exact goal"), Grid->ReserveCell(1, RoamerD,
            Grid->Cells[6].Id, SharedRoute.Purpose, 5.f, &SharedRoute));
        Grid->ReleaseReservations(RoamerD);
        SharedRoute.bApproachRouteCapacityRelaxed = true;
        TestTrue(TEXT("A saturated route remains usable when the all-routes-congested relaxation is explicit"),
            Grid->ReserveCell(1, RoamerD, Grid->Cells[6].Id, SharedRoute.Purpose, 5.f, &SharedRoute));
        Grid->ReleaseReservations(RoamerA); Grid->ReleaseReservations(RoamerB);
        Grid->ReleaseReservations(RoamerC); Grid->ReleaseReservations(RoamerD);
        TestTrue(TEXT("Combat/death cleanup API releases strategic reservations"), Grid->Reservations.IsEmpty());
    }

    World->DestroyWorld(false);
    return true;
}

#endif
