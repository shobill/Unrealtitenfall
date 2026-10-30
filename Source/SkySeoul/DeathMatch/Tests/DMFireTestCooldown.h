#pragma once
#include "GameplayEffect.h"
#include "DMFireTestCooldown.generated.h"

/** Duration effect used to verify actual ASC cooldown consumption in fire regression tests. */
UCLASS(Transient, NotBlueprintable, HideDropdown)
class UDMFireTestCooldown : public UGameplayEffect
{
    GENERATED_BODY()
public:
    UDMFireTestCooldown()
    {
        DurationPolicy = EGameplayEffectDurationType::HasDuration;
        DurationMagnitude = FScalableFloat(5.f);
    }
};
