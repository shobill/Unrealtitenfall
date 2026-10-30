#include "DMCombatTargetSelectionPolicy.h"

#include "DeathMatch/AI/Knowledge/DMCombatKnowledgeComponent.h"
#include "DeathMatch/Core/DeathMatchTeamUtils.h"

AActor* UDMCombatTargetSelectionPolicy::SelectTarget(
	const UDMCombatKnowledgeComponent& Knowledge, const APawn* Observer) const
{
	AActor* BestTarget = nullptr;
	float BestScore = -TNumericLimits<float>::Max();
	for (const FDMRememberedThreat& Candidate : Knowledge.GetThreatRecords())
	{
		if (!IsValid(Candidate.Actor) || !DeathMatchTeamUtils::IsCombatTargetCandidate(Candidate.Actor.Get()))
		{
			continue;
		}
		const float Score = ScoreCandidate(Candidate, Knowledge.GetCombatTarget(), Observer);
		if (!BestTarget || Score > BestScore)
		{
			BestTarget = Candidate.Actor.Get();
			BestScore = Score;
		}
	}
	return BestTarget;
}

float UDMCombatTargetSelectionPolicy::ScoreCandidate(
	const FDMRememberedThreat& Candidate, const AActor* CurrentTarget, const APawn* Observer) const
{
	float Score = Candidate.bVisible ? VisibleBonus : 0.f;
	Score += Candidate.Actor.Get() == CurrentTarget ? CurrentTargetBonus : 0.f;
	if (Observer)
	{
		Score -= FVector::Distance(Observer->GetActorLocation(), Candidate.LastKnownLocation)
			* FMath::Max(0.f, DistancePenaltyPerMeter) / 100.f;
	}
	return Score;
}
