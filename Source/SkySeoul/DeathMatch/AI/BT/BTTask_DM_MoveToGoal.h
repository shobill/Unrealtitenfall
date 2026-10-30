#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "DeathMatch/AI/Move/DMMoveTypes.h"
#include "BTTask_DM_MoveToGoal.generated.h"

UCLASS()
class SKYSEOUL_API UBTTask_DM_MoveToGoal : public UBTTaskNode
{
	GENERATED_BODY()
public:
	UBTTask_DM_MoveToGoal();
	virtual uint16 GetInstanceMemorySize() const override;
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

	UPROPERTY(EditAnywhere, Category="Blackboard") FBlackboardKeySelector MoveGoalKey;
	UPROPERTY(EditAnywhere, Category="Move", meta=(ClampMin="1")) float AcceptanceRadius = 100.f;
	UPROPERTY(EditAnywhere, Category="Move") EDMMoveRequestOwner RequestOwner = EDMMoveRequestOwner::Combat;
	UPROPERTY(EditAnywhere, Category="Move") int32 Priority = DMMovePriority::Combat;
	UPROPERTY(EditAnywhere, Category="Move") bool bCombatMovement = true;
	UPROPERTY(EditAnywhere, Category="Move", meta=(ClampMin="0")) float RoamFailureCooldown = 3.f;

};
