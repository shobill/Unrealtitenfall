#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "DMNonCombatActor.generated.h"

/**
 * Marker for actors that must never participate in DeathMatch combat sensing,
 * targeting, threat memory, or combat-space occupancy.
 */
UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UDMNonCombatActor : public UInterface
{
	GENERATED_BODY()
};

class SKYSEOUL_API IDMNonCombatActor
{
	GENERATED_BODY()
};
