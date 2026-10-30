#pragma once

#include "CoreMinimal.h"
#include "DeathMatchMessageStructure.generated.h"

USTRUCT(BlueprintType)
struct FDeathMessagePayload
{
    GENERATED_BODY();

    UPROPERTY(BlueprintReadWrite)
    AController* Victim = nullptr;

    UPROPERTY(BlueprintReadWrite)
    uint8 VictimTeamId = 255;
};

