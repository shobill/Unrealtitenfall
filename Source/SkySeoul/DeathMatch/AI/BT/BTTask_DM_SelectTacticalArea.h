#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "BTTask_DM_SelectTacticalArea.generated.h"

class ADMTacticalAreaVolume;

UCLASS()
class SKYSEOUL_API UBTTask_DM_SelectTacticalArea : public UBTTaskNode
{
	GENERATED_BODY()
public:
	UBTTask_DM_SelectTacticalArea();
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

	static float CalculateDangerFit(float VolumeDanger, float PreferredDanger, float DangerTolerance);
	static float CalculateSelectionWeight(float VolumeDanger, float PreferredDanger, float DangerTolerance,
		int32 FriendlyCount, int32 IncomingCount, float DistanceCm, bool bRecent, bool bCurrent,
		float BaseExplorationWeight, float DangerPreferenceWeight, float FriendlyCrowdingWeight,
		float IncomingAIWeight, float DistanceWeight, float RecentVolumePenalty,
		float CurrentVolumePenalty, float MinimumSelectionWeight);
	static int32 SelectWeightedIndex(const TArray<float>& Weights, FRandomStream& Stream);
	static float CalculatePersonalDangerBias(int32 StableSeed, float BiasRange);

	UPROPERTY(EditAnywhere, Category="Blackboard") FBlackboardKeySelector SelectedAreaKey;
	UPROPERTY(EditAnywhere, Category="Roam|Scoring", meta=(ClampMin="0", ClampMax="10")) float PreferredDanger = 6.f;
	UPROPERTY(EditAnywhere, Category="Roam|Scoring", meta=(ClampMin="0.01")) float DangerTolerance = 4.f;
	UPROPERTY(EditAnywhere, Category="Roam|Scoring", meta=(ClampMin="0")) float DangerPreferenceWeight = 4.f;
	UPROPERTY(EditAnywhere, Category="Roam|Scoring", meta=(ClampMin="0")) float BaseExplorationWeight = 1.f;
	UPROPERTY(EditAnywhere, Category="Roam|Scoring", meta=(ClampMin="0.001")) float MinimumSelectionWeight = .05f;
	UPROPERTY(EditAnywhere, Category="Roam|Scoring", meta=(ClampMin="0", ClampMax="10")) float PersonalDangerBiasRange = 1.f;
	UPROPERTY(EditAnywhere, Category="Roam|Scoring", meta=(ClampMin="0")) float FriendlyCrowdingWeight = .5f;
	UPROPERTY(EditAnywhere, Category="Roam|Scoring", meta=(ClampMin="0")) float IncomingAIWeight = .75f;
	UPROPERTY(EditAnywhere, Category="Roam|Scoring", meta=(ClampMin="0")) float DistanceWeight = .15f;
	UPROPERTY(EditAnywhere, Category="Roam|Scoring", meta=(ClampMin="0")) float RecentVolumePenalty = 2.f;
	UPROPERTY(EditAnywhere, Category="Roam|Scoring", meta=(ClampMin="0")) float CurrentVolumePenalty = 2.f;

};
