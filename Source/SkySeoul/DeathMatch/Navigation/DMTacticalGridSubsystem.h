#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "DMTacticalGridTypes.h"
#include "DMTacticalGridSubsystem.generated.h"

class AController;
class UNavigationSystemV1;
struct FDMCombatThreatSnapshot;

/** One world-owned low-frequency strategic map. It never returns an exact combat destination. */
UCLASS(meta=(DisplayName="DeathMatch Tactical Grid (Deprecated)", DeprecationMessage="Use DMTacticalNavRegionSubsystem."))
class SKYSEOUL_API UDMTacticalGridSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override;
    virtual bool IsTickableInEditor() const override { return false; }

    UFUNCTION(BlueprintPure, Category="DeathMatch|Tactical Grid")
    EDMTacticalGridBuildState GetBuildState() const { return BuildState; }
    UFUNCTION(BlueprintPure, Category="DeathMatch|Tactical Grid")
    bool IsReady() const { return BuildState == EDMTacticalGridBuildState::Ready; }
    const FDMTacticalGridSettings& GetSettings() const { return Settings; }
    UFUNCTION(BlueprintCallable, Category="DeathMatch|Tactical Grid")
    void RequestRebuild();

    const TArray<FDMTacticalCellStaticData>& GetCells() const { return Cells; }
    bool GetCell(const FDMTacticalCellId& Id, FDMTacticalCellStaticData& OutCell) const;
    const FDMTacticalCellStaticData* FindCell(const FDMTacticalCellId& Id) const;
    bool FindCellAtLocation(const FVector& Location, FDMTacticalCellId& OutId, bool bRequireSpawnReachable = true) const;
    bool IsLocationInsideCell(const FDMTacticalCellId& Id, const FVector& Location, float Tolerance = 25.f) const;

    void RegisterActor(AActor* Actor, uint8 Team, bool bCombatant = false);
    void UnregisterActor(AActor* Actor);
    bool UpdateActorCell(AActor* Actor, uint8 Team, bool bCombatant, const FVector& FocusDirection = FVector::ZeroVector);
    void ReportThreat(uint8 Team, AActor* Observer, AActor* Threat, const FVector& KnownLocation,
        bool bVisible, bool bRemembered, bool bEngagement, float Confidence = 1.f);
    void ReportDamageEvent(uint8 Team, const FVector& Location);
    void ReportFireEvent(uint8 Team, const FVector& Location);
    void ReportObservation(uint8 Team, const FDMTacticalCellId& CellId, float Confidence);
    bool GetTeamCellData(uint8 Team, const FDMTacticalCellId& Id, FDMTacticalCellDynamicData& OutData) const;
    FDMTacticalFormationSnapshot GetFormationSnapshot(uint8 Team, const FVector& RequesterLocation);

    /** Noncombat-only strategic API. Returns regions, never exact movement coordinates. */
    bool FindStrategicRegions(AActor* Requester, uint8 Team, const FDMTacticalEvaluationContext& Context,
        int32 MaximumCandidates, TArray<FDMTacticalStrategicRegionSelection>& OutRegions);
    bool AnalyzeTacticalPath(AActor* Requester, uint8 Team, const FDMTacticalCellId& StartCell,
        const FDMTacticalCellId& DestinationCell, FDMTacticalPathResult& OutPath);
    bool ReserveCell(uint8 Team, AActor* Actor, const FDMTacticalCellId& CellId, EDMTacticalPurpose Purpose,
        float Lifetime = 0.f, const FDMTacticalCellScore* Score = nullptr,
        const FVector& ReservedWorldLocation = FVector::ZeroVector, float GoalReservationRadius = 0.f);
    void ReleaseReservations(AActor* Actor);
    void GetReservedLocationsForCell(uint8 Team, const FDMTacticalCellId& CellId,
        AActor* IgnoreActor, TArray<FVector>& OutLocations) const;
    void GetRegisteredFriendlyActors(uint8 Team, AActor* IgnoreActor, TArray<AActor*>& OutActors) const;

    void RecordFailure(AActor* Actor, uint8 Team, const FDMTacticalCellId& CellId, FName Action,
        EDMTacticalPurpose Purpose, EDMTacticalFailureReason Reason, float Cooldown = 0.f);
    bool IsFailureCoolingDown(AActor* Actor, uint8 Team, const FDMTacticalCellId& CellId,
        FName Action, EDMTacticalPurpose Purpose) const;
    void InvalidateFailuresForTeam(uint8 Team);

    static EDMTacticalCellType ClassifyCellType(float WalkableRatio, float Openness);
    static float CalculateDecayedObservation(float Confidence, float ElapsedSeconds, float DecayPerSecond);
    static FDMTacticalScoreWeights ApplyPersonalityCorrection(const FDMTacticalScoreWeights& StateWeights,
        const FDMTacticalPersonality& Personality);

#if !UE_BUILD_SHIPPING
    /** Read-only copies for debug/HUD; never runs navigation queries. */
    void GetDebugSnapshot(uint8 Team, TArray<FDMTacticalCellStaticData>& OutStatic,
        TArray<FDMTacticalCellDynamicData>& OutDynamic, TArray<FDMTacticalCellReservation>& OutReservations) const;
#endif

private:
    friend class FDMTacticalGridCoreTest;
    friend class FDMTacticalGridRuntimeTest;
    friend class FDMTacticalGridStrategicTest;
    /** Internal score engine retained for strategic weights and white-box regression tests. */
    bool FindTacticalCandidates(AActor* Requester, uint8 Team, const FDMTacticalEvaluationContext& Context,
        int32 SearchRadius, int32 MaximumCandidates, TArray<FDMTacticalCellScore>& OutCandidates);
    struct FSampleTask
    {
        int32 X = 0, Y = 0, SampleX = 0, SampleY = 0, VerticalIndex = 0;
        FVector Query = FVector::ZeroVector;
    };
    struct FSampleHit
    {
        int32 SampleIndex = 0;
        FVector Location = FVector::ZeroVector;
        float WallClearance = 0.f;
        FVector OpenDirection = FVector::ZeroVector;
    };
    struct FColumn
    {
        int32 X = 0, Y = 0;
        TArray<FSampleHit> Hits;
    };
    struct FConnectionPair { int32 A = INDEX_NONE, B = INDEX_NONE; };
    struct FPendingConnection { int32 NeighborIndex = INDEX_NONE; float Cost = 0.f; float Clearance = 0.f; EDMTacticalConnectionType Type = EDMTacticalConnectionType::Normal; };
    struct FSpawnDijkstraNode { int32 CellIndex = INDEX_NONE; float Distance = 0.f; int32 Hops = 0; };

    struct FRegisteredActor
    {
        TWeakObjectPtr<AActor> Actor;
        uint8 Team = 255;
        FDMTacticalCellId CellId;
        bool bHasCell = false;
        bool bCombatant = false;
        FVector FocusDirection = FVector::ZeroVector;
    };
    struct FThreatRecord
    {
        TWeakObjectPtr<AActor> Threat;
        TWeakObjectPtr<AActor> Observer;
        FDMTacticalCellId CellId;
        float LastUpdate = 0.f;
        float Confidence = 0.f;
        bool bVisible = false;
        bool bRemembered = false;
        bool bEngagement = false;
    };
    struct FTimedEvent { FDMTacticalCellId CellId; float Time = 0.f; };
    struct FTeamRuntime
    {
        TMap<FDMTacticalCellId, FDMTacticalCellDynamicData> Cells;
        TMap<TWeakObjectPtr<AActor>, FThreatRecord> Threats;
        TArray<FTimedEvent> DamageEvents;
        TArray<FTimedEvent> FireEvents;
        FDMTacticalFormationSnapshot Formation;
        uint32 Revision = 0;
    };
    struct FFailureKey
    {
        TWeakObjectPtr<AActor> Actor;
        uint8 Team = 255;
        FDMTacticalCellId CellId;
        FName Action;
        EDMTacticalPurpose Purpose = EDMTacticalPurpose::None;
        friend bool operator==(const FFailureKey& A, const FFailureKey& B)
        { return A.Actor == B.Actor && A.Team == B.Team && A.CellId == B.CellId && A.Action == B.Action && A.Purpose == B.Purpose; }
        friend uint32 GetTypeHash(const FFailureKey& K)
        {
            uint32 H = HashCombine(GetTypeHash(K.Actor), GetTypeHash(K.Team));
            H = HashCombine(H, GetTypeHash(K.CellId)); H = HashCombine(H, GetTypeHash(K.Action));
            return HashCombine(H, GetTypeHash(static_cast<uint8>(K.Purpose)));
        }
    };
    struct FFailureValue { EDMTacticalFailureReason Reason = EDMTacticalFailureReason::None; float Expiration = 0.f; };
    struct FPathCacheKey
    {
        FDMTacticalCellId Start;
        FDMTacticalCellId Destination;
        uint8 Team = 255;
        uint32 GridRevision = 0;
        friend bool operator==(const FPathCacheKey& A, const FPathCacheKey& B)
        { return A.Start == B.Start && A.Destination == B.Destination && A.Team == B.Team
            && A.GridRevision == B.GridRevision; }
        friend uint32 GetTypeHash(const FPathCacheKey& K)
        {
            uint32 H = HashCombine(GetTypeHash(K.Start), GetTypeHash(K.Destination));
            H = HashCombine(H, GetTypeHash(K.Team));
            return HashCombine(H, GetTypeHash(K.GridRevision));
        }
    };

    void TickBuild();
    bool GatherAnalysisBounds();
    void PrepareSampleTasks();
    void GenerateSamples();
    void BuildCells();
    void PrepareConnections();
    void BuildConnections();
    bool ValidateConnection(int32 A, int32 B, FPendingConnection& Out) const;
    void PrepareReachability();
    void MarkReachability();
    void FinalizeStrategicCellAvailability();
    void CalculateStaticMetrics();
    void PrepareSpawnFields();
    void CalculateSpawnFields();
    void CalculateSpawnControl();
    void FinalizeRegionsAndIds();
    void RebuildCellIndex();
    void UpdateRuntime(float DeltaTime);
    void RebuildDynamicCounts(float Now);
    void ApplyFriendlyGraphObservation(float Now);
    void RebuildExplorationStates(uint8 Team, float Now);
    void PurgeExpired(float Now);
    float MeasureWallClearance(const FVector& Location, FVector& OutOpenDirection) const;
    bool HasCapsuleClearance(const FVector& Ground, float Scale = 1.f) const;
    int32 FindClosestCellIndex(const FVector& Location, bool bRequireSpawnReachable) const;
    FDMTacticalCellDynamicData& FindOrAddDynamic(uint8 Team, const FDMTacticalCellId& Id);
    const FDMTacticalCellDynamicData* FindDynamic(uint8 Team, const FDMTacticalCellId& Id) const;
    EDMTacticalPurpose ChoosePurpose(const FDMTacticalEvaluationContext& Context,
        const FDMTacticalCellScore& Score) const;
    void BuildScoreContributors(FDMTacticalCellScore& Score) const;
    void PopulatePathMetrics(AActor* Requester, uint8 Team, FDMTacticalPathResult& Path) const;
    float GetRecentRoamPenalty(const FDMTacticalEvaluationContext& Context, const FDMTacticalCellId& CellId) const;
    bool IsTeamReachable(const FDMTacticalCellStaticData& Cell, uint8 Team) const;
    bool IsStrategicCellPlayable(const FDMTacticalCellStaticData& Cell, uint8 RequestingTeam) const;
    int32 CountCellReservations(uint8 Team, const FDMTacticalCellId& CellId, AActor* IgnoreActor) const;
    int32 CountApproachRouteReservations(uint8 Team, int32 ApproachRouteId, AActor* IgnoreActor) const;
    static void ResetDynamicPathMetrics(FDMTacticalPathResult& Path);
    float GetFailurePenalty(AActor* Actor, uint8 Team, const FDMTacticalCellId& CellId, EDMTacticalPurpose Purpose) const;
#if !UE_BUILD_SHIPPING
    void DrawDebugGrid() const;
#endif

    UPROPERTY(VisibleAnywhere, Category="DeathMatch|Tactical Grid|Debug")
    EDMTacticalGridBuildState BuildState = EDMTacticalGridBuildState::WaitingForNavigation;
    UPROPERTY(VisibleAnywhere, Category="DeathMatch|Tactical Grid|Debug")
    FDMTacticalGridSettings Settings;
    FBox AnalysisBounds = FBox(ForceInit);
    TArray<FBox> AnalysisAreas;
    TArray<FSampleTask> SampleTasks;
    TArray<FColumn> Columns;
    TMap<FIntPoint, int32> ColumnIndex;
    int32 SampleCursor = 0, CellCursor = 0, ConnectionCursor = 0, MetricCursor = 0;
    TArray<FDMTacticalCellStaticData> Cells;
    TArray<TArray<FPendingConnection>> PendingConnections;
    TArray<FConnectionPair> ConnectionPairs;
    TMap<FDMTacticalCellId, int32> CellIndexById;
    float NextNavigationRetryTime = 0.f;
    float VerticalProjectionExtent = 125.f;

    struct FReachabilityWork { int32 CellIndex = INDEX_NONE; uint8 Team = 255; };
    TArray<FReachabilityWork> ReachabilityQueue;
    TSet<uint64> ReachabilityVisited;
    int32 ReachabilityCursor = 0;
    bool bHadSpawn = false;
    TMap<uint8, FVector> SpawnCenters;
    TMap<uint8, int32> SpawnCounts;
    TMap<uint8, TArray<int32>> SpawnCellIndicesByTeam;
    TMap<uint8, int32> RepresentativeSpawnCellByTeam;
    TMap<uint8, FString> RepresentativeSpawnNameByTeam;
    uint8 StrategicTeamA = 255;
    uint8 StrategicTeamB = 255;
    TArray<uint8> SpawnFieldTeams;
    int32 SpawnFieldTeamCursor = 0;
    bool bSpawnFieldActive = false;
    uint8 ActiveSpawnFieldTeam = 255;
    TArray<FSpawnDijkstraNode> SpawnFieldHeap;
    TArray<float> SpawnFieldDistances;
    TArray<int32> SpawnFieldHops;
    TArray<int32> SpawnFieldNextIndices;
    int32 SpawnControlCursor = 0;
    uint32 GridRevision = 0;

    TMap<TWeakObjectPtr<AActor>, FRegisteredActor> RegisteredActors;
    TMap<uint8, FTeamRuntime> TeamRuntime;
    TArray<FDMTacticalCellReservation> Reservations;
    TMap<FFailureKey, FFailureValue> Failures;
    TMap<FPathCacheKey, FDMTacticalPathResult> PathCache;
    float NextActorValidationTime = 0.f;
    float NextDynamicRefreshTime = 0.f;
    float NextObservationRefreshTime = 0.f;
};

