#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DeathMatch/AI/Knowledge/DMCombatKnowledgeTypes.h"
#include "DMCombatPositionProfile.generated.h"

class ADMTacticalAreaVolume;

UENUM(BlueprintType)
enum class EDMCombatPositionPurpose : uint8
{
	Attack,
	Approach,
	Hide
};

UENUM(BlueprintType)
enum class EDMCombatScoreDirection : uint8
{
	HigherIsBetter,
	LowerIsBetter,
	PeakAtIdeal
};

/** Editor-facing configuration shared by each optional filter/score. */
USTRUCT(BlueprintType)
struct FDMCombatPositionCriterion
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Criterion") bool bEnabled = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Criterion") bool bRequiredFilter = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Criterion", meta=(ClampMin="0.0")) float Weight = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Criterion") float AllowedMin = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Criterion") float AllowedMax = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Criterion") float NormalizeMin = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Criterion") float NormalizeMax = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Criterion") float IdealValue = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Criterion") EDMCombatScoreDirection ScoreDirection = EDMCombatScoreDirection::HigherIsBetter;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Criterion", meta=(ClampMin="0.0", Units="cm")) float CheckRadius = 0.f;
};

USTRUCT(BlueprintType)
struct FDMCombatPositionSearchSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Search", meta=(ClampMin="1")) int32 MaxCandidateCount = 64;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Search", meta=(ClampMin="1")) int32 MaxThreatCount = 8;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Search", meta=(ClampMin="1")) int32 ExpensiveCheckCandidateCount = 12;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Search|Hide", meta=(ToolTip="Run one representative-threat exposure trace for every capped Hide candidate before expensive checks.")) bool bUseHideRepresentativeThreatPrefilter = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Search|Hide", meta=(ClampMin="1", EditCondition="bUseHideRepresentativeThreatPrefilter")) int32 HidePrefilterCandidateCount = 12;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Search|Hide", meta=(ClampMin="0.0", ClampMax="1.0", EditCondition="bUseHideRepresentativeThreatPrefilter", ToolTip="Fraction of the Hide shortlist reserved for representative-threat-exposed points to preserve spatial alternatives.")) float HideExposedCandidateFraction = .25f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Search", meta=(ClampMin="0.0", Units="s")) float SearchCooldown = .25f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Search", meta=(ClampMin="0.0", Units="s")) float ResultReuseTime = .5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Search", meta=(ClampMin="0.0", ClampMax="10.0", ToolTip="Cached combat positions are re-evaluated when current or candidate Tactical Area danger changes by more than this amount.")) float AreaDangerChangeTolerance = .1f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Search", meta=(ClampMin="0.0", Units="s")) float ThreatMemoryMaxAge = 5.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Search", meta=(ClampMin="0.0", Units="cm", ToolTip="Fallback only when neither an active weapon muzzle nor Pawn View origin can provide a fire-origin offset.")) float CandidateEyeHeight = 60.f;
};

/** A purpose-specific data asset. The EQS asset only generates common NavMesh candidates. */
UCLASS(BlueprintType)
class SKYSEOUL_API UDMCombatPositionProfile : public UDataAsset
{
	GENERATED_BODY()

public:
	UDMCombatPositionProfile();
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Profile") EDMCombatPositionPurpose Purpose = EDMCombatPositionPurpose::Attack;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Profile") FDMCombatPositionSearchSettings Search;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Evaluation|Optional Scoring", meta=(ToolTip="Optional score/filter metadata. Attack and Approach always require a clear focusing-target firing line regardless of bEnabled or bRequiredFilter.")) FDMCombatPositionCriterion FireFeasibility;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Evaluation") FDMCombatPositionCriterion ThreatExposure;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Evaluation|Optional Scoring", meta=(ToolTip="Optional score/filter metadata. Attack and Approach always enforce weapon maximum range regardless of bEnabled or bRequiredFilter.")) FDMCombatPositionCriterion RangeFitness;
	/** Attack/Approach candidate-only buffer from the absolute weapon maximum. Hide ignores it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Evaluation|Range", meta=(ClampMin="0.0", Units="cm", ToolTip="Candidate selection safety margin inside the active weapon maximum range. Actual firing still uses GetMaxFireRange()."))
	float MaximumRangeSafetyMargin = 75.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Evaluation") FDMCombatPositionCriterion NearbyFriendlyCount;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Evaluation") FDMCombatPositionCriterion PathCost;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Evaluation|Tactical Area", meta=(ToolTip="Absolute team-relative danger of the candidate Tactical Area.")) FDMCombatPositionCriterion AreaDanger;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Evaluation|Tactical Area", meta=(ToolTip="Positive team-relative danger increase from the AI's current Tactical Area to the candidate area.")) FDMCombatPositionCriterion AreaDangerIncrease;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Evaluation|Tactical Area", meta=(ClampMin="0.0", ClampMax="10.0", ToolTip="Neutral danger used when the current or candidate location has no assigned Tactical Area.")) float UnassignedAreaDanger = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Exposure", meta=(ClampMin="0.0")) float FocusingTargetExposureWeight = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Exposure", meta=(ClampMin="0.0")) float OtherThreatExposureWeight = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Exposure", meta=(ClampMin="0.0", ClampMax="1.0")) float MinimumConcealedFraction = .5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Execution", meta=(ToolTip="Allow ADS while moving. False does not disable ADS after reaching the combat position.")) bool bUseADSWhileMoving = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Execution") bool bAllowFireWhileMoving = false;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};

struct FDMCombatPositionScore
{
	FVector Location = FVector::ZeroVector;
	float Total = 0.f;
	float FireFeasibility = 0.f;
	float ThreatExposure = 0.f;
	float RangeFitness = 0.f;
	float NearbyFriendlyCount = 0.f;
	float PathCost = 0.f;
	float AreaDanger = 0.f;
	float CurrentAreaDanger = 0.f;
	float AreaDangerIncrease = 0.f;
	TWeakObjectPtr<ADMTacticalAreaVolume> Area;
	TWeakObjectPtr<ADMTacticalAreaVolume> CurrentArea;
	int32 ExposedThreats = 0;
	int32 EvaluatedThreats = 0;
};

/** Small, testable record used by purpose-specific shortlist selection. */
struct FDMCombatPositionPrefilterCandidate
{
	FVector Location = FVector::ZeroVector;
	float CheapScore = 0.f;
	bool bFocusLineClear = false;
	bool bRepresentativeConcealed = false;
};

namespace DMCombatPositionEvaluation
{
	SKYSEOUL_API float NormalizeCriterion(float Value, const FDMCombatPositionCriterion& Criterion);
	SKYSEOUL_API float ComputeRangeFitness(float Distance, float EffectiveRange, float MaximumRange);
	SKYSEOUL_API bool IsWithinMaximumRange(float Distance, float MaximumRange, float SafetyMargin = 0.f);
	SKYSEOUL_API float ComputeWeightedExposure(const TArray<bool>& ExposureByThreat,
		const TArray<bool>& IsFocusingTarget, float FocusWeight, float OtherWeight, int32& OutExposedCount);
	SKYSEOUL_API int32 CountNearbyLocations(const FVector& Candidate, const TArray<FVector>& Locations, float Radius);
	SKYSEOUL_API void SelectValidThreats(const TArray<FDMRememberedThreat>& Records, const AActor* FocusTarget,
		float Now, float MaxAge, int32 MaxCount, TArray<const FDMRememberedThreat*>& OutThreats);
	SKYSEOUL_API bool IsCriterionAllowed(float Value, const FDMCombatPositionCriterion& Criterion);
	SKYSEOUL_API float ResolveAreaDanger(const ADMTacticalAreaVolume* Area, uint8 TeamId,
		float UnassignedAreaDanger);
	SKYSEOUL_API float ComputeAreaDangerIncrease(float CandidateAreaDanger, float CurrentAreaDanger);
	SKYSEOUL_API float ComputeAreaDangerScore(float CandidateAreaDanger, float CurrentAreaDanger,
		const FDMCombatPositionCriterion& AreaDangerCriterion,
		const FDMCombatPositionCriterion& AreaDangerIncreaseCriterion);
	SKYSEOUL_API bool HasMeaningfulDangerChange(float CachedDanger, float CurrentDanger, float Tolerance);
	SKYSEOUL_API void SelectSpatiallyDiverseLocations(const TArray<FVector>& Locations, int32 MaxCount,
		TArray<FVector>& OutLocations);
	SKYSEOUL_API void SelectPurposeShortlist(const TArray<FDMCombatPositionPrefilterCandidate>& Candidates,
		EDMCombatPositionPurpose Purpose, int32 MaxCount, float HideExposedFraction,
		TArray<int32>& OutIndices);
	SKYSEOUL_API uint32 ComputeThreatSnapshotHash(const TArray<const FDMRememberedThreat*>& Threats);
}
