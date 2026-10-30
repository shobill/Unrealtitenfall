#include "DMTacticalGridSubsystem.h"

#include "DMTacticalAnalysisVolume.h"
#include "DeathMatch/PlayerStart/PlayerStartByTeam.h"
#include "AI/NavigationSystemBase.h"
#include "NavigationSystem.h"
#include "NavigationData.h"
#include "NavigationPath.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/PlayerStart.h"
#include "DrawDebugHelpers.h"
#include "HAL/IConsoleManager.h"

namespace
{
TAutoConsoleVariable<int32> CVarDMTacticalGrid(
    TEXT("dm.TacticalGrid"), 0,
    TEXT("0 off, 1 valid cells/connections, 2 static labels, 3 exploration/reservations, 4 score details, 5 spawn fields/frontier/path."),
    ECVF_Cheat);
TAutoConsoleVariable<int32> CVarDMTacticalGridTeam(
    TEXT("dm.TacticalGridTeam"), 0, TEXT("Team id displayed by dm.TacticalGrid 3."), ECVF_Cheat);
TAutoConsoleVariable<int32> CVarDMTacticalGridActor(
    TEXT("dm.TacticalGridActor"), -1, TEXT("Optional actor UniqueID filter for tactical reservation/path debug. -1 shows all."), ECVF_Cheat);
TAutoConsoleVariable<int32> CVarDMLegacyTacticalGridRuntime(
    TEXT("dm.LegacyTacticalGridRuntime"), 0,
    TEXT("Compatibility only. 0 keeps the retired square Tactical Grid from building/ticking; 1 temporarily re-enables it."),
    ECVF_Cheat);

constexpr float WallProbeScale = .45f;
constexpr float RecentEventSeconds = 5.f;

FColor CellColor(EDMTacticalCellType Type)
{
    switch (Type)
    {
    case EDMTacticalCellType::Blocked: return FColor(90, 20, 20);
    case EDMTacticalCellType::Narrow: return FColor::Orange;
    case EDMTacticalCellType::Partial: return FColor::Yellow;
    case EDMTacticalCellType::Normal: return FColor::Green;
    case EDMTacticalCellType::Open: return FColor::Cyan;
    default: return FColor::White;
    }
}

uint64 ReachabilityKey(uint8 Team, int32 CellIndex)
{
    return (static_cast<uint64>(Team) << 32) | static_cast<uint32>(CellIndex);
}
}

void UDMTacticalGridSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    BuildState = EDMTacticalGridBuildState::WaitingForNavigation;
    NextNavigationRetryTime = 0.f;
}

void UDMTacticalGridSubsystem::Deinitialize()
{
    SampleTasks.Reset(); Columns.Reset(); ColumnIndex.Reset(); Cells.Reset(); PendingConnections.Reset();
    ConnectionPairs.Reset(); CellIndexById.Reset(); RegisteredActors.Reset(); TeamRuntime.Reset();
    Reservations.Reset(); Failures.Reset(); ReachabilityQueue.Reset(); ReachabilityVisited.Reset();
    SpawnCellIndicesByTeam.Reset(); RepresentativeSpawnCellByTeam.Reset(); RepresentativeSpawnNameByTeam.Reset();
    SpawnFieldTeams.Reset(); SpawnFieldHeap.Reset(); PathCache.Reset();
    Super::Deinitialize();
}

TStatId UDMTacticalGridSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(UDMTacticalGridSubsystem, STATGROUP_Tickables);
}

void UDMTacticalGridSubsystem::RequestRebuild()
{
    SampleTasks.Reset(); Columns.Reset(); ColumnIndex.Reset(); Cells.Reset(); PendingConnections.Reset();
    ConnectionPairs.Reset(); CellIndexById.Reset(); ReachabilityQueue.Reset(); ReachabilityVisited.Reset();
    AnalysisBounds = FBox(ForceInit); AnalysisAreas.Reset(); SampleCursor = CellCursor = ConnectionCursor = MetricCursor = 0;
    bHadSpawn = false; SpawnCenters.Reset(); SpawnCounts.Reset(); SpawnCellIndicesByTeam.Reset();
    SpawnFieldTeams.Reset(); SpawnFieldHeap.Reset(); SpawnFieldDistances.Reset(); SpawnFieldHops.Reset();
    SpawnFieldNextIndices.Reset(); SpawnFieldTeamCursor = SpawnControlCursor = 0; bSpawnFieldActive = false;
    RepresentativeSpawnCellByTeam.Reset(); RepresentativeSpawnNameByTeam.Reset();
    StrategicTeamA = StrategicTeamB = 255;
    PathCache.Reset(); ++GridRevision;
    BuildState = EDMTacticalGridBuildState::WaitingForNavigation;
    NextNavigationRetryTime = 0.f;
}

void UDMTacticalGridSubsystem::Tick(float DeltaTime)
{
    if (!GetWorld() || GetWorld()->bIsTearingDown) return;
    if (CVarDMLegacyTacticalGridRuntime.GetValueOnGameThread() == 0) return;
    if (!IsReady()) TickBuild();
    if (IsReady()) UpdateRuntime(DeltaTime);
#if !UE_BUILD_SHIPPING
    if (CVarDMTacticalGrid.GetValueOnGameThread() > 0) DrawDebugGrid();
#endif
}

void UDMTacticalGridSubsystem::TickBuild()
{
    switch (BuildState)
    {
    case EDMTacticalGridBuildState::WaitingForNavigation:
        if (GetWorld()->GetTimeSeconds() < NextNavigationRetryTime) return;
        if (GatherAnalysisBounds()) PrepareSampleTasks();
        else NextNavigationRetryTime = GetWorld()->GetTimeSeconds() + .5f;
        break;
    case EDMTacticalGridBuildState::GeneratingSamples: GenerateSamples(); break;
    case EDMTacticalGridBuildState::BuildingCells: BuildCells(); break;
    case EDMTacticalGridBuildState::BuildingConnections: BuildConnections(); break;
    case EDMTacticalGridBuildState::MarkingReachability: MarkReachability(); break;
    case EDMTacticalGridBuildState::CalculatingStaticMetrics: CalculateStaticMetrics(); break;
    case EDMTacticalGridBuildState::CalculatingSpawnFields: CalculateSpawnFields(); break;
    case EDMTacticalGridBuildState::CalculatingSpawnControl: CalculateSpawnControl(); break;
    default: break;
    }
}

bool UDMTacticalGridSubsystem::GatherAnalysisBounds()
{
    UWorld* World = GetWorld();
    AnalysisAreas.Reset();
    UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
    const ANavigationData* NavData = Nav ? Nav->GetDefaultNavDataInstance(FNavigationSystem::DontCreate) : nullptr;
    if (!Nav || !NavData) return false;

    FBox Bounds(ForceInit);
    bool bAnalysisVolume = false;
    for (TActorIterator<ADMTacticalAnalysisVolume> It(World); It; ++It)
    {
        if (!It->bEnabled) continue;
        const FBox Area = It->GetComponentsBoundingBox(true); Bounds += Area; AnalysisAreas.Add(Area);
        if (!bAnalysisVolume) Settings = It->TacticalGridSettings;
        bAnalysisVolume = true;
    }
    if (!bAnalysisVolume)
    {
        for (TActorIterator<ANavMeshBoundsVolume> It(World); It; ++It)
        { const FBox Area = It->GetComponentsBoundingBox(true); Bounds += Area; AnalysisAreas.Add(Area); }
    }
    if (!Bounds.IsValid)
    {
        Bounds = NavData->GetBounds();
        if (Bounds.IsValid) AnalysisAreas.Add(Bounds);
    }
    if (!Bounds.IsValid || Bounds.GetSize().X < 1.f || Bounds.GetSize().Y < 1.f) return false;

    Settings.Build.CellSize = FMath::Clamp(Settings.Build.CellSize, 300.f, 1200.f);
    Settings.Build.SamplesPerAxis = FMath::Clamp(Settings.Build.SamplesPerAxis, 2, 5);
    Settings.Build.MinimumWalkableRatio = FMath::Clamp(Settings.Build.MinimumWalkableRatio, 0.f, 1.f);
    Settings.Build.LayerHeightThreshold = FMath::Max(50.f, Settings.Build.LayerHeightThreshold);
    if (Settings.Build.bUseNavDataAgentDimensions)
    {
        const FNavDataConfig& AgentConfig = NavData->GetConfig();
        if (AgentConfig.AgentRadius > 0.f) Settings.Build.AgentRadius = AgentConfig.AgentRadius;
        if (AgentConfig.AgentHeight > 0.f) Settings.Build.AgentHalfHeight = AgentConfig.AgentHeight * .5f;
    }
    AnalysisBounds = Bounds;
#if !UE_BUILD_SHIPPING
    if (Settings.Build.CellSize < 400.f || Settings.Build.SamplesPerAxis > 4)
        UE_LOG(LogTemp, Warning, TEXT("DM Tactical Grid uses expensive settings: CellSize=%.0f SamplesPerAxis=%d"), Settings.Build.CellSize, Settings.Build.SamplesPerAxis);
#endif
    return true;
}

void UDMTacticalGridSubsystem::PrepareSampleTasks()
{
    const float Size = Settings.Build.CellSize;
    const int32 MinX = FMath::FloorToInt(AnalysisBounds.Min.X / Size);
    const int32 MaxX = FMath::CeilToInt(AnalysisBounds.Max.X / Size) - 1;
    const int32 MinY = FMath::FloorToInt(AnalysisBounds.Min.Y / Size);
    const int32 MaxY = FMath::CeilToInt(AnalysisBounds.Max.Y / Size) - 1;
    const int64 BoundingColumnCount = static_cast<int64>(MaxX - MinX + 1) * (MaxY - MinY + 1);
    if (BoundingColumnCount <= 0) { BuildState = EDMTacticalGridBuildState::Failed; return; }

    const float ZSpan = FMath::Max(1.f, AnalysisBounds.Max.Z - AnalysisBounds.Min.Z);
    const int32 VerticalCount = FMath::Clamp(FMath::CeilToInt(ZSpan / Settings.Build.LayerHeightThreshold), 1, Settings.Build.MaximumVerticalSamples);
    const float VerticalStep = ZSpan / VerticalCount;
    VerticalProjectionExtent = FMath::Max(Settings.Build.LayerHeightThreshold * .55f, VerticalStep * .55f);
    const int32 Samples = Settings.Build.SamplesPerAxis;
    Columns.Reserve(static_cast<int32>(FMath::Min<int64>(BoundingColumnCount, FMath::Max(64, Settings.Build.MaximumGridColumns))));
    for (int32 X = MinX; X <= MaxX; ++X)
    {
        for (int32 Y = MinY; Y <= MaxY; ++Y)
        {
            const FBox CellXY(FVector(X * Size, Y * Size, AnalysisBounds.Min.Z), FVector((X + 1) * Size, (Y + 1) * Size, AnalysisBounds.Max.Z));
            const bool bInsideAnyArea = AnalysisAreas.ContainsByPredicate([&CellXY](const FBox& Area)
            { return Area.IsValid && Area.Max.X >= CellXY.Min.X && Area.Min.X <= CellXY.Max.X && Area.Max.Y >= CellXY.Min.Y && Area.Min.Y <= CellXY.Max.Y; });
            if (!bInsideAnyArea) continue;
            if (Columns.Num() >= FMath::Max(64, Settings.Build.MaximumGridColumns))
            {
                UE_LOG(LogTemp, Error, TEXT("DM Tactical Grid exceeded the %d analysis column cap."), Settings.Build.MaximumGridColumns);
                SampleTasks.Reset(); Columns.Reset(); ColumnIndex.Reset(); BuildState = EDMTacticalGridBuildState::Failed; return;
            }
            const int32 Column = Columns.AddDefaulted();
            Columns[Column].X = X; Columns[Column].Y = Y; ColumnIndex.Add(FIntPoint(X, Y), Column);
            for (int32 SX = 0; SX < Samples; ++SX)
            {
                for (int32 SY = 0; SY < Samples; ++SY)
                {
                    const float PX = (X + (SX + .5f) / Samples) * Size;
                    const float PY = (Y + (SY + .5f) / Samples) * Size;
                    if (PX < AnalysisBounds.Min.X || PX > AnalysisBounds.Max.X || PY < AnalysisBounds.Min.Y || PY > AnalysisBounds.Max.Y) continue;
                    for (int32 V = 0; V < VerticalCount; ++V)
                    {
                        FSampleTask& Task = SampleTasks.AddDefaulted_GetRef();
                        Task.X = X; Task.Y = Y; Task.SampleX = SX; Task.SampleY = SY; Task.VerticalIndex = V;
                        Task.Query = FVector(PX, PY, AnalysisBounds.Min.Z + (V + .5f) * VerticalStep);
                    }
                }
            }
        }
    }
    SampleCursor = 0;
    BuildState = EDMTacticalGridBuildState::GeneratingSamples;
}

void UDMTacticalGridSubsystem::GenerateSamples()
{
    UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
    if (!Nav) { BuildState = EDMTacticalGridBuildState::WaitingForNavigation; return; }
    const double EndTime = FPlatformTime::Seconds() + FMath::Max(.1f, Settings.Build.MaximumBuildTimePerTickMs) / 1000.0;
    const int32 End = FMath::Min(SampleTasks.Num(), SampleCursor + FMath::Max(1, Settings.Build.MaximumSamplesPerTick));
    const FVector Extent(Settings.Build.CellSize / FMath::Max(2, Settings.Build.SamplesPerAxis) * .45f,
        Settings.Build.CellSize / FMath::Max(2, Settings.Build.SamplesPerAxis) * .45f,
        VerticalProjectionExtent);
    for (; SampleCursor < End && FPlatformTime::Seconds() < EndTime; ++SampleCursor)
    {
        const FSampleTask& Task = SampleTasks[SampleCursor];
        FNavLocation Projected;
        if (!Nav->ProjectPointToNavigation(Task.Query, Projected, Extent) || !HasCapsuleClearance(Projected.Location)) continue;
        FColumn* Column = nullptr;
        if (const int32* Index = ColumnIndex.Find(FIntPoint(Task.X, Task.Y))) Column = &Columns[*Index];
        if (!Column) continue;
        const int32 SampleIndex = Task.SampleX * Settings.Build.SamplesPerAxis + Task.SampleY;
        bool bDuplicate = false;
        for (const FSampleHit& Existing : Column->Hits)
        {
            if (Existing.SampleIndex == SampleIndex && FMath::Abs(Existing.Location.Z - Projected.Location.Z) < 50.f)
            { bDuplicate = true; break; }
        }
        if (bDuplicate) continue;
        FSampleHit& Hit = Column->Hits.AddDefaulted_GetRef();
        Hit.SampleIndex = SampleIndex; Hit.Location = Projected.Location;
        Hit.WallClearance = MeasureWallClearance(Projected.Location, Hit.OpenDirection);
    }
    if (SampleCursor >= SampleTasks.Num())
    {
        SampleTasks.Reset(); CellCursor = 0;
        BuildState = EDMTacticalGridBuildState::BuildingCells;
    }
}

void UDMTacticalGridSubsystem::BuildCells()
{
    const double EndTime = FPlatformTime::Seconds() + FMath::Max(.1f, Settings.Build.MaximumBuildTimePerTickMs) / 1000.0;
    const int32 End = FMath::Min(Columns.Num(), CellCursor + FMath::Max(1, Settings.Build.MaximumSamplesPerTick / 4));
    const int32 TotalSamples = FMath::Square(Settings.Build.SamplesPerAxis);
    for (; CellCursor < End && FPlatformTime::Seconds() < EndTime; ++CellCursor)
    {
        FColumn& Column = Columns[CellCursor];
        Column.Hits.Sort([](const FSampleHit& A, const FSampleHit& B) { return A.Location.Z < B.Location.Z; });
        TArray<TArray<FSampleHit>> Layers;
        for (const FSampleHit& Hit : Column.Hits)
        {
            if (Layers.IsEmpty() || FMath::Abs(Hit.Location.Z - Layers.Last()[0].Location.Z) > Settings.Build.LayerHeightThreshold)
                Layers.AddDefaulted();
            Layers.Last().Add(Hit);
        }
        if (Layers.IsEmpty()) Layers.AddDefaulted();
        for (int32 LayerIndex = 0; LayerIndex < Layers.Num(); ++LayerIndex)
        {
            const TArray<FSampleHit>& Layer = Layers[LayerIndex];
            FDMTacticalCellStaticData& Cell = Cells.AddDefaulted_GetRef();
            Cell.Id = {Column.X, Column.Y, LayerIndex, INDEX_NONE};
            TSet<int32> UniqueSamples;
            FVector Mean = FVector::ZeroVector, Open = FVector::ZeroVector;
            float Clearance = 0.f;
            for (const FSampleHit& Hit : Layer)
            {
                UniqueSamples.Add(Hit.SampleIndex); Mean += Hit.Location; Clearance += Hit.WallClearance; Open += Hit.OpenDirection;
                Cell.ValidSamples.Add(Hit.Location);
            }
            Cell.WalkableRatio = static_cast<float>(UniqueSamples.Num()) / FMath::Max(1, TotalSamples);
            if (!Layer.IsEmpty())
            {
                Mean /= Layer.Num(); Clearance /= Layer.Num();
                const FSampleHit* Representative = &Layer[0];
                for (const FSampleHit& Hit : Layer)
                    if (FVector::DistSquared(Hit.Location, Mean) < FVector::DistSquared(Representative->Location, Mean)) Representative = &Hit;
                Cell.RepresentativeLocation = Representative->Location;
                Cell.AverageWallClearance = Clearance;
                Cell.Openness = FMath::Clamp(Clearance / FMath::Max(1.f, Settings.Build.CellSize * WallProbeScale), 0.f, 1.f);
                Cell.MajorOpenDirection = Open.GetSafeNormal2D(); Cell.Height = Cell.RepresentativeLocation.Z;
            }
            Cell.CellType = Cell.WalkableRatio < Settings.Build.MinimumWalkableRatio
                ? EDMTacticalCellType::Blocked : ClassifyCellType(Cell.WalkableRatio, Cell.Openness);
            Cell.ValidNavSampleCount = Cell.ValidSamples.Num();
            Cell.bTraversable = Cell.CellType != EDMTacticalCellType::Blocked && Cell.ValidNavSampleCount > 0;
            const float MinZ = Layer.IsEmpty() ? AnalysisBounds.Min.Z : Layer[0].Location.Z - Settings.Build.LayerHeightThreshold * .5f;
            const float MaxZ = Layer.IsEmpty() ? AnalysisBounds.Max.Z : Layer.Last().Location.Z + Settings.Build.LayerHeightThreshold * .5f;
            Cell.Bounds = FBox(FVector(Column.X * Settings.Build.CellSize, Column.Y * Settings.Build.CellSize, MinZ),
                FVector((Column.X + 1) * Settings.Build.CellSize, (Column.Y + 1) * Settings.Build.CellSize, MaxZ));
        }
    }
    if (CellCursor >= Columns.Num())
    {
        Columns.Reset(); ColumnIndex.Reset(); PrepareConnections();
    }
}

void UDMTacticalGridSubsystem::PrepareConnections()
{
    PendingConnections.SetNum(Cells.Num());
    for (int32 A = 0; A < Cells.Num(); ++A)
    {
        if (Cells[A].CellType == EDMTacticalCellType::Blocked) continue;
        for (int32 B = A + 1; B < Cells.Num(); ++B)
        {
            if (Cells[B].CellType == EDMTacticalCellType::Blocked) continue;
            const int32 DX = FMath::Abs(Cells[A].Id.X - Cells[B].Id.X), DY = FMath::Abs(Cells[A].Id.Y - Cells[B].Id.Y);
            if (DX > 1 || DY > 1 || (DX == 0 && DY == 0 && Cells[A].Id.Layer == Cells[B].Id.Layer)) continue;
            if (FVector::Distance(Cells[A].RepresentativeLocation, Cells[B].RepresentativeLocation)
                > Settings.Build.CellSize * FMath::Max(1.f, Settings.Build.MaximumNeighborDistanceScale)) continue;
            ConnectionPairs.Add({A, B});
        }
    }
    ConnectionCursor = 0;
    BuildState = EDMTacticalGridBuildState::BuildingConnections;
}

void UDMTacticalGridSubsystem::BuildConnections()
{
    const double EndTime = FPlatformTime::Seconds() + FMath::Max(.1f, Settings.Build.MaximumBuildTimePerTickMs) / 1000.0;
    const int32 End = FMath::Min(ConnectionPairs.Num(), ConnectionCursor + FMath::Max(1, Settings.Build.MaximumConnectionChecksPerTick));
    for (; ConnectionCursor < End && FPlatformTime::Seconds() < EndTime; ++ConnectionCursor)
    {
        const FConnectionPair Pair = ConnectionPairs[ConnectionCursor];
        FPendingConnection Forward;
        if (!ValidateConnection(Pair.A, Pair.B, Forward)) continue;
        Forward.NeighborIndex = Pair.B;
        FPendingConnection Reverse = Forward; Reverse.NeighborIndex = Pair.A;
        PendingConnections[Pair.A].Add(Forward); PendingConnections[Pair.B].Add(Reverse);
    }
    if (ConnectionCursor >= ConnectionPairs.Num())
    {
        ConnectionPairs.Reset(); FinalizeRegionsAndIds(); PrepareReachability();
    }
}

bool UDMTacticalGridSubsystem::ValidateConnection(int32 A, int32 B, FPendingConnection& Out) const
{
    if (!Cells.IsValidIndex(A) || !Cells.IsValidIndex(B)) return false;
    const FVector Start = Cells[A].RepresentativeLocation, End = Cells[B].RepresentativeLocation;
    const float Direct = FVector::Distance(Start, End);
    if (Direct < 1.f) return false;
    UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(GetWorld(), Start, End);
    if (!Path || !Path->IsValid() || Path->IsPartial()) return false;
    const float Length = Path->GetPathLength();
    if (!FMath::IsFinite(Length) || Length > Direct * FMath::Max(1.f, Settings.Build.MaximumNeighborPathRatio)) return false;
    const FVector Mid = (Start + End) * .5f;
    if (!HasCapsuleClearance(Mid, .9f)) return false;
    Out.Cost = Length; Out.Clearance = Settings.Build.AgentRadius * 2.f;
    const float Height = FMath::Abs(Start.Z - End.Z);
    if (Cells[A].CellType == EDMTacticalCellType::Narrow || Cells[B].CellType == EDMTacticalCellType::Narrow)
        Out.Type = EDMTacticalConnectionType::Narrow;
    else if (Height > Settings.Build.AgentHalfHeight) Out.Type = EDMTacticalConnectionType::Stair;
    else if (Height > Settings.Build.LayerHeightThreshold * .35f) Out.Type = EDMTacticalConnectionType::Ramp;
    else Out.Type = EDMTacticalConnectionType::Normal;
    return true;
}

void UDMTacticalGridSubsystem::FinalizeRegionsAndIds()
{
    int32 Region = 0;
    TArray<int32> Queue;
    TSet<int32> Visited;
    for (int32 Root = 0; Root < Cells.Num(); ++Root)
    {
        if (Cells[Root].CellType == EDMTacticalCellType::Blocked || Visited.Contains(Root)) continue;
        Queue.Reset(); Queue.Add(Root); Visited.Add(Root);
        for (int32 Cursor = 0; Cursor < Queue.Num(); ++Cursor)
        {
            const int32 Index = Queue[Cursor]; Cells[Index].Id.Region = Region;
            for (const FPendingConnection& Edge : PendingConnections[Index])
                if (!Visited.Contains(Edge.NeighborIndex)) { Visited.Add(Edge.NeighborIndex); Queue.Add(Edge.NeighborIndex); }
        }
        ++Region;
    }
    RebuildCellIndex();
    for (int32 Index = 0; Index < Cells.Num(); ++Index)
    {
        Cells[Index].Connections.Reset();
        for (const FPendingConnection& Edge : PendingConnections[Index])
        {
            if (!Cells.IsValidIndex(Edge.NeighborIndex)) continue;
            FDMTacticalConnection& Connection = Cells[Index].Connections.AddDefaulted_GetRef();
            Connection.Neighbor = Cells[Edge.NeighborIndex].Id; Connection.TravelCost = Edge.Cost;
            Connection.MinimumClearance = Edge.Clearance; Connection.Type = Edge.Type;
            Connection.bHasValidNavPath = true; Connection.bPartialPath = false;
            Connection.ActualPathLength = Edge.Cost;
            Connection.HeightDelta = Cells[Edge.NeighborIndex].Height - Cells[Index].Height;
            Connection.bAgentCapsulePassable = Edge.Clearance + KINDA_SMALL_NUMBER >= Settings.Build.AgentRadius * 2.f;
        }
    }
}

void UDMTacticalGridSubsystem::PrepareReachability()
{
    ReachabilityQueue.Reset(); ReachabilityVisited.Reset(); ReachabilityCursor = 0;
    bHadSpawn = false; SpawnCenters.Reset(); SpawnCounts.Reset(); SpawnCellIndicesByTeam.Reset();
    RepresentativeSpawnCellByTeam.Reset(); RepresentativeSpawnNameByTeam.Reset();
    StrategicTeamA = StrategicTeamB = 255;
    struct FRepresentativeCandidate
    {
        uint8 Team = 255;
        int32 CellIndex = INDEX_NONE;
        FString Name;
    };
    TArray<FRepresentativeCandidate> RepresentativeCandidates;
    UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
    const FVector ProjectionExtent(
        FMath::Max(Settings.Build.AgentRadius * 2.f, 50.f),
        FMath::Max(Settings.Build.AgentRadius * 2.f, 50.f),
        FMath::Max(Settings.Build.AgentHalfHeight * 2.f, 100.f));
    for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
    {
        FNavLocation Projected;
        if (!Nav || !Nav->ProjectPointToNavigation(It->GetActorLocation(), Projected, ProjectionExtent))
        {
            UE_LOG(LogTemp, Warning, TEXT("DM Tactical Grid ignored spawn %s: NavMesh projection failed for agent R%.0f H%.0f."),
                *GetNameSafe(*It), Settings.Build.AgentRadius, Settings.Build.AgentHalfHeight * 2.f);
            continue;
        }
        const int32 Index = FindClosestCellIndex(Projected.Location, false);
        if (Index == INDEX_NONE || Cells[Index].CellType == EDMTacticalCellType::Blocked)
        {
            UE_LOG(LogTemp, Warning, TEXT("DM Tactical Grid ignored spawn %s: projected location did not map to a valid grid cell."),
                *GetNameSafe(*It));
            continue;
        }
        uint8 Team = 255;
        if (const APlayerStartByTeam* TeamStart = Cast<APlayerStartByTeam>(*It))
        {
            Team = TeamStart->TeamId;
            if (Team != 255)
            {
                SpawnCellIndicesByTeam.FindOrAdd(Team).AddUnique(Index);
                RepresentativeCandidates.Add({Team, Index, TeamStart->GetName()});
            }
        }
        SpawnCenters.FindOrAdd(Team) += Projected.Location; SpawnCounts.FindOrAdd(Team)++;
    }
    for (auto& Pair : SpawnCenters) Pair.Value /= FMath::Max(1, SpawnCounts.FindRef(Pair.Key));
    RepresentativeCandidates.StableSort([](const FRepresentativeCandidate& A, const FRepresentativeCandidate& B)
    {
        if (A.Team != B.Team) return A.Team < B.Team;
        const int32 NameOrder = A.Name.Compare(B.Name, ESearchCase::CaseSensitive);
        return NameOrder != 0 ? NameOrder < 0 : A.CellIndex < B.CellIndex;
    });
    for (const FRepresentativeCandidate& Candidate : RepresentativeCandidates)
    {
        if (RepresentativeSpawnCellByTeam.Contains(Candidate.Team)) continue;
        RepresentativeSpawnCellByTeam.Add(Candidate.Team, Candidate.CellIndex);
        RepresentativeSpawnNameByTeam.Add(Candidate.Team, Candidate.Name);
        const uint64 Key = ReachabilityKey(Candidate.Team, Candidate.CellIndex);
        ReachabilityVisited.Add(Key);
        ReachabilityQueue.Add({Candidate.CellIndex, Candidate.Team});
        UE_LOG(LogTemp, Log, TEXT("DM Tactical Grid representative spawn: Team %d %s -> cell %d,%d L%d."),
            Candidate.Team, *Candidate.Name, Cells[Candidate.CellIndex].Id.X,
            Cells[Candidate.CellIndex].Id.Y, Cells[Candidate.CellIndex].Id.Layer);
    }
    TArray<uint8> RepresentativeTeams;
    RepresentativeSpawnCellByTeam.GetKeys(RepresentativeTeams);
    RepresentativeTeams.Sort();
    if (!RepresentativeTeams.IsEmpty()) StrategicTeamA = RepresentativeTeams[0];
    if (RepresentativeTeams.Num() > 1) StrategicTeamB = RepresentativeTeams[1];
    bHadSpawn = !RepresentativeSpawnCellByTeam.IsEmpty();
    if (StrategicTeamA == 255 || StrategicTeamB == 255)
        UE_LOG(LogTemp, Warning, TEXT("DM Tactical Grid strategic reachability requires two valid team representative spawns; found %d."), RepresentativeTeams.Num());
    BuildState = EDMTacticalGridBuildState::MarkingReachability;
}

void UDMTacticalGridSubsystem::MarkReachability()
{
    if (!bHadSpawn)
    {
        MetricCursor = 0; BuildState = EDMTacticalGridBuildState::CalculatingStaticMetrics; return;
    }
    const int32 End = FMath::Min(ReachabilityQueue.Num(), ReachabilityCursor + FMath::Max(1, Settings.Build.MaximumConnectionChecksPerTick));
    for (; ReachabilityCursor < End; ++ReachabilityCursor)
    {
        const FReachabilityWork Work = ReachabilityQueue[ReachabilityCursor];
        if (!Cells.IsValidIndex(Work.CellIndex)) continue;
        FDMTacticalCellStaticData& Cell = Cells[Work.CellIndex]; Cell.bReachableFromAnySpawn = true;
        if (Work.Team == 255) Cell.ReachableTeamMask = -1;
        else if (Work.Team < 31) Cell.ReachableTeamMask |= (1 << Work.Team);
        for (const FPendingConnection& Edge : PendingConnections[Work.CellIndex])
        {
            const uint64 Key = ReachabilityKey(Work.Team, Edge.NeighborIndex);
            if (ReachabilityVisited.Contains(Key)) continue;
            ReachabilityVisited.Add(Key); ReachabilityQueue.Add({Edge.NeighborIndex, Work.Team});
        }
    }
    if (ReachabilityCursor >= ReachabilityQueue.Num())
    {
        ReachabilityQueue.Reset(); ReachabilityVisited.Reset(); MetricCursor = 0;
        BuildState = EDMTacticalGridBuildState::CalculatingStaticMetrics;
    }
}

void UDMTacticalGridSubsystem::CalculateStaticMetrics()
{
    const int32 End = FMath::Min(Cells.Num(), MetricCursor + FMath::Max(1, Settings.Build.MaximumSamplesPerTick / 2));
    for (; MetricCursor < End; ++MetricCursor)
    {
        FDMTacticalCellStaticData& Cell = Cells[MetricCursor];
        if (Cell.CellType == EDMTacticalCellType::Blocked) continue;
        float Cost = 0.f, NeighborHeight = 0.f; int32 ValidNeighbors = 0;
        for (const FDMTacticalConnection& Edge : Cell.Connections)
        {
            Cost += Edge.TravelCost;
            if (Edge.Type == EDMTacticalConnectionType::Narrow) ++Cell.NarrowConnectionCount;
            if (const FDMTacticalCellStaticData* Other = FindCell(Edge.Neighbor)) { NeighborHeight += Other->Height; ++ValidNeighbors; }
        }
        Cell.AverageTravelCost = Cell.Connections.IsEmpty() ? 0.f : Cost / Cell.Connections.Num();
        Cell.RelativeHeight = ValidNeighbors ? Cell.Height - NeighborHeight / ValidNeighbors : 0.f;
        Cell.ConnectionCount = Cell.Connections.Num(); Cell.EscapeRouteCount = Cell.Connections.Num();
        Cell.bDeadEnd = Cell.Connections.Num() <= 1; Cell.bJunction = Cell.Connections.Num() >= 3;
        Cell.bTraversable = Cell.CellType != EDMTacticalCellType::Blocked && Cell.ValidNavSampleCount > 0
            && Cell.Connections.ContainsByPredicate([](const FDMTacticalConnection& Edge)
                { return Edge.bHasValidNavPath && !Edge.bPartialPath && Edge.bAgentCapsulePassable; });
    }
    if (MetricCursor >= Cells.Num())
    {
        FinalizeStrategicCellAvailability();
        PendingConnections.Reset(); PrepareSpawnFields();
    }
}

void UDMTacticalGridSubsystem::FinalizeStrategicCellAvailability()
{
    int32 ActiveCount = 0;
    TMap<EDMTacticalStrategicCellExclusionReason, int32> ExclusionCounts;
    const bool bValidRepresentatives = StrategicTeamA != 255 && StrategicTeamB != 255;
    for (FDMTacticalCellStaticData& Cell : Cells)
    {
        Cell.StrategicExclusionReasons.Reset();
        Cell.bNavMeshProjectionSucceeded = Cell.ValidNavSampleCount > 0;
        Cell.bHasValidGridConnection = Cell.Connections.ContainsByPredicate([](const FDMTacticalConnection& Edge)
        {
            return Edge.bHasValidNavPath && !Edge.bPartialPath && Edge.bAgentCapsulePassable;
        });
        Cell.bReachableFromTeamARepresentativeSpawn = bValidRepresentatives && IsTeamReachable(Cell, StrategicTeamA);
        Cell.bReachableFromTeamBRepresentativeSpawn = bValidRepresentatives && IsTeamReachable(Cell, StrategicTeamB);
        if (!Cell.bNavMeshProjectionSucceeded)
            Cell.StrategicExclusionReasons.Add(EDMTacticalStrategicCellExclusionReason::NoNavProjection);
        if (!Cell.bHasValidGridConnection)
        {
            Cell.StrategicExclusionReasons.Add(EDMTacticalStrategicCellExclusionReason::NoValidConnection);
            Cell.StrategicExclusionReasons.Add(EDMTacticalStrategicCellExclusionReason::IsolatedRegion);
        }
        else if (Cell.Id.Region == INDEX_NONE)
            Cell.StrategicExclusionReasons.Add(EDMTacticalStrategicCellExclusionReason::IsolatedRegion);
        if (!bValidRepresentatives)
            Cell.StrategicExclusionReasons.Add(EDMTacticalStrategicCellExclusionReason::InvalidSpawnReference);
        else
        {
            if (!Cell.bReachableFromTeamARepresentativeSpawn)
                Cell.StrategicExclusionReasons.Add(EDMTacticalStrategicCellExclusionReason::UnreachableFromTeamA);
            if (!Cell.bReachableFromTeamBRepresentativeSpawn)
                Cell.StrategicExclusionReasons.Add(EDMTacticalStrategicCellExclusionReason::UnreachableFromTeamB);
        }
        const bool bBaseValid = Cell.bNavMeshProjectionSucceeded && Cell.bHasValidGridConnection
            && Cell.Id.Region != INDEX_NONE && bValidRepresentatives;
        switch (Settings.Strategic.StrategicReachabilityPolicy)
        {
        case EDMTacticalStrategicReachabilityPolicy::ReachableFromEitherTeam:
            Cell.bPlayableStrategicCell = bBaseValid
                && (Cell.bReachableFromTeamARepresentativeSpawn || Cell.bReachableFromTeamBRepresentativeSpawn);
            break;
        case EDMTacticalStrategicReachabilityPolicy::ReachableFromRequestingTeam:
            Cell.bPlayableStrategicCell = bBaseValid
                && (Cell.bReachableFromTeamARepresentativeSpawn || Cell.bReachableFromTeamBRepresentativeSpawn);
            break;
        default:
            Cell.bPlayableStrategicCell = bBaseValid
                && Cell.bReachableFromTeamARepresentativeSpawn && Cell.bReachableFromTeamBRepresentativeSpawn;
            break;
        }
        if (Cell.bPlayableStrategicCell) ++ActiveCount;
        else for (const EDMTacticalStrategicCellExclusionReason Reason : Cell.StrategicExclusionReasons)
            ++ExclusionCounts.FindOrAdd(Reason);
    }
    UE_LOG(LogTemp, Log, TEXT("DM Tactical Grid strategic analysis: total=%d active=%d inactive=%d policy=%s TeamA=%d TeamB=%d NoNav=%d NoConnection=%d UnreachableA=%d UnreachableB=%d Isolated=%d InvalidSpawn=%d."),
        Cells.Num(), ActiveCount, Cells.Num() - ActiveCount,
        *StaticEnum<EDMTacticalStrategicReachabilityPolicy>()->GetNameStringByValue(static_cast<int64>(Settings.Strategic.StrategicReachabilityPolicy)),
        StrategicTeamA, StrategicTeamB,
        ExclusionCounts.FindRef(EDMTacticalStrategicCellExclusionReason::NoNavProjection),
        ExclusionCounts.FindRef(EDMTacticalStrategicCellExclusionReason::NoValidConnection),
        ExclusionCounts.FindRef(EDMTacticalStrategicCellExclusionReason::UnreachableFromTeamA),
        ExclusionCounts.FindRef(EDMTacticalStrategicCellExclusionReason::UnreachableFromTeamB),
        ExclusionCounts.FindRef(EDMTacticalStrategicCellExclusionReason::IsolatedRegion),
        ExclusionCounts.FindRef(EDMTacticalStrategicCellExclusionReason::InvalidSpawnReference));
}

void UDMTacticalGridSubsystem::PrepareSpawnFields()
{
    SpawnFieldTeams.Reset();
    SpawnCellIndicesByTeam.GetKeys(SpawnFieldTeams);
    SpawnFieldTeams.Sort();
    SpawnFieldTeamCursor = 0;
    bSpawnFieldActive = false;
    SpawnFieldHeap.Reset();
    for (FDMTacticalCellStaticData& Cell : Cells)
    {
        Cell.SpawnFieldsByTeam.Reset();
        Cell.SpawnControlByEnemyTeam.Reset();
    }
    BuildState = EDMTacticalGridBuildState::CalculatingSpawnFields;
}

void UDMTacticalGridSubsystem::CalculateSpawnFields()
{
    const auto MinHeap = [](const FSpawnDijkstraNode& A, const FSpawnDijkstraNode& B)
    { return A.Distance < B.Distance; };
    if (!bSpawnFieldActive)
    {
        if (!SpawnFieldTeams.IsValidIndex(SpawnFieldTeamCursor))
        {
            SpawnControlCursor = 0;
            BuildState = EDMTacticalGridBuildState::CalculatingSpawnControl;
            return;
        }
        ActiveSpawnFieldTeam = SpawnFieldTeams[SpawnFieldTeamCursor];
        SpawnFieldDistances.Init(TNumericLimits<float>::Max(), Cells.Num());
        SpawnFieldHops.Init(MAX_int32, Cells.Num());
        SpawnFieldNextIndices.Init(INDEX_NONE, Cells.Num());
        SpawnFieldHeap.Reset();
        for (const int32 Root : SpawnCellIndicesByTeam.FindRef(ActiveSpawnFieldTeam))
        {
            if (!Cells.IsValidIndex(Root) || Cells[Root].CellType == EDMTacticalCellType::Blocked) continue;
            SpawnFieldDistances[Root] = 0.f; SpawnFieldHops[Root] = 0; SpawnFieldNextIndices[Root] = Root;
            SpawnFieldHeap.HeapPush({Root, 0.f, 0}, MinHeap);
        }
        bSpawnFieldActive = true;
    }

    const double EndTime = FPlatformTime::Seconds() + FMath::Max(.1f, Settings.Build.MaximumBuildTimePerTickMs) / 1000.0;
    int32 Processed = 0;
    while (!SpawnFieldHeap.IsEmpty() && Processed < FMath::Max(1, Settings.Build.MaximumConnectionChecksPerTick)
        && FPlatformTime::Seconds() < EndTime)
    {
        FSpawnDijkstraNode Node;
        SpawnFieldHeap.HeapPop(Node, MinHeap, EAllowShrinking::No);
        ++Processed;
        if (!Cells.IsValidIndex(Node.CellIndex) || Node.Distance > SpawnFieldDistances[Node.CellIndex] + KINDA_SMALL_NUMBER) continue;
        const FDMTacticalCellStaticData& Cell = Cells[Node.CellIndex];
        for (const FDMTacticalConnection& Edge : Cell.Connections)
        {
            if (!Edge.bHasValidNavPath || Edge.bPartialPath || !Edge.bAgentCapsulePassable) continue;
            const int32* NeighborPtr = CellIndexById.Find(Edge.Neighbor);
            if (!NeighborPtr || !Cells.IsValidIndex(*NeighborPtr)) continue;
            const int32 Neighbor = *NeighborPtr;
            const float EdgeCost = Edge.ActualPathLength > 0.f ? Edge.ActualPathLength : Edge.TravelCost;
            const float NewDistance = Node.Distance + FMath::Max(1.f, EdgeCost);
            if (NewDistance + KINDA_SMALL_NUMBER >= SpawnFieldDistances[Neighbor]) continue;
            SpawnFieldDistances[Neighbor] = NewDistance;
            SpawnFieldHops[Neighbor] = Node.Hops + 1;
            SpawnFieldNextIndices[Neighbor] = Node.CellIndex;
            SpawnFieldHeap.HeapPush({Neighbor, NewDistance, Node.Hops + 1}, MinHeap);
        }
    }
    if (!SpawnFieldHeap.IsEmpty()) return;

    for (int32 Index = 0; Index < Cells.Num(); ++Index)
    {
        if (!FMath::IsFinite(SpawnFieldDistances[Index]) || SpawnFieldDistances[Index] == TNumericLimits<float>::Max()) continue;
        FDMTacticalSpawnFieldData& Field = Cells[Index].SpawnFieldsByTeam.FindOrAdd(ActiveSpawnFieldTeam);
        Field.Hops = SpawnFieldHops[Index]; Field.PathDistance = SpawnFieldDistances[Index];
        Field.bInsideSpawnExclusionArea = Field.Hops <= Settings.Strategic.EnemySpawnExclusionHops;
        const int32 Next = SpawnFieldNextIndices[Index];
        if (Cells.IsValidIndex(Next) && Next != Index)
        {
            Field.NextDirectionTowardSpawn = (Cells[Next].RepresentativeLocation - Cells[Index].RepresentativeLocation).GetSafeNormal2D();
            Field.NextCellTowardSpawn = Cells[Next].Id;
        }
    }
    ++SpawnFieldTeamCursor;
    bSpawnFieldActive = false;
}

void UDMTacticalGridSubsystem::CalculateSpawnControl()
{
    const double EndTime = FPlatformTime::Seconds() + FMath::Max(.1f, Settings.Build.MaximumBuildTimePerTickMs) / 1000.0;
    const int32 End = FMath::Min(Cells.Num(), SpawnControlCursor + FMath::Max(1, Settings.Build.MaximumSamplesPerTick / 2));
    for (; SpawnControlCursor < End && FPlatformTime::Seconds() < EndTime; ++SpawnControlCursor)
    {
        FDMTacticalCellStaticData& Cell = Cells[SpawnControlCursor];
        if (Cell.CellType == EDMTacticalCellType::Blocked || !Cell.bTraversable) continue;

        for (const uint8 FriendlyTeam : SpawnFieldTeams)
        {
            FDMTacticalSpawnFieldData* FriendlyField = Cell.SpawnFieldsByTeam.Find(FriendlyTeam);
            if (!FriendlyField) continue;
            float EnemyDistance = TNumericLimits<float>::Max();
            for (const uint8 OtherTeam : SpawnFieldTeams)
                if (OtherTeam != FriendlyTeam)
                    if (const FDMTacticalSpawnFieldData* Other = Cell.SpawnFieldsByTeam.Find(OtherTeam))
                        EnemyDistance = FMath::Min(EnemyDistance, Other->PathDistance);
            if (FMath::IsFinite(EnemyDistance) && EnemyDistance < TNumericLimits<float>::Max())
                FriendlyField->FrontProgress = FriendlyField->PathDistance
                    / FMath::Max(1.f, FriendlyField->PathDistance + EnemyDistance);
        }

        for (const uint8 EnemyTeam : SpawnFieldTeams)
        {
            const FDMTacticalSpawnFieldData* EnemyField = Cell.SpawnFieldsByTeam.Find(EnemyTeam);
            if (!EnemyField) continue;
            FDMTacticalSpawnControlData& Control = Cell.SpawnControlByEnemyTeam.FindOrAdd(EnemyTeam);
            Control.RelatedEnemySpawnTeam = EnemyTeam;
            Control.EnemySpawnHops = EnemyField->Hops;
            Control.EnemySpawnPathDistance = EnemyField->PathDistance;
            Control.ControlledApproachDirection = EnemyField->NextDirectionTowardSpawn;
            const float Angle = FMath::Atan2(Control.ControlledApproachDirection.Y, Control.ControlledApproachDirection.X);
            const int32 DirectionBucket = FMath::FloorToInt((Angle + PI) / (2.f * PI) * 8.f) & 7;
            // Direction alone merged unrelated corridors across the entire map. Region and a coarse local
            // corridor bucket keep genuinely separate graph routes distinct without making every cell unique.
            uint32 RouteHash = HashCombine(GetTypeHash(EnemyTeam), GetTypeHash(Cell.Id.Region));
            RouteHash = HashCombine(RouteHash, GetTypeHash(DirectionBucket));
            RouteHash = HashCombine(RouteHash, GetTypeHash(FIntPoint(
                FMath::FloorToInt(static_cast<float>(Cell.Id.X) / 4.f),
                FMath::FloorToInt(static_cast<float>(Cell.Id.Y) / 4.f))));
            Control.ApproachRouteId = static_cast<int32>(RouteHash & 0x7fffffff);

            bool bFriendlyReachable = false;
            float NearestFriendlyDistance = TNumericLimits<float>::Max();
            float InterSpawnDistance = TNumericLimits<float>::Max();
            for (const uint8 FriendlyTeam : SpawnFieldTeams)
            {
                if (FriendlyTeam == EnemyTeam) continue;
                if (const FDMTacticalSpawnFieldData* FriendlyField = Cell.SpawnFieldsByTeam.Find(FriendlyTeam))
                {
                    bFriendlyReachable = true;
                    NearestFriendlyDistance = FMath::Min(NearestFriendlyDistance, FriendlyField->PathDistance);
                }
                for (const int32 FriendlySpawnIndex : SpawnCellIndicesByTeam.FindRef(FriendlyTeam))
                    if (Cells.IsValidIndex(FriendlySpawnIndex))
                        if (const FDMTacticalSpawnFieldData* AtSpawn = Cells[FriendlySpawnIndex].SpawnFieldsByTeam.Find(EnemyTeam))
                            InterSpawnDistance = FMath::Min(InterSpawnDistance, AtSpawn->PathDistance);
            }
            const bool bExcluded = EnemyField->bInsideSpawnExclusionArea;
            const bool bPreferredRange = EnemyField->Hops >= Settings.Strategic.PreferredSpawnControlMinHops
                && EnemyField->Hops <= Settings.Strategic.PreferredSpawnControlMaxHops;
            const bool bOnMainApproach = bFriendlyReachable && FMath::IsFinite(InterSpawnDistance)
                && EnemyField->PathDistance + NearestFriendlyDistance <= InterSpawnDistance * 1.25f;
            float FrontProgress = .5f;
            if (bFriendlyReachable)
                FrontProgress = NearestFriendlyDistance / FMath::Max(1.f, NearestFriendlyDistance + EnemyField->PathDistance);
            const float FrontShape = FMath::Clamp(1.f - FMath::Abs(FrontProgress - .7f) / .7f, 0.f, 1.f);
            const float ApproachOpenness = Cell.MajorOpenDirection.IsNearlyZero() ? Cell.Openness * .5f
                : Cell.Openness * (.5f + .5f * FMath::Abs(FVector::DotProduct(
                    Cell.MajorOpenDirection.GetSafeNormal2D(), Control.ControlledApproachDirection)));
            Control.Score = (Cell.bJunction ? Settings.Strategic.SpawnControlJunctionWeight : 0.f)
                + (Cell.NarrowConnectionCount > 0 ? Settings.Strategic.SpawnControlChokeWeight : 0.f)
                + FMath::Min(3, Cell.EscapeRouteCount) * Settings.Strategic.SpawnControlEscapeRouteWeight
                + ApproachOpenness * Settings.Strategic.SpawnControlOpennessWeight
                + FrontShape * Settings.Strategic.SpawnControlFrontProgressWeight
                + (bPreferredRange ? 15.f : 0.f) + (bOnMainApproach ? 15.f : 0.f)
                - (Cell.bDeadEnd ? Settings.Strategic.SpawnControlDeadEndPenalty : 0.f)
                - (Cell.CellType == EDMTacticalCellType::Narrow ? Settings.Strategic.SpawnControlNarrowPenalty : 0.f);
            Control.bCandidate = !bExcluded && bPreferredRange && bFriendlyReachable && Cell.EscapeRouteCount >= 2
                && Control.Score > 0.f;
        }
    }
    if (SpawnControlCursor < Cells.Num()) return;

    ++GridRevision;
    PathCache.Reset();
    BuildState = EDMTacticalGridBuildState::Ready;
    NextActorValidationTime = NextDynamicRefreshTime = NextObservationRefreshTime = 0.f;
    UE_LOG(LogTemp, Log, TEXT("DM Tactical Grid ready: %d cells, %d team spawn fields, CellSize %.0f, Samples %dx%d."),
        Cells.Num(), SpawnFieldTeams.Num(), Settings.Build.CellSize, Settings.Build.SamplesPerAxis, Settings.Build.SamplesPerAxis);
}

void UDMTacticalGridSubsystem::RebuildCellIndex()
{
    CellIndexById.Reset();
    for (int32 Index = 0; Index < Cells.Num(); ++Index)
        if (Cells[Index].Id.IsValid()) CellIndexById.Add(Cells[Index].Id, Index);
}

bool UDMTacticalGridSubsystem::GetCell(const FDMTacticalCellId& Id, FDMTacticalCellStaticData& OutCell) const
{
    if (const FDMTacticalCellStaticData* Cell = FindCell(Id)) { OutCell = *Cell; return true; }
    return false;
}

const FDMTacticalCellStaticData* UDMTacticalGridSubsystem::FindCell(const FDMTacticalCellId& Id) const
{
    const int32* Index = CellIndexById.Find(Id);
    return Index && Cells.IsValidIndex(*Index) ? &Cells[*Index] : nullptr;
}

int32 UDMTacticalGridSubsystem::FindClosestCellIndex(const FVector& Location, bool bRequireSpawnReachable) const
{
    int32 Best = INDEX_NONE; float BestDistance = TNumericLimits<float>::Max();
    const float MaximumDistance = FMath::Square(Settings.Build.CellSize * 2.5f);
    for (int32 Index = 0; Index < Cells.Num(); ++Index)
    {
        const FDMTacticalCellStaticData& Cell = Cells[Index];
        if (Cell.CellType == EDMTacticalCellType::Blocked || (bRequireSpawnReachable && !Cell.bReachableFromAnySpawn)) continue;
        if (!Cell.Bounds.ExpandBy(FVector(1.f, 1.f, Settings.Build.LayerHeightThreshold)).IsInsideOrOn(Location))
        {
            const int32 X = FMath::FloorToInt(Location.X / Settings.Build.CellSize), Y = FMath::FloorToInt(Location.Y / Settings.Build.CellSize);
            if (Cell.Id.X != X || Cell.Id.Y != Y) continue;
        }
        const float Distance = FVector::DistSquared(Location, Cell.RepresentativeLocation);
        if (Distance < BestDistance && Distance <= MaximumDistance) { BestDistance = Distance; Best = Index; }
    }
    return Best;
}

bool UDMTacticalGridSubsystem::FindCellAtLocation(const FVector& Location, FDMTacticalCellId& OutId, bool bRequireSpawnReachable) const
{
    if (!IsReady()) return false;
    const int32 Index = FindClosestCellIndex(Location, bRequireSpawnReachable);
    if (!Cells.IsValidIndex(Index)) return false;
    OutId = Cells[Index].Id; return true;
}

bool UDMTacticalGridSubsystem::IsLocationInsideCell(const FDMTacticalCellId& Id, const FVector& Location, float Tolerance) const
{
    const FDMTacticalCellStaticData* Cell = FindCell(Id);
    return Cell && Cell->Bounds.ExpandBy(FMath::Max(0.f, Tolerance)).IsInsideOrOn(Location);
}

EDMTacticalCellType UDMTacticalGridSubsystem::ClassifyCellType(float WalkableRatio, float Openness)
{
    if (WalkableRatio < .2f) return EDMTacticalCellType::Blocked;
    if (WalkableRatio < .5f) return EDMTacticalCellType::Narrow;
    if (WalkableRatio < .8f) return EDMTacticalCellType::Partial;
    return Openness >= .72f ? EDMTacticalCellType::Open : EDMTacticalCellType::Normal;
}

float UDMTacticalGridSubsystem::CalculateDecayedObservation(float Confidence, float ElapsedSeconds, float DecayPerSecond)
{
    return FMath::Clamp(Confidence - FMath::Max(0.f, ElapsedSeconds) * FMath::Max(0.f, DecayPerSecond), 0.f, 1.f);
}

bool UDMTacticalGridSubsystem::HasCapsuleClearance(const FVector& Ground, float Scale) const
{
    if (!GetWorld()) return false;
    const float Radius = FMath::Max(10.f, Settings.Build.AgentRadius * Scale);
    const float HalfHeight = FMath::Max(Radius, Settings.Build.AgentHalfHeight * Scale);
    // Build-time topology only considers permanent world geometry. Doors, movers and pawns belong in the runtime failure cache.
    FCollisionObjectQueryParams Objects; Objects.AddObjectTypesToQuery(ECC_WorldStatic);
    FCollisionQueryParams Params(SCENE_QUERY_STAT(DMTacticalGridClearance), false);
    return !GetWorld()->OverlapAnyTestByObjectType(Ground + FVector(0, 0, HalfHeight + 2.f), FQuat::Identity,
        Objects, FCollisionShape::MakeCapsule(Radius, HalfHeight), Params);
}

float UDMTacticalGridSubsystem::MeasureWallClearance(const FVector& Location, FVector& OutOpenDirection) const
{
    const float Probe = FMath::Max(100.f, Settings.Build.CellSize * WallProbeScale);
    const FVector Directions[8] = {
        FVector(1,0,0), FVector(-1,0,0), FVector(0,1,0), FVector(0,-1,0),
        FVector(1,1,0).GetSafeNormal(), FVector(1,-1,0).GetSafeNormal(), FVector(-1,1,0).GetSafeNormal(), FVector(-1,-1,0).GetSafeNormal() };
    FCollisionObjectQueryParams Objects; Objects.AddObjectTypesToQuery(ECC_WorldStatic);
    FCollisionQueryParams Params(SCENE_QUERY_STAT(DMTacticalGridWall), false);
    float Total = 0.f, Best = -1.f; OutOpenDirection = FVector::ZeroVector;
    const FVector Start = Location + FVector(0, 0, Settings.Build.AgentHalfHeight * .5f);
    for (const FVector& Direction : Directions)
    {
        FHitResult Hit;
        const bool bHit = GetWorld()->LineTraceSingleByObjectType(Hit, Start, Start + Direction * Probe, Objects, Params);
        const float Distance = bHit ? Hit.Distance : Probe; Total += Distance;
        if (Distance > Best) { Best = Distance; OutOpenDirection = Direction; }
    }
    return Total / UE_ARRAY_COUNT(Directions);
}

void UDMTacticalGridSubsystem::RegisterActor(AActor* Actor, uint8 Team, bool bCombatant)
{
    if (!IsValid(Actor)) return;
    FRegisteredActor& Entry = RegisteredActors.FindOrAdd(Actor);
    Entry.Actor = Actor; Entry.Team = Team; Entry.bCombatant = bCombatant;
    if (IsReady()) UpdateActorCell(Actor, Team, bCombatant);
}

void UDMTacticalGridSubsystem::UnregisterActor(AActor* Actor)
{
    if (!Actor) return;
    RegisteredActors.Remove(Actor); ReleaseReservations(Actor);
    for (auto& TeamPair : TeamRuntime) TeamPair.Value.Threats.Remove(Actor);
}

bool UDMTacticalGridSubsystem::UpdateActorCell(AActor* Actor, uint8 Team, bool bCombatant, const FVector& FocusDirection)
{
    if (!IsValid(Actor) || !IsReady()) return false;
    FRegisteredActor& Entry = RegisteredActors.FindOrAdd(Actor);
    Entry.Actor = Actor; Entry.Team = Team; Entry.bCombatant = bCombatant; Entry.FocusDirection = FocusDirection.GetSafeNormal2D();
    FDMTacticalCellId NewCell;
    Entry.bHasCell = FindCellAtLocation(Actor->GetActorLocation(), NewCell, false);
    if (Entry.bHasCell) Entry.CellId = NewCell;
    return Entry.bHasCell;
}

void UDMTacticalGridSubsystem::ReportThreat(uint8 Team, AActor* Observer, AActor* Threat, const FVector& KnownLocation,
    bool bVisible, bool bRemembered, bool bEngagement, float Confidence)
{
    if (!IsReady() || !IsValid(Threat)) return;
    FDMTacticalCellId CellId;
    if (!FindCellAtLocation(KnownLocation, CellId, false)) return;
    FThreatRecord& Record = TeamRuntime.FindOrAdd(Team).Threats.FindOrAdd(Threat);
    Record.Threat = Threat; Record.Observer = Observer; Record.CellId = CellId; Record.LastUpdate = GetWorld()->GetTimeSeconds();
    Record.Confidence = FMath::Clamp(Confidence, 0.f, 1.f); Record.bVisible = bVisible;
    Record.bRemembered = bRemembered || !bVisible; Record.bEngagement = bEngagement;
    FDMTacticalCellDynamicData& Dynamic = FindOrAddDynamic(Team, CellId);
    if (bVisible)
    {
        Dynamic.LastEnemySeenTime = Record.LastUpdate;
        Dynamic.LastFriendlyObservationTime = Record.LastUpdate;
        Dynamic.ObservationConfidence = FMath::Max(Dynamic.ObservationConfidence, .65f);
    }
}

void UDMTacticalGridSubsystem::ReportDamageEvent(uint8 Team, const FVector& Location)
{
    FDMTacticalCellId Id; if (FindCellAtLocation(Location, Id, false)) TeamRuntime.FindOrAdd(Team).DamageEvents.Add({Id, static_cast<float>(GetWorld()->GetTimeSeconds())});
}

void UDMTacticalGridSubsystem::ReportFireEvent(uint8 Team, const FVector& Location)
{
    FDMTacticalCellId Id; if (FindCellAtLocation(Location, Id, false)) TeamRuntime.FindOrAdd(Team).FireEvents.Add({Id, static_cast<float>(GetWorld()->GetTimeSeconds())});
}

void UDMTacticalGridSubsystem::ReportObservation(uint8 Team, const FDMTacticalCellId& CellId, float Confidence)
{
    if (!FindCell(CellId)) return;
    FDMTacticalCellDynamicData& Dynamic = FindOrAddDynamic(Team, CellId);
    Dynamic.ObservationConfidence = FMath::Max(Dynamic.ObservationConfidence, FMath::Clamp(Confidence, 0.f, 1.f));
    Dynamic.LastFriendlyObservationTime = GetWorld()->GetTimeSeconds();
    ++TeamRuntime.FindOrAdd(Team).Revision;
}

FDMTacticalCellDynamicData& UDMTacticalGridSubsystem::FindOrAddDynamic(uint8 Team, const FDMTacticalCellId& Id)
{
    FDMTacticalCellDynamicData& Data = TeamRuntime.FindOrAdd(Team).Cells.FindOrAdd(Id);
    if (Data.FocusDirectionOccupancy.Num() != FMath::Clamp(Settings.Runtime.FocusDirectionBuckets, 1, 16))
        Data.FocusDirectionOccupancy.SetNumZeroed(FMath::Clamp(Settings.Runtime.FocusDirectionBuckets, 1, 16));
    return Data;
}

const FDMTacticalCellDynamicData* UDMTacticalGridSubsystem::FindDynamic(uint8 Team, const FDMTacticalCellId& Id) const
{
    const FTeamRuntime* Runtime = TeamRuntime.Find(Team);
    return Runtime ? Runtime->Cells.Find(Id) : nullptr;
}

bool UDMTacticalGridSubsystem::GetTeamCellData(uint8 Team, const FDMTacticalCellId& Id, FDMTacticalCellDynamicData& OutData) const
{
    if (const FDMTacticalCellDynamicData* Data = FindDynamic(Team, Id)) { OutData = *Data; return true; }
    return false;
}

void UDMTacticalGridSubsystem::PurgeExpired(float Now)
{
    TSet<uint8> ChangedReservationTeams;
    Reservations.RemoveAll([Now, &ChangedReservationTeams](const FDMTacticalCellReservation& R)
    {
        const bool bRemove = !R.Actor.IsValid() || R.ExpirationTime <= Now;
        if (bRemove) ChangedReservationTeams.Add(R.Team);
        return bRemove;
    });
    for (const uint8 Team : ChangedReservationTeams) ++TeamRuntime.FindOrAdd(Team).Revision;
    for (auto& TeamPair : TeamRuntime)
    {
        FTeamRuntime& Runtime = TeamPair.Value;
        for (auto It = Runtime.Threats.CreateIterator(); It; ++It)
            if (!It.Key().IsValid() || Now - It.Value().LastUpdate > FMath::Max(0.f, Settings.Runtime.ThreatMemorySeconds)) It.RemoveCurrent();
        Runtime.DamageEvents.RemoveAll([Now](const FTimedEvent& E) { return Now - E.Time > RecentEventSeconds; });
        Runtime.FireEvents.RemoveAll([Now](const FTimedEvent& E) { return Now - E.Time > RecentEventSeconds; });
    }
    for (auto It = Failures.CreateIterator(); It; ++It)
        if (It.Value().Expiration <= Now || (It.Key().Actor.IsExplicitlyNull() ? false : !It.Key().Actor.IsValid())) It.RemoveCurrent();
}

void UDMTacticalGridSubsystem::UpdateRuntime(float DeltaTime)
{
    const float Now = GetWorld()->GetTimeSeconds(); PurgeExpired(Now);
    if (Now >= NextActorValidationTime)
    {
        NextActorValidationTime = Now + FMath::Max(.05f, Settings.Runtime.ActorValidationInterval);
        TArray<TWeakObjectPtr<AActor>> Invalid;
        for (auto& Pair : RegisteredActors)
        {
            FRegisteredActor& Entry = Pair.Value;
            if (!Entry.Actor.IsValid()) { Invalid.Add(Pair.Key); continue; }
            UpdateActorCell(Entry.Actor.Get(), Entry.Team, Entry.bCombatant, Entry.FocusDirection);
        }
        for (const TWeakObjectPtr<AActor>& Actor : Invalid) RegisteredActors.Remove(Actor);
    }
    for (auto& TeamPair : TeamRuntime)
        for (auto& CellPair : TeamPair.Value.Cells)
            CellPair.Value.ObservationConfidence = CalculateDecayedObservation(CellPair.Value.ObservationConfidence, DeltaTime, Settings.Runtime.ObservationDecayPerSecond);
    if (Now >= NextDynamicRefreshTime)
    {
        NextDynamicRefreshTime = Now + FMath::Max(.05f, Settings.Runtime.ActorValidationInterval);
        RebuildDynamicCounts(Now);
    }
    if (Now >= NextObservationRefreshTime)
    {
        NextObservationRefreshTime = Now + FMath::Max(.1f, Settings.Runtime.ObservationRefreshInterval);
        ApplyFriendlyGraphObservation(Now);
        for (auto& TeamPair : TeamRuntime) RebuildExplorationStates(TeamPair.Key, Now);
    }
}

void UDMTacticalGridSubsystem::RebuildDynamicCounts(float Now)
{
    for (const auto& Pair : RegisteredActors) TeamRuntime.FindOrAdd(Pair.Value.Team);
    for (auto& TeamPair : TeamRuntime)
    {
        for (auto& CellPair : TeamPair.Value.Cells)
        {
            FDMTacticalCellDynamicData& D = CellPair.Value;
            D.FriendlyActorCount = D.FriendlyCombatantCount = D.FriendlyReservedGoalCount = D.PhysicalEnemyCount = 0;
            D.KnownThreatCount = D.VisibleThreatCount = D.RememberedThreatCount = D.EngagementThreatCount = 0;
            D.RecentDamageEvents = D.RecentFireEvents = 0; D.CombatIntensity = 0.f;
            D.AverageFocusDirection = FVector::ZeroVector; D.FocusDirectionOccupancy.SetNumZeroed(FMath::Clamp(Settings.Runtime.FocusDirectionBuckets, 1, 16));
            D.bOccupied = D.bCovered = D.bInferredCovered = D.bContested = false;
        }
    }
    for (const auto& Pair : RegisteredActors)
    {
        const FRegisteredActor& Actor = Pair.Value; if (!Actor.bHasCell) continue;
        FDMTacticalCellDynamicData& Own = FindOrAddDynamic(Actor.Team, Actor.CellId);
        ++Own.FriendlyActorCount; if (Actor.bCombatant) { ++Own.FriendlyCombatantCount; Own.LastCombatTime = Now; }
        Own.LastFriendlyVisitTime = Now; Own.bOccupied = true;
        if (!Actor.FocusDirection.IsNearlyZero())
        {
            Own.AverageFocusDirection += Actor.FocusDirection;
            float Angle = FMath::Atan2(Actor.FocusDirection.Y, Actor.FocusDirection.X); if (Angle < 0) Angle += 2.f * PI;
            const int32 Bucket = FMath::Clamp(FMath::FloorToInt(Angle / (2.f * PI) * Own.FocusDirectionOccupancy.Num()), 0, Own.FocusDirectionOccupancy.Num() - 1);
            ++Own.FocusDirectionOccupancy[Bucket];
        }
        for (auto& TeamPair : TeamRuntime) if (TeamPair.Key != Actor.Team) ++FindOrAddDynamic(TeamPair.Key, Actor.CellId).PhysicalEnemyCount;
    }
    for (const FDMTacticalCellReservation& Reservation : Reservations)
        ++FindOrAddDynamic(Reservation.Team, Reservation.CellId).FriendlyReservedGoalCount;
    for (auto& TeamPair : TeamRuntime)
    {
        const uint8 Team = TeamPair.Key; FTeamRuntime& Runtime = TeamPair.Value;
        for (const auto& ThreatPair : Runtime.Threats)
        {
            const FThreatRecord& Threat = ThreatPair.Value; FDMTacticalCellDynamicData& D = FindOrAddDynamic(Team, Threat.CellId);
            ++D.KnownThreatCount; if (Threat.bVisible) ++D.VisibleThreatCount; else ++D.RememberedThreatCount;
            if (Threat.bEngagement) ++D.EngagementThreatCount;
        }
        for (const FTimedEvent& Event : Runtime.DamageEvents)
        {
            FDMTacticalCellDynamicData& D = FindOrAddDynamic(Team, Event.CellId); ++D.RecentDamageEvents;
            D.CombatIntensity += 1.5f * FMath::Clamp(1.f - (Now - Event.Time) / RecentEventSeconds, 0.f, 1.f);
        }
        for (const FTimedEvent& Event : Runtime.FireEvents)
        {
            FDMTacticalCellDynamicData& D = FindOrAddDynamic(Team, Event.CellId); ++D.RecentFireEvents;
            D.CombatIntensity += FMath::Clamp(1.f - (Now - Event.Time) / RecentEventSeconds, 0.f, 1.f);
        }
        for (auto& CellPair : Runtime.Cells)
        {
            FDMTacticalCellDynamicData& D = CellPair.Value;
            D.AverageFocusDirection = D.AverageFocusDirection.GetSafeNormal2D();
            D.CombatIntensity += D.FriendlyCombatantCount + D.VisibleThreatCount * 2.f;
            if (D.CombatIntensity > 0.f) D.LastCombatTime = Now;
            if (const FDMTacticalCellStaticData* Cell = FindCell(CellPair.Key))
            {
                const FDMTacticalSpawnControlData* BestControl = nullptr;
                for (const auto& ControlPair : Cell->SpawnControlByEnemyTeam)
                {
                    if (ControlPair.Key == Team || !ControlPair.Value.bCandidate) continue;
                    if (!BestControl || ControlPair.Value.Score > BestControl->Score) BestControl = &ControlPair.Value;
                }
                if (BestControl)
                {
                    D.ExpectedEnemyDirection = BestControl->ControlledApproachDirection;
                    D.EnemyApproachProbability = FMath::Clamp(BestControl->Score / 80.f, .1f, 1.f);
                }
            }
        }
        ++Runtime.Revision;
        Runtime.Formation.BuiltTime = -1.f;
    }
}

void UDMTacticalGridSubsystem::ApplyFriendlyGraphObservation(float Now)
{
    const int32 MaxHops = FMath::Clamp(Settings.Runtime.FriendlyObservationRadiusHops, 0, 8);
    for (const auto& Pair : RegisteredActors)
    {
        const FRegisteredActor& Actor = Pair.Value;
        if (!Actor.Actor.IsValid() || !Actor.bHasCell) continue;
        const int32* StartPtr = CellIndexById.Find(Actor.CellId);
        if (!StartPtr) continue;
        if (!Settings.Runtime.bUseConnectionGraphObservation)
        {
            for (const FDMTacticalCellStaticData& Cell : Cells)
            {
                const int32 Hops = FMath::CeilToInt(FVector::Dist2D(Cell.RepresentativeLocation,
                    Cells[*StartPtr].RepresentativeLocation) / FMath::Max(1.f, Settings.Build.CellSize));
                if (Hops > MaxHops) continue;
                const float ExtraFalloff = Hops > 1 ? (Hops - 1) * .1f : 0.f;
                const float Confidence = FMath::Clamp(Settings.Runtime.FriendlyObservationConfidenceAtSelf
                    - Hops * Settings.Runtime.FriendlyObservationConfidencePerHop - ExtraFalloff, 0.f, 1.f);
                FDMTacticalCellDynamicData& Dynamic = FindOrAddDynamic(Actor.Team, Cell.Id);
                Dynamic.ObservationConfidence = FMath::Max(Dynamic.ObservationConfidence, Confidence);
                Dynamic.LastFriendlyObservationTime = Now;
                if (Hops > 0) Dynamic.bCovered = true;
            }
            continue;
        }
        struct FObserveNode { int32 Index = INDEX_NONE; int32 Hops = 0; };
        TArray<FObserveNode> Queue; Queue.Add({*StartPtr, 0});
        TSet<int32> Visited; Visited.Add(*StartPtr);
        for (int32 Cursor = 0; Cursor < Queue.Num(); ++Cursor)
        {
            const FObserveNode Node = Queue[Cursor];
            if (!Cells.IsValidIndex(Node.Index)) continue;
            const float ExtraFalloff = Node.Hops > 1 ? (Node.Hops - 1) * .1f : 0.f;
            const float Confidence = FMath::Clamp(Settings.Runtime.FriendlyObservationConfidenceAtSelf
                - Node.Hops * Settings.Runtime.FriendlyObservationConfidencePerHop - ExtraFalloff, 0.f, 1.f);
            FDMTacticalCellDynamicData& Dynamic = FindOrAddDynamic(Actor.Team, Cells[Node.Index].Id);
            Dynamic.ObservationConfidence = FMath::Max(Dynamic.ObservationConfidence, Confidence);
            Dynamic.LastFriendlyObservationTime = Now;
            if (Node.Hops > 0) Dynamic.bCovered = true;
            if (Node.Hops >= MaxHops) continue;
            for (const FDMTacticalConnection& Edge : Cells[Node.Index].Connections)
            {
                if (!Edge.bHasValidNavPath || Edge.bPartialPath || !Edge.bAgentCapsulePassable) continue;
                const int32* Neighbor = CellIndexById.Find(Edge.Neighbor);
                if (Neighbor && !Visited.Contains(*Neighbor)) { Visited.Add(*Neighbor); Queue.Add({*Neighbor, Node.Hops + 1}); }
            }
        }
    }
}

void UDMTacticalGridSubsystem::RebuildExplorationStates(uint8 Team, float Now)
{
    FTeamRuntime& Runtime = TeamRuntime.FindOrAdd(Team);
    for (const FDMTacticalCellStaticData& Cell : Cells)
    {
        if (Cell.CellType == EDMTacticalCellType::Blocked) continue;
        FDMTacticalCellDynamicData& D = FindOrAddDynamic(Team, Cell.Id);
        const float ObservationAge = D.LastFriendlyObservationTime < 0.f
            ? TNumericLimits<float>::Max() : Now - D.LastFriendlyObservationTime;
        const bool bEverKnown = D.LastFriendlyObservationTime >= 0.f || D.LastFriendlyVisitTime >= 0.f;
        D.bUnknown = !bEverKnown && D.ObservationConfidence <= Settings.Runtime.UnknownConfidenceThreshold;
        D.bRecentlyObserved = bEverKnown && ObservationAge <= Settings.Runtime.RecentlyObservedSeconds;
        D.bStale = bEverKnown && ObservationAge > Settings.Runtime.StaleObservationSeconds;
        D.bKnown = bEverKnown && !D.bRecentlyObserved && !D.bStale;
        D.bOccupied = D.FriendlyActorCount > 0;
        D.bContested = D.VisibleThreatCount > 0 || D.RememberedThreatCount > 0
            || D.RecentDamageEvents > 0 || D.RecentFireEvents > 0 || D.CombatIntensity > 0.f;
        D.bFrontier = false; D.AdjacentUnknownCellCount = 0; D.ConnectedUnknownRegionSize = 0;
    }

    TMap<FDMTacticalCellId, int32> UnknownRegionSizes;
    TSet<FDMTacticalCellId> AssignedUnknown;
    for (const FDMTacticalCellStaticData& Root : Cells)
    {
        const FDMTacticalCellDynamicData* RootDynamic = FindDynamic(Team, Root.Id);
        if (!RootDynamic || !RootDynamic->bUnknown || AssignedUnknown.Contains(Root.Id)) continue;
        TArray<FDMTacticalCellId> Region; Region.Add(Root.Id); AssignedUnknown.Add(Root.Id);
        for (int32 Cursor = 0; Cursor < Region.Num(); ++Cursor)
        {
            const FDMTacticalCellStaticData* Cell = FindCell(Region[Cursor]); if (!Cell) continue;
            for (const FDMTacticalConnection& Edge : Cell->Connections)
            {
                const FDMTacticalCellDynamicData* NeighborDynamic = FindDynamic(Team, Edge.Neighbor);
                if (NeighborDynamic && NeighborDynamic->bUnknown && !AssignedUnknown.Contains(Edge.Neighbor))
                { AssignedUnknown.Add(Edge.Neighbor); Region.Add(Edge.Neighbor); }
            }
        }
        for (const FDMTacticalCellId& Id : Region) UnknownRegionSizes.Add(Id, Region.Num());
    }

    for (const FDMTacticalCellStaticData& Cell : Cells)
    {
        FDMTacticalCellDynamicData* D = Runtime.Cells.Find(Cell.Id); if (!D || D->bUnknown) continue;
        TSet<FDMTacticalCellId> UnknownNeighbors;
        int32 LargestRegion = 0;
        bool bAllCovered = !Cell.Connections.IsEmpty();
        for (const FDMTacticalConnection& Edge : Cell.Connections)
        {
            const FDMTacticalCellDynamicData* Neighbor = FindDynamic(Team, Edge.Neighbor);
            if (Neighbor && Neighbor->bUnknown)
            {
                UnknownNeighbors.Add(Edge.Neighbor);
                LargestRegion = FMath::Max(LargestRegion, UnknownRegionSizes.FindRef(Edge.Neighbor));
            }
            if (!Neighbor || (!Neighbor->bCovered && !Neighbor->bOccupied)) bAllCovered = false;
        }
        D->AdjacentUnknownCellCount = UnknownNeighbors.Num();
        D->ConnectedUnknownRegionSize = LargestRegion;
        D->bFrontier = Cell.bTraversable && D->AdjacentUnknownCellCount > 0
            && (D->bRecentlyObserved || D->bKnown || D->bCovered || D->bStale || D->bOccupied);
        D->bInferredCovered = !D->bCovered && !D->bOccupied && bAllCovered;
    }
    ++Runtime.Revision;
}

FDMTacticalFormationSnapshot UDMTacticalGridSubsystem::GetFormationSnapshot(uint8 Team, const FVector& RequesterLocation)
{
    FTeamRuntime& Runtime = TeamRuntime.FindOrAdd(Team);
    const float Now = GetWorld()->GetTimeSeconds();
    if (Runtime.Formation.BuiltTime >= 0.f && Now - Runtime.Formation.BuiltTime <= FMath::Max(.05f, Settings.Runtime.FormationCacheSeconds))
        return Runtime.Formation;
    TArray<FVector> Locations; TSet<FDMTacticalCellId> UniqueCells;
    for (const auto& Pair : RegisteredActors)
    {
        const FRegisteredActor& Entry = Pair.Value;
        if (Entry.Team == Team && Entry.bHasCell && Entry.Actor.IsValid()) { Locations.Add(Entry.Actor->GetActorLocation()); UniqueCells.Add(Entry.CellId); }
    }
    FDMTacticalFormationSnapshot Result; Result.BuiltTime = Now; Result.FriendlyCellCount = UniqueCells.Num();
    if (Locations.IsEmpty()) { Runtime.Formation = Result; return Result; }
    for (const FVector& L : Locations) Result.FriendlyFormationCenter += L;
    Result.FriendlyFormationCenter /= Locations.Num();
    TArray<float> X, Y, Z; X.Reserve(Locations.Num()); Y.Reserve(Locations.Num()); Z.Reserve(Locations.Num());
    for (const FVector& L : Locations) { X.Add(L.X); Y.Add(L.Y); Z.Add(L.Z); }
    X.Sort(); Y.Sort(); Z.Sort(); const int32 Middle = Locations.Num() / 2;
    const FVector Median(X[Middle], Y[Middle], Z[Middle]);
    TArray<FVector> Dense; for (const FVector& L : Locations) if (FVector::Dist2D(L, Median) <= Settings.Runtime.FormationOutlierDistance) Dense.Add(L);
    if (Dense.IsEmpty()) Dense = Locations;
    for (const FVector& L : Dense) Result.RobustFormationCenter += L;
    Result.RobustFormationCenter /= Dense.Num();
    Result.NearestFriendlyDistance = TNumericLimits<float>::Max();
    for (const FVector& L : Locations)
    {
        Result.FormationRadius = FMath::Max(Result.FormationRadius, FVector::Dist2D(L, Result.RobustFormationCenter));
        const float ToRequester = FVector::Dist2D(L, RequesterLocation); if (ToRequester > 1.f) Result.NearestFriendlyDistance = FMath::Min(Result.NearestFriendlyDistance, ToRequester);
        if (FVector::Dist2D(L, Result.RobustFormationCenter) > Settings.Runtime.FormationOutlierDistance) ++Result.IsolatedFriendlyCount;
    }
    if (!FMath::IsFinite(Result.NearestFriendlyDistance)) Result.NearestFriendlyDistance = 0.f;
    Result.LocalFriendlyDensity = Locations.Num() / FMath::Max(1.f, PI * FMath::Square(FMath::Max(Settings.Build.CellSize, Result.FormationRadius)) / 1000000.f);
    for (const auto& CellPair : Runtime.Cells) if (CellPair.Value.FriendlyCombatantCount > 0) Result.CombatCells.Add(CellPair.Key);
    Runtime.Formation = Result; return Result;
}

FDMTacticalScoreWeights UDMTacticalGridSubsystem::ApplyPersonalityCorrection(
    const FDMTacticalScoreWeights& StateWeights, const FDMTacticalPersonality& P)
{
    FDMTacticalScoreWeights W = StateWeights;
    const auto Factor = [](float Axis) { return 1.f + .35f * FMath::Clamp(Axis, -1.f, 1.f); };
    W.FriendlyFormationWeight *= Factor(P.FormationPreference);
    W.CombatOpportunityWeight *= Factor(P.CombatPreference);
    W.AmbushWeight *= Factor(-P.CombatPreference);
    W.ExplorationWeight *= Factor(P.ExplorationPreference);
    W.SupportedAdvanceWeight *= Factor(P.Aggression);
    W.IsolationRiskWeight *= Factor(-P.Aggression);
    W.KnownEnemyExposureWeight *= Factor(-P.Aggression);
    W.CoverageWeight *= Factor(P.CoveragePreference);
    return W;
}

EDMTacticalPurpose UDMTacticalGridSubsystem::ChoosePurpose(
    const FDMTacticalEvaluationContext& Context, const FDMTacticalCellScore& S) const
{
    struct FOption { EDMTacticalPurpose Purpose; float Utility; };
    TArray<FOption> Options;
    switch (Context.State)
    {
    case EDMTacticalEvaluationState::Visible:
        Options = {{EDMTacticalPurpose::Attack, S.CombatOpportunityScore + S.SupportedAdvanceScore},
            {EDMTacticalPurpose::Join, S.FriendlyFormationScore + S.CombatOpportunityScore * .5f},
            {EDMTacticalPurpose::Ambush, S.AmbushScore + S.CoverageScore * .25f}};
        break;
    case EDMTacticalEvaluationState::LostGrace:
        Options = {{EDMTacticalPurpose::Watch, S.LastKnownThreatScore + S.CoverageScore + S.CurrentCellRetentionScore},
            {EDMTacticalPurpose::Attack, S.CombatOpportunityScore * .6f + S.LastKnownThreatScore * .5f},
            {EDMTacticalPurpose::Ambush, S.AmbushScore + S.LastKnownThreatScore * .4f}};
        break;
    case EDMTacticalEvaluationState::PendingSwitch:
        Options = {{EDMTacticalPurpose::Watch, S.CurrentCellRetentionScore + S.CoverageScore},
            {EDMTacticalPurpose::Attack, S.CombatOpportunityScore},
            {EDMTacticalPurpose::Join, S.FriendlyFormationScore + S.CurrentCellRetentionScore * .5f}};
        break;
    case EDMTacticalEvaluationState::Investigating:
        Options = {{EDMTacticalPurpose::Explore, S.LastKnownThreatScore + S.ExplorationScore},
            {EDMTacticalPurpose::Watch, S.LastKnownThreatScore * .7f + S.CoverageScore},
            {EDMTacticalPurpose::Ambush, S.AmbushScore + S.LastKnownThreatScore * .35f}};
        break;
    case EDMTacticalEvaluationState::WatchingLastKnown:
        Options = {{EDMTacticalPurpose::Watch, S.LastKnownThreatScore + S.CoverageScore + S.CurrentCellRetentionScore},
            {EDMTacticalPurpose::Ambush, S.AmbushScore + S.CoverageScore * .4f},
            {EDMTacticalPurpose::Explore, S.ExplorationScore * .5f + S.LastKnownThreatScore * .35f}};
        break;
    default:
        Options = {{EDMTacticalPurpose::Explore, S.ExplorationScore + S.StaleVisitScore + S.EnemyApproachProbabilityScore * .35f},
            {EDMTacticalPurpose::Watch, S.CoverageScore + S.EnemyApproachProbabilityScore + S.StaleVisitScore * .35f}};
        break;
    }
    Options.StableSort([](const FOption& A, const FOption& B) { return A.Utility > B.Utility; });
    return Options.IsEmpty() ? EDMTacticalPurpose::None : Options[0].Purpose;
}

void UDMTacticalGridSubsystem::BuildScoreContributors(FDMTacticalCellScore& S) const
{
    TArray<TPair<FString, float>> Positive = {
        {TEXT("Formation"), S.FriendlyFormationScore}, {TEXT("Combat"), S.CombatOpportunityScore},
        {TEXT("Ambush"), S.AmbushScore}, {TEXT("Exploration"), S.ExplorationScore},
        {TEXT("Coverage"), S.CoverageScore}, {TEXT("Advance"), S.SupportedAdvanceScore},
        {TEXT("StaleVisit"), S.StaleVisitScore}, {TEXT("EnemyApproach"), S.EnemyApproachProbabilityScore},
        {TEXT("LastKnown"), S.LastKnownThreatScore}, {TEXT("Retention"), S.CurrentCellRetentionScore},
        {TEXT("Frontier"), S.FrontierScore}, {TEXT("UnknownRegion"), S.UnknownRegionSizeScore},
        {TEXT("PreferredHops"), S.PreferredHopDistanceScore}, {TEXT("SpawnControl"), S.SpawnControlScore},
        {TEXT("StableTie"), FMath::Max(0.f, S.StableTieBreakScore)}};
    TArray<TPair<FString, float>> Negative = {
        {TEXT("Isolation"), S.IsolationRisk}, {TEXT("Overcrowding"), S.OvercrowdingPenalty},
        {TEXT("Travel"), S.TravelCost}, {TEXT("Exposure"), S.KnownEnemyExposure},
        {TEXT("Failure"), S.RecentlyFailedGridPenalty}, {TEXT("RecentRoam"), S.RecentRoamCellPenalty},
        {TEXT("RouteRisk"), S.RouteRiskPenalty}, {TEXT("SpawnExclusion"), S.SpawnExclusionPenalty},
        {TEXT("CellReservation"), S.StrategicReservationPenalty},
        {TEXT("ApproachCongestion"), S.ApproachRouteCongestionPenalty},
        {TEXT("StableTie"), FMath::Max(0.f, -S.StableTieBreakScore)}};
    Positive.StableSort([](const auto& A, const auto& B) { return A.Value > B.Value; });
    Negative.StableSort([](const auto& A, const auto& B) { return A.Value > B.Value; });
    for (int32 Index = 0; Index < FMath::Min(3, Positive.Num()); ++Index)
        if (Positive[Index].Value > 0.f) S.TopPositiveContributors.Add(
            FString::Printf(TEXT("%s +%.1f"), *Positive[Index].Key, Positive[Index].Value));
    for (int32 Index = 0; Index < FMath::Min(3, Negative.Num()); ++Index)
        if (Negative[Index].Value > 0.f) S.TopNegativeContributors.Add(
            FString::Printf(TEXT("%s -%.1f"), *Negative[Index].Key, Negative[Index].Value));
}

bool UDMTacticalGridSubsystem::IsTeamReachable(const FDMTacticalCellStaticData& Cell, uint8 Team) const
{
    return Team >= 31 || Cell.ReachableTeamMask == -1 || (Cell.ReachableTeamMask & (1 << Team)) != 0;
}

bool UDMTacticalGridSubsystem::IsStrategicCellPlayable(
    const FDMTacticalCellStaticData& Cell, uint8 RequestingTeam) const
{
    if (!Cell.bNavMeshProjectionSucceeded || !Cell.bHasValidGridConnection || Cell.Id.Region == INDEX_NONE
        || StrategicTeamA == 255 || StrategicTeamB == 255)
        return false;
    switch (Settings.Strategic.StrategicReachabilityPolicy)
    {
    case EDMTacticalStrategicReachabilityPolicy::ReachableFromEitherTeam:
        return Cell.bReachableFromTeamARepresentativeSpawn || Cell.bReachableFromTeamBRepresentativeSpawn;
    case EDMTacticalStrategicReachabilityPolicy::ReachableFromRequestingTeam:
        return IsTeamReachable(Cell, RequestingTeam);
    default:
        return Cell.bReachableFromTeamARepresentativeSpawn && Cell.bReachableFromTeamBRepresentativeSpawn;
    }
}

int32 UDMTacticalGridSubsystem::CountCellReservations(
    uint8 Team, const FDMTacticalCellId& CellId, AActor* IgnoreActor) const
{
    int32 Count = 0;
    for (const FDMTacticalCellReservation& Reservation : Reservations)
        if (Reservation.Team == Team && Reservation.CellId == CellId && Reservation.Actor.Get() != IgnoreActor
            && (Reservation.Purpose == EDMTacticalPurpose::ExploreFrontier
                || Reservation.Purpose == EDMTacticalPurpose::SpawnControl))
            ++Count;
    return Count;
}

int32 UDMTacticalGridSubsystem::CountApproachRouteReservations(
    uint8 Team, int32 ApproachRouteId, AActor* IgnoreActor) const
{
    if (ApproachRouteId == INDEX_NONE) return 0;
    int32 Count = 0;
    for (const FDMTacticalCellReservation& Reservation : Reservations)
        if (Reservation.Team == Team && Reservation.Purpose == EDMTacticalPurpose::SpawnControl
            && Reservation.ApproachRouteId == ApproachRouteId && Reservation.Actor.Get() != IgnoreActor)
            ++Count;
    return Count;
}

void UDMTacticalGridSubsystem::ResetDynamicPathMetrics(FDMTacticalPathResult& Path)
{
    Path.NarrowCellCount = Path.DeadEndCellCount = Path.UnknownCellCount = Path.FrontierCellCount = 0;
    Path.ContestedCellCount = Path.FriendlyOccupiedCellCount = Path.FriendlyReservedCellCount = 0;
    Path.RecentlyFailedCellCount = 0;
    Path.AverageRisk = Path.MaximumRisk = Path.PathExposure = Path.TotalPathCost = 0.f;
    Path.EstimatedTravelSeconds = 0.f;
}

float UDMTacticalGridSubsystem::GetRecentRoamPenalty(
    const FDMTacticalEvaluationContext& Context, const FDMTacticalCellId& CellId) const
{
    const float RankScale[] = {1.f, .6f, .3f, .15f, .08f};
    for (int32 Index = 0; Index < Context.RecentRoamCells.Num(); ++Index)
    {
        if (Context.RecentRoamCells[Index] != CellId) continue;
        const float Age = Context.RecentRoamTimes.IsValidIndex(Index)
            ? FMath::Max(0.f, Context.Now - Context.RecentRoamTimes[Index]) : 0.f;
        const float TimeScale = FMath::Clamp(1.f - Age / FMath::Max(.1f,
            Settings.Strategic.RecentRoamPenaltyDecaySeconds), 0.f, 1.f);
        const int32 RankIndex = FMath::Min(Index, static_cast<int32>(UE_ARRAY_COUNT(RankScale)) - 1);
        const float Scale = RankScale[RankIndex];
        return Settings.Strategic.RecentRoamCellPenalty * Scale * TimeScale;
    }
    return 0.f;
}

void UDMTacticalGridSubsystem::PopulatePathMetrics(
    AActor* Requester, uint8 Team, FDMTacticalPathResult& Path) const
{
    float RiskSum = 0.f;
    for (const FDMTacticalCellId& Id : Path.PathCells)
    {
        const FDMTacticalCellStaticData* Cell = FindCell(Id); if (!Cell) continue;
        const FDMTacticalCellDynamicData* Dynamic = FindDynamic(Team, Id);
        const FDMTacticalCellDynamicData Empty;
        const FDMTacticalCellDynamicData& D = Dynamic ? *Dynamic : Empty;
        if (Cell->CellType == EDMTacticalCellType::Narrow) ++Path.NarrowCellCount;
        if (Cell->bDeadEnd) ++Path.DeadEndCellCount;
        if (D.bUnknown) ++Path.UnknownCellCount;
        if (D.bFrontier) ++Path.FrontierCellCount;
        if (D.bContested) ++Path.ContestedCellCount;
        if (D.FriendlyActorCount > 0) ++Path.FriendlyOccupiedCellCount;
        if (D.FriendlyReservedGoalCount > 0) ++Path.FriendlyReservedCellCount;
        for (const auto& Failure : Failures)
            if (Failure.Key.Actor.Get() == Requester && Failure.Key.Team == Team && Failure.Key.CellId == Id
                && Failure.Value.Expiration > GetWorld()->GetTimeSeconds())
            { ++Path.RecentlyFailedCellCount; break; }
        const float Risk = FMath::Clamp(D.CombatIntensity * .15f + D.VisibleThreatCount * .35f
            + D.RememberedThreatCount * .15f + (D.bContested ? .25f : 0.f), 0.f, 1.f);
        RiskSum += Risk; Path.MaximumRisk = FMath::Max(Path.MaximumRisk, Risk);
        Path.PathExposure += Risk * (.5f + Cell->Openness * .5f);
    }
    const int32 Count = FMath::Max(1, Path.PathCells.Num());
    Path.AverageRisk = RiskSum / Count;
    Path.PathExposure /= Count;
    Path.EstimatedTravelSeconds = Path.TotalTravelDistance
        / FMath::Max(1.f, Settings.Strategic.EstimatedTravelSpeed);
    Path.TotalPathCost = Path.TotalTravelDistance / FMath::Max(1.f, Settings.Build.CellSize)
        + Path.CellHopCount * Settings.Strategic.HopCountCost
        + Path.NarrowCellCount * Settings.Strategic.NarrowRouteCost
        + Path.ContestedCellCount * Settings.Strategic.ContestedRouteCost
        + (Path.FriendlyOccupiedCellCount + Path.FriendlyReservedCellCount) * Settings.Strategic.CongestionRouteCost
        + Path.RecentlyFailedCellCount * Settings.Strategic.FailedRouteCost
        + (Path.AverageRisk + Path.PathExposure) * Settings.Strategic.PathRiskCost;
}

bool UDMTacticalGridSubsystem::AnalyzeTacticalPath(AActor* Requester, uint8 Team,
    const FDMTacticalCellId& StartCell, const FDMTacticalCellId& DestinationCell, FDMTacticalPathResult& OutPath)
{
    OutPath = FDMTacticalPathResult();
    const int32* StartPtr = CellIndexById.Find(StartCell);
    const int32* GoalPtr = CellIndexById.Find(DestinationCell);
    if (!IsReady() || !StartPtr || !GoalPtr) return false;
    FPathCacheKey Key; Key.Start = StartCell; Key.Destination = DestinationCell;
    Key.Team = Team; Key.GridRevision = GridRevision;
    if (const FDMTacticalPathResult* Cached = PathCache.Find(Key))
    {
        OutPath = *Cached;
        ResetDynamicPathMetrics(OutPath);
        if (OutPath.bReachable) PopulatePathMetrics(Requester, Team, OutPath);
        return OutPath.bReachable;
    }

    struct FPathNode { int32 Index = INDEX_NONE; float Distance = 0.f; };
    const auto MinHeap = [](const FPathNode& A, const FPathNode& B) { return A.Distance < B.Distance; };
    TArray<float> Distances; Distances.Init(TNumericLimits<float>::Max(), Cells.Num());
    TArray<int32> Previous; Previous.Init(INDEX_NONE, Cells.Num());
    TArray<FPathNode> Heap; Distances[*StartPtr] = 0.f; Heap.HeapPush({*StartPtr, 0.f}, MinHeap);
    while (!Heap.IsEmpty())
    {
        FPathNode Node; Heap.HeapPop(Node, MinHeap, EAllowShrinking::No);
        if (Node.Distance > Distances[Node.Index] + KINDA_SMALL_NUMBER) continue;
        if (Node.Index == *GoalPtr) break;
        for (const FDMTacticalConnection& Edge : Cells[Node.Index].Connections)
        {
            if (!Edge.bHasValidNavPath || Edge.bPartialPath || !Edge.bAgentCapsulePassable) continue;
            const int32* NeighborPtr = CellIndexById.Find(Edge.Neighbor); if (!NeighborPtr) continue;
            if (!Cells[*NeighborPtr].bTraversable || !IsTeamReachable(Cells[*NeighborPtr], Team)) continue;
            const float Cost = Edge.ActualPathLength > 0.f ? Edge.ActualPathLength : Edge.TravelCost;
            const float NewDistance = Node.Distance + FMath::Max(1.f, Cost);
            if (NewDistance + KINDA_SMALL_NUMBER >= Distances[*NeighborPtr]) continue;
            Distances[*NeighborPtr] = NewDistance; Previous[*NeighborPtr] = Node.Index;
            Heap.HeapPush({*NeighborPtr, NewDistance}, MinHeap);
        }
    }
    if (!FMath::IsFinite(Distances[*GoalPtr]) || Distances[*GoalPtr] == TNumericLimits<float>::Max())
    { PathCache.Add(Key, OutPath); return false; }

    TArray<FDMTacticalCellId> Reverse;
    for (int32 Index = *GoalPtr; Index != INDEX_NONE; Index = Previous[Index])
    {
        Reverse.Add(Cells[Index].Id);
        if (Index == *StartPtr) break;
    }
    if (Reverse.IsEmpty() || Reverse.Last() != StartCell) { PathCache.Add(Key, OutPath); return false; }
    for (int32 Index = Reverse.Num() - 1; Index >= 0; --Index) OutPath.PathCells.Add(Reverse[Index]);
    OutPath.bReachable = true; OutPath.CellHopCount = FMath::Max(0, OutPath.PathCells.Num() - 1);
    OutPath.TotalTravelDistance = Distances[*GoalPtr];
    if (PathCache.Num() >= 256) PathCache.Reset();
    PathCache.Add(Key, OutPath);
    PopulatePathMetrics(Requester, Team, OutPath);
    return true;
}

bool UDMTacticalGridSubsystem::FindTacticalCandidates(AActor* Requester, uint8 Team, const FDMTacticalEvaluationContext& Context,
    int32 SearchRadius, int32 MaximumCandidates, TArray<FDMTacticalCellScore>& OutCandidates)
{
    OutCandidates.Reset(); if (!IsReady() || !IsValid(Requester)) return false;
    const int32 Start = FindClosestCellIndex(Requester->GetActorLocation(), true); if (Start == INDEX_NONE) return false;
    const bool bRoam = Context.State == EDMTacticalEvaluationState::DisengagedRoam;
    const int32 Radius = bRoam ? MAX_int32 : FMath::Clamp(SearchRadius, 1, 12);
    const FDMTacticalScoreWeights StateWeights = Settings.StateProfiles.ForState(Context.State);
    const FDMTacticalScoreWeights W = ApplyPersonalityCorrection(StateWeights, Context.Personality);
    const FDMTacticalFormationSnapshot Formation = GetFormationSnapshot(Team, Requester->GetActorLocation());

    struct FSearchNode { int32 Index = INDEX_NONE; float Distance = 0.f; int32 Hops = 0; };
    const auto MinHeap = [](const FSearchNode& A, const FSearchNode& B) { return A.Distance < B.Distance; };
    TArray<float> Distances; Distances.Init(TNumericLimits<float>::Max(), Cells.Num());
    TArray<int32> Hops; Hops.Init(MAX_int32, Cells.Num());
    TArray<int32> Previous; Previous.Init(INDEX_NONE, Cells.Num());
    TArray<FSearchNode> Heap; Distances[Start] = 0.f; Hops[Start] = 0; Heap.HeapPush({Start, 0.f, 0}, MinHeap);
    while (!Heap.IsEmpty())
    {
        FSearchNode Node; Heap.HeapPop(Node, MinHeap, EAllowShrinking::No);
        if (Node.Distance > Distances[Node.Index] + KINDA_SMALL_NUMBER || (!bRoam && Node.Hops >= Radius)) continue;
        for (const FDMTacticalConnection& Edge : Cells[Node.Index].Connections)
        {
            if (!Edge.bHasValidNavPath || Edge.bPartialPath || !Edge.bAgentCapsulePassable) continue;
            const int32* Neighbor = CellIndexById.Find(Edge.Neighbor); if (!Neighbor) continue;
            if (!Cells[*Neighbor].bTraversable || !IsTeamReachable(Cells[*Neighbor], Team)
                || (bRoam && !IsStrategicCellPlayable(Cells[*Neighbor], Team))) continue;
            const float NewDistance = Node.Distance + FMath::Max(1.f,
                Edge.ActualPathLength > 0.f ? Edge.ActualPathLength : Edge.TravelCost);
            const int32 NewHops = Node.Hops + 1;
            if ((!bRoam && NewHops > Radius) || NewDistance + KINDA_SMALL_NUMBER >= Distances[*Neighbor]) continue;
            Distances[*Neighbor] = NewDistance; Hops[*Neighbor] = NewHops; Previous[*Neighbor] = Node.Index;
            Heap.HeapPush({*Neighbor, NewDistance, NewHops}, MinHeap);
        }
    }

    auto Gather = [&](bool bAllowNearFallback)
    {
        TArray<int32> Eligible;
        for (int32 Index = 0; Index < Cells.Num(); ++Index)
        {
            const FDMTacticalCellStaticData& Cell = Cells[Index];
            if (!FMath::IsFinite(Distances[Index]) || Distances[Index] == TNumericLimits<float>::Max()
                || Cell.CellType == EDMTacticalCellType::Blocked || !Cell.bTraversable || !Cell.bReachableFromAnySpawn
                || !IsTeamReachable(Cell, Team) || (bRoam && !IsStrategicCellPlayable(Cell, Team))) continue;
            if (bRoam && !bAllowNearFallback && Hops[Index] < Settings.Strategic.MinimumRoamCellHops) continue;
            const FDMTacticalCellDynamicData* Dynamic = FindDynamic(Team, Cell.Id); const FDMTacticalCellDynamicData Empty;
            const FDMTacticalCellDynamicData& D = Dynamic ? *Dynamic : Empty;
            const FDMTacticalSpawnControlData* BestControl = nullptr;
            for (const auto& Pair : Cell.SpawnControlByEnemyTeam)
                if (Pair.Key != Team && Pair.Value.bCandidate && (!BestControl || Pair.Value.Score > BestControl->Score))
                    BestControl = &Pair.Value;
            if (bRoam && !bAllowNearFallback && !D.bFrontier && !BestControl) continue;
            Eligible.Add(Index);
        }
        Eligible.StableSort([&](int32 A, int32 B)
        {
            const FDMTacticalCellDynamicData* DA = FindDynamic(Team, Cells[A].Id);
            const FDMTacticalCellDynamicData* DB = FindDynamic(Team, Cells[B].Id);
            const float FrontierA = DA && DA->bFrontier ? DA->AdjacentUnknownCellCount * 8.f + DA->ConnectedUnknownRegionSize * 2.f : 0.f;
            const float FrontierB = DB && DB->bFrontier ? DB->AdjacentUnknownCellCount * 8.f + DB->ConnectedUnknownRegionSize * 2.f : 0.f;
            float ControlA = 0.f, ControlB = 0.f;
            for (const auto& Pair : Cells[A].SpawnControlByEnemyTeam) if (Pair.Key != Team && Pair.Value.bCandidate) ControlA = FMath::Max(ControlA, Pair.Value.Score);
            for (const auto& Pair : Cells[B].SpawnControlByEnemyTeam) if (Pair.Key != Team && Pair.Value.bCandidate) ControlB = FMath::Max(ControlB, Pair.Value.Score);
            const float RoughA = FrontierA + ControlA - Distances[A] / FMath::Max(1.f, Settings.Build.CellSize);
            const float RoughB = FrontierB + ControlB - Distances[B] / FMath::Max(1.f, Settings.Build.CellSize);
            return RoughA > RoughB;
        });
        if (bRoam)
        {
            const int32 AnalysisLimit = FMath::Clamp(FMath::Max(MaximumCandidates * 4, 32), 32, 128);
            if (Eligible.Num() > AnalysisLimit) Eligible.SetNum(AnalysisLimit);
        }

        for (const int32 Index : Eligible)
        {
            const FDMTacticalCellStaticData& Cell = Cells[Index];
            const FDMTacticalCellDynamicData* Dynamic = FindDynamic(Team, Cell.Id); const FDMTacticalCellDynamicData Empty;
            const FDMTacticalCellDynamicData& D = Dynamic ? *Dynamic : Empty;
            const FDMTacticalSpawnControlData* BestControl = nullptr;
            for (const auto& Pair : Cell.SpawnControlByEnemyTeam)
                if (Pair.Key != Team && Pair.Value.bCandidate && (!BestControl || Pair.Value.Score > BestControl->Score))
                    BestControl = &Pair.Value;

            FDMTacticalCellScore Base; Base.CellId = Cell.Id; Base.EvaluationState = Context.State;
            Base.StateWeights = StateWeights; Base.AppliedWeights = W;
            TArray<FDMTacticalCellId> Reverse;
            for (int32 PathIndex = Index; PathIndex != INDEX_NONE; PathIndex = Previous[PathIndex])
            {
                Reverse.Add(Cells[PathIndex].Id);
                if (PathIndex == Start) break;
            }
            if (Reverse.IsEmpty() || Reverse.Last() != Cells[Start].Id) continue;
            for (int32 ReverseIndex = Reverse.Num() - 1; ReverseIndex >= 0; --ReverseIndex) Base.Path.PathCells.Add(Reverse[ReverseIndex]);
            Base.Path.bReachable = true; Base.Path.CellHopCount = FMath::Max(0, Base.Path.PathCells.Num() - 1);
            Base.Path.TotalTravelDistance = Distances[Index]; PopulatePathMetrics(Requester, Team, Base.Path);
            FPathCacheKey CacheKey; CacheKey.Start = Cells[Start].Id; CacheKey.Destination = Cell.Id;
            CacheKey.Team = Team; CacheKey.GridRevision = GridRevision;
            if (PathCache.Num() >= 256) PathCache.Reset();
            FDMTacticalPathResult StaticPath = Base.Path; ResetDynamicPathMetrics(StaticPath); PathCache.Add(CacheKey, StaticPath);
            const float TravelCells = Base.Path.TotalTravelDistance / FMath::Max(1.f, Settings.Build.CellSize);
            const float FormationDistance = FVector::Dist2D(Cell.RepresentativeLocation, Formation.RobustFormationCenter)
                / FMath::Max(1.f, Settings.Build.CellSize);
            Base.FriendlyFormationScore = W.FriendlyFormationWeight * (25.f / (1.f + FormationDistance) + D.FriendlyCombatantCount * 12.f);
            Base.CombatOpportunityScore = W.CombatOpportunityWeight * (D.CombatIntensity * 12.f + D.KnownThreatCount * 8.f);
            Base.AmbushScore = W.AmbushWeight * (Cell.bJunction ? 16.f : 5.f) * (1.f - FMath::Abs(Cell.Openness - .5f));
            Base.ExplorationScore = W.ExplorationWeight * (1.f - D.ObservationConfidence) * (Cell.bJunction ? 24.f : 16.f);
            int32 Watched = 0; for (int32 Count : D.FocusDirectionOccupancy) if (Count > 0) ++Watched;
            const float CoverageGap = 1.f - static_cast<float>(Watched) / FMath::Max(1, D.FocusDirectionOccupancy.Num());
            Base.CoverageScore = W.CoverageWeight * CoverageGap * 18.f;
            const float DefensiveSuppression = 1.f - .6f * FMath::Clamp(Context.HealthRisk + Context.RecentDamagePressure * .5f, 0.f, 1.f);
            Base.SupportedAdvanceScore = W.SupportedAdvanceWeight * FMath::Min(18.f, Hops[Index] * 3.f)
                * (FormationDistance < 5.f ? 1.f : .25f) * DefensiveSuppression;
            Base.IsolationRisk = W.IsolationRiskWeight * (FormationDistance > 5.f ? (FormationDistance - 5.f) * 10.f : 0.f)
                * (1.f + Context.HealthRisk * .5f);
            Base.OvercrowdingPenalty = W.OvercrowdingWeight * (D.FriendlyActorCount * 8.f + D.FriendlyReservedGoalCount * 15.f
                + (Cell.CellType == EDMTacticalCellType::Narrow ? 18.f : 0.f) + (Cell.bDeadEnd ? 10.f : 0.f));
            Base.TravelCost = W.TravelCostWeight * TravelCells * 4.f
                + (bRoam ? Base.Path.CellHopCount * Settings.Strategic.HopCountCost : 0.f);
            Base.KnownEnemyExposure = W.KnownEnemyExposureWeight * (D.KnownThreatCount * 8.f + D.VisibleThreatCount * 14.f)
                * (1.f + Context.HealthRisk + Context.RecentDamagePressure * .5f);
            const float VisitAge = D.LastFriendlyVisitTime < 0.f ? 1.f : FMath::Clamp((Context.Now - D.LastFriendlyVisitTime) / 30.f, 0.f, 1.f);
            const float ObservationAge = D.LastFriendlyObservationTime < 0.f ? 1.f : FMath::Clamp((Context.Now - D.LastFriendlyObservationTime) / 30.f, 0.f, 1.f);
            Base.StaleVisitScore = W.StaleVisitWeight * (VisitAge * 12.f + ObservationAge * 8.f);
            Base.EnemyApproachProbabilityScore = W.EnemyApproachProbabilityWeight * D.EnemyApproachProbability * 12.f;
            if (Context.bHasLastKnownThreatLocation)
            {
                const float Distance = FVector::Dist2D(Cell.RepresentativeLocation, Context.LastKnownThreatLocation)
                    / FMath::Max(1.f, Settings.Build.CellSize);
                const float Shape = Context.State == EDMTacticalEvaluationState::WatchingLastKnown
                    ? 1.f / (1.f + FMath::Abs(Distance - 1.5f)) : 1.f / (1.f + Distance);
                Base.LastKnownThreatScore = W.LastKnownThreatWeight * Shape * 24.f;
            }
            if ((Context.bHasCurrentCell && Cell.Id == Context.CurrentCell)
                || (Context.bHasCurrentSelection && Cell.Id == Context.CurrentSelectedCell))
                Base.CurrentCellRetentionScore = W.CurrentCellRetentionWeight * 20.f;
            Base.FrontierScore = D.bFrontier ? Settings.Strategic.FrontierScoreWeight
                * (D.AdjacentUnknownCellCount * 8.f + VisitAge * 5.f + ObservationAge * 4.f) : 0.f;
            Base.UnknownRegionSizeScore = D.bFrontier ? Settings.Strategic.UnknownRegionSizeWeight
                * FMath::Min(30.f, static_cast<float>(D.ConnectedUnknownRegionSize) * 2.f) : 0.f;
            Base.FrontierScore *= Settings.Strategic.ExploreFrontierPurposeWeight;
            Base.UnknownRegionSizeScore *= Settings.Strategic.ExploreFrontierPurposeWeight;
            if (Hops[Index] >= Settings.Strategic.PreferredRoamCellHopsMin
                && Hops[Index] <= Settings.Strategic.PreferredRoamCellHopsMax)
                Base.PreferredHopDistanceScore = Settings.Strategic.HopDistancePreferenceWeight * 12.f;
            if (Context.bHoldRoamDirection && !Context.PreferredRoamDirection.IsNearlyZero())
            {
                const float Dot = FVector::DotProduct((Cell.RepresentativeLocation - Requester->GetActorLocation()).GetSafeNormal2D(),
                    Context.PreferredRoamDirection.GetSafeNormal2D());
                Base.PreferredHopDistanceScore += FMath::Max(0.f, Dot) * 8.f;
                if (Dot < 0.f) Base.RouteRiskPenalty += -Dot * 12.f;
            }
            Base.RecentRoamCellPenalty = GetRecentRoamPenalty(Context, Cell.Id);
            if (bRoam)
                Base.RouteRiskPenalty += Base.Path.NarrowCellCount * Settings.Strategic.NarrowRouteCost
                    + Base.Path.ContestedCellCount * Settings.Strategic.ContestedRouteCost
                    + (Base.Path.FriendlyOccupiedCellCount + Base.Path.FriendlyReservedCellCount) * Settings.Strategic.CongestionRouteCost
                    + Base.Path.RecentlyFailedCellCount * Settings.Strategic.FailedRouteCost
                    + (Base.Path.AverageRisk + Base.Path.PathExposure) * Settings.Strategic.PathRiskCost;
            Base.StrategicCellReservationCount = CountCellReservations(Team, Cell.Id, Requester);
            Base.StrategicCellCapacity = FMath::Max(1, Settings.Strategic.StrategicCellCapacity);
            Base.StrategicReservationPenalty = Base.StrategicCellReservationCount
                * FMath::Max(0.f, Settings.Strategic.StrategicReservationPenaltyPerController);
            const uint32 TieHash = HashCombine(GetTypeHash(Requester->GetUniqueID()), GetTypeHash(Cell.Id));
            Base.StableTieBreakScore = (static_cast<float>(TieHash % 2001) / 1000.f - 1.f)
                * FMath::Max(0.f, Settings.Strategic.StableControllerTieBreakMagnitude);

            auto FinalizeScore = [&](FDMTacticalCellScore& S)
            {
                S.RecentlyFailedGridPenalty = GetFailurePenalty(Requester, Team, Cell.Id, S.Purpose);
                S.TotalScore = S.FriendlyFormationScore + S.CombatOpportunityScore + S.AmbushScore + S.ExplorationScore
                    + S.CoverageScore + S.SupportedAdvanceScore + S.StaleVisitScore + S.EnemyApproachProbabilityScore
                    + S.LastKnownThreatScore + S.CurrentCellRetentionScore + S.FrontierScore + S.UnknownRegionSizeScore
                    + S.PreferredHopDistanceScore + S.SpawnControlScore + S.StableTieBreakScore
                    - S.IsolationRisk - S.OvercrowdingPenalty - S.TravelCost - S.KnownEnemyExposure
                    - S.RecentlyFailedGridPenalty - S.RecentRoamCellPenalty - S.RouteRiskPenalty
                    - S.SpawnExclusionPenalty - S.StrategicReservationPenalty - S.ApproachRouteCongestionPenalty;
                BuildScoreContributors(S);
            };

            if (!bRoam)
            {
                FDMTacticalCellScore S = Base;
                S.Purpose = ChoosePurpose(Context, S);
                FinalizeScore(S);
                if (S.RecentlyFailedGridPenalty <= 0.f) OutCandidates.Add(MoveTemp(S));
                continue;
            }

            if (D.bFrontier || bAllowNearFallback)
            {
                FDMTacticalCellScore Frontier = Base;
                Frontier.Purpose = EDMTacticalPurpose::ExploreFrontier;
                Frontier.SpawnControlScore = 0.f;
                Frontier.ApproachRouteId = INDEX_NONE;
                Frontier.ControlledApproachDirection = FVector::ZeroVector;
                FinalizeScore(Frontier);
                if (Frontier.RecentlyFailedGridPenalty <= 0.f) OutCandidates.Add(MoveTemp(Frontier));
            }
            if (BestControl)
            {
                FDMTacticalCellScore Control = Base;
                Control.Purpose = EDMTacticalPurpose::SpawnControl;
                Control.FrontierScore = Control.UnknownRegionSizeScore = 0.f;
                Control.SpawnControlScore = BestControl->Score * Settings.Strategic.SpawnControlPurposeWeight;
                Control.RelatedEnemySpawnTeam = BestControl->RelatedEnemySpawnTeam;
                Control.ApproachRouteId = BestControl->ApproachRouteId;
                Control.ControlledApproachDirection = BestControl->ControlledApproachDirection;
                Control.ApproachRouteCapacity = FMath::Max(1, Settings.Strategic.MaximumControllersPerApproachRoute);
                Control.ApproachRouteUserCount = CountApproachRouteReservations(Team, Control.ApproachRouteId, Requester);
                Control.ApproachRouteCongestionPenalty = Control.ApproachRouteUserCount
                    * FMath::Max(0.f, Settings.Strategic.ApproachRouteCongestionPenaltyPerController);
                FinalizeScore(Control);
                if (Control.RecentlyFailedGridPenalty <= 0.f) OutCandidates.Add(MoveTemp(Control));
            }
        }
    };
    Gather(false);
    if (bRoam && OutCandidates.IsEmpty()) Gather(true);

    if (bRoam && !OutCandidates.IsEmpty())
    {
        float BestFrontierValue = 0.f;
        bool bHasUncongestedApproach = false;
        bool bHasSpawnControl = false;
        int32 LeastCongestedApproachUsers = MAX_int32;
        for (const FDMTacticalCellScore& S : OutCandidates)
        {
            if (S.Purpose == EDMTacticalPurpose::ExploreFrontier)
                BestFrontierValue = FMath::Max(BestFrontierValue, S.FrontierScore + S.UnknownRegionSizeScore);
            if (S.Purpose == EDMTacticalPurpose::SpawnControl)
            {
                bHasSpawnControl = true;
                LeastCongestedApproachUsers = FMath::Min(LeastCongestedApproachUsers, S.ApproachRouteUserCount);
                if (S.ApproachRouteUserCount < S.ApproachRouteCapacity) bHasUncongestedApproach = true;
            }
        }
        const bool bPreferFrontier = BestFrontierValue >= Settings.Strategic.MinimumFrontierValueBeforeSpawnControl;
        for (FDMTacticalCellScore& S : OutCandidates)
        {
            if ((bPreferFrontier && S.Purpose == EDMTacticalPurpose::ExploreFrontier)
                || (!bPreferFrontier && bHasSpawnControl && S.Purpose == EDMTacticalPurpose::SpawnControl))
                S.TotalScore += FMath::Max(0.f, Settings.Strategic.StrategicPurposePreferenceBonus);
            if (S.Purpose == EDMTacticalPurpose::SpawnControl
                && S.ApproachRouteUserCount >= S.ApproachRouteCapacity && !bHasUncongestedApproach
                && S.ApproachRouteUserCount == LeastCongestedApproachUsers)
                S.bApproachRouteCapacityRelaxed = true;
            S.Reason = FString::Printf(TEXT("%s/%s full-grid=1 hops=%d dist=%.0f score=%.1f cell=%d/%d cellPen=%.1f route=%d users=%d/%d routePen=%.1f relaxed=%d"),
                *StaticEnum<EDMTacticalEvaluationState>()->GetNameStringByValue(static_cast<int64>(Context.State)),
                *StaticEnum<EDMTacticalPurpose>()->GetNameStringByValue(static_cast<int64>(S.Purpose)),
                S.Path.CellHopCount, S.Path.TotalTravelDistance, S.TotalScore,
                S.StrategicCellReservationCount, S.StrategicCellCapacity, S.StrategicReservationPenalty,
                S.ApproachRouteId, S.ApproachRouteUserCount, S.ApproachRouteCapacity,
                S.ApproachRouteCongestionPenalty, S.bApproachRouteCapacityRelaxed ? 1 : 0);
        }
    }
    const int32 EvaluatedCount = OutCandidates.Num();
    OutCandidates.StableSort([](const FDMTacticalCellScore& A, const FDMTacticalCellScore& B)
    {
        return A.TotalScore > B.TotalScore;
    });
    const int32 FinalLimit = FMath::Clamp(MaximumCandidates, 1, 32);
    if (OutCandidates.Num() > FinalLimit) OutCandidates.SetNum(FinalLimit);
    int32 ActiveStrategicCells = 0;
    if (bRoam) for (const FDMTacticalCellStaticData& Cell : Cells) if (IsStrategicCellPlayable(Cell, Team)) ++ActiveStrategicCells;
    const int32 ReservableCandidates = OutCandidates.Num();
    UE_LOG(LogTemp, Verbose, TEXT("DM Tactical Grid candidates: %s fullGrid=%d totalCells=%d activeStrategic=%d inactive=%d evaluated=%d returned=%d reservable=%d region=%d."),
        *GetNameSafe(Requester), bRoam ? 1 : 0, Cells.Num(), ActiveStrategicCells,
        bRoam ? Cells.Num() - ActiveStrategicCells : 0, EvaluatedCount, OutCandidates.Num(),
        ReservableCandidates, Cells[Start].Id.Region);
    return !OutCandidates.IsEmpty();
}

bool UDMTacticalGridSubsystem::FindStrategicRegions(AActor* Requester, uint8 Team,
    const FDMTacticalEvaluationContext& Context, int32 MaximumCandidates,
    TArray<FDMTacticalStrategicRegionSelection>& OutRegions)
{
    OutRegions.Reset();
    // Tactical Grid never supplies a combat destination. Combat actions own purpose-specific EQS.
    if (Context.bInCombat || Context.State != EDMTacticalEvaluationState::DisengagedRoam) return false;
    TArray<FDMTacticalCellScore> Scores;
    if (!FindTacticalCandidates(Requester, Team, Context, 1, MaximumCandidates, Scores)) return false;
    OutRegions.Reserve(Scores.Num());
    for (const FDMTacticalCellScore& Score : Scores)
    {
        const FDMTacticalCellStaticData* Cell = FindCell(Score.CellId);
        if (!Cell) continue;
        FDMTacticalStrategicRegionSelection Region;
        Region.CellId = Score.CellId;
        Region.LayerId = Score.CellId.Layer;
        Region.NavRegionId = Score.CellId.Region;
        Region.Bounds = Cell->Bounds;
        Region.Purpose = Score.Purpose;
        Region.Score = Score.TotalScore;
        Region.SelectionReason = Score.Reason;
        Region.TravelCost = Score.Path.TotalTravelDistance;
        if (const FDMTacticalCellDynamicData* Dynamic = FindDynamic(Team, Score.CellId))
            Region.FriendlyCount = Dynamic->FriendlyActorCount;
        Region.StrategicCrowdingCount = CountCellReservations(Team, Score.CellId, Requester);
        Region.ScoreDetails = Score;
        OutRegions.Add(MoveTemp(Region));
    }
    return !OutRegions.IsEmpty();
}

bool UDMTacticalGridSubsystem::ReserveCell(uint8 Team, AActor* Actor, const FDMTacticalCellId& CellId,
    EDMTacticalPurpose Purpose, float Lifetime, const FDMTacticalCellScore* Score,
    const FVector& ReservedWorldLocation, float GoalReservationRadius)
{
    if (!IsValid(Actor) || !FindCell(CellId)) return false;
    const float Now = GetWorld()->GetTimeSeconds(); PurgeExpired(Now);
    int32 Count = 0, ExistingIndex = INDEX_NONE;
    const bool bStrategicPurpose = Purpose == EDMTacticalPurpose::ExploreFrontier
        || Purpose == EDMTacticalPurpose::SpawnControl;
    for (int32 Index = 0; Index < Reservations.Num(); ++Index)
    {
        const FDMTacticalCellReservation& R = Reservations[Index];
        if (R.Actor.Get() == Actor) { ExistingIndex = Index; continue; }
        if (R.Team == Team && R.CellId == CellId
            && (!bStrategicPurpose || R.Purpose == EDMTacticalPurpose::ExploreFrontier
                || R.Purpose == EDMTacticalPurpose::SpawnControl)) ++Count;
    }
    const int32 Capacity = (Purpose == EDMTacticalPurpose::Explore || Purpose == EDMTacticalPurpose::Watch)
            ? 1 : FMath::Max(1, Settings.Runtime.MaximumReservationsPerCell);
    if (!bStrategicPurpose && Count >= Capacity) return false;
    // Large strategic cells and approach routes are soft-crowding signals. Only an exact
    // NavMesh arrival overlap is exclusive, and only for the short lifetime of that goal.
    if (bStrategicPurpose && GoalReservationRadius > 0.f)
    {
        const float RequestedRadius = FMath::Max(1.f, GoalReservationRadius);
        for (const FDMTacticalCellReservation& R : Reservations)
        {
            if (R.Actor.Get() == Actor || R.Team != Team || !R.bHasReservedWorldLocation) continue;
            const float Separation = RequestedRadius + FMath::Max(1.f, R.GoalReservationRadius);
            if (FVector::DistSquared2D(R.ReservedWorldLocation, ReservedWorldLocation) < FMath::Square(Separation))
                return false;
        }
    }
    FDMTacticalCellReservation NewReservation;
    NewReservation.Team = Team; NewReservation.Actor = Actor; NewReservation.CellId = CellId; NewReservation.Purpose = Purpose;
    NewReservation.ExpirationTime = Now + (Lifetime > 0.f ? Lifetime : Settings.Runtime.CellReservationLifetime);
    if (Score)
    {
        NewReservation.EvaluationState = Score->EvaluationState;
        NewReservation.Score = Score->TotalScore;
        NewReservation.StateWeights = Score->StateWeights;
        NewReservation.AppliedWeights = Score->AppliedWeights;
        NewReservation.ScoreReason = Score->Reason;
        NewReservation.ApproachRouteId = Score->ApproachRouteId;
        NewReservation.RelatedEnemySpawnTeam = Score->RelatedEnemySpawnTeam;
        NewReservation.Path = Score->Path;
        NewReservation.ReservedWorldLocation = ReservedWorldLocation;
        NewReservation.bHasReservedWorldLocation = bStrategicPurpose && GoalReservationRadius > 0.f;
        NewReservation.GoalReservationRadius = FMath::Max(0.f, GoalReservationRadius);
        NewReservation.StrategicCellReservationCountAtSelection = Score->StrategicCellReservationCount;
        NewReservation.StrategicCellCapacity = Score->StrategicCellCapacity;
        NewReservation.StrategicReservationPenalty = Score->StrategicReservationPenalty;
        NewReservation.ApproachRouteUserCountAtSelection = Score->ApproachRouteUserCount;
        NewReservation.ApproachRouteCapacity = Score->ApproachRouteCapacity;
        NewReservation.ApproachRouteCongestionPenalty = Score->ApproachRouteCongestionPenalty;
        NewReservation.bApproachRouteCapacityRelaxed = Score->bApproachRouteCapacityRelaxed;
    }
    else if (ExistingIndex != INDEX_NONE && Reservations[ExistingIndex].CellId == CellId
        && Reservations[ExistingIndex].Purpose == Purpose)
    {
        NewReservation.EvaluationState = Reservations[ExistingIndex].EvaluationState;
        NewReservation.Score = Reservations[ExistingIndex].Score;
        NewReservation.StateWeights = Reservations[ExistingIndex].StateWeights;
        NewReservation.AppliedWeights = Reservations[ExistingIndex].AppliedWeights;
        NewReservation.ScoreReason = Reservations[ExistingIndex].ScoreReason;
        NewReservation.ApproachRouteId = Reservations[ExistingIndex].ApproachRouteId;
        NewReservation.RelatedEnemySpawnTeam = Reservations[ExistingIndex].RelatedEnemySpawnTeam;
        NewReservation.Path = Reservations[ExistingIndex].Path;
        const bool bRefreshesExactGoal = bStrategicPurpose && GoalReservationRadius > 0.f;
        NewReservation.ReservedWorldLocation = bRefreshesExactGoal
            ? ReservedWorldLocation : Reservations[ExistingIndex].ReservedWorldLocation;
        NewReservation.bHasReservedWorldLocation = bRefreshesExactGoal
            || Reservations[ExistingIndex].bHasReservedWorldLocation;
        NewReservation.GoalReservationRadius = bRefreshesExactGoal
            ? GoalReservationRadius : Reservations[ExistingIndex].GoalReservationRadius;
        NewReservation.StrategicCellReservationCountAtSelection = Reservations[ExistingIndex].StrategicCellReservationCountAtSelection;
        NewReservation.StrategicCellCapacity = Reservations[ExistingIndex].StrategicCellCapacity;
        NewReservation.StrategicReservationPenalty = Reservations[ExistingIndex].StrategicReservationPenalty;
        NewReservation.ApproachRouteUserCountAtSelection = Reservations[ExistingIndex].ApproachRouteUserCountAtSelection;
        NewReservation.ApproachRouteCapacity = Reservations[ExistingIndex].ApproachRouteCapacity;
        NewReservation.ApproachRouteCongestionPenalty = Reservations[ExistingIndex].ApproachRouteCongestionPenalty;
        NewReservation.bApproachRouteCapacityRelaxed = Reservations[ExistingIndex].bApproachRouteCapacityRelaxed;
    }
    if (ExistingIndex != INDEX_NONE) Reservations[ExistingIndex] = MoveTemp(NewReservation);
    else Reservations.Add(MoveTemp(NewReservation));
    ++TeamRuntime.FindOrAdd(Team).Revision;
    return true;
}

void UDMTacticalGridSubsystem::GetRegisteredFriendlyActors(uint8 Team, AActor* IgnoreActor,
    TArray<AActor*>& OutActors) const
{
    OutActors.Reset();
    for (const auto& Pair : RegisteredActors)
    {
        AActor* Actor = Pair.Value.Actor.Get();
        if (Actor && Actor != IgnoreActor && Pair.Value.Team == Team) OutActors.Add(Actor);
    }
}

void UDMTacticalGridSubsystem::GetReservedLocationsForCell(uint8 Team, const FDMTacticalCellId& CellId,
    AActor* IgnoreActor, TArray<FVector>& OutLocations) const
{
    OutLocations.Reset();
    for (const FDMTacticalCellReservation& Reservation : Reservations)
        if (Reservation.Team == Team && Reservation.CellId == CellId && Reservation.Actor.Get() != IgnoreActor
            && Reservation.bHasReservedWorldLocation)
            OutLocations.Add(Reservation.ReservedWorldLocation);
}

void UDMTacticalGridSubsystem::ReleaseReservations(AActor* Actor)
{
    TSet<uint8> ChangedTeams;
    Reservations.RemoveAll([Actor, &ChangedTeams](const FDMTacticalCellReservation& R)
    {
        if (R.Actor.Get() != Actor) return false;
        ChangedTeams.Add(R.Team); return true;
    });
    for (const uint8 Team : ChangedTeams) ++TeamRuntime.FindOrAdd(Team).Revision;
}

void UDMTacticalGridSubsystem::RecordFailure(AActor* Actor, uint8 Team, const FDMTacticalCellId& CellId, FName Action,
    EDMTacticalPurpose Purpose, EDMTacticalFailureReason Reason, float Cooldown)
{
    if (!CellId.IsValid()) return;
    FFailureKey Key; Key.Actor = Actor; Key.Team = Team; Key.CellId = CellId; Key.Action = Action; Key.Purpose = Purpose;
    FFailureValue& Value = Failures.FindOrAdd(Key); Value.Reason = Reason;
    Value.Expiration = GetWorld()->GetTimeSeconds() + (Cooldown > 0.f ? Cooldown : Settings.Runtime.FailureCooldown);
    ++TeamRuntime.FindOrAdd(Team).Revision;
}

bool UDMTacticalGridSubsystem::IsFailureCoolingDown(AActor* Actor, uint8 Team, const FDMTacticalCellId& CellId,
    FName Action, EDMTacticalPurpose Purpose) const
{
    FFailureKey Key; Key.Actor = Actor; Key.Team = Team; Key.CellId = CellId; Key.Action = Action; Key.Purpose = Purpose;
    const FFailureValue* Value = Failures.Find(Key);
    return Value && Value->Expiration > GetWorld()->GetTimeSeconds();
}

float UDMTacticalGridSubsystem::GetFailurePenalty(AActor* Actor, uint8 Team, const FDMTacticalCellId& CellId, EDMTacticalPurpose Purpose) const
{
    const float Now = GetWorld()->GetTimeSeconds();
    for (const auto& Pair : Failures)
        if (Pair.Key.Actor.Get() == Actor && Pair.Key.Team == Team && Pair.Key.CellId == CellId && Pair.Key.Purpose == Purpose && Pair.Value.Expiration > Now) return 1000.f;
    return 0.f;
}

void UDMTacticalGridSubsystem::InvalidateFailuresForTeam(uint8 Team)
{
    for (auto It = Failures.CreateIterator(); It; ++It) if (It.Key().Team == Team) It.RemoveCurrent();
    ++TeamRuntime.FindOrAdd(Team).Revision;
}

#if !UE_BUILD_SHIPPING
void UDMTacticalGridSubsystem::GetDebugSnapshot(uint8 Team, TArray<FDMTacticalCellStaticData>& OutStatic,
    TArray<FDMTacticalCellDynamicData>& OutDynamic, TArray<FDMTacticalCellReservation>& OutReservations) const
{
    OutStatic = Cells; OutReservations = Reservations; OutDynamic.Reset(); OutDynamic.Reserve(Cells.Num());
    for (const FDMTacticalCellStaticData& Cell : Cells)
    {
        if (const FDMTacticalCellDynamicData* Dynamic = FindDynamic(Team, Cell.Id)) OutDynamic.Add(*Dynamic);
        else OutDynamic.AddDefaulted();
    }
}

void UDMTacticalGridSubsystem::DrawDebugGrid() const
{
    const int32 Level = CVarDMTacticalGrid.GetValueOnGameThread(); if (Level <= 0 || !GetWorld()) return;
    const uint8 Team = static_cast<uint8>(FMath::Clamp(CVarDMTacticalGridTeam.GetValueOnGameThread(), 0, 254));
    const int32 ActorFilter = CVarDMTacticalGridActor.GetValueOnGameThread();
    for (const FDMTacticalCellStaticData& Cell : Cells)
    {
        const FVector Center = Cell.Bounds.GetCenter(); FVector Extent = Cell.Bounds.GetExtent(); Extent.Z = 8.f;
        FColor Color = CellColor(Cell.CellType);
        if (!Cell.bNavMeshProjectionSucceeded) Color = FColor::Red;
        else if (!Cell.bHasValidGridConnection) Color = FColor::Purple;
        else if (Cell.bReachableFromTeamARepresentativeSpawn && !Cell.bReachableFromTeamBRepresentativeSpawn) Color = FColor::Blue;
        else if (!Cell.bReachableFromTeamARepresentativeSpawn && Cell.bReachableFromTeamBRepresentativeSpawn) Color = FColor::Orange;
        else if (!Cell.bPlayableStrategicCell) Color = FColor::Magenta;
        const FDMTacticalCellDynamicData* D = FindDynamic(Team, Cell.Id);
        if (Level >= 3 && D)
        {
            if (D->bContested) Color = FColor::Red;
            else if (D->bFrontier) Color = FColor::Yellow;
            else if (D->bUnknown) Color = FColor(70, 70, 70);
            else if (D->bRecentlyObserved) Color = FColor::Green;
            else if (D->bCovered) Color = FColor(70, 140, 255);
            else if (D->bInferredCovered) Color = FColor(120, 100, 220);
            else if (D->bStale) Color = FColor::Orange;
        }
        DrawDebugBox(GetWorld(), FVector(Center.X, Center.Y, Cell.RepresentativeLocation.Z + 8.f), Extent, Color, false, -.01f, 0, 2.f);
        if (Cell.CellType != EDMTacticalCellType::Blocked)
        {
            DrawDebugPoint(GetWorld(), Cell.RepresentativeLocation + FVector(0,0,15), 8.f, FColor::White, false, -.01f);
            for (const FDMTacticalConnection& Edge : Cell.Connections)
                if (const FDMTacticalCellStaticData* Other = FindCell(Edge.Neighbor))
                    DrawDebugLine(GetWorld(), Cell.RepresentativeLocation, Other->RepresentativeLocation,
                        Edge.Type == EDMTacticalConnectionType::Narrow ? FColor::Orange : FColor(60,120,255), false, -.01f, 0, 1.f);
        }
        if (Level >= 2)
            DrawDebugString(GetWorld(), Cell.RepresentativeLocation + FVector(0,0,35),
                FString::Printf(TEXT("%d,%d L%d R%d %s %.2f open %.2f Strategic=%d A=%d B=%d"), Cell.Id.X, Cell.Id.Y, Cell.Id.Layer, Cell.Id.Region,
                    *StaticEnum<EDMTacticalCellType>()->GetNameStringByValue(static_cast<int64>(Cell.CellType)), Cell.WalkableRatio, Cell.Openness,
                    Cell.bPlayableStrategicCell ? 1 : 0, Cell.bReachableFromTeamARepresentativeSpawn ? 1 : 0,
                    Cell.bReachableFromTeamBRepresentativeSpawn ? 1 : 0),
                nullptr, Color, -.01f, false, 1.f);
        if (Level >= 3)
            if (D)
                DrawDebugString(GetWorld(), Cell.RepresentativeLocation + FVector(0,0,70),
                    FString::Printf(TEXT("T%d %s%s%s%s%s%s%s F%d C%d K%d V%d I%.1f O%.2f U%d/%d R%d"), Team,
                        D->bUnknown ? TEXT("Unknown ") : TEXT(""), D->bFrontier ? TEXT("Frontier ") : TEXT(""),
                        D->bRecentlyObserved ? TEXT("Recent ") : (D->bKnown ? TEXT("Known ") : TEXT("")),
                        D->bStale ? TEXT("Stale ") : TEXT(""), D->bCovered ? TEXT("Covered ") : TEXT(""),
                        D->bInferredCovered ? TEXT("Inferred ") : TEXT(""), D->bContested ? TEXT("Contested ") : TEXT(""),
                        D->FriendlyActorCount,
                        D->FriendlyCombatantCount, D->KnownThreatCount, D->VisibleThreatCount, D->CombatIntensity,
                        D->ObservationConfidence, D->AdjacentUnknownCellCount, D->ConnectedUnknownRegionSize,
                        D->FriendlyReservedGoalCount), nullptr, FColor::White, -.01f, false, 1.f);
        if (Level >= 5)
        {
            FString SpawnText;
            if (const FDMTacticalSpawnFieldData* Field = Cell.SpawnFieldsByTeam.Find(Team))
                SpawnText += FString::Printf(TEXT("Spawn T%d H%d D%.0f Front%.2f%s "), Team, Field->Hops,
                    Field->PathDistance, Field->FrontProgress, Field->bInsideSpawnExclusionArea ? TEXT(" EXCL") : TEXT(""));
            const FDMTacticalSpawnControlData* BestControl = nullptr;
            for (const auto& Pair : Cell.SpawnControlByEnemyTeam)
                if (Pair.Key != Team && (!BestControl || Pair.Value.Score > BestControl->Score)) BestControl = &Pair.Value;
            if (BestControl)
                SpawnText += FString::Printf(TEXT("Control E%d %.1f H%d Route%d%s"), BestControl->RelatedEnemySpawnTeam,
                    BestControl->Score, BestControl->EnemySpawnHops, BestControl->ApproachRouteId,
                    BestControl->bCandidate ? TEXT(" CAND") : TEXT(""));
            if (!SpawnText.IsEmpty()) DrawDebugString(GetWorld(), Cell.RepresentativeLocation + FVector(0,0,95),
                SpawnText, nullptr, FColor(120, 220, 255), -.01f, false, .8f);
        }
    }
    if (Level >= 3)
    {
        for (const FDMTacticalCellReservation& R : Reservations)
        {
            if (ActorFilter >= 0 && (!R.Actor.IsValid() || R.Actor->GetUniqueID() != ActorFilter)) continue;
            if (const FDMTacticalCellStaticData* Cell = FindCell(R.CellId))
            {
                DrawDebugSphere(GetWorld(), Cell->RepresentativeLocation + FVector(0,0,30), 35.f, 8, FColor::Purple, false, -.01f, 0, 2.f);
                DrawDebugString(GetWorld(), Cell->RepresentativeLocation + FVector(0,0,95),
                    FString::Printf(TEXT("%s: %s/%s %.1f Cell %d/%d pen %.1f"), *GetNameSafe(R.Actor.Get()),
                        *StaticEnum<EDMTacticalEvaluationState>()->GetNameStringByValue(static_cast<int64>(R.EvaluationState)),
                        *StaticEnum<EDMTacticalPurpose>()->GetNameStringByValue(static_cast<int64>(R.Purpose)), R.Score,
                        R.StrategicCellReservationCountAtSelection + 1, R.StrategicCellCapacity, R.StrategicReservationPenalty),
                    nullptr, FColor::Purple, -.01f, false, 1.f);
                if (Level >= 4 && !R.ScoreReason.IsEmpty())
                {
                    DrawDebugString(GetWorld(), Cell->RepresentativeLocation + FVector(0,0,120), R.ScoreReason,
                        nullptr, FColor(210,160,255), -.01f, false, .8f);
                    const FDMTacticalScoreWeights& B = R.StateWeights;
                    const FDMTacticalScoreWeights& A = R.AppliedWeights;
                    DrawDebugString(GetWorld(), Cell->RepresentativeLocation + FVector(0,0,145),
                        FString::Printf(TEXT("Weights F %.2f>%.2f C %.2f>%.2f A %.2f>%.2f E %.2f>%.2f Cov %.2f>%.2f Adv %.2f>%.2f"),
                            B.FriendlyFormationWeight, A.FriendlyFormationWeight,
                            B.CombatOpportunityWeight, A.CombatOpportunityWeight,
                            B.AmbushWeight, A.AmbushWeight,
                            B.ExplorationWeight, A.ExplorationWeight,
                            B.CoverageWeight, A.CoverageWeight,
                            B.SupportedAdvanceWeight, A.SupportedAdvanceWeight),
                        nullptr, FColor(180,210,255), -.01f, false, .75f);
                }
                if (Level >= 5 && R.Path.bReachable)
                {
                    for (int32 Index = 1; Index < R.Path.PathCells.Num(); ++Index)
                    {
                        const FDMTacticalCellStaticData* From = FindCell(R.Path.PathCells[Index - 1]);
                        const FDMTacticalCellStaticData* To = FindCell(R.Path.PathCells[Index]);
                        if (From && To) DrawDebugDirectionalArrow(GetWorld(), From->RepresentativeLocation + FVector(0,0,25),
                            To->RepresentativeLocation + FVector(0,0,25), 80.f, FColor::Purple, false, -.01f, 0, 3.f);
                    }
                    DrawDebugString(GetWorld(), Cell->RepresentativeLocation + FVector(0,0,170),
                        FString::Printf(TEXT("Path H%d D%.0f T%.1fs N%d F%d X%d Risk %.2f/%.2f"),
                            R.Path.CellHopCount, R.Path.TotalTravelDistance, R.Path.EstimatedTravelSeconds,
                            R.Path.NarrowCellCount, R.Path.FrontierCellCount, R.Path.ContestedCellCount,
                            R.Path.AverageRisk, R.Path.MaximumRisk), nullptr, FColor::Purple, -.01f, false, .75f);
                }
                if (Level >= 5 && R.ApproachRouteId != INDEX_NONE)
                    DrawDebugString(GetWorld(), Cell->RepresentativeLocation + FVector(0,0,195),
                        FString::Printf(TEXT("Route %d users %d/%d pen %.1f relaxed=%d"), R.ApproachRouteId,
                            R.ApproachRouteUserCountAtSelection + 1, R.ApproachRouteCapacity,
                            R.ApproachRouteCongestionPenalty, R.bApproachRouteCapacityRelaxed ? 1 : 0),
                        nullptr, FColor(255,190,120), -.01f, false, .75f);
            }
        }
        for (const auto& Pair : Failures)
            if (Pair.Value.Expiration > GetWorld()->GetTimeSeconds())
                if (const FDMTacticalCellStaticData* Cell = FindCell(Pair.Key.CellId))
                    DrawDebugSphere(GetWorld(), Cell->RepresentativeLocation + FVector(0,0,55), 48.f, 8, FColor::Red, false, -.01f, 0, 2.f);
    }
}
#endif

