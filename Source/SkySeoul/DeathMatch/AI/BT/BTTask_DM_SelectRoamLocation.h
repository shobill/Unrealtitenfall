#pragma once
#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "BTTask_DM_SelectRoamLocation.generated.h"
UCLASS()
class SKYSEOUL_API UBTTask_DM_SelectRoamLocation : public UBTTaskNode
{
	GENERATED_BODY()
public:
	UBTTask_DM_SelectRoamLocation();
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	UPROPERTY(EditAnywhere, Category="Blackboard") FBlackboardKeySelector SelectedAreaKey;
	UPROPERTY(EditAnywhere, Category="Blackboard") FBlackboardKeySelector MoveGoalKey;
	UPROPERTY(EditAnywhere, Category="Roam", meta=(ClampMin="1", ClampMax="12")) int32 MaxAttempts = 3;
	UPROPERTY(EditAnywhere, Category="Roam", meta=(ClampMin="0")) float FailureCooldown = 3.f;
};
