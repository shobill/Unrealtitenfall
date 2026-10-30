#pragma once

#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"

/** Output Log filter: "LogDM" or "DM" — DeathMatch AI / combat / spawn / team. */
DECLARE_LOG_CATEGORY_EXTERN(LogDM, Log, All);

/** Console: `dm.CombatDiag 1` — verbose combat/sense/framework diagnostics (default off). */
extern TAutoConsoleVariable<int32> CVarDMCombatDiag;

/** Console: `dm.AimSightDiag 1` — AimTarget Head/Body LOS for AI Sight (default off). */
extern TAutoConsoleVariable<int32> CVarDMAimSightDiag;

FORCEINLINE bool DMIsCombatDiagEnabled()
{
	return CVarDMCombatDiag.GetValueOnAnyThread() != 0;
}

FORCEINLINE bool DMIsAimSightDiagEnabled()
{
	return CVarDMAimSightDiag.GetValueOnAnyThread() != 0;
}
