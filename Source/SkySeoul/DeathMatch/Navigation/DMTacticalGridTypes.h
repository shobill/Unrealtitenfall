#pragma once

#include "CoreMinimal.h"
#include "DMTacticalGridTypes.generated.h"

UENUM(BlueprintType)
enum class EDMTacticalGridBuildState : uint8
{
    WaitingForNavigation,
    GeneratingSamples,
    BuildingCells,
    BuildingConnections,
    MarkingReachability,
    CalculatingStaticMetrics,
    Ready,
    Failed,
    CalculatingSpawnFields,
    CalculatingSpawnControl
};

UENUM(BlueprintType)
enum class EDMTacticalCellType : uint8 { Blocked, Narrow, Partial, Normal, Open };
UENUM(BlueprintType)
enum class EDMTacticalConnectionType : uint8 { Normal, Narrow, Stair, Ramp, Drop, OneWay };
UENUM(BlueprintType)
enum class EDMTacticalPurpose : uint8
{
    None, Join, Attack, Ambush, Explore, Watch, Retreat, Hide,
    ExploreFrontier,
    SpawnControl
};
/** Thin adapter over CombatTactics threat awareness plus SituationFramework's noncombat Roam. */
UENUM(BlueprintType)
enum class EDMTacticalEvaluationState : uint8
{
    Visible,
    LostGrace,
    PendingSwitch,
    Investigating,
    WatchingLastKnown,
    DisengagedRoam
};
UENUM(BlueprintType)
enum class EDMTacticalFailureReason : uint8
{
    None, NoValidPosition, NoLineOfSight, NoCover, InvalidPath, CapsuleBlocked,
    Occupied, ReservationConflict, TooDangerous
};

UENUM(BlueprintType)
enum class EDMTacticalStrategicReachabilityPolicy : uint8
{
    ReachableFromBothTeams,
    ReachableFromEitherTeam,
    ReachableFromRequestingTeam
};

UENUM(BlueprintType)
enum class EDMTacticalStrategicCellExclusionReason : uint8
{
    None,
    NoNavProjection,
    NoValidConnection,
    UnreachableFromTeamA,
    UnreachableFromTeamB,
    IsolatedRegion,
    InvalidSpawnReference
};

/** Stable tactical address. XY alone is never enough: Layer separates height and Region separates connectivity. */
USTRUCT(BlueprintType)
struct FDMTacticalCellId
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tactical Cell") int32 X = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tactical Cell") int32 Y = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tactical Cell") int32 Layer = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tactical Cell") int32 Region = INDEX_NONE;

    bool IsValid() const { return Region != INDEX_NONE; }
    friend bool operator==(const FDMTacticalCellId& A, const FDMTacticalCellId& B)
    { return A.X == B.X && A.Y == B.Y && A.Layer == B.Layer && A.Region == B.Region; }
    friend bool operator!=(const FDMTacticalCellId& A, const FDMTacticalCellId& B) { return !(A == B); }
};

FORCEINLINE uint32 GetTypeHash(const FDMTacticalCellId& Id)
{
    uint32 Hash = HashCombine(GetTypeHash(Id.X), GetTypeHash(Id.Y));
    Hash = HashCombine(Hash, GetTypeHash(Id.Layer));
    return HashCombine(Hash, GetTypeHash(Id.Region));
}

USTRUCT(BlueprintType)
struct FDMTacticalGridBuildSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category="DeathMatch|Tactical Grid|Build", meta=(DeprecatedProperty, DeprecationMessage="Use Tactical Nav Region TargetRegionSize.", ClampMin="400", ClampMax="2400", Units="cm"))
    float CellSize = 1200.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category="DeathMatch|Tactical Grid|Sampling", meta=(DeprecatedProperty, DeprecationMessage="Square grid sampling is no longer used.", ClampMin="2", ClampMax="5"))
    int32 SamplesPerAxis = 3;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category="DeathMatch|Tactical Grid|Sampling", meta=(DeprecatedProperty, DeprecationMessage="Walkable-ratio cells are no longer used.", ClampMin="0", ClampMax="1"))
    float MinimumWalkableRatio = .35f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category="DeathMatch|Tactical Grid|Sampling", meta=(DeprecatedProperty, DeprecationMessage="Use Tactical Nav Region RegionHeightBoundary.", ClampMin="50", Units="cm"))
    float LayerHeightThreshold = 250.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DeathMatch|Tactical Grid|Build", meta=(ClampMin="1", ToolTip="Maximum NavMesh projection probes processed per subsystem tick."))
    int32 MaximumSamplesPerTick = 96;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DeathMatch|Tactical Grid|Build", meta=(ClampMin="1", ToolTip="Maximum neighbor path checks or reachability nodes processed per subsystem tick."))
    int32 MaximumConnectionChecksPerTick = 24;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DeathMatch|Tactical Grid|Build", meta=(ClampMin="0.1", Units="ms", ToolTip="Soft wall-clock budget per build tick. Work also stops at the item budgets."))
    float MaximumBuildTimePerTickMs = 2.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DeathMatch|Tactical Grid|Build", meta=(ClampMin="1.0", ToolTip="Maximum verified path length divided by direct distance for adjacent cells."))
    float MaximumNeighborPathRatio = 2.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category="DeathMatch|Tactical Grid|Build", meta=(DeprecatedProperty, DeprecationMessage="Region adjacency comes from NavMesh portals.", ClampMin="1.0"))
    float MaximumNeighborDistanceScale = 2.25f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DeathMatch|Tactical Grid|Sampling", meta=(ClampMin="10", Units="cm", ToolTip="Capsule radius used by static standing and connection-clearance checks."))
    float AgentRadius = 42.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DeathMatch|Tactical Grid|Sampling", meta=(ClampMin="20", Units="cm", ToolTip="Capsule half height used by static standing and connection-clearance checks."))
    float AgentHalfHeight = 96.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DeathMatch|Tactical Grid|Sampling", meta=(ToolTip="Use the default navigation data's supported agent radius and height for projection and static capsule checks."))
    bool bUseNavDataAgentDimensions = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DeathMatch|Tactical Grid|Sampling", meta=(ClampMin="1", ClampMax="32", ToolTip="Maximum vertical projection seeds per XY sample. Prevents tall analysis bounds from causing unbounded work."))
    int32 MaximumVerticalSamples = 12;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category="DeathMatch|Tactical Grid|Build", meta=(DeprecatedProperty, DeprecationMessage="XY grid columns are no longer generated.", ClampMin="64"))
    int32 MaximumGridColumns = 20000;
};

USTRUCT(BlueprintType)
struct FDMTacticalGridRuntimeSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DeathMatch|Tactical Grid|Runtime", meta=(ClampMin="0.05", Units="s", ToolTip="Low-frequency interval for registered actor cell validation. No world actor scan is performed."))
    float ActorValidationInterval = .25f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DeathMatch|Tactical Grid|Observation", meta=(ClampMin="0", ToolTip="Observation confidence lost per second. Zero disables time decay."))
    float ObservationDecayPerSecond = .04f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DeathMatch|Tactical Grid|Observation", meta=(ClampMin="0", Units="s", ToolTip="Known threat memory lifetime. Zero keeps only observations from the current update."))
    float ThreatMemorySeconds = 8.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DeathMatch|Tactical Grid|Runtime", meta=(ClampMin="0.05", Units="s", ToolTip="Short cache duration for team formation statistics."))
    float FormationCacheSeconds = .35f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DeathMatch|Tactical Grid|Runtime", meta=(ClampMin="100", Units="cm", ToolTip="Actors farther than this from the coordinate median are excluded from the robust formation center when a dense group remains."))
    float FormationOutlierDistance = 2400.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DeathMatch|Tactical Grid|Runtime", meta=(ClampMin="0.1", Units="s", ToolTip="Default tactical region reservation lifetime."))
    float CellReservationLifetime = 4.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DeathMatch|Tactical Grid|Runtime", meta=(ClampMin="0.1", Units="s", ToolTip="Default cell/action failure cooldown. Failures expire and are reevaluated."))
    float FailureCooldown = 3.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DeathMatch|Tactical Grid|Runtime", meta=(ClampMin="1", ToolTip="Maximum simultaneous general reservations in one cell. Explore and Watch remain exclusive."))
    int32 MaximumReservationsPerCell = 3;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DeathMatch|Tactical Grid|Observation", meta=(ClampMin="1", ClampMax="16", ToolTip="Angular sectors used for team focus/coverage aggregation."))
    int32 FocusDirectionBuckets = 8;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DeathMatch|Tactical Grid|Observation", meta=(ClampMin="0.1", Units="s"))
    float ObservationRefreshInterval = .5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DeathMatch|Tactical Grid|Observation", meta=(ClampMin="0", ClampMax="8"))
    int32 FriendlyObservationRadiusHops = 2;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DeathMatch|Tactical Grid|Observation", meta=(ClampMin="0", ClampMax="1"))
    float FriendlyObservationConfidenceAtSelf = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DeathMatch|Tactical Grid|Observation", meta=(ClampMin="0", ClampMax="1", ToolTip="Confidence removed for each connection-graph hop. Defaults yield 1.0, 0.8, 0.5."))
    float FriendlyObservationConfidencePerHop = .2f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DeathMatch|Tactical Grid|Observation")
    bool bUseConnectionGraphObservation = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DeathMatch|Tactical Grid|Observation", meta=(ClampMin="0", ClampMax="1"))
    float UnknownConfidenceThreshold = .1f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DeathMatch|Tactical Grid|Observation", meta=(ClampMin="0", Units="s"))
    float RecentlyObservedSeconds = 6.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DeathMatch|Tactical Grid|Observation", meta=(ClampMin="0", Units="s"))
    float StaleObservationSeconds = 30.f;
};

USTRUCT(BlueprintType)
struct FDMTacticalStrategicSettings
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category="DeathMatch|Tactical Grid|Roam", meta=(DeprecatedProperty, DeprecationMessage="Use Tactical Nav Region path cost.", ClampMin="0")) int32 MinimumRoamCellHops = 2;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category="DeathMatch|Tactical Grid|Roam", meta=(DeprecatedProperty, DeprecationMessage="Use Tactical Nav Region path cost.", ClampMin="0")) int32 PreferredRoamCellHopsMin = 3;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category="DeathMatch|Tactical Grid|Roam", meta=(DeprecatedProperty, DeprecationMessage="Use Tactical Nav Region path cost.", ClampMin="1")) int32 PreferredRoamCellHopsMax = 6;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DeathMatch|Tactical Grid|Roam", meta=(ClampMin="1", ClampMax="16")) int32 RecentRoamHistorySize = 5;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DeathMatch|Tactical Grid|Roam", meta=(ClampMin="0")) float RecentRoamCellPenalty = 100.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DeathMatch|Tactical Grid|Roam", meta=(ClampMin="0.1", Units="s")) float RecentRoamPenaltyDecaySeconds = 30.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DeathMatch|Tactical Grid|Roam", meta=(ClampMin="0", Units="s")) float RoamDirectionHoldSeconds = 5.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DeathMatch|Tactical Grid|Roam", meta=(ClampMin="0")) float FrontierScoreWeight = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category="DeathMatch|Tactical Grid|Roam", meta=(DeprecatedProperty, DeprecationMessage="Use Exploration event interpretation.", ClampMin="0")) float UnknownRegionSizeWeight = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category="DeathMatch|Tactical Grid|Roam", meta=(DeprecatedProperty, DeprecationMessage="Use Tactical Nav Region path cost.", ClampMin="0")) float HopDistancePreferenceWeight = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DeathMatch|Tactical Grid|Roam", meta=(ClampMin="0")) float MinimumFrontierValueBeforeSpawnControl = 18.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DeathMatch|Tactical Grid|Roam", meta=(ClampMin="0")) float ExploreFrontierPurposeWeight = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DeathMatch|Tactical Grid|Roam", meta=(ClampMin="0")) float SpawnControlPurposeWeight = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DeathMatch|Tactical Grid|Roam", meta=(ClampMin="0", ToolTip="Score bonus for the purpose currently preferred by the frontier threshold. The other purpose remains available as an alternative.")) float StrategicPurposePreferenceBonus = 60.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DeathMatch|Tactical Grid|Roam", meta=(DeprecatedProperty, DeprecationMessage="Global SpawnControl occupancy is no longer a hard gate. Use StrategicCellCapacity and MaximumControllersPerApproachRoute.")) int32 TeamSpawnControlOccupancyLimit = 3;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category="DeathMatch|Tactical Grid|Roam", meta=(DeprecatedProperty, DeprecationMessage="Regions are not exclusively reserved; use exact Nav Point reservations.", ClampMin="1")) int32 StrategicCellCapacity = 2;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category="DeathMatch|Tactical Grid|Roam", meta=(DeprecatedProperty, DeprecationMessage="Use Tactical Nav Region crowding penalty.", ClampMin="0")) float StrategicReservationPenaltyPerController = 14.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category="DeathMatch|Tactical Grid|Roam", meta=(DeprecatedProperty, DeprecationMessage="Use Region connection failure/crowding state.", ClampMin="1")) int32 MaximumControllersPerApproachRoute = 3;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category="DeathMatch|Tactical Grid|Roam", meta=(DeprecatedProperty, DeprecationMessage="Use Region connection failure/crowding state.", ClampMin="0")) float ApproachRouteCongestionPenaltyPerController = 10.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DeathMatch|Tactical Grid|Roam", meta=(ClampMin="0", Units="cm", ToolTip="Minimum desired separation between exact arrival locations reserved inside the same strategic cell.")) float StrategicArrivalSeparation = 180.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DeathMatch|Tactical Grid|Roam", meta=(ClampMin="0", Units="cm", ToolTip="Strategic Roam MoveTo arrival tolerance used when separating exact goals. Keep aligned with the Roam PathMove acceptance radius.")) float StrategicGoalAcceptanceRadius = 150.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DeathMatch|Tactical Grid|Roam", meta=(ClampMin="0", ToolTip="Small deterministic requester/cell score variation used to spread otherwise equal strategic choices.")) float StableControllerTieBreakMagnitude = 1.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DeathMatch|Tactical Grid|Reachability") EDMTacticalStrategicReachabilityPolicy StrategicReachabilityPolicy = EDMTacticalStrategicReachabilityPolicy::ReachableFromBothTeams;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DeathMatch|Tactical Grid|Path", meta=(ClampMin="1", Units="cm/s")) float EstimatedTravelSpeed = 450.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DeathMatch|Tactical Grid|Path", meta=(ClampMin="0")) float HopCountCost = .5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DeathMatch|Tactical Grid|Path", meta=(ClampMin="0")) float NarrowRouteCost = 4.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DeathMatch|Tactical Grid|Path", meta=(ClampMin="0")) float ContestedRouteCost = 7.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DeathMatch|Tactical Grid|Path", meta=(ClampMin="0")) float CongestionRouteCost = 3.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DeathMatch|Tactical Grid|Path", meta=(ClampMin="0")) float FailedRouteCost = 8.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DeathMatch|Tactical Grid|Path", meta=(ClampMin="0")) float PathRiskCost = 10.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DeathMatch|Tactical Grid|Spawn Control", meta=(ClampMin="0")) int32 EnemySpawnExclusionHops = 3;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DeathMatch|Tactical Grid|Spawn Control", meta=(ClampMin="0")) int32 PreferredSpawnControlMinHops = 4;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DeathMatch|Tactical Grid|Spawn Control", meta=(ClampMin="1")) int32 PreferredSpawnControlMaxHops = 8;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DeathMatch|Tactical Grid|Spawn Control") float SpawnControlJunctionWeight = 18.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DeathMatch|Tactical Grid|Spawn Control") float SpawnControlChokeWeight = 12.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DeathMatch|Tactical Grid|Spawn Control") float SpawnControlEscapeRouteWeight = 6.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DeathMatch|Tactical Grid|Spawn Control") float SpawnControlOpennessWeight = 8.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DeathMatch|Tactical Grid|Spawn Control") float SpawnControlFrontProgressWeight = 15.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DeathMatch|Tactical Grid|Spawn Control") float SpawnControlDeadEndPenalty = 40.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DeathMatch|Tactical Grid|Spawn Control") float SpawnControlNarrowPenalty = 20.f;
};

/** State-owned score multipliers. Personality may only apply a bounded multiplier to these values. */
USTRUCT(BlueprintType)
struct FDMTacticalScoreWeights
{
    GENERATED_BODY()
    FDMTacticalScoreWeights() = default;
    FDMTacticalScoreWeights(float Formation, float Combat, float Ambush, float Exploration, float Coverage,
        float Advance, float Isolation, float Overcrowding, float Travel, float Exposure, float StaleVisit,
        float EnemyApproach, float LastKnown, float Retention)
        : FriendlyFormationWeight(Formation), CombatOpportunityWeight(Combat), AmbushWeight(Ambush),
          ExplorationWeight(Exploration), CoverageWeight(Coverage), SupportedAdvanceWeight(Advance),
          IsolationRiskWeight(Isolation), OvercrowdingWeight(Overcrowding), TravelCostWeight(Travel),
          KnownEnemyExposureWeight(Exposure), StaleVisitWeight(StaleVisit),
          EnemyApproachProbabilityWeight(EnemyApproach), LastKnownThreatWeight(LastKnown),
          CurrentCellRetentionWeight(Retention)
    {}
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tactical Weights", meta=(ClampMin="0")) float FriendlyFormationWeight = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tactical Weights", meta=(ClampMin="0")) float CombatOpportunityWeight = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tactical Weights", meta=(ClampMin="0")) float AmbushWeight = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tactical Weights", meta=(ClampMin="0")) float ExplorationWeight = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tactical Weights", meta=(ClampMin="0")) float CoverageWeight = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tactical Weights", meta=(ClampMin="0")) float SupportedAdvanceWeight = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tactical Weights", meta=(ClampMin="0")) float IsolationRiskWeight = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tactical Weights", meta=(ClampMin="0")) float OvercrowdingWeight = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tactical Weights", meta=(ClampMin="0")) float TravelCostWeight = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tactical Weights", meta=(ClampMin="0")) float KnownEnemyExposureWeight = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tactical Weights", meta=(ClampMin="0")) float StaleVisitWeight = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tactical Weights", meta=(ClampMin="0")) float EnemyApproachProbabilityWeight = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tactical Weights", meta=(ClampMin="0")) float LastKnownThreatWeight = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tactical Weights", meta=(ClampMin="0")) float CurrentCellRetentionWeight = 0.f;
};

/** Blueprint-tunable defaults for every existing tactical situation. */
USTRUCT(BlueprintType)
struct FDMTacticalStateWeightProfiles
{
    GENERATED_BODY()

    FDMTacticalStateWeightProfiles()
    {
        Visible = {1.f, 1.f, .5f, 0.f, .4f, .8f, 1.f, 1.f, .5f, .7f, 0.f, 0.f, 0.f, .2f};
        LostGrace = {1.f, .8f, .4f, .1f, .7f, .3f, 1.f, 1.f, .7f, .8f, 0.f, 0.f, .9f, .8f};
        PendingSwitch = {1.f, .7f, .3f, 0.f, .6f, .1f, 1.f, 1.f, 1.2f, .9f, 0.f, 0.f, .5f, 1.25f};
        Investigating = {.8f, .4f, .5f, .6f, .6f, .4f, 1.f, .8f, .8f, 1.f, .2f, 0.f, 1.3f, .25f};
        WatchingLastKnown = {.9f, .2f, .6f, .3f, 1.f, 0.f, 1.f, .9f, 1.f, 1.2f, .15f, 0.f, 1.1f, 1.1f};
        DisengagedRoam = {.2f, 0.f, .15f, 1.3f, .6f, .2f, .6f, 1.4f, .2f, .2f, 1.2f, .5f, 0.f, 0.f};
    }

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DeathMatch|Tactical Grid|State Profiles") FDMTacticalScoreWeights Visible;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DeathMatch|Tactical Grid|State Profiles") FDMTacticalScoreWeights LostGrace;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DeathMatch|Tactical Grid|State Profiles") FDMTacticalScoreWeights PendingSwitch;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DeathMatch|Tactical Grid|State Profiles") FDMTacticalScoreWeights Investigating;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DeathMatch|Tactical Grid|State Profiles") FDMTacticalScoreWeights WatchingLastKnown;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DeathMatch|Tactical Grid|State Profiles") FDMTacticalScoreWeights DisengagedRoam;

    const FDMTacticalScoreWeights& ForState(EDMTacticalEvaluationState State) const
    {
        switch (State)
        {
        case EDMTacticalEvaluationState::Visible: return Visible;
        case EDMTacticalEvaluationState::LostGrace: return LostGrace;
        case EDMTacticalEvaluationState::PendingSwitch: return PendingSwitch;
        case EDMTacticalEvaluationState::Investigating: return Investigating;
        case EDMTacticalEvaluationState::WatchingLastKnown: return WatchingLastKnown;
        default: return DisengagedRoam;
        }
    }
};

USTRUCT(BlueprintType)
struct FDMTacticalGridSettings
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DeathMatch|Tactical Grid") FDMTacticalGridBuildSettings Build;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DeathMatch|Tactical Grid") FDMTacticalGridRuntimeSettings Runtime;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DeathMatch|Tactical Grid") FDMTacticalStrategicSettings Strategic;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DeathMatch|Tactical Grid") FDMTacticalStateWeightProfiles StateProfiles;
};

USTRUCT(BlueprintType)
struct FDMTacticalConnection
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Cell") FDMTacticalCellId Neighbor;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Cell") float TravelCost = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Cell") EDMTacticalConnectionType Type = EDMTacticalConnectionType::Normal;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Cell") float MinimumClearance = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Cell") bool bHasValidNavPath = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Cell") bool bPartialPath = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Cell") float ActualPathLength = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Cell") float HeightDelta = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Cell") bool bAgentCapsulePassable = false;
};

USTRUCT(BlueprintType)
struct FDMTacticalSpawnFieldData
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Spawn") int32 Hops = MAX_int32;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Spawn") float PathDistance = MAX_flt;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Spawn") FVector NextDirectionTowardSpawn = FVector::ZeroVector;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Spawn") FDMTacticalCellId NextCellTowardSpawn;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Spawn") bool bInsideSpawnExclusionArea = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Spawn") float FrontProgress = .5f;
};

USTRUCT(BlueprintType)
struct FDMTacticalSpawnControlData
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Spawn Control") float Score = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Spawn Control") bool bCandidate = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Spawn Control") FVector ControlledApproachDirection = FVector::ZeroVector;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Spawn Control") uint8 RelatedEnemySpawnTeam = 255;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Spawn Control") int32 EnemySpawnHops = MAX_int32;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Spawn Control") float EnemySpawnPathDistance = MAX_flt;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Spawn Control") int32 ApproachRouteId = INDEX_NONE;
};

USTRUCT(BlueprintType)
struct FDMTacticalCellStaticData
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Cell") FDMTacticalCellId Id;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Cell") FBox Bounds = FBox(ForceInit);
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Cell") FVector RepresentativeLocation = FVector::ZeroVector;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Cell") EDMTacticalCellType CellType = EDMTacticalCellType::Blocked;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Cell") float WalkableRatio = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Cell") TArray<FDMTacticalConnection> Connections;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Cell") TArray<FVector> ValidSamples;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Cell") float AverageWallClearance = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Cell") float Openness = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Cell") float Height = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Cell") float AverageTravelCost = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Cell") float RelativeHeight = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Cell") FVector MajorOpenDirection = FVector::ZeroVector;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Cell") int32 NarrowConnectionCount = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Cell") bool bDeadEnd = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Cell") bool bJunction = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Cell") bool bReachableFromAnySpawn = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Cell") int32 ReachableTeamMask = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Cell") bool bTraversable = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Cell") int32 ConnectionCount = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Cell") int32 EscapeRouteCount = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Cell") int32 ValidNavSampleCount = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Strategic Cell") bool bNavMeshProjectionSucceeded = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Strategic Cell") bool bHasValidGridConnection = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Strategic Cell") bool bReachableFromTeamARepresentativeSpawn = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Strategic Cell") bool bReachableFromTeamBRepresentativeSpawn = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Strategic Cell") bool bPlayableStrategicCell = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Strategic Cell") TArray<EDMTacticalStrategicCellExclusionReason> StrategicExclusionReasons;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Cell") TMap<uint8, FDMTacticalSpawnFieldData> SpawnFieldsByTeam;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Cell") TMap<uint8, FDMTacticalSpawnControlData> SpawnControlByEnemyTeam;
};

USTRUCT(BlueprintType)
struct FDMTacticalCellDynamicData
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Cell") int32 FriendlyActorCount = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Cell") int32 FriendlyCombatantCount = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Cell") int32 FriendlyReservedGoalCount = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Cell") int32 PhysicalEnemyCount = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Cell") int32 KnownThreatCount = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Cell") int32 VisibleThreatCount = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Cell") int32 RememberedThreatCount = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Cell") int32 EngagementThreatCount = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Cell") int32 RecentDamageEvents = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Cell") int32 RecentFireEvents = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Cell") float CombatIntensity = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Cell") float ObservationConfidence = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Cell") float LastFriendlyVisitTime = -1.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Cell") float LastFriendlyObservationTime = -1.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Cell") float LastEnemySeenTime = -1.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Cell") float LastCombatTime = -1.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Cell") FVector AverageFocusDirection = FVector::ZeroVector;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Cell") TArray<int32> FocusDirectionOccupancy;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Cell") FVector ExpectedEnemyDirection = FVector::ZeroVector;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Cell") float EnemyApproachProbability = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Exploration") bool bUnknown = true;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Exploration") bool bFrontier = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Exploration") bool bRecentlyObserved = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Exploration") bool bKnown = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Exploration") bool bStale = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Exploration") bool bOccupied = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Exploration") bool bCovered = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Exploration") bool bInferredCovered = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Exploration") bool bContested = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Exploration") int32 AdjacentUnknownCellCount = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Exploration") int32 ConnectedUnknownRegionSize = 0;
};

/** Independent -1..1 personality axes. Positive meanings are named in each property tooltip. */
USTRUCT(BlueprintType)
struct FDMTacticalPersonality
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Tactical Personality", meta=(ClampMin="-1", ClampMax="1", UIMin="-1", UIMax="1", ToolTip="-1 prefers solo spacing; +1 prefers joining and reinforcing the formation.")) float FormationPreference = .35f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Tactical Personality", meta=(ClampMin="-1", ClampMax="1", UIMin="-1", UIMax="1", ToolTip="-1 prefers ambush/flank opportunities; +1 prefers direct combat opportunities.")) float CombatPreference = .4f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Tactical Personality", meta=(ClampMin="-1", ClampMax="1", UIMin="-1", UIMax="1", ToolTip="-1 stays on the known front; +1 values low-confidence unexplored areas.")) float ExplorationPreference = -.2f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Tactical Personality", meta=(ClampMin="-1", ClampMax="1", UIMin="-1", UIMax="1", ToolTip="-1 holds formation; +1 accepts supported advance.")) float Aggression = .2f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Tactical Personality", meta=(ClampMin="-1", ClampMax="1", UIMin="-1", UIMax="1", ToolTip="-1 reinforces existing coverage; +1 covers reasonable unwatched directions.")) float CoveragePreference = .1f;
};

/** Per-request facts supplied by the owning combat or roam system. This stores no transitions. */
USTRUCT(BlueprintType)
struct FDMTacticalEvaluationContext
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tactical Evaluation") EDMTacticalEvaluationState State = EDMTacticalEvaluationState::DisengagedRoam;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tactical Evaluation") bool bInCombat = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tactical Evaluation") bool bHasVisibleEnemy = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tactical Evaluation") bool bHasRememberedEnemy = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tactical Evaluation") bool bHasLastKnownThreatLocation = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tactical Evaluation") FVector LastKnownThreatLocation = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tactical Evaluation") bool bHasCurrentCell = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tactical Evaluation") FDMTacticalCellId CurrentCell;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tactical Evaluation") bool bHasCurrentSelection = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tactical Evaluation") FDMTacticalCellId CurrentSelectedCell;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tactical Evaluation") EDMTacticalPurpose CurrentPurpose = EDMTacticalPurpose::None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tactical Evaluation", meta=(ClampMin="0", ClampMax="1")) float HealthRisk = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tactical Evaluation", meta=(ClampMin="0", ClampMax="1")) float RecentDamagePressure = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tactical Evaluation") float Now = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tactical Evaluation") FDMTacticalPersonality Personality;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tactical Evaluation") TArray<FDMTacticalCellId> RecentRoamCells;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tactical Evaluation") TArray<float> RecentRoamTimes;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tactical Evaluation") FVector PreferredRoamDirection = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tactical Evaluation") bool bHoldRoamDirection = false;
};

USTRUCT(BlueprintType)
struct FDMTacticalPathResult
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Path") bool bReachable = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Path") TArray<FDMTacticalCellId> PathCells;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Path") int32 CellHopCount = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Path") float TotalTravelDistance = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Path") float EstimatedTravelSeconds = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Path") int32 NarrowCellCount = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Path") int32 DeadEndCellCount = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Path") int32 UnknownCellCount = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Path") int32 FrontierCellCount = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Path") int32 ContestedCellCount = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Path") int32 FriendlyOccupiedCellCount = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Path") int32 FriendlyReservedCellCount = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Path") int32 RecentlyFailedCellCount = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Path") float AverageRisk = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Path") float MaximumRisk = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Path") float PathExposure = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Path") float TotalPathCost = 0.f;
};

USTRUCT(BlueprintType)
struct FDMTacticalCellScore
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Score") FDMTacticalCellId CellId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Score") EDMTacticalPurpose Purpose = EDMTacticalPurpose::None;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Score") float FriendlyFormationScore = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Score") float CombatOpportunityScore = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Score") float AmbushScore = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Score") float ExplorationScore = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Score") float CoverageScore = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Score") float SupportedAdvanceScore = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Score") float IsolationRisk = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Score") float OvercrowdingPenalty = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Score") float TravelCost = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Score") float KnownEnemyExposure = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Score") float StaleVisitScore = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Score") float EnemyApproachProbabilityScore = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Score") float LastKnownThreatScore = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Score") float CurrentCellRetentionScore = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Score") float RecentlyFailedGridPenalty = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Score") float FrontierScore = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Score") float UnknownRegionSizeScore = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Score") float PreferredHopDistanceScore = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Score") float RecentRoamCellPenalty = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Score") float RouteRiskPenalty = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Score") float SpawnControlScore = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Score") float SpawnExclusionPenalty = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Score") int32 StrategicCellReservationCount = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Score") int32 StrategicCellCapacity = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Score") float StrategicReservationPenalty = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Score") int32 ApproachRouteUserCount = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Score") int32 ApproachRouteCapacity = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Score") float ApproachRouteCongestionPenalty = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Score") bool bApproachRouteCapacityRelaxed = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Score") float StableTieBreakScore = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Score") uint8 RelatedEnemySpawnTeam = 255;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Score") int32 ApproachRouteId = INDEX_NONE;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Score") FVector ControlledApproachDirection = FVector::ZeroVector;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Score") FDMTacticalPathResult Path;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Score") float TotalScore = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Score") EDMTacticalEvaluationState EvaluationState = EDMTacticalEvaluationState::DisengagedRoam;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Score") FDMTacticalScoreWeights StateWeights;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Score") FDMTacticalScoreWeights AppliedWeights;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Score") TArray<FString> TopPositiveContributors;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Score") TArray<FString> TopNegativeContributors;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Score") FString Reason;
};

/**
 * Low-resolution noncombat selection returned by Tactical Grid.  This intentionally contains
 * bounds/identity rather than an exact FVector goal; the Roam provider must resolve a reachable
 * NavMesh point inside the same layer and connected region.
 */
USTRUCT(BlueprintType)
struct FDMTacticalStrategicRegionSelection
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Strategic Region") FDMTacticalCellId CellId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Strategic Region") int32 LayerId = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Strategic Region") int32 NavRegionId = INDEX_NONE;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Strategic Region") FBox Bounds = FBox(ForceInit);
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Strategic Region") EDMTacticalPurpose Purpose = EDMTacticalPurpose::None;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Strategic Region") float Score = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Strategic Region") FString SelectionReason;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Strategic Region") float TravelCost = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Strategic Region") int32 FriendlyCount = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Strategic Region") int32 StrategicCrowdingCount = 0;
    /** Full diagnostic score retained for path metadata, debug and exact-goal reservation. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Strategic Region") FDMTacticalCellScore ScoreDetails;
};

USTRUCT(BlueprintType)
struct FDMTacticalFormationSnapshot
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Formation") FVector FriendlyFormationCenter = FVector::ZeroVector;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Formation") FVector RobustFormationCenter = FVector::ZeroVector;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Formation") float FormationRadius = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Formation") int32 FriendlyCellCount = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Formation") float NearestFriendlyDistance = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Formation") float LocalFriendlyDensity = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Formation") int32 IsolatedFriendlyCount = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Formation") TArray<FDMTacticalCellId> CombatCells;
    float BuiltTime = -1.f;
};

USTRUCT(BlueprintType)
struct FDMTacticalCellReservation
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Reservation") uint8 Team = 255;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Reservation") TWeakObjectPtr<AActor> Actor;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Reservation") FDMTacticalCellId CellId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Reservation") EDMTacticalPurpose Purpose = EDMTacticalPurpose::None;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Reservation") float ExpirationTime = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Reservation") EDMTacticalEvaluationState EvaluationState = EDMTacticalEvaluationState::DisengagedRoam;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Reservation") float Score = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Reservation") FDMTacticalScoreWeights StateWeights;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Reservation") FDMTacticalScoreWeights AppliedWeights;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Reservation") FString ScoreReason;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Reservation") int32 ApproachRouteId = INDEX_NONE;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Reservation") uint8 RelatedEnemySpawnTeam = 255;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Reservation") FDMTacticalPathResult Path;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Reservation") FVector ReservedWorldLocation = FVector::ZeroVector;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Reservation") bool bHasReservedWorldLocation = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Reservation") float GoalReservationRadius = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Reservation") int32 StrategicCellReservationCountAtSelection = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Reservation") int32 StrategicCellCapacity = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Reservation") float StrategicReservationPenalty = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Reservation") int32 ApproachRouteUserCountAtSelection = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Reservation") int32 ApproachRouteCapacity = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Reservation") float ApproachRouteCongestionPenalty = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Reservation") bool bApproachRouteCapacityRelaxed = false;
};

