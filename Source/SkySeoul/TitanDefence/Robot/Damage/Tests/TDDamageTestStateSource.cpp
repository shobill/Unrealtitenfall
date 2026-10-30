#include "TitanDefence/Robot/Damage/Tests/TDDamageTestStateSource.h"

bool UTDDamageTestStateSource::ApplyResolvedBodyDamage(
	float DamageAmount, const FTDDamageResult& DamageResult)
{
	++ApplyCallCount;
	if (!bAcceptDamage)
	{
		return false;
	}

	TotalAppliedBodyDamage += DamageAmount;
	return true;
}

void UTDDamageTestStateSource::ObservePartDepleted(
	FGameplayTag PartTag, const FTDDamageResult& DamageResult)
{
	++PartDepletedCount;
}

void UTDDamageTestStateSource::ObservePartResolutionFailed(const FTDDamageResult& DamageResult)
{
	++PartResolutionFailedCount;
}

