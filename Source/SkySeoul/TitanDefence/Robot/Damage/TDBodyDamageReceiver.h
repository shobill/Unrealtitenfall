#pragma once

#include "CoreMinimal.h"
#include "TitanDefence/Robot/Damage/TDDamageTypes.h"
#include "UObject/Interface.h"
#include "TDBodyDamageReceiver.generated.h"

/** Minimal contract used by damage resolvers to submit final body damage. */
UINTERFACE(BlueprintType, meta = (CannotImplementInterfaceInBlueprint))
class SKYSEOUL_API UTDBodyDamageReceiver : public UInterface
{
	GENERATED_BODY()
};

class SKYSEOUL_API ITDBodyDamageReceiver
{
	GENERATED_BODY()

public:
	/** Applies one resolved positive body-damage value without owning part-health policy. */
	UFUNCTION(BlueprintCallable, Category = "Titan Defence|Damage")
	virtual bool ApplyResolvedBodyDamage(float DamageAmount, const FTDDamageResult& DamageResult) = 0;
};

