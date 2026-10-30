#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "BTTask_DM_ReserveCombatPosition.generated.h"

UCLASS()
class SKYSEOUL_API UBTTask_DM_ReserveCombatPosition : public UBTTaskNode
{
	GENERATED_BODY()
public:
	UBTTask_DM_ReserveCombatPosition();
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	UPROPERTY(EditAnywhere, Category="Blackboard") FBlackboardKeySelector LocationKey;
	UPROPERTY(EditAnywhere, Category="Reservation", meta=(ClampMin="1")) float Radius = 150.f;
	UPROPERTY(EditAnywhere, Category="Reservation", meta=(ClampMin="0.1")) float Lifetime = 3.f;
};
