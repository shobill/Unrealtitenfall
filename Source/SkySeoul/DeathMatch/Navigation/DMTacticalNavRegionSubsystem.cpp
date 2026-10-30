#include "DMTacticalNavRegionSubsystem.h"

#include "DMTacticalEventProfile.h"
#include "DMTacticalNavRegionVolume.h"
#include "DeathMatch/PlayerStart/PlayerStartByTeam.h"
#include "AI/NavigationSystemBase.h"
#include "NavigationSystem.h"
#include "NavMesh/RecastNavMesh.h"
#include "GameFramework/PlayerStart.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "DrawDebugHelpers.h"
#include "HAL/IConsoleManager.h"

namespace
{
TAutoConsoleVariable<int32> CVarDMTacticalNavRegions(
	TEXT("dm.TacticalNavRegions"), 0,
	TEXT("0 off, 1 selected/summary, 2 graph, 3 scores, 4 nav points and failure caches."), ECVF_Cheat);
TAutoConsoleVariable<int32> CVarDMTacticalNavRegionTeam(
	TEXT("dm.TacticalNavRegionTeam"), 0, TEXT("Team shown by TacticalNavRegions debug."), ECVF_Cheat);

float PolygonArea2D(const TArray<FVector>& Vertices)
{
	if (Vertices.Num() < 3) return 0.f;
	double TwiceArea = 0.0;
	for (int32 Index = 0; Index < Vertices.Num(); ++Index)
	{
		const FVector& A = Vertices[Index];
		const FVector& B = Vertices[(Index + 1) % Vertices.Num()];
		TwiceArea += A.X * B.Y - B.X * A.Y;
	}
	return static_cast<float>(FMath::Abs(TwiceArea) * .5);
}

uint64 ConnectionKey(int32 A, int32 B)
{
	const uint32 Low = static_cast<uint32>(FMath::Min(A, B));
	const uint32 High = static_cast<uint32>(FMath::Max(A, B));
	return (static_cast<uint64>(Low) << 32) | High;
}
}

void UDMTacticalNavRegionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	BuildState = EDMTacticalNavRegionBuildState::WaitingForNavigation;
	if (UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
		Nav->OnNavigationGenerationFinishedDelegate.AddDynamic(this, &UDMTacticalNavRegionSubsystem::HandleNavigationGenerationFinished);
}

void UDMTacticalNavRegionSubsystem::Deinitialize()
{
	if (UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
		Nav->OnNavigationGenerationFinishedDelegate.RemoveDynamic(this, &UDMTacticalNavRegionSubsystem::HandleNavigationGenerationFinished);
	Regions.Reset(); Polygons.Reset(); PolyIndexByRef.Reset(); RegionIndexById.Reset();
	RuntimeByTeam.Reset(); ActiveEvents.Reset(); Presence.Reset(); ExplorationByTeam.Reset();
	NavPointFailures.Reset(); ConnectionFailures.Reset();
	Super::Deinitialize();
}

TStatId UDMTacticalNavRegionSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UDMTacticalNavRegionSubsystem, STATGROUP_Tickables);
}

void UDMTacticalNavRegionSubsystem::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!World) return;
	const float Now = World->GetTimeSeconds();
	if (BuildState == EDMTacticalNavRegionBuildState::WaitingForNavigation && Now >= NextNavigationRetryTime)
	{
		NextNavigationRetryTime = Now + .5f;
		UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
		ARecastNavMesh* Recast = Nav ? Cast<ARecastNavMesh>(Nav->GetDefaultNavDataInstance(FNavigationSystem::DontCreate)) : nullptr;
		if (Recast && GatherBuildConfiguration())
		{
			BuildState = EDMTacticalNavRegionBuildState::GatheringPolygons;
			if (!GatherPolygons(*Recast)) BuildState = EDMTacticalNavRegionBuildState::Failed;
			else
			{
				BuildState = EDMTacticalNavRegionBuildState::BuildingRegions;
				PartitionPolygons();
				MergeSmallRegions();
				BuildRegionData(*Recast);
				BuildState = EDMTacticalNavRegionBuildState::BuildingConnections;
				BuildRegionConnections(*Recast);
				BuildState = EDMTacticalNavRegionBuildState::MarkingReachability;
				BuildSpawnReachability();
				BuildState = Regions.IsEmpty() ? EDMTacticalNavRegionBuildState::Failed : EDMTacticalNavRegionBuildState::Ready;
				++NavigationRevision;
				for (FDMTacticalNavRegionStaticData& Region : Regions) Region.NavigationRevision = NavigationRevision;
				NextEventUpdateTime = 0.f;
			}
		}
	}
	if (BuildState == EDMTacticalNavRegionBuildState::Ready && Now >= NextEventUpdateTime)
	{
		NextEventUpdateTime = Now + FMath::Max(.05f, Settings.EventUpdateInterval);
		PurgeTransientData(Now);
		RebuildRuntimeScores(Now);
	}
#if !UE_BUILD_SHIPPING
	if (CVarDMTacticalNavRegions.GetValueOnGameThread() > 0) DrawDebugRegions();
#endif
}

void UDMTacticalNavRegionSubsystem::RequestRebuild()
{
	Regions.Reset(); Polygons.Reset(); PolyIndexByRef.Reset(); RegionIndexById.Reset(); RuntimeByTeam.Reset();
	BuildState = EDMTacticalNavRegionBuildState::WaitingForNavigation;
	NextNavigationRetryTime = 0.f;
}

void UDMTacticalNavRegionSubsystem::HandleNavigationGenerationFinished(ANavigationData* NavData)
{
	if (NavData && NavData == Cast<ANavigationData>(FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld())
		? FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld())->GetDefaultNavDataInstance(FNavigationSystem::DontCreate) : nullptr))
		RequestRebuild();
}

bool UDMTacticalNavRegionSubsystem::GatherBuildConfiguration()
{
	AnalysisBounds = FBox(ForceInit);
	EventProfiles.Reset();
	for (TActorIterator<ADMTacticalNavRegionVolume> It(GetWorld()); It; ++It)
	{
		if (!It->bEnabled) continue;
		Settings = It->RegionSettings;
		AnalysisBounds = It->GetComponentsBoundingBox(true);
		for (const auto& Pair : It->EventProfiles) EventProfiles.Add(Pair.Key, Pair.Value);
		break;
	}
	if (!AnalysisBounds.IsValid)
	{
		UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
		const ARecastNavMesh* Recast = Nav ? Cast<ARecastNavMesh>(Nav->GetDefaultNavDataInstance(FNavigationSystem::DontCreate)) : nullptr;
		if (Recast) AnalysisBounds = Recast->GetNavMeshBounds();
	}
	return AnalysisBounds.IsValid != 0;
}

bool UDMTacticalNavRegionSubsystem::GatherPolygons(ARecastNavMesh& Recast)
{
	Polygons.Reset(); PolyIndexByRef.Reset();
	TArray<FNavTileRef> TileRefs;
	Recast.GetAllNavMeshTiles(TileRefs);
	for (const FNavTileRef TileRef : TileRefs)
	{
		if (!Recast.GetNavMeshTileBounds(TileRef).Intersect(AnalysisBounds)) continue;
		TArray<FNavPoly> TilePolys;
		if (!Recast.GetPolysInTile(TileRef, TilePolys)) continue;
		for (const FNavPoly& Poly : TilePolys)
		{
			if (Poly.Ref == INVALID_NAVNODEREF || !AnalysisBounds.IsInsideOrOn(Poly.Center)) continue;
			FPolyRecord Record;
			Record.Ref = static_cast<uint64>(Poly.Ref);
			Record.Center = Poly.Center;
			if (!Recast.GetPolyVerts(Poly.Ref, Record.Vertices) || Record.Vertices.Num() < 3) continue;
			Record.Area = PolygonArea2D(Record.Vertices);
			for (const FVector& Vertex : Record.Vertices) Record.Bounds += Vertex;
			PolyIndexByRef.Add(Record.Ref, Polygons.Add(MoveTemp(Record)));
		}
	}
	for (FPolyRecord& Poly : Polygons)
	{
		Recast.GetPolyNeighbors(static_cast<NavNodeRef>(Poly.Ref), Poly.Portals);
		for (const FNavigationPortalEdge& Portal : Poly.Portals)
			if (PolyIndexByRef.Contains(static_cast<uint64>(Portal.ToRef))) Poly.Neighbors.Add(static_cast<uint64>(Portal.ToRef));
	}
	return !Polygons.IsEmpty();
}

void UDMTacticalNavRegionSubsystem::PartitionPolygons()
{
	for (FPolyRecord& Poly : Polygons) Poly.RegionId = INDEX_NONE;
	TArray<int32> Order;
	for (int32 Index = 0; Index < Polygons.Num(); ++Index) Order.Add(Index);
	Order.Sort([this](int32 A, int32 B) { return Polygons[A].Ref < Polygons[B].Ref; });
	int32 NextRegionId = 0;
	for (const int32 SeedIndex : Order)
	{
		if (Polygons[SeedIndex].RegionId != INDEX_NONE) continue;
		const FVector SeedCenter = Polygons[SeedIndex].Center;
		float RegionArea = 0.f;
		TArray<int32> Queue{SeedIndex};
		Polygons[SeedIndex].RegionId = NextRegionId;
		for (int32 Cursor = 0; Cursor < Queue.Num(); ++Cursor)
		{
			FPolyRecord& Current = Polygons[Queue[Cursor]];
			RegionArea += Current.Area;
			for (const FNavigationPortalEdge& Portal : Current.Portals)
			{
				const int32* NeighborIndex = PolyIndexByRef.Find(static_cast<uint64>(Portal.ToRef));
				if (!NeighborIndex || Polygons[*NeighborIndex].RegionId != INDEX_NONE) continue;
				const FPolyRecord& Neighbor = Polygons[*NeighborIndex];
				const float PortalWidth = Portal.GetLength();
				if (PortalWidth < Settings.NarrowPortalWidth) continue;
				if (FMath::Abs(Neighbor.Center.Z - Current.Center.Z) > Settings.RegionHeightBoundary) continue;
				if (RegionArea + Neighbor.Area > Settings.MaximumRegionArea) continue;
				if (FVector::Dist2D(SeedCenter, Neighbor.Center) > Settings.TargetRegionSize) continue;
				Polygons[*NeighborIndex].RegionId = NextRegionId;
				Queue.Add(*NeighborIndex);
			}
		}
		++NextRegionId;
	}
}

void UDMTacticalNavRegionSubsystem::MergeSmallRegions()
{
	TMap<int32, float> Areas;
	for (const FPolyRecord& Poly : Polygons) Areas.FindOrAdd(Poly.RegionId) += Poly.Area;
	for (const auto& Pair : Areas)
	{
		if (Pair.Value >= Settings.MinimumRegionArea) continue;
		int32 BestNeighbor = INDEX_NONE;
		float BestWidth = 0.f;
		for (const FPolyRecord& Poly : Polygons)
		{
			if (Poly.RegionId != Pair.Key) continue;
			for (const FNavigationPortalEdge& Portal : Poly.Portals)
			{
				const int32* OtherIndex = PolyIndexByRef.Find(static_cast<uint64>(Portal.ToRef));
				if (!OtherIndex) continue;
				const int32 OtherRegion = Polygons[*OtherIndex].RegionId;
				if (OtherRegion == Pair.Key || Pair.Value + Areas.FindRef(OtherRegion) > Settings.MaximumRegionArea) continue;
				if (Portal.GetLength() > BestWidth) { BestWidth = Portal.GetLength(); BestNeighbor = OtherRegion; }
			}
		}
		if (BestNeighbor != INDEX_NONE)
			for (FPolyRecord& Poly : Polygons) if (Poly.RegionId == Pair.Key) Poly.RegionId = BestNeighbor;
	}
	TMap<int32, int32> CompactIds;
	for (FPolyRecord& Poly : Polygons)
	{
		int32* Compact = CompactIds.Find(Poly.RegionId);
		if (!Compact) { const int32 NewId = CompactIds.Num(); CompactIds.Add(Poly.RegionId, NewId); Compact = CompactIds.Find(Poly.RegionId); }
		Poly.RegionId = *Compact;
	}
}

void UDMTacticalNavRegionSubsystem::BuildRegionData(ARecastNavMesh& Recast)
{
	Regions.Reset(); RegionIndexById.Reset();
	int32 RegionCount = 0;
	for (const FPolyRecord& Poly : Polygons) RegionCount = FMath::Max(RegionCount, Poly.RegionId + 1);
	Regions.SetNum(RegionCount);
	for (int32 Index = 0; Index < Regions.Num(); ++Index) { Regions[Index].RegionId = Index; RegionIndexById.Add(Index, Index); }
	for (const FPolyRecord& Poly : Polygons)
	{
		FDMTacticalNavRegionStaticData& Region = Regions[Poly.RegionId];
		Region.NavPolyRefs.Add(Poly.Ref);
		Region.Area += Poly.Area;
		Region.Bounds += Poly.Bounds;
	}
	for (FDMTacticalNavRegionStaticData& Region : Regions)
	{
		const FVector BoundsCenter = Region.Bounds.GetCenter();
		float BestDistance = BIG_NUMBER;
		for (const uint64 Ref : Region.NavPolyRefs)
		{
			const FPolyRecord& Poly = Polygons[PolyIndexByRef.FindChecked(Ref)];
			const float Distance = FVector::DistSquared(Poly.Center, BoundsCenter);
			if (Distance < BestDistance) { BestDistance = Distance; Region.RepresentativeLocation = Poly.Center; }
		}
		Region.ApproximateDiameter = Region.Bounds.GetSize().Size2D();
		GenerateRegionSamples(Recast, Region);
		float OpennessSum = 0.f;
		for (const FDMTacticalNavPoint& Point : Region.ValidNavPoints) OpennessSum += Point.Openness;
		Region.AverageOpenness = Region.ValidNavPoints.IsEmpty() ? 0.f : OpennessSum / Region.ValidNavPoints.Num();
		Region.bNarrow = Region.AverageOpenness < .35f;
	}
}

void UDMTacticalNavRegionSubsystem::GenerateRegionSamples(ARecastNavMesh& Recast, FDMTacticalNavRegionStaticData& Region)
{
	TArray<FDMTacticalNavPoint> Candidates;
	for (const uint64 Ref : Region.NavPolyRefs)
	{
		const FPolyRecord& Poly = Polygons[PolyIndexByRef.FindChecked(Ref)];
		if (!HasStaticCapsuleClearance(Poly.Center)) continue;
		FDMTacticalNavPoint Point;
		Point.Location = Poly.Center; Point.NavPolyRef = Ref; Point.RegionId = Region.RegionId;
		Point.WallClearance = MeasureWallClearance(Point.Location);
		Point.Openness = FMath::Clamp(Point.WallClearance / FMath::Max(100.f, Settings.TargetRegionSize * .35f), 0.f, 1.f);
		Candidates.Add(Point);
	}
	Candidates.StableSort([](const FDMTacticalNavPoint& A, const FDMTacticalNavPoint& B) { return A.WallClearance > B.WallClearance; });
	const int32 Desired = FMath::Clamp(Settings.RegionSamplePointCount, 2, 32);
	const float MinimumSpacing = FMath::Max(80.f, Settings.TargetRegionSize / FMath::Sqrt(static_cast<float>(Desired)) * .35f);
	for (const FDMTacticalNavPoint& Candidate : Candidates)
	{
		if (Region.ValidNavPoints.ContainsByPredicate([&](const FDMTacticalNavPoint& Existing)
			{ return FVector::DistSquared2D(Existing.Location, Candidate.Location) < FMath::Square(MinimumSpacing); })) continue;
		Region.ValidNavPoints.Add(Candidate);
		if (Region.ValidNavPoints.Num() >= Desired) break;
	}
	if (Region.ValidNavPoints.IsEmpty() && !Candidates.IsEmpty()) Region.ValidNavPoints.Add(Candidates[0]);
}

void UDMTacticalNavRegionSubsystem::BuildRegionConnections(ARecastNavMesh& Recast)
{
	struct FConnectionAggregate { int32 A=INDEX_NONE, B=INDEX_NONE; FNavigationPortalEdge Portal; float Width=0.f; };
	TMap<uint64, FConnectionAggregate> Aggregates;
	for (const FPolyRecord& Poly : Polygons)
	{
		for (const FNavigationPortalEdge& Portal : Poly.Portals)
		{
			const int32* OtherIndex = PolyIndexByRef.Find(static_cast<uint64>(Portal.ToRef));
			if (!OtherIndex) continue;
			const int32 OtherRegion = Polygons[*OtherIndex].RegionId;
			if (OtherRegion == Poly.RegionId) continue;
			const uint64 Key = ConnectionKey(Poly.RegionId, OtherRegion);
			FConnectionAggregate& Aggregate = Aggregates.FindOrAdd(Key);
			if (Aggregate.A == INDEX_NONE) { Aggregate.A = FMath::Min(Poly.RegionId, OtherRegion); Aggregate.B = FMath::Max(Poly.RegionId, OtherRegion); }
			if (Portal.GetLength() > Aggregate.Width) { Aggregate.Width = Portal.GetLength(); Aggregate.Portal = Portal; }
		}
	}
	for (const auto& Pair : Aggregates)
	{
		const FConnectionAggregate& Aggregate = Pair.Value;
		if (!Regions.IsValidIndex(Aggregate.A) || !Regions.IsValidIndex(Aggregate.B)) continue;
		const FVector PortalPoint = Aggregate.Portal.GetMiddlePoint();
		if (!HasStaticCapsuleClearance(PortalPoint)) continue;
		const float Cost = FVector::Distance(Regions[Aggregate.A].RepresentativeLocation, Regions[Aggregate.B].RepresentativeLocation);
		auto AddConnection = [&](int32 From, int32 To)
		{
			FDMTacticalNavRegionConnection Edge;
			Edge.NeighborRegionId = To; Edge.TravelCost = FMath::Max(1.f, Cost);
			const FVector Direction = (Regions[To].RepresentativeLocation - Regions[From].RepresentativeLocation).GetSafeNormal2D();
			Edge.EntryPoint = PortalPoint - Direction * 10.f; Edge.ExitPoint = PortalPoint + Direction * 10.f;
			Edge.PortalWidth = Aggregate.Width;
			Edge.MinimumClearance = FMath::Min(Aggregate.Width * .5f, MeasureWallClearance(PortalPoint));
			Edge.ConnectionType = Aggregate.Width < Settings.NarrowPortalWidth ? EDMTacticalNavConnectionType::Narrow
				: (FMath::Abs(Regions[From].RepresentativeLocation.Z - Regions[To].RepresentativeLocation.Z) > 60.f
					? EDMTacticalNavConnectionType::Step : EDMTacticalNavConnectionType::Open);
			Regions[From].Neighbors.Add(Edge);
		};
		AddConnection(Aggregate.A, Aggregate.B); AddConnection(Aggregate.B, Aggregate.A);
	}
	for (FDMTacticalNavRegionStaticData& Region : Regions)
	{
		Region.bDeadEnd = Region.Neighbors.Num() == 1;
		Region.bIsolated = Region.Neighbors.IsEmpty();
	}
}

void UDMTacticalNavRegionSubsystem::BuildSpawnReachability()
{
	TMap<uint8, TArray<int32>> StartsByTeam;
	TArray<int32> NeutralStarts;
	TSet<uint8> ExplicitTeams;
	for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
	{
		const int32 RegionId = GetRegionAtLocation(It->GetActorLocation());
		if (RegionId == INDEX_NONE) continue;
		if (const APlayerStartByTeam* TeamStart = Cast<APlayerStartByTeam>(*It); TeamStart && TeamStart->TeamId != 255)
		{
			StartsByTeam.FindOrAdd(TeamStart->TeamId).AddUnique(RegionId); ExplicitTeams.Add(TeamStart->TeamId);
		}
		else NeutralStarts.AddUnique(RegionId);
	}
	if (ExplicitTeams.IsEmpty()) { ExplicitTeams.Add(0); ExplicitTeams.Add(1); }
	for (const uint8 Team : ExplicitTeams) StartsByTeam.FindOrAdd(Team).Append(NeutralStarts);
	for (const auto& TeamStarts : StartsByTeam)
	{
		TArray<float> Distances; Distances.Init(BIG_NUMBER, Regions.Num());
		TArray<int32> Open;
		for (const int32 Start : TeamStarts.Value) if (Regions.IsValidIndex(Start)) { Distances[Start] = 0.f; Open.AddUnique(Start); }
		while (!Open.IsEmpty())
		{
			int32 BestOpen = 0;
			for (int32 Index = 1; Index < Open.Num(); ++Index) if (Distances[Open[Index]] < Distances[Open[BestOpen]]) BestOpen = Index;
			const int32 Current = Open[BestOpen]; Open.RemoveAtSwap(BestOpen);
			for (const FDMTacticalNavRegionConnection& Edge : Regions[Current].Neighbors)
			{
				if (!Edge.bPassable || !Regions.IsValidIndex(Edge.NeighborRegionId)) continue;
				const float NewDistance = Distances[Current] + Edge.TravelCost;
				if (NewDistance >= Distances[Edge.NeighborRegionId]) continue;
				Distances[Edge.NeighborRegionId] = NewDistance; Open.AddUnique(Edge.NeighborRegionId);
			}
		}
		for (int32 Index = 0; Index < Regions.Num(); ++Index)
			if (Distances[Index] < BIG_NUMBER) { Regions[Index].ReachableTeams.AddUnique(TeamStarts.Key); Regions[Index].TeamSpawnPathCosts.Add(TeamStarts.Key, Distances[Index]); }
	}
	const bool bNoSpawns = StartsByTeam.IsEmpty();
	for (FDMTacticalNavRegionStaticData& Region : Regions) Region.bIsolated = Region.bIsolated || (!bNoSpawns && Region.ReachableTeams.IsEmpty());
}

bool UDMTacticalNavRegionSubsystem::HasStaticCapsuleClearance(const FVector& GroundLocation) const
{
	const float Radius = 34.f * FMath::Clamp(Settings.NavigationTestCapsuleRadiusScale, .75f, 1.f);
	const float HalfHeight = 88.f * FMath::Clamp(Settings.NavigationTestCapsuleHalfHeightScale, .75f, 1.f);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DMTacticalNavRegionCapsule), false);
	const FVector CapsuleCenter = GroundLocation + FVector(0, 0, HalfHeight + 2.f);
	return !GetWorld()->OverlapBlockingTestByChannel(CapsuleCenter, FQuat::Identity, ECC_WorldStatic,
		FCollisionShape::MakeCapsule(Radius, HalfHeight), Params);
}

float UDMTacticalNavRegionSubsystem::MeasureWallClearance(const FVector& GroundLocation) const
{
	float Minimum = FMath::Max(100.f, Settings.TargetRegionSize * .35f);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DMTacticalNavRegionWall), false);
	const FVector Start = GroundLocation + FVector(0, 0, 50.f);
	for (int32 Index = 0; Index < 8; ++Index)
	{
		const FVector Direction = FVector::ForwardVector.RotateAngleAxis(Index * 45.f, FVector::UpVector);
		FHitResult Hit;
		if (GetWorld()->LineTraceSingleByChannel(Hit, Start, Start + Direction * Minimum, ECC_WorldStatic, Params))
			Minimum = FMath::Min(Minimum, Hit.Distance);
	}
	return Minimum;
}

const FDMTacticalNavRegionStaticData* UDMTacticalNavRegionSubsystem::FindRegion(int32 RegionId) const
{
	const int32* Index = RegionIndexById.Find(RegionId);
	return Index && Regions.IsValidIndex(*Index) ? &Regions[*Index] : nullptr;
}

int32 UDMTacticalNavRegionSubsystem::GetRegionAtLocation(const FVector& Location) const
{
	if (BuildState != EDMTacticalNavRegionBuildState::Ready && BuildState != EDMTacticalNavRegionBuildState::MarkingReachability) return INDEX_NONE;
	UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	const ARecastNavMesh* Recast = Nav ? Cast<ARecastNavMesh>(Nav->GetDefaultNavDataInstance(FNavigationSystem::DontCreate)) : nullptr;
	if (!Recast) return INDEX_NONE;
	const NavNodeRef Ref = Recast->FindNearestPoly(Location, FVector(100.f, 100.f, 250.f), nullptr, nullptr);
	const int32* PolyIndex = PolyIndexByRef.Find(static_cast<uint64>(Ref));
	return PolyIndex ? Polygons[*PolyIndex].RegionId : INDEX_NONE;
}

void UDMTacticalNavRegionSubsystem::GetRegionNeighbors(int32 RegionId, TArray<FDMTacticalNavRegionConnection>& OutNeighbors) const
{
	OutNeighbors.Reset(); if (const FDMTacticalNavRegionStaticData* Region = FindRegion(RegionId)) OutNeighbors = Region->Neighbors;
}

void UDMTacticalNavRegionSubsystem::GetValidNavPoints(int32 RegionId, TArray<FDMTacticalNavPoint>& OutPoints) const
{
	OutPoints.Reset(); if (const FDMTacticalNavRegionStaticData* Region = FindRegion(RegionId)) OutPoints = Region->ValidNavPoints;
}

bool UDMTacticalNavRegionSubsystem::FindRegionPath(int32 StartRegionId, int32 EndRegionId, TArray<int32>& OutRegionPath, float& OutCost) const
{
	OutRegionPath.Reset(); OutCost = 0.f;
	if (!FindRegion(StartRegionId) || !FindRegion(EndRegionId)) return false;
	TMap<int32, float> Distances; TMap<int32, int32> Parents; TArray<int32> Open{StartRegionId}; Distances.Add(StartRegionId, 0.f);
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	while (!Open.IsEmpty())
	{
		int32 Best = 0; for (int32 I=1; I<Open.Num(); ++I) if (Distances.FindRef(Open[I]) < Distances.FindRef(Open[Best])) Best=I;
		const int32 Current = Open[Best]; Open.RemoveAtSwap(Best);
		if (Current == EndRegionId) break;
		const FDMTacticalNavRegionStaticData* Region = FindRegion(Current); if (!Region) continue;
		for (const FDMTacticalNavRegionConnection& Edge : Region->Neighbors)
		{
			if (!Edge.bPassable || GetConnectionModifier(Current, Edge, Now) <= 0.f) continue;
			const float NewCost = Distances.FindRef(Current) + Edge.TravelCost;
			const float* Old = Distances.Find(Edge.NeighborRegionId);
			if (Old && *Old <= NewCost) continue;
			Distances.Add(Edge.NeighborRegionId, NewCost); Parents.Add(Edge.NeighborRegionId, Current); Open.AddUnique(Edge.NeighborRegionId);
		}
	}
	const float* FinalCost = Distances.Find(EndRegionId); if (!FinalCost) return false;
	for (int32 Node=EndRegionId;; Node=Parents.FindRef(Node)) { OutRegionPath.Insert(Node,0); if (Node==StartRegionId) break; if (!Parents.Contains(Node)) return false; }
	OutCost = *FinalCost; return true;
}

float UDMTacticalNavRegionSubsystem::CalculateEventStrength(float BaseStrength, int32 Hops, float DistanceFalloff,
	float Age, float Lifetime, float TimeDecay, float ConnectionModifier)
{
	if (BaseStrength <= 0.f || Age >= Lifetime || Lifetime <= 0.f) return 0.f;
	const float Distance = FMath::Pow(FMath::Clamp(DistanceFalloff,0.f,1.f),FMath::Max(0,Hops));
	const float Time = FMath::Pow(FMath::Clamp(1.f-Age/Lifetime,0.f,1.f),FMath::Max(.01f,TimeDecay));
	return BaseStrength*Distance*Time*FMath::Clamp(ConnectionModifier,0.f,1.f);
}

void UDMTacticalNavRegionSubsystem::ReportTacticalEvent(const FDMTacticalRegionEvent& EventData)
{
	if (!IsReady() || !FindRegion(EventData.OriginRegionId)) return;
	FDMTacticalRegionEvent Event=EventData; const float Now=GetWorld()->GetTimeSeconds();
	if(const TWeakObjectPtr<UDMTacticalEventProfile>* ProfilePtr=EventProfiles.Find(Event.EventType))
		if(const UDMTacticalEventProfile* Profile=ProfilePtr->Get())
		{ Event.BaseStrength=EventData.BaseStrength>0?EventData.BaseStrength:Profile->BaseStrength; Event.PropagationDistance=Profile->PropagationDistance; Event.DistanceFalloff=Profile->DistanceFalloff; Event.Lifetime=Profile->Lifetime; Event.TimeDecay=Profile->TimeDecay; }
	if(Event.PropagationDistance<0) Event.PropagationDistance=Settings.DefaultPropagationDistance;
	if(Event.DistanceFalloff<=0) Event.DistanceFalloff=Settings.DefaultDistanceFalloff;
	if(Event.Lifetime<=0) Event.Lifetime=Settings.DefaultEventLifetime;
	if(Event.TimeDecay<=0) Event.TimeDecay=Settings.DefaultTimeDecay;
	Event.Timestamp=Now; if(!Event.EventId.IsValid()) Event.EventId=FGuid::NewGuid();
	if(FDMTacticalRegionEvent* Existing=ActiveEvents.FindByPredicate([&](const FDMTacticalRegionEvent& Other)
		{return Other.EventType==Event.EventType && Other.SourceTeam==Event.SourceTeam && Other.SourceActor==Event.SourceActor && Other.OriginRegionId==Event.OriginRegionId && Now-Other.Timestamp<.3f;}))
	{ Existing->BaseStrength=FMath::Max(Existing->BaseStrength,Event.BaseStrength); Existing->Timestamp=Now; Existing->Lifetime=FMath::Max(Existing->Lifetime,Event.Lifetime); return; }
	ActiveEvents.Add(MoveTemp(Event));
}

void UDMTacticalNavRegionSubsystem::ReportCombatEvent(const FVector& Location,uint8 SourceTeam,float Strength,AActor* SourceActor)
{
	FDMTacticalRegionEvent Event; Event.EventType=EDMTacticalRegionEventType::Combat; Event.SourceTeam=SourceTeam; Event.SourceActor=SourceActor; Event.OriginRegionId=GetRegionAtLocation(Location); Event.BaseStrength=Strength;
	Event.PropagationDistance=Settings.DefaultPropagationDistance; Event.DistanceFalloff=Settings.DefaultDistanceFalloff; Event.Lifetime=Settings.DefaultEventLifetime; Event.TimeDecay=Settings.DefaultTimeDecay; ReportTacticalEvent(Event);
}

void UDMTacticalNavRegionSubsystem::ReportThreatEvent(const FVector& Location,uint8 SourceTeam,float Strength,AActor* SourceActor)
{
	FDMTacticalRegionEvent Event; Event.EventType=EDMTacticalRegionEventType::Threat; Event.SourceTeam=SourceTeam; Event.SourceActor=SourceActor; Event.OriginRegionId=GetRegionAtLocation(Location); Event.BaseStrength=Strength;
	Event.PropagationDistance=Settings.DefaultPropagationDistance; Event.DistanceFalloff=Settings.DefaultDistanceFalloff; Event.Lifetime=Settings.DefaultEventLifetime; Event.TimeDecay=Settings.DefaultTimeDecay; ReportTacticalEvent(Event);
}

void UDMTacticalNavRegionSubsystem::UpdateActorPresence(AActor* Actor,uint8 Team,int32 RegionId,bool bInCombat)
{
	if(!Actor) return; if(RegionId==INDEX_NONE) RegionId=GetRegionAtLocation(Actor->GetActorLocation());
	FPresenceRecord& Record=Presence.FindOrAdd(Actor); Record.Actor=Actor; Record.Team=Team; Record.RegionId=RegionId; Record.bInCombat=bInCombat; Record.LastUpdateTime=GetWorld()->GetTimeSeconds();
}

void UDMTacticalNavRegionSubsystem::RemoveActorPresence(AActor* Actor){if(Actor) Presence.Remove(Actor);}

void UDMTacticalNavRegionSubsystem::GetPresentFriendlyActors(uint8 Team,AActor* IgnoreActor,TArray<AActor*>& OutActors) const
{
	OutActors.Reset();
	for(const auto& Pair:Presence) if(Pair.Value.Team==Team && Pair.Value.Actor.IsValid() && Pair.Value.Actor.Get()!=IgnoreActor) OutActors.Add(Pair.Value.Actor.Get());
}

void UDMTacticalNavRegionSubsystem::MarkRegionExplored(int32 RegionId,uint8 Team,float Confidence)
{
	if(!FindRegion(RegionId)) return; FExplorationRecord& Record=ExplorationByTeam.FindOrAdd(Team).FindOrAdd(RegionId); Record.Confidence=FMath::Max(Record.Confidence,FMath::Clamp(Confidence,0.f,1.f)); Record.LastObservedTime=GetWorld()->GetTimeSeconds();
}

FDMTacticalNavRegionRuntimeData& UDMTacticalNavRegionSubsystem::FindOrAddRuntime(uint8 Team,int32 RegionId){return RuntimeByTeam.FindOrAdd(Team).FindOrAdd(RegionId);}
const FDMTacticalNavRegionRuntimeData* UDMTacticalNavRegionSubsystem::FindRuntime(uint8 Team,int32 RegionId) const {const TMap<int32,FDMTacticalNavRegionRuntimeData>* TeamMap=RuntimeByTeam.Find(Team); return TeamMap?TeamMap->Find(RegionId):nullptr;}

bool UDMTacticalNavRegionSubsystem::GetRegionRuntimeData(int32 RegionId,uint8 Team,FDMTacticalNavRegionRuntimeData& OutData) const
{ if(const FDMTacticalNavRegionRuntimeData* Data=FindRuntime(Team,RegionId)){OutData=*Data;return true;} OutData={}; return false; }

float UDMTacticalNavRegionSubsystem::GetConnectionModifier(int32 FromRegion,const FDMTacticalNavRegionConnection& Connection,float Now) const
{
	for(const FConnectionFailure& Failure:ConnectionFailures) if(Failure.From==FromRegion && Failure.To==Connection.NeighborRegionId)
		return Failure.BlockedUntil>Now?0.f:FMath::Clamp(1.f-Failure.Score*.2f,.2f,1.f);
	const float WidthModifier=FMath::Clamp(Connection.PortalWidth/FMath::Max(1.f,Settings.NarrowPortalWidth*2.f),.35f,1.f);
	const float CostModifier=1.f/(1.f+Connection.TravelCost/FMath::Max(100.f,Settings.TargetRegionSize*2.f));
	return FMath::Clamp(FMath::Lerp(WidthModifier,CostModifier,.35f),.2f,1.f);
}

void UDMTacticalNavRegionSubsystem::ApplyEventToRuntime(const FDMTacticalRegionEvent& Event,float Now)
{
	struct FNode{int32 Region=INDEX_NONE;int32 Hops=0;float Modifier=1.f;}; TArray<FNode> Queue{{Event.OriginRegionId,0,1.f}}; TMap<int32,float> Best; Best.Add(Event.OriginRegionId,1.f);
	TSet<uint8> KnownTeams; KnownTeams.Add(Event.SourceTeam); for(const auto& Pair:Presence) KnownTeams.Add(Pair.Value.Team); for(const auto& Pair:ExplorationByTeam) KnownTeams.Add(Pair.Key);
	for(int32 Cursor=0;Cursor<Queue.Num();++Cursor)
	{
		const FNode Node=Queue[Cursor]; const float Strength=CalculateEventStrength(Event.BaseStrength,Node.Hops,Event.DistanceFalloff,Now-Event.Timestamp,Event.Lifetime,Event.TimeDecay,Node.Modifier);
		if(Strength<Settings.MinimumPropagationScore) continue;
		auto Apply=[&](uint8 Team)
		{
			FDMTacticalNavRegionRuntimeData& Data=FindOrAddRuntime(Team,Node.Region); Data.LastUpdatedTime=Now;
			switch(Event.EventType){case EDMTacticalRegionEventType::Combat:Data.CombatScore+=Strength;break;case EDMTacticalRegionEventType::Threat:Data.ThreatScore+=Strength;Data.EnemyPresenceScore+=Strength*.5f;break;case EDMTacticalRegionEventType::FriendlyPresence:Data.FriendlyPresenceScore+=Strength;break;case EDMTacticalRegionEventType::EnemyPresence:Data.EnemyPresenceScore+=Strength;break;case EDMTacticalRegionEventType::Exploration:Data.ExplorationScore=FMath::Max(0.f,Data.ExplorationScore-Strength);break;case EDMTacticalRegionEventType::SpawnPressure:Data.SpawnPressureScore+=Strength;break;}
		};
		if(Event.EventType==EDMTacticalRegionEventType::Combat) for(uint8 Team:KnownTeams) Apply(Team); else Apply(Event.SourceTeam);
		if(Node.Hops>=Event.PropagationDistance) continue; const FDMTacticalNavRegionStaticData* Region=FindRegion(Node.Region); if(!Region) continue;
		for(const FDMTacticalNavRegionConnection& Edge:Region->Neighbors)
		{
			const float Modifier=Node.Modifier*GetConnectionModifier(Node.Region,Edge,Now); const float* Old=Best.Find(Edge.NeighborRegionId); if(Old && *Old>=Modifier) continue;
			Best.Add(Edge.NeighborRegionId,Modifier); Queue.Add({Edge.NeighborRegionId,Node.Hops+1,Modifier});
		}
	}
}

void UDMTacticalNavRegionSubsystem::RebuildRuntimeScores(float Now)
{
	RuntimeByTeam.Reset(); TSet<uint8> Teams; for(const auto& Pair:Presence) if(Pair.Value.Team!=255) Teams.Add(Pair.Value.Team); for(const auto& Pair:ExplorationByTeam) Teams.Add(Pair.Key); for(const FDMTacticalRegionEvent& Event:ActiveEvents) if(Event.SourceTeam!=255) Teams.Add(Event.SourceTeam);
	for(uint8 Team:Teams) for(const FDMTacticalNavRegionStaticData& Region:Regions)
	{
		FDMTacticalNavRegionRuntimeData& Data=FindOrAddRuntime(Team,Region.RegionId); Data.LastUpdatedTime=Now; Data.ExplorationScore=1.f;
		if(const TMap<int32,FExplorationRecord>* TeamExploration=ExplorationByTeam.Find(Team)) if(const FExplorationRecord* Record=TeamExploration->Find(Region.RegionId))
			Data.ExplorationScore=1.f-FMath::Clamp(Record->Confidence-FMath::Max(0.f,Now-Record->LastObservedTime)*.01f,0.f,1.f);
		float EnemySpawnDistance=BIG_NUMBER; for(const auto& SpawnCost:Region.TeamSpawnPathCosts) if(SpawnCost.Key!=Team) EnemySpawnDistance=FMath::Min(EnemySpawnDistance,SpawnCost.Value);
		if(EnemySpawnDistance<BIG_NUMBER) Data.SpawnPressureScore=1.f/(1.f+EnemySpawnDistance/FMath::Max(100.f,Settings.TargetRegionSize*3.f));
	}
	for(const auto& Pair:Presence)
	{
		const FPresenceRecord& Record=Pair.Value; if(Record.RegionId==INDEX_NONE || Now-Record.LastUpdateTime>2.f) continue;
		for(uint8 Team:Teams){FDMTacticalNavRegionRuntimeData& Data=FindOrAddRuntime(Team,Record.RegionId); if(Team==Record.Team) Data.FriendlyPresenceScore+=1.f; else Data.EnemyPresenceScore+=1.f; if(Record.bInCombat) Data.CombatScore+=.5f;}
	}
	const int32 Limit=FMath::Min(ActiveEvents.Num(),FMath::Max(1,Settings.MaximumEventsProcessedPerTick)); for(int32 Index=0;Index<Limit;++Index) ApplyEventToRuntime(ActiveEvents[Index],Now);
	for(auto& TeamPair:RuntimeByTeam) for(auto& RegionPair:TeamPair.Value){auto& D=RegionPair.Value;D.CombatScore=FMath::Min(D.CombatScore,10.f);D.ThreatScore=FMath::Min(D.ThreatScore,10.f);D.FriendlyPresenceScore=FMath::Min(D.FriendlyPresenceScore,10.f);D.EnemyPresenceScore=FMath::Min(D.EnemyPresenceScore,10.f);D.SpawnPressureScore=FMath::Min(D.SpawnPressureScore,10.f);}
}

void UDMTacticalNavRegionSubsystem::ReportNavPointFailure(int32 RegionId,const FVector& Location,EDMTacticalNavFailureReason Reason)
{
	if(Reason==EDMTacticalNavFailureReason::None) return; const float Now=GetWorld()->GetTimeSeconds(); FNavPointFailure* Found=NavPointFailures.FindByPredicate([&](const FNavPointFailure& F){return F.RegionId==RegionId && FVector::DistSquared2D(F.Location,Location)<FMath::Square(75.f);});
	if(!Found){FNavPointFailure New;New.RegionId=RegionId;New.Location=Location;Found=&NavPointFailures.Add_GetRef(New);} ++Found->Count; Found->ExpirationTime=Now+FMath::Min(12.f,2.f+Found->Count*2.f);
	if(int32* Index=RegionIndexById.Find(RegionId)) for(FDMTacticalNavPoint& Point:Regions[*Index].ValidNavPoints) if(FVector::DistSquared2D(Point.Location,Location)<FMath::Square(75.f)){Point.LastFailureTime=Now;Point.TemporaryFailureCount=Found->Count;}
}

bool UDMTacticalNavRegionSubsystem::IsNavPointCoolingDown(int32 RegionId,const FVector& Location,float Now) const
{return NavPointFailures.ContainsByPredicate([&](const FNavPointFailure& F){return F.RegionId==RegionId && F.ExpirationTime>Now && FVector::DistSquared2D(F.Location,Location)<FMath::Square(75.f);});}

void UDMTacticalNavRegionSubsystem::ReportConnectionFailure(int32 FromRegionId,int32 ToRegionId,EDMTacticalNavFailureReason Reason)
{
	if(Reason==EDMTacticalNavFailureReason::None) return; const float Now=GetWorld()->GetTimeSeconds(); FConnectionFailure* Found=ConnectionFailures.FindByPredicate([&](const FConnectionFailure& F){return F.From==FromRegionId&&F.To==ToRegionId;}); if(!Found){FConnectionFailure New;New.From=FromRegionId;New.To=ToRegionId;Found=&ConnectionFailures.Add_GetRef(New);} Found->Score=FMath::Min(5.f,Found->Score+1.f); if(Found->Score>=3.f) Found->BlockedUntil=Now+8.f;
}

void UDMTacticalNavRegionSubsystem::PurgeTransientData(float Now)
{
	ActiveEvents.RemoveAll([Now](const auto& E){return Now-E.Timestamp>=E.Lifetime;});
	for(auto It=Presence.CreateIterator();It;++It) if(!It.Value().Actor.IsValid()||Now-It.Value().LastUpdateTime>3.f) It.RemoveCurrent();
	NavPointFailures.RemoveAll([Now](const auto& F){return F.ExpirationTime<=Now;});
	for(FConnectionFailure& Failure:ConnectionFailures) if(Failure.BlockedUntil<=Now) Failure.Score=FMath::Max(0.f,Failure.Score-.25f); ConnectionFailures.RemoveAll([](const auto& F){return F.Score<=0.f;});
}

#if !UE_BUILD_SHIPPING
void UDMTacticalNavRegionSubsystem::DrawDebugRegions() const
{
	const int32 Level=CVarDMTacticalNavRegions.GetValueOnGameThread(); const uint8 Team=static_cast<uint8>(FMath::Clamp(CVarDMTacticalNavRegionTeam.GetValueOnGameThread(),0,254)); UWorld* World=GetWorld(); if(!World)return;
	for(const auto& Region:Regions)
	{
		const FColor Color=Region.bIsolated?FColor::Red:(Region.bNarrow?FColor::Orange:FColor::Cyan); DrawDebugBox(World,Region.Bounds.GetCenter(),Region.Bounds.GetExtent(),Color,false,-1.f,0,2.f);
		DrawDebugString(World,Region.RepresentativeLocation+FVector(0,0,70),FString::Printf(TEXT("R%d"),Region.RegionId),nullptr,Color,0.f,true);
		if(Level>=2)for(const auto& Edge:Region.Neighbors)if(Edge.NeighborRegionId>Region.RegionId)if(const auto* Other=FindRegion(Edge.NeighborRegionId))DrawDebugLine(World,Region.RepresentativeLocation,Other->RepresentativeLocation,Edge.ConnectionType==EDMTacticalNavConnectionType::Narrow?FColor::Orange:FColor::Green,false,-1.f,0,2.f);
		if(Level>=3){FDMTacticalNavRegionRuntimeData D;GetRegionRuntimeData(Region.RegionId,Team,D);DrawDebugString(World,Region.RepresentativeLocation+FVector(0,0,105),FString::Printf(TEXT("C%.1f T%.1f F%.1f E%.1f X%.1f S%.1f"),D.CombatScore,D.ThreatScore,D.FriendlyPresenceScore,D.EnemyPresenceScore,D.ExplorationScore,D.SpawnPressureScore),nullptr,FColor::White,0.f,true);}
		if(Level>=4)for(const auto& Point:Region.ValidNavPoints)DrawDebugSphere(World,Point.Location+FVector(0,0,12),12.f,8,IsNavPointCoolingDown(Region.RegionId,Point.Location,World->GetTimeSeconds())?FColor::Red:FColor::Blue,false,-1.f,0,1.5f);
	}
}
#endif
