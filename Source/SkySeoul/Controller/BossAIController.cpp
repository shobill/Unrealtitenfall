// Fill out your copyright notice in the Description page of Project Settings.


#include "Controller/BossAIController.h"

ABossAIController::ABossAIController()
{
}

void ABossAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	if (!BehaviorTree)
		return;
	RunBehaviorTree(BehaviorTree);
}
