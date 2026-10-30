#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_DM_ClearCombatTarget.generated.h"

UCLASS()
class SKYSEOUL_API UBTTask_DM_ClearCombatTarget : public UBTTaskNode
{
	GENERATED_BODY()
public:
	UBTTask_DM_ClearCombatTarget();
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};
