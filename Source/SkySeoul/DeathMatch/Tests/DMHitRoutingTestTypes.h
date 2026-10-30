#pragma once

#include "DeathMatch/Abilities/DMGameplayAbility_Fire.h"
#include "PartsCharacter/PartsComponent.h"
#include "DMHitRoutingTestTypes.generated.h"

UCLASS(Transient, NotBlueprintable, HideDropdown)
class UDMHitRoutingTestAbility : public UDMGameplayAbility_Fire
{
    GENERATED_BODY()
public:
    using UDMGameplayAbility_Fire::ApplyFireDamage;
    ECollisionChannel GetTraceChannel() const { return TraceChannel; }
    int32 DestroyedEvents = 0;
    UFUNCTION()
    void ObserveDestroyed(FGameplayTag Part) { ++DestroyedEvents; }
};

UCLASS(Transient, NotBlueprintable, HideDropdown)
class UDMHitRoutingTestParts : public UPartsComponent
{
    GENERATED_BODY()
public:
    using UPartsComponent::ConfigureOwnerMeshForWeaponTrace;
};
