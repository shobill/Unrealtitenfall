#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Subsystems/WorldSubsystem.h"
#include "DeathMatchMessageStructure.h"
#include "DeathMatchEventSubsystem.generated.h"

DECLARE_MULTICAST_DELEGATE_TwoParams(FDeathMatchDeathEvent, FGameplayTag, const FDeathMessagePayload&);

/** World-local death notifications, isolated between matches and PIE worlds. */
UCLASS()
class SKYSEOUL_API UDeathMatchEventSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    FDeathMatchDeathEvent OnDeath;
};