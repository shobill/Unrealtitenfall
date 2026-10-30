#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_DM_DisengageCombat.generated.h"

UCLASS()
class SKYSEOUL_API UBTTask_DM_DisengageCombat : public UBTTaskNode
{
	GENERATED_BODY()
public:
	UBTTask_DM_DisengageCombat();
protected:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};
