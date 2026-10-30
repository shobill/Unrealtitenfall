// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Tasks/BTTask_BlackboardBase.h"
#include "BTTask_RobotAttack.generated.h"

/**
 * 
 */
UCLASS(config = Game)
class SKYSEOUL_API UBTTask_RobotAttack : public UBTTask_BlackboardBase
{
	GENERATED_BODY()

public:
	UBTTask_RobotAttack();

protected:

	UPROPERTY(config, Category = Node, EditAnywhere, meta = (ClampMin = "0.0"))
	float Precision;

private:
	float PrecisionDot;

public:
	virtual void PostInitProperties() override;
	virtual void PostLoad() override;

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
};
