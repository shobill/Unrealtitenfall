#pragma once
#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_DM_ReleaseCombatPosition.generated.h"
UCLASS()
class SKYSEOUL_API UBTTask_DM_ReleaseCombatPosition : public UBTTaskNode
{
	GENERATED_BODY()
public:
	UBTTask_DM_ReleaseCombatPosition();
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};
