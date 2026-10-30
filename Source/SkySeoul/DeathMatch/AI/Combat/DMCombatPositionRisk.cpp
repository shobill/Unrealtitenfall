#include "DMCombatPositionRisk.h"
#include "DMCombatPositionRegistry.h"

FDMPositionRisk DMPositionRisk::EvaluateCheap(const FDMPositionRiskInput& Input, const FVector& Ground,
	const FDMPositionRiskSettings& Settings)
{
	FDMPositionRisk Result;
	for (const FVector& Threat : Input.VisibleThreatLocations)
	{
		const float Distance = FVector::Dist2D(Ground, Threat);
		Result.Proximity += FMath::Clamp(1.f - Distance / FMath::Max(1.f, Settings.MinThreatDistance * 4.f), 0.f, 1.f) * 40.f;
		++Result.EvaluatedThreats;
		++Result.ExposedEnemies;
	}
	for (const FVector& Threat : Input.RememberedThreatLocations)
	{
		const float Distance = FVector::Dist2D(Ground, Threat);
		Result.RememberedRisk += FMath::Clamp(1.f - Distance / FMath::Max(1.f, Settings.MinThreatDistance * 4.f), 0.f, 1.f) * 15.f;
		++Result.EvaluatedThreats;
	}
	Result.HealthRatio = FMath::Clamp(Input.HealthRatio, 0.f, 1.f);
	Result.HealthRisk = (1.f - Result.HealthRatio) * (Result.Proximity + Result.RememberedRisk) * .5f;
	Result.TotalHealthRiskContribution = Result.HealthRisk;
	Result.Total = Result.Proximity + Result.RememberedRisk + Result.HealthRisk;
	Result.ExcessRiskPenalty = Settings.bRejectDangerousPositions
		? FMath::Max(0.f, Result.Total - Settings.MaxAcceptableRisk) * FMath::Max(0.f, Settings.ExcessRiskPenaltyMultiplier) : 0.f;
	Result.bAcceptable = true;
	return Result;
}

FDMPositionRisk DMPositionRisk::Evaluate(const FDMPositionRiskInput& Input, const FVector& Ground,
	const FDMPositionRiskSettings& Settings, const TArray<FDMPositionNeighbor>& Neighbors, bool)
{
	FDMPositionRisk Result = EvaluateCheap(Input, Ground, Settings);
	for (const FDMPositionNeighbor& Neighbor : Neighbors)
	{
		if (FVector::DistSquared2D(Ground, Neighbor.Location) <= FMath::Square(FMath::Max(100.f, Neighbor.Radius)))
			Result.AllySupport += 5.f;
	}
	Result.Total = FMath::Max(0.f, Result.Total - Result.AllySupport);
	return Result;
}
