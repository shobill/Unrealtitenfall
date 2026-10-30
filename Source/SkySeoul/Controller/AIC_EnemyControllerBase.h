// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "AIC_EnemyControllerBase.generated.h"

/**
 * 
 */
UCLASS()
class SKYSEOUL_API AAIC_EnemyControllerBase : public AAIController
{
	GENERATED_BODY()

	// ----------------------------------------------------------------------- //
		// Funcs

public:
	AAIC_EnemyControllerBase();

	UFUNCTION(BlueprintCallable)
	void InitTargetData(AActor* Target);
protected:
	virtual void BeginPlay() override;

private:

	// ----------------------------------------------------------------------- //
		// Value

public:
	/*UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UBlackboardData* BlackBoardData;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlackBoardRef")
	UBlackboardComponent* BlackBoardRef;
	*/

	

protected:


private:
	/*UPROPERTY(EditAnywhere)
	UBehaviorTree* EnemyTree;*/
	
};
