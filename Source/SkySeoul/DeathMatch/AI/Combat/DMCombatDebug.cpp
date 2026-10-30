#include "DMCombatDebug.h"

#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

namespace
{
TAutoConsoleVariable<int32> CVarDMCombatDebug(TEXT("dm.CombatDebug"), 0,
	TEXT("0=off, 1=compact BT data, 2=detailed BT/EQS/move/fire data"), ECVF_Cheat);
TAutoConsoleVariable<float> CVarDMCombatDebugRange(TEXT("dm.CombatDebug.Range"), 5000.f,
	TEXT("Maximum AI debug draw distance in centimeters"), ECVF_Cheat);
}

int32 DMCombatDebug::Mode()
{
	return FMath::Clamp(CVarDMCombatDebug.GetValueOnGameThread(), 0, 2);
}

float DMCombatDebug::Range()
{
	return FMath::Max(0.f, CVarDMCombatDebugRange.GetValueOnGameThread());
}

bool DMCombatDebug::IsEnabled(const UWorld* World)
{
	return World && World->GetNetMode() != NM_DedicatedServer && Mode() > 0;
}
