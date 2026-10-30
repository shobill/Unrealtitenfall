#pragma once

#include "CoreMinimal.h"
#include "DMTacticalNavRegionTypes.generated.h"

UENUM(BlueprintType)
enum class EDMTacticalNavRegionBuildState : uint8
{
	WaitingForNavigation,
	GatheringPolygons,
	BuildingRegions,
	BuildingConnections,
	MarkingReachability,
	Ready,
	Failed
};

UENUM(BlueprintType)
enum class EDMTacticalNavConnectionType : uint8
{
	Open,
	Narrow,
	Step,
	OffMeshLink
};

UENUM(BlueprintType)
enum class EDMTacticalRegionEventType : uint8
{
	Combat,
	Threat,
	FriendlyPresence,
	EnemyPresence,
	Exploration,
	SpawnPressure
};

UENUM(BlueprintType)
enum class EDMTacticalNavFailureReason : uint8
{
	None,
	NoCompletePath,
	MoveRequestRejected,
	Stuck,
	StaticObstruction
};

USTRUCT(BlueprintType)
struct FDMTacticalNavRegionSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Region Generation", meta=(ClampMin="300", Units="cm"))
	float TargetRegionSize = 1200.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Region Generation", meta=(ClampMin="10000"))
	float MinimumRegionArea = 250000.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Region Generation", meta=(ClampMin="10000"))
	float MaximumRegionArea = 2250000.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Region Generation", meta=(ClampMin="0.75", ClampMax="1.0"))
	float NavigationTestCapsuleRadiusScale = .9f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Region Generation", meta=(ClampMin="0.75", ClampMax="1.0"))
	float NavigationTestCapsuleHalfHeightScale = .9f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Region Generation", meta=(ClampMin="2", ClampMax="32"))
	int32 RegionSamplePointCount = 10;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category="Region Generation", meta=(ClampMin="40", Units="cm"))
	float NarrowPortalWidth = 140.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category="Region Generation", meta=(ClampMin="10", Units="cm"))
	float RegionHeightBoundary = 180.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Events", meta=(ClampMin="0.05", Units="s"))
	float EventUpdateInterval = .25f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Events", meta=(ClampMin="0"))
	int32 DefaultPropagationDistance = 3;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Events", meta=(ClampMin="0.01", ClampMax="1.0"))
	float DefaultDistanceFalloff = .65f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Events", meta=(ClampMin="0.1", Units="s"))
	float DefaultEventLifetime = 6.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Events", meta=(ClampMin="0.01"))
	float DefaultTimeDecay = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Events", meta=(ClampMin="0.0"))
	float MinimumPropagationScore = .02f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category="Performance", meta=(ClampMin="0.1", Units="ms"))
	float MaximumBuildTimePerTick = 2.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category="Performance", meta=(ClampMin="1"))
	int32 MaximumRegionOperationsPerTick = 256;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category="Performance", meta=(ClampMin="1"))
	int32 MaximumEventsProcessedPerTick = 64;
};

USTRUCT(BlueprintType)
struct FDMTacticalNavPoint
{
	GENERATED_BODY()
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Nav Region") FVector Location = FVector::ZeroVector;
	UPROPERTY(VisibleAnywhere, Category="Tactical Nav Region") uint64 NavPolyRef = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Nav Region") int32 RegionId = INDEX_NONE;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Nav Region") float WallClearance = 0.f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Nav Region") float Openness = 0.f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Nav Region") float LastFailureTime = -BIG_NUMBER;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Nav Region") int32 TemporaryFailureCount = 0;
};

USTRUCT(BlueprintType)
struct FDMTacticalNavRegionConnection
{
	GENERATED_BODY()
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Nav Region") int32 NeighborRegionId = INDEX_NONE;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Nav Region") float TravelCost = 0.f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Nav Region") FVector EntryPoint = FVector::ZeroVector;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Nav Region") FVector ExitPoint = FVector::ZeroVector;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Nav Region") float PortalWidth = 0.f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Nav Region") float MinimumClearance = 0.f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Nav Region") EDMTacticalNavConnectionType ConnectionType = EDMTacticalNavConnectionType::Open;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Nav Region") bool bPassable = true;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Nav Region") float TemporaryFailureScore = 0.f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Nav Region") float TemporaryBlockedUntil = 0.f;
};

USTRUCT(BlueprintType)
struct FDMTacticalNavRegionStaticData
{
	GENERATED_BODY()
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Nav Region") int32 RegionId = INDEX_NONE;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Nav Region") FBox Bounds = FBox(ForceInit);
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Nav Region") FVector RepresentativeLocation = FVector::ZeroVector;
	UPROPERTY(VisibleAnywhere, Category="Tactical Nav Region") TArray<uint64> NavPolyRefs;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Nav Region") TArray<FDMTacticalNavPoint> ValidNavPoints;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Nav Region") TArray<FDMTacticalNavRegionConnection> Neighbors;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Nav Region") float Area = 0.f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Nav Region") float ApproximateDiameter = 0.f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Nav Region") float AverageOpenness = 0.f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Nav Region") bool bNarrow = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Nav Region") bool bDeadEnd = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Nav Region") bool bIsolated = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Nav Region") TArray<uint8> ReachableTeams;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Nav Region") TMap<uint8, float> TeamSpawnPathCosts;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Nav Region") int32 NavigationRevision = 0;
};

USTRUCT(BlueprintType)
struct FDMTacticalRegionEvent
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tactical Event") EDMTacticalRegionEventType EventType = EDMTacticalRegionEventType::Combat;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tactical Event") uint8 SourceTeam = 255;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tactical Event") TObjectPtr<AActor> SourceActor = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tactical Event") int32 OriginRegionId = INDEX_NONE;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tactical Event", meta=(ClampMin="0")) float BaseStrength = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tactical Event", meta=(ClampMin="0")) int32 PropagationDistance = 3;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tactical Event", meta=(ClampMin="0.01", ClampMax="1.0")) float DistanceFalloff = .65f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tactical Event", meta=(ClampMin="0.1", Units="s")) float Lifetime = 6.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tactical Event", meta=(ClampMin="0.01")) float TimeDecay = 1.f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Event") float Timestamp = 0.f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Event") FGuid EventId;
};

USTRUCT(BlueprintType)
struct FDMTacticalNavRegionRuntimeData
{
	GENERATED_BODY()
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Event") float CombatScore = 0.f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Event") float ThreatScore = 0.f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Event") float FriendlyPresenceScore = 0.f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Event") float EnemyPresenceScore = 0.f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Event") float ExplorationScore = 0.f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Event") float SpawnPressureScore = 0.f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactical Event") float LastUpdatedTime = 0.f;
};

