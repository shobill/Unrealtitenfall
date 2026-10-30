// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectExecutionCalculation.h"
#include "SkySeoulEffectExecution.generated.h"

/**
 * 
 */
UCLASS()
class SKYSEOUL_API USkySeoulEffectExecution : public UGameplayEffectExecutionCalculation
{
	GENERATED_BODY()
public:

	USkySeoulEffectExecution();
protected:

	virtual void Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams, FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const override;

	
};
