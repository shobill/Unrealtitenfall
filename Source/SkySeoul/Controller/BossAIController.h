// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "BossAIController.generated.h"

/**
 * 
 */
UCLASS()
class SKYSEOUL_API ABossAIController : public AAIController
{
	GENERATED_BODY()

	ABossAIController();
public:

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	class UBehaviorTree* BehaviorTree;


public:

	virtual void OnPossess(APawn* InPawn) override;
};
