#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectExecutionCalculation.h"
#include "TDRobotBodyDamageExecutionCalculation.generated.h"

/** Applies the robot body-damage SetByCaller value to shared character Health. */
UCLASS(BlueprintType, meta = (DisplayName = "TD Robot Body Damage"))
class SKYSEOUL_API UTDRobotBodyDamageExecutionCalculation
	: public UGameplayEffectExecutionCalculation
{
	GENERATED_BODY()

public:
	virtual void Execute_Implementation(
		const FGameplayEffectCustomExecutionParameters& ExecutionParams,
		FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const override;
};
