#pragma once

#include "CoreMinimal.h"
#include "DMMovementPolicyTypes.generated.h"

UENUM(BlueprintType)
enum class EDMMovementSpeedSource : uint8
{
	Roam,
	MovementAction,
	ADS,
	Firing,
	ExternalStatus,
	ActionIntent
};
