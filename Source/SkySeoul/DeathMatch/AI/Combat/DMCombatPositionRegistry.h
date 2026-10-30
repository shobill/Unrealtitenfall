#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "DMCombatPositionRegistry.generated.h"

class ADeathMatchRobotAIController;
struct FDMPositionNeighbor
{
    TWeakObjectPtr<AActor> Owner;
    FVector Location = FVector::ZeroVector;
    bool bReserved = false;
    float Radius = 0;
    float Expires = 0;
    FVector FireLaneEnd = FVector::ZeroVector;
    bool bHasFireLane = false;
};

/** Game-thread claims are immediately visible, including within the same frame. */
UCLASS()
class SKYSEOUL_API UDMCombatPositionRegistry : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    void Gather(AActor* Self, float Now, TArray<FDMPositionNeighbor>& Out);
	/** Current friendly pawn positions only; excludes all reservation claims. */
	void GatherFriendlyOccupants(AActor* Self, TArray<FDMPositionNeighbor>& Out);
    bool Reserve(AActor* Owner, const TArray<FVector>& Goals, float Radius, float Now, float Lifetime);
    void Release(AActor* Owner);
    bool IsClaimed(AActor* Self, const FVector& Goal, float Radius, float Now);
    static bool Overlaps(const FVector& A, const FVector& B, float Radius);
private:
    void Prune(float Now);
    void RefreshOccupants();
    uint64 SnapshotFrame = MAX_uint64;
    TArray<FDMPositionNeighbor> Occupants;
    TArray<FDMPositionNeighbor> Claims;
};

