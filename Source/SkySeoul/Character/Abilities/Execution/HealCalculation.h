// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectExecutionCalculation.h"
#include "HealCalculation.generated.h"

/**
 * 
 */
UCLASS()
class SKYSEOUL_API UHealCalculation : public UGameplayEffectExecutionCalculation
{
	GENERATED_BODY()
public:
	UHealCalculation();
protected:

	virtual void Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams, FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const override;

};
