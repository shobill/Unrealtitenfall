// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "GameplayTagContainer.h"
#include "RobotAIController.generated.h"

class ACharacterBase;

/**
 * 
 */
UCLASS()
class SKYSEOUL_API ARobotAIController : public AAIController
{
	GENERATED_BODY()
public:
	ARobotAIController();
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	class UBehaviorTree* BehaviorTree;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	class UAIPerceptionComponent* AIPerceptionComponent;
	UPROPERTY(EditAnywhere)
	class UAISenseConfig_Sight* SightConfig;
	UPROPERTY(EditAnywhere)
	class UAISenseConfig_Damage* DamageConfig;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector SenseLocation;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	ACharacterBase* OwnerCharacter;

public:
	virtual void OnPossess(APawn* InPawn) override;

	void SetRobotGoalLocation(AActor* Caller);
	void SetRobotGoalLocation(FVector Location);
	void SetOwnerCharacter(AActor* OwnerCharacterActor);

	void SetPerceptionSystem();


};
