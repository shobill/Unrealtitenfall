#include "DMCombatFireTypes.h"

void UDMCombatFireModule::Reset() {}

bool UDMCombatFireModule::IsReady(float Now) const
{
	return Now >= NextShotTime;
}

void UDMCombatFireModule::ShotFired(float Now)
{
	FDMFireCycleRuntime State;
	State.Settings = GetCycleSettings();
	State.RecordSuccessfulShot(Now);
	NextShotTime = State.NextShotTime;
}

void UDMCombatFire_Precision::ShotFired(float Now)
{
	NextShotTime = Now + FMath::Max(0.f, ShotPauseSeconds);
}

void UDMCombatFire_Burst::Reset()
{
	Super::Reset();
	Shots = 0;
}

void UDMCombatFire_Burst::ShotFired(float Now)
{
	++Shots;
	const int32 Count = FMath::Max(1, ShotsPerBurst);
	if (Shots >= Count)
	{
		Shots = 0;
		NextShotTime = Now + FMath::Max(.01f, BurstPause);
	}
	else
	{
		NextShotTime = Now + FMath::Max(.01f, ShotInterval);
	}
}

void UDMCombatFire_Automatic::ShotFired(float Now)
{
	++Shots;
	if (MaxAutomaticShots > 0 && Shots >= MaxAutomaticShots)
	{
		Shots = 0;
		NextShotTime = Now + FMath::Max(0.f, AutomaticPause);
	}
	else
	{
		NextShotTime = Now + FMath::Max(.01f, ShotInterval);
	}
}
