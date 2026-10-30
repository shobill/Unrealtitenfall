#pragma once
#include "DeathMatch/Abilities/DMGameplayAbility_Fire.h"
#include "DMAdaptiveFireTestAbility.generated.h"

/** Deterministic ExecuteFire failure injection for the real notify/commit path. */
UCLASS(Transient, NotBlueprintable, HideDropdown)
class UDMAdaptiveFireTestAbility : public UDMGameplayAbility_Fire
{
    GENERATED_BODY()
public:
    bool bFailExecution = false;
    virtual bool ExecuteFire_Implementation(AActor* Target) override
    { return !bFailExecution && Super::ExecuteFire_Implementation(Target); }
};
