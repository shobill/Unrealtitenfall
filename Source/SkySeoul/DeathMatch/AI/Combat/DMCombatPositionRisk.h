#pragma once
#include "CoreMinimal.h"
#include "DMCombatPositionRisk.generated.h"

struct FDMPositionNeighbor;

UENUM(BlueprintType)
enum class EDMCombatRiskTier : uint8 { Safe, Caution, Dangerous, Critical };

USTRUCT(BlueprintType)
struct FDMCombatRiskTierSettings
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category="Risk Tiers", meta=(ClampMin="0", ToolTip="Behavior risk threshold. Runtime sorts thresholds into ascending order.")) float CautionThreshold = 40;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category="Risk Tiers", meta=(ClampMin="0", ToolTip="Behavior risk threshold. Runtime sorts thresholds into ascending order.")) float DangerousThreshold = 65;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category="Risk Tiers", meta=(ClampMin="0", ToolTip="Behavior risk threshold. Runtime sorts thresholds into ascending order.")) float CriticalThreshold = 85;
    FVector Ordered() const
    {
        float A=FMath::Max(0.f,CautionThreshold), B=FMath::Max(0.f,DangerousThreshold), C=FMath::Max(0.f,CriticalThreshold);
        if (A>B) Swap(A,B); if (B>C) Swap(B,C); if (A>B) Swap(A,B);
        return FVector(A,B,C);
    }
};

USTRUCT(BlueprintType)
struct FDMPositionRiskSettings
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category="Risk", meta=(ClampMin="0", ToolTip="Multiplier applied to risk above Soft Risk Limit. Zero disables excess penalty.")) float ExcessRiskPenaltyMultiplier = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Risk") float MinThreatDistance = 400;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Risk") float MaxAdvanceDistance = 400;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Risk") float HardMaxAdvanceDistance = 800;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Risk") float AdvanceRiskWeight = 25;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Risk") float DestinationExposureWeight = 12;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Risk") float PathExposureWeight = 20;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Risk", meta=(DisplayName="Soft Risk Limit", ToolTip="Risk score above this limit receives a soft penalty; never a hard rejection. Serialized name retained.")) float MaxAcceptableRisk = 65;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Risk", meta=(DisplayName="Minimum Base Position Score", ToolTip="Minimum quality before risk subtraction. Preserves existing BP semantics; risk remains a soft cost.")) float MinimumAcceptablePositionScore = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Risk", meta=(DisplayName="Apply Excess Risk Penalty", ToolTip="Apply soft excess risk cost, not rejection. Serialized name retained.")) bool bRejectDangerousPositions = true;
};

struct FDMPositionRisk
{
    float Total = 0, Proximity = 0, Advance = 0, DestinationExposure = 0, PathExposure = 0, AllySupport = 0, Escape = 0;
    bool bAcceptable = false;
    FName Rejection;
    int32 ExposedEnemies = 0;
    int32 EvaluatedThreats = 0;
    float WeightedExposure = 0, FlankRear = 0, RememberedRisk = 0, PathLength = 0;
    float RecentDamageRisk = 0, ExcessRiskPenalty = 0, SensedWeightedExposure = 0;
    TWeakObjectPtr<AActor> MostDangerous;
    float WeightedPathExposure = 0;
    float HealthRatio = 1, HealthRisk = 0, HealthExposureRisk = 0, HealthPathRisk = 0;
    float TotalHealthRiskContribution = 0;
    bool bCacheHit = false;
};

/** Data-only input suitable for an EQS custom test. */
struct FDMPositionRiskInput
{
    FVector SelfLocation = FVector::ZeroVector;
    TArray<FVector> VisibleThreatLocations;
    TArray<FVector> RememberedThreatLocations;
    float HealthRatio = 1.f;
};

namespace DMPositionRisk
{
    // No traces or navigation. All threats contribute, including those outside the LOS budget.
    FDMPositionRisk EvaluateCheap(const FDMPositionRiskInput& Input, const FVector& Ground, const FDMPositionRiskSettings& Settings);
    inline EDMCombatRiskTier Tier(float Risk, const FDMCombatRiskTierSettings& Settings = FDMCombatRiskTierSettings())
    { const FVector T=Settings.Ordered(); return Risk >= T.Z ? EDMCombatRiskTier::Critical : Risk >= T.Y ? EDMCombatRiskTier::Dangerous : Risk >= T.X ? EDMCombatRiskTier::Caution : EDMCombatRiskTier::Safe; }
    // No actor transforms from remembered enemies. Path samples follow nav polyline length.
    FDMPositionRisk Evaluate(const FDMPositionRiskInput& Input, const FVector& Ground,
        const FDMPositionRiskSettings& Settings, const TArray<FDMPositionNeighbor>& Neighbors, bool bPath);
}
