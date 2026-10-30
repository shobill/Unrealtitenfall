#pragma once

#include "CoreMinimal.h"
#include "DMCombatStatusTypes.generated.h"

UENUM(BlueprintType)
enum class EDMCombatAIState : uint8
{
	Idle,
	Alert,
	Combat,
	Dead
};

UENUM(BlueprintType)
enum class EDMHealthRiskTier : uint8
{
	Safe,
	Caution,
	Dangerous,
	Critical
};
