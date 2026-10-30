#pragma once

#include "CoreMinimal.h"
#include "AI/Navigation/NavigationTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "DMTacticalNavRegionTypes.h"
#include "DMTacticalNavRegionSubsystem.generated.h"

class ANavigationData;
class ARecastNavMesh;
class UDMTacticalEventProfile;

/**
 * World-owned strategic summary of the live Recast graph. It never issues MoveTo and never
 * chooses an exact combat position; combat placement remains purpose-specific EQS work.
 */
UCLASS(meta=(DisplayName="DeathMatch Tactical Nav Regions"))
class SKYSEOUL_API UDMTacticalNavRegionSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableInEditor() const override { return false; }

	UFUNCTION(BlueprintPure, Category="DeathMatch|Tactical Nav Region") bool IsReady() const { return BuildState == EDMTacticalNavRegionBuildState::Ready; }
	UFUNCTION(BlueprintPure, Category="DeathMatch|Tactical Nav Region") EDMTacticalNavRegionBuildState GetBuildState() const { return BuildState; }
	UFUNCTION(BlueprintPure, Category="DeathMatch|Tactical Nav Region") int32 GetNavigationRevision() const { return NavigationRevision; }
	UFUNCTION(BlueprintCallable, Category="DeathMatch|Tactical Nav Region") void RequestRebuild();

	const FDMTacticalNavRegionSettings& GetSettings() const { return Settings; }
	const TArray<FDMTacticalNavRegionStaticData>& GetRegions() const { return Regions; }
	const FDMTacticalNavRegionStaticData* FindRegion(int32 RegionId) const;

	UFUNCTION(BlueprintCallable, Category="DeathMatch|Tactical Nav Region") int32 GetRegionAtLocation(const FVector& Location) const;
	UFUNCTION(BlueprintCallable, Category="DeathMatch|Tactical Nav Region") bool GetRegionRuntimeData(int32 RegionId, uint8 Team, FDMTacticalNavRegionRuntimeData& OutData) const;
	UFUNCTION(BlueprintCallable, Category="DeathMatch|Tactical Nav Region") void GetRegionNeighbors(int32 RegionId, TArray<FDMTacticalNavRegionConnection>& OutNeighbors) const;
	UFUNCTION(BlueprintCallable, Category="DeathMatch|Tactical Nav Region") void GetValidNavPoints(int32 RegionId, TArray<FDMTacticalNavPoint>& OutPoints) const;
	UFUNCTION(BlueprintCallable, Category="DeathMatch|Tactical Nav Region") bool FindRegionPath(int32 StartRegionId, int32 EndRegionId, TArray<int32>& OutRegionPath, float& OutCost) const;

	UFUNCTION(BlueprintCallable, Category="DeathMatch|Tactical Nav Region|Events") void ReportTacticalEvent(const FDMTacticalRegionEvent& EventData);
	UFUNCTION(BlueprintCallable, Category="DeathMatch|Tactical Nav Region|Events") void ReportCombatEvent(const FVector& Location, uint8 SourceTeam, float Strength = 1.f, AActor* SourceActor = nullptr);
	UFUNCTION(BlueprintCallable, Category="DeathMatch|Tactical Nav Region|Events") void ReportThreatEvent(const FVector& Location, uint8 SourceTeam, float Strength = 1.f, AActor* SourceActor = nullptr);
	UFUNCTION(BlueprintCallable, Category="DeathMatch|Tactical Nav Region|Events") void UpdateActorPresence(AActor* Actor, uint8 Team, int32 RegionId = -1, bool bInCombat = false);
	UFUNCTION(BlueprintCallable, Category="DeathMatch|Tactical Nav Region|Events") void RemoveActorPresence(AActor* Actor);
	void GetPresentFriendlyActors(uint8 Team, AActor* IgnoreActor, TArray<AActor*>& OutActors) const;
	UFUNCTION(BlueprintCallable, Category="DeathMatch|Tactical Nav Region|Events") void MarkRegionExplored(int32 RegionId, uint8 Team, float Confidence = 1.f);

	UFUNCTION(BlueprintCallable, Category="DeathMatch|Tactical Nav Region|Failures") void ReportNavPointFailure(int32 RegionId, const FVector& Location, EDMTacticalNavFailureReason Reason);
	UFUNCTION(BlueprintCallable, Category="DeathMatch|Tactical Nav Region|Failures") void ReportConnectionFailure(int32 FromRegionId, int32 ToRegionId, EDMTacticalNavFailureReason Reason);
	bool IsNavPointCoolingDown(int32 RegionId, const FVector& Location, float Now) const;

	static float CalculateEventStrength(float BaseStrength, int32 Hops, float DistanceFalloff,
		float Age, float Lifetime, float TimeDecay, float ConnectionModifier = 1.f);

private:
	friend class FDMTacticalNavRegionCoreTest;
	friend class FDMTacticalNavRegionRuntimeTest;

	struct FPolyRecord
	{
		uint64 Ref = 0;
		FVector Center = FVector::ZeroVector;
		FBox Bounds = FBox(ForceInit);
		float Area = 0.f;
		TArray<FVector> Vertices;
		TArray<uint64> Neighbors;
		TArray<FNavigationPortalEdge> Portals;
		int32 RegionId = INDEX_NONE;
	};
	struct FPresenceRecord
	{
		TWeakObjectPtr<AActor> Actor;
		uint8 Team = 255;
		int32 RegionId = INDEX_NONE;
		bool bInCombat = false;
		float LastUpdateTime = 0.f;
	};
	struct FExplorationRecord { float Confidence = 0.f; float LastObservedTime = 0.f; };
	struct FNavPointFailure { int32 RegionId = INDEX_NONE; FVector Location = FVector::ZeroVector; float ExpirationTime = 0.f; int32 Count = 0; };
	struct FConnectionFailure { int32 From = INDEX_NONE; int32 To = INDEX_NONE; float Score = 0.f; float BlockedUntil = 0.f; };

	UFUNCTION() void HandleNavigationGenerationFinished(ANavigationData* NavData);
	bool GatherBuildConfiguration();
	bool GatherPolygons(ARecastNavMesh& Recast);
	void PartitionPolygons();
	void MergeSmallRegions();
	void BuildRegionData(ARecastNavMesh& Recast);
	void BuildRegionConnections(ARecastNavMesh& Recast);
	void BuildSpawnReachability();
	void GenerateRegionSamples(ARecastNavMesh& Recast, FDMTacticalNavRegionStaticData& Region);
	bool HasStaticCapsuleClearance(const FVector& GroundLocation) const;
	float MeasureWallClearance(const FVector& GroundLocation) const;
	void RebuildRuntimeScores(float Now);
	void ApplyEventToRuntime(const FDMTacticalRegionEvent& Event, float Now);
	FDMTacticalNavRegionRuntimeData& FindOrAddRuntime(uint8 Team, int32 RegionId);
	const FDMTacticalNavRegionRuntimeData* FindRuntime(uint8 Team, int32 RegionId) const;
	float GetConnectionModifier(int32 FromRegion, const FDMTacticalNavRegionConnection& Connection, float Now) const;
	void PurgeTransientData(float Now);
#if !UE_BUILD_SHIPPING
	void DrawDebugRegions() const;
#endif

	UPROPERTY(VisibleAnywhere, Category="DeathMatch|Tactical Nav Region|Debug")
	EDMTacticalNavRegionBuildState BuildState = EDMTacticalNavRegionBuildState::WaitingForNavigation;
	UPROPERTY(VisibleAnywhere, Category="DeathMatch|Tactical Nav Region|Debug")
	FDMTacticalNavRegionSettings Settings;
	UPROPERTY(VisibleAnywhere, Category="DeathMatch|Tactical Nav Region|Debug")
	int32 NavigationRevision = 0;

	FBox AnalysisBounds = FBox(ForceInit);
	TMap<EDMTacticalRegionEventType, TWeakObjectPtr<UDMTacticalEventProfile>> EventProfiles;
	TArray<FPolyRecord> Polygons;
	TMap<uint64, int32> PolyIndexByRef;
	TArray<FDMTacticalNavRegionStaticData> Regions;
	TMap<int32, int32> RegionIndexById;
	TMap<uint8, TMap<int32, FDMTacticalNavRegionRuntimeData>> RuntimeByTeam;
	TArray<FDMTacticalRegionEvent> ActiveEvents;
	TMap<TWeakObjectPtr<AActor>, FPresenceRecord> Presence;
	TMap<uint8, TMap<int32, FExplorationRecord>> ExplorationByTeam;
	TArray<FNavPointFailure> NavPointFailures;
	TArray<FConnectionFailure> ConnectionFailures;
	float NextNavigationRetryTime = 0.f;
	float NextEventUpdateTime = 0.f;
};
