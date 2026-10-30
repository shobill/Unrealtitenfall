#include "DMCombatPositionProfile.h"

#include "DeathMatch/Navigation/DMTacticalAreaVolume.h"

UDMCombatPositionProfile::UDMCombatPositionProfile()
{
	// Attack/Approach fire feasibility is a fixed task contract; this criterion controls optional scoring only.
	FireFeasibility.bRequiredFilter = false;
	FireFeasibility.AllowedMin = 1.f;
	FireFeasibility.AllowedMax = 1.f;
	FireFeasibility.Weight = 2.f;
	ThreatExposure.ScoreDirection = EDMCombatScoreDirection::LowerIsBetter;
	ThreatExposure.Weight = 2.f;
	RangeFitness.Weight = 2.f;
	NearbyFriendlyCount.ScoreDirection = EDMCombatScoreDirection::LowerIsBetter;
	NearbyFriendlyCount.NormalizeMax = 5.f;
	NearbyFriendlyCount.CheckRadius = 500.f;
	PathCost.ScoreDirection = EDMCombatScoreDirection::LowerIsBetter;
	PathCost.NormalizeMax = 5000.f;
	PathCost.Weight = .5f;
	AreaDanger.ScoreDirection = EDMCombatScoreDirection::LowerIsBetter;
	AreaDanger.AllowedMax = 10.f;
	AreaDanger.NormalizeMax = 10.f;
	AreaDanger.Weight = 1.f;
	AreaDangerIncrease.ScoreDirection = EDMCombatScoreDirection::LowerIsBetter;
	AreaDangerIncrease.AllowedMax = 10.f;
	AreaDangerIncrease.NormalizeMax = 10.f;
	AreaDangerIncrease.Weight = 2.f;
	bUseADSWhileMoving = true;
	bAllowFireWhileMoving = true;
}

#if WITH_EDITOR
void UDMCombatPositionProfile::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	if (!PropertyChangedEvent.Property
		|| PropertyChangedEvent.Property->GetFName() != GET_MEMBER_NAME_CHECKED(UDMCombatPositionProfile, Purpose)) return;
	const bool bNeedsFire = Purpose != EDMCombatPositionPurpose::Hide;
	FireFeasibility.bEnabled = bNeedsFire;
	FireFeasibility.bRequiredFilter = bNeedsFire;
	RangeFitness.bEnabled = bNeedsFire;
	bUseADSWhileMoving = Purpose == EDMCombatPositionPurpose::Attack;
	bAllowFireWhileMoving = Purpose == EDMCombatPositionPurpose::Attack;
}
#endif

float DMCombatPositionEvaluation::NormalizeCriterion(float Value, const FDMCombatPositionCriterion& Criterion)
{
	const float Low = FMath::Min(Criterion.NormalizeMin, Criterion.NormalizeMax);
	const float High = FMath::Max(Criterion.NormalizeMin, Criterion.NormalizeMax);
	if (FMath::IsNearlyEqual(Low, High)) return Value >= High ? 1.f : 0.f;
	const float Alpha = FMath::Clamp((Value - Low) / (High - Low), 0.f, 1.f);
	switch (Criterion.ScoreDirection)
	{
	case EDMCombatScoreDirection::LowerIsBetter: return 1.f - Alpha;
	case EDMCombatScoreDirection::PeakAtIdeal:
	{
		const float Ideal = FMath::Clamp(Criterion.IdealValue, Low, High);
		return Value <= Ideal
			? FMath::GetMappedRangeValueClamped(FVector2D(Low, Ideal), FVector2D(0.f, 1.f), Value)
			: FMath::GetMappedRangeValueClamped(FVector2D(Ideal, High), FVector2D(1.f, 0.f), Value);
	}
	default: return Alpha;
	}
}

float DMCombatPositionEvaluation::ComputeRangeFitness(float Distance, float EffectiveRange, float MaximumRange)
{
	const float Effective = FMath::Max(1.f, EffectiveRange);
	const float Maximum = FMath::Max(Effective, MaximumRange);
	if (Distance < 0.f || Distance > Maximum) return 0.f;
	const float ComfortableNear = Effective * .35f;
	if (Distance < ComfortableNear) return FMath::Clamp(Distance / FMath::Max(1.f, ComfortableNear), 0.f, 1.f);
	if (Distance <= Effective) return 1.f;
	return FMath::IsNearlyEqual(Effective, Maximum)
		? 1.f : FMath::Clamp(1.f - (Distance - Effective) / (Maximum - Effective), 0.f, 1.f);
}

bool DMCombatPositionEvaluation::IsWithinMaximumRange(
	float Distance, float MaximumRange, float SafetyMargin)
{
	const float BufferedMaximum = FMath::Max(0.f,
		FMath::Max(0.f, MaximumRange) - FMath::Max(0.f, SafetyMargin));
	return FMath::IsFinite(Distance) && Distance >= 0.f && Distance <= BufferedMaximum;
}

float DMCombatPositionEvaluation::ComputeWeightedExposure(const TArray<bool>& ExposureByThreat,
	const TArray<bool>& IsFocusingTarget, float FocusWeight, float OtherWeight, int32& OutExposedCount)
{
	OutExposedCount = 0;
	float ExposedWeight = 0.f;
	float TotalWeight = 0.f;
	for (int32 Index = 0; Index < ExposureByThreat.Num(); ++Index)
	{
		const float Weight = IsFocusingTarget.IsValidIndex(Index) && IsFocusingTarget[Index]
			? FMath::Max(0.f, FocusWeight) : FMath::Max(0.f, OtherWeight);
		TotalWeight += Weight;
		if (ExposureByThreat[Index])
		{
			ExposedWeight += Weight;
			++OutExposedCount;
		}
	}
	return TotalWeight > 0.f ? ExposedWeight / TotalWeight : 0.f;
}

int32 DMCombatPositionEvaluation::CountNearbyLocations(const FVector& Candidate, const TArray<FVector>& Locations, float Radius)
{
	const float RadiusSq = FMath::Square(FMath::Max(0.f, Radius));
	int32 Count = 0;
	for (const FVector& Location : Locations)
	{
		if (FVector::DistSquared2D(Candidate, Location) <= RadiusSq) ++Count;
	}
	return Count;
}

void DMCombatPositionEvaluation::SelectValidThreats(const TArray<FDMRememberedThreat>& Records,
	const AActor* FocusTarget, float Now, float MaxAge, int32 MaxCount,
	TArray<const FDMRememberedThreat*>& OutThreats)
{
	OutThreats.Reset();
	TArray<const FDMRememberedThreat*> Valid;
	for (const FDMRememberedThreat& Record : Records)
	{
		if (!IsValid(Record.Actor) || Now - Record.LastSensedTime > FMath::Max(0.f, MaxAge)) continue;
		Valid.Add(&Record);
	}
	Valid.Sort([FocusTarget](const FDMRememberedThreat& A, const FDMRememberedThreat& B)
	{
		if ((A.Actor == FocusTarget) != (B.Actor == FocusTarget)) return A.Actor == FocusTarget;
		if (A.bVisible != B.bVisible) return A.bVisible;
		return A.LastSensedTime > B.LastSensedTime;
	});
	for (int32 Index = 0; Index < FMath::Min(FMath::Max(0, MaxCount), Valid.Num()); ++Index)
	{
		OutThreats.Add(Valid[Index]);
	}
}

bool DMCombatPositionEvaluation::IsCriterionAllowed(float Value, const FDMCombatPositionCriterion& Criterion)
{
	return !Criterion.bEnabled || !Criterion.bRequiredFilter
		|| (Value >= FMath::Min(Criterion.AllowedMin, Criterion.AllowedMax)
			&& Value <= FMath::Max(Criterion.AllowedMin, Criterion.AllowedMax));
}

float DMCombatPositionEvaluation::ResolveAreaDanger(
	const ADMTacticalAreaVolume* Area, uint8 TeamId, float UnassignedAreaDanger)
{
	return Area && TeamId != 255
		? Area->GetTeamDanger(TeamId)
		: FMath::Clamp(UnassignedAreaDanger, 0.f, 10.f);
}

float DMCombatPositionEvaluation::ComputeAreaDangerIncrease(
	float CandidateAreaDanger, float CurrentAreaDanger)
{
	return FMath::Max(0.f, CandidateAreaDanger - CurrentAreaDanger);
}

float DMCombatPositionEvaluation::ComputeAreaDangerScore(
	float CandidateAreaDanger, float CurrentAreaDanger,
	const FDMCombatPositionCriterion& AreaDangerCriterion,
	const FDMCombatPositionCriterion& AreaDangerIncreaseCriterion)
{
	float Score = 0.f;
	if (AreaDangerCriterion.bEnabled)
	{
		Score += NormalizeCriterion(CandidateAreaDanger, AreaDangerCriterion) * AreaDangerCriterion.Weight;
	}
	if (AreaDangerIncreaseCriterion.bEnabled)
	{
		Score += NormalizeCriterion(ComputeAreaDangerIncrease(CandidateAreaDanger, CurrentAreaDanger),
			AreaDangerIncreaseCriterion) * AreaDangerIncreaseCriterion.Weight;
	}
	return Score;
}

bool DMCombatPositionEvaluation::HasMeaningfulDangerChange(
	float CachedDanger, float CurrentDanger, float Tolerance)
{
	return FMath::Abs(CurrentDanger - CachedDanger) > FMath::Max(0.f, Tolerance);
}

void DMCombatPositionEvaluation::SelectSpatiallyDiverseLocations(const TArray<FVector>& Locations,
	int32 MaxCount, TArray<FVector>& OutLocations)
{
	OutLocations.Reset();
	if (Locations.IsEmpty() || MaxCount <= 0) return;

	TArray<FVector> Remaining = Locations;
	Remaining.Sort([](const FVector& A, const FVector& B)
	{
		if (!FMath::IsNearlyEqual(A.X, B.X)) return A.X < B.X;
		if (!FMath::IsNearlyEqual(A.Y, B.Y)) return A.Y < B.Y;
		return A.Z < B.Z;
	});
	if (Remaining.Num() <= MaxCount)
	{
		OutLocations = MoveTemp(Remaining);
		return;
	}

	FVector Centroid = FVector::ZeroVector;
	for (const FVector& Location : Remaining) Centroid += Location;
	Centroid /= Remaining.Num();
	int32 FirstIndex = 0;
	float FirstDistance = BIG_NUMBER;
	for (int32 Index = 0; Index < Remaining.Num(); ++Index)
	{
		const float Distance = FVector::DistSquared(Remaining[Index], Centroid);
		if (Distance < FirstDistance)
		{
			FirstDistance = Distance;
			FirstIndex = Index;
		}
	}
	OutLocations.Add(Remaining[FirstIndex]);
	Remaining.RemoveAt(FirstIndex, EAllowShrinking::No);
	while (OutLocations.Num() < MaxCount && !Remaining.IsEmpty())
	{
		int32 BestIndex = 0;
		float BestMinimumDistance = -1.f;
		for (int32 Index = 0; Index < Remaining.Num(); ++Index)
		{
			float MinimumDistance = BIG_NUMBER;
			for (const FVector& Selected : OutLocations)
			{
				MinimumDistance = FMath::Min(MinimumDistance,
					FVector::DistSquared2D(Remaining[Index], Selected));
			}
			if (MinimumDistance > BestMinimumDistance)
			{
				BestMinimumDistance = MinimumDistance;
				BestIndex = Index;
			}
		}
		OutLocations.Add(Remaining[BestIndex]);
		Remaining.RemoveAt(BestIndex, EAllowShrinking::No);
	}
}

void DMCombatPositionEvaluation::SelectPurposeShortlist(
	const TArray<FDMCombatPositionPrefilterCandidate>& Candidates, EDMCombatPositionPurpose Purpose,
	int32 MaxCount, float HideExposedFraction, TArray<int32>& OutIndices)
{
	OutIndices.Reset();
	const int32 Limit = FMath::Min(FMath::Max(0, MaxCount), Candidates.Num());
	if (Limit == 0) return;

	TArray<int32> Eligible;
	for (int32 Index = 0; Index < Candidates.Num(); ++Index)
	{
		if (Purpose == EDMCombatPositionPurpose::Hide || Candidates[Index].bFocusLineClear)
			Eligible.Add(Index);
	}
	Eligible.Sort([&Candidates](int32 A, int32 B)
	{
		if (!FMath::IsNearlyEqual(Candidates[A].CheapScore, Candidates[B].CheapScore))
			return Candidates[A].CheapScore > Candidates[B].CheapScore;
		const FVector& LA = Candidates[A].Location;
		const FVector& LB = Candidates[B].Location;
		if (!FMath::IsNearlyEqual(LA.X, LB.X)) return LA.X < LB.X;
		if (!FMath::IsNearlyEqual(LA.Y, LB.Y)) return LA.Y < LB.Y;
		return LA.Z < LB.Z;
	});

	auto AddDiverse = [&Candidates, &OutIndices](const TArray<int32>& Pool, int32 Count)
	{
		TArray<int32> Remaining = Pool;
		while (Count-- > 0 && !Remaining.IsEmpty())
		{
			int32 BestPoolIndex = 0;
			float BestValue = -BIG_NUMBER;
			for (int32 PoolIndex = 0; PoolIndex < Remaining.Num(); ++PoolIndex)
			{
				const int32 CandidateIndex = Remaining[PoolIndex];
				float MinDistance = OutIndices.IsEmpty() ? 0.f : BIG_NUMBER;
				for (const int32 SelectedIndex : OutIndices)
					MinDistance = FMath::Min(MinDistance, FVector::DistSquared2D(
						Candidates[CandidateIndex].Location, Candidates[SelectedIndex].Location));
				const float Value = Candidates[CandidateIndex].CheapScore * 1000000.f + FMath::Sqrt(MinDistance);
				if (Value > BestValue)
				{
					BestValue = Value;
					BestPoolIndex = PoolIndex;
				}
			}
			OutIndices.Add(Remaining[BestPoolIndex]);
			Remaining.RemoveAt(BestPoolIndex, EAllowShrinking::No);
		}
	};

	if (Purpose != EDMCombatPositionPurpose::Hide)
	{
		AddDiverse(Eligible, FMath::Min(Limit, Eligible.Num()));
		return;
	}

	TArray<int32> Concealed;
	TArray<int32> Exposed;
	for (const int32 Index : Eligible)
		(Candidates[Index].bRepresentativeConcealed ? Concealed : Exposed).Add(Index);
	const int32 ExposedSlots = FMath::Min(Exposed.Num(), FMath::Clamp(
		FMath::RoundToInt(Limit * FMath::Clamp(HideExposedFraction, 0.f, 1.f)), 0, Limit));
	AddDiverse(Concealed, FMath::Min(Concealed.Num(), Limit - ExposedSlots));
	AddDiverse(Exposed, FMath::Min(Exposed.Num(), Limit - OutIndices.Num()));
	if (OutIndices.Num() < Limit)
	{
		TArray<int32> RemainingConcealed;
		for (const int32 Index : Concealed) if (!OutIndices.Contains(Index)) RemainingConcealed.Add(Index);
		AddDiverse(RemainingConcealed, Limit - OutIndices.Num());
	}
}

uint32 DMCombatPositionEvaluation::ComputeThreatSnapshotHash(
	const TArray<const FDMRememberedThreat*>& Threats)
{
	uint32 Hash = 0;
	for (const FDMRememberedThreat* Threat : Threats)
	{
		if (!Threat) continue;
		Hash = HashCombineFast(Hash, GetTypeHash(Threat->Actor.Get()));
		Hash = HashCombineFast(Hash, GetTypeHash(Threat->LastKnownLocation.GridSnap(10.f)));
		Hash = HashCombineFast(Hash, GetTypeHash(Threat->bVisible));
	}
	return Hash;
}
