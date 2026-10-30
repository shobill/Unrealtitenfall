#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_DM_SelectMoveLocation.generated.h"

class ADMMovePoint;

/**
 * Gather DMMovePoints, apply filters + weights, pick one into Blackboard.
 * Weight = BaseWeight / (1+NearbyPawns) / (1+ZoneOccupancy)
 */
UCLASS(meta=(DeprecatedNode, DeprecationMessage="DM Select Move Location is retained for asset compatibility. Use DM Find Combat Position for combat or DM Select Roam Location for roam."))
class SKYSEOUL_API UBTTask_DM_SelectMoveLocation : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_DM_SelectMoveLocation();

protected:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

	virtual ADMMovePoint* SelectMovePoint(
		UBehaviorTreeComponent& OwnerComp,
		const TArray<ADMMovePoint*>& Candidates) const;

	void GatherMovePoints(UWorld* World, TArray<ADMMovePoint*>& OutPoints) const;
	float ComputePointWeight(
		UWorld* World,
		APawn* SelfPawn,
		ADMMovePoint* Point,
		const TMap<uint8, int32>& ZoneOccupancy) const;
	void BuildZoneOccupancy(UWorld* World, const TArray<ADMMovePoint*>& AllPoints, TMap<uint8, int32>& OutOccupancy) const;
	int32 CountPawnsNearLocation(UWorld* World, const FVector& Location, float Radius, APawn* IgnorePawn) const;
	static ADMMovePoint* PickWeightedRandom(const TArray<ADMMovePoint*>& Points, const TArray<float>& Weights);

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector MoveGoalLocationKey;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector HasMoveGoalKey;

	UPROPERTY(EditAnywhere, Category = "DeathMatch|MovePoint")
	uint8 RequiredZoneId = 255;

	UPROPERTY(EditAnywhere, Category = "DeathMatch|MovePoint")
	bool bOnlyEnabledPoints = true;

	/** Skip points closer than this to self. */
	UPROPERTY(EditAnywhere, Category = "DeathMatch|MovePoint|Weight", meta = (ClampMin = "0.0"))
	float MinDistanceFromSelf = 800.0f;

	/** Skip the last chosen point when possible. */
	UPROPERTY(EditAnywhere, Category = "DeathMatch|MovePoint|Weight")
	bool bExcludeLastMovePoint = true;

	/** Pawn density radius around a point (lower weight if crowded). */
	UPROPERTY(EditAnywhere, Category = "DeathMatch|MovePoint|Weight", meta = (ClampMin = "0.0"))
	float NearbyPawnRadius = 1200.0f;

	/** How strongly nearby pawns reduce weight. Final /= (1 + Count * Scale). */
	UPROPERTY(EditAnywhere, Category = "DeathMatch|MovePoint|Weight", meta = (ClampMin = "0.0"))
	float NearbyPawnPenaltyScale = 1.0f;

	/** Zone occupancy penalty. Final /= (1 + ZoneCount * Scale). ZoneId 255 ignored. */
	UPROPERTY(EditAnywhere, Category = "DeathMatch|MovePoint|Weight", meta = (ClampMin = "0.0"))
	float ZoneOccupancyPenaltyScale = 0.75f;

	/** Radius used when attributing a pawn to a zone (nearest enabled point's ZoneId). */
	UPROPERTY(EditAnywhere, Category = "DeathMatch|MovePoint|Weight", meta = (ClampMin = "0.0"))
	float ZoneAttributionRadius = 2500.0f;
};
