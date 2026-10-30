// Fill out your copyright notice in the Description page of Project Settings.


#include "Controller/AIC_EnemyControllerBase.h"
#include "Kismet/GameplayStatics.h"


void AAIC_EnemyControllerBase::BeginPlay()
{
	Super::BeginPlay();

	/*if (EnemyTree != nullptr)
	{
		RunBehaviorTree(EnemyTree);

		UseBlackboard(BlackBoardData, BlackBoardComponent);
	}*/
}

AAIC_EnemyControllerBase::AAIC_EnemyControllerBase()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AAIC_EnemyControllerBase::InitTargetData(AActor* Target)
{
	Target = nullptr;
}
