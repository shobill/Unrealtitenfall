#include "DeathMatch/Core/DeathMatchLog.h"

DEFINE_LOG_CATEGORY(LogDM);

TAutoConsoleVariable<int32> CVarDMCombatDiag(
	TEXT("dm.CombatDiag"),
	0,
	TEXT("DeathMatch combat diagnostics (Sight/HostileSeen/Approach/Attack/WantAttack). 0=off (default), 1=on."),
	ECVF_Default);

TAutoConsoleVariable<int32> CVarDMAimSightDiag(
	TEXT("dm.AimSightDiag"),
	0,
	TEXT("DeathMatch AimTarget Head/Body LOS for AI Sight. 0=off (default), 1=on."),
	ECVF_Default);
