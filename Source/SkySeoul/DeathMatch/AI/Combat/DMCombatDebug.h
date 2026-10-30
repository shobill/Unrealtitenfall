#pragma once

#include "CoreMinimal.h"

class UWorld;

/** Read-only debug switches. They never reevaluate behavior or run EQS. */
namespace DMCombatDebug
{
	SKYSEOUL_API int32 Mode();
	SKYSEOUL_API float Range();
	SKYSEOUL_API bool IsEnabled(const UWorld* World);
}
