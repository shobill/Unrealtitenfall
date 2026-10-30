#pragma once
#include "CoreMinimal.h"
#include "DMCombatHealthEvaluation.generated.h"

USTRUCT(BlueprintType)
struct FDMCombatHealthSettings
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category="Health Evaluation", meta=(ClampMin="0", ClampMax="1", ToolTip="Accumulated health fraction change that requests early score revalidation. Zero uses normal cache intervals; emergency threshold crossings still refresh.")) float HealthRatioReevaluationThreshold = .05f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Health Evaluation", meta=(ToolTip="Enable continuous health costs and movement adjustments. False preserves legacy emergency conditions.")) bool bEnableHealthEvaluation = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Health Evaluation", meta=(ClampMin="0", ToolTip="Unitless multiplier; zero disables all continuous health adjustments. Negative/nonfinite values become zero.")) float HealthEvaluationWeight = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Health Evaluation", meta=(ClampMin="0", ClampMax="1", ToolTip="Health fraction where defensive preference begins; zero gives no positive-health band. Runtime sorts with Critical.")) float CautionHealthRatio = .65f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Health Evaluation", meta=(ClampMin="0", ClampMax="1", ToolTip="Health fraction where defensive preference reaches full strength; zero reaches full strength at zero HP. Runtime sorts with Caution.")) float CriticalHealthRatio = .30f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Health Evaluation", meta=(ClampMin="0", ToolTip="Risk points per weighted destination exposure at full health risk. Zero disables; higher increases risk.")) float LowHealthExposurePenalty = 20;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Health Evaluation", meta=(ClampMin="0", ToolTip="Risk points per mean weighted path exposure at full health risk. Zero disables; higher increases risk.")) float LowHealthPathExposurePenalty = 10;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Health Evaluation", meta=(ClampMin="0", ToolTip="Position score reward for protection under threat. Zero disables; higher favors cover without bypassing safety.")) float LowHealthCoverReward = 20;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Health Evaluation", meta=(ClampMin="0", ToolTip="Movement utility reward for a safer retreat under exposure. Zero disables; higher favors safe withdrawal.")) float LowHealthRetreatReward = 15;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Health Evaluation", meta=(ClampMin="0", ToolTip="Position/movement penalty points for unnecessary exposed advance. Zero disables; never changes weapon or target scores.")) float LowHealthAggressionPenalty = 15;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category="Health Evaluation", meta=(ClampMin="0", ClampMax="1", ToolTip="Unitless micro radius multiplier at full weighted health risk. One preserves radius; zero suppresses movement. Does not affect cover peek or explicit Strafe.")) float LowHealthMicroMoveScale = .5f;
};

// Data-only health evaluation. Decorators and EQS tests consume coefficients without choosing behavior.
struct FDMCombatHealthEvaluation
{
    float CurrentHealth = 0, MaxHealth = 0, HealthRatio = 1, HealthRisk = 0;
    float ExposurePenalty = 0, PathExposurePenalty = 0, CoverReward = 0, RetreatReward = 0, AggressionPenalty = 0;
    float MicroMoveScale = 1;
};

namespace DMCombatHealth
{
    inline float NonNegative(float V) { return FMath::IsFinite(V) ? FMath::Max(0.f,V) : 0.f; }
    inline float Unit(float V) { return FMath::Clamp(NonNegative(V),0.f,1.f); }
    inline FDMCombatHealthEvaluation Evaluate(float Current, float Maximum, const FDMCombatHealthSettings& S)
    {
        FDMCombatHealthEvaluation H;
        H.CurrentHealth=Current; H.MaxHealth=Maximum;
        if (FMath::IsFinite(Maximum) && Maximum>0 && FMath::IsFinite(Current))
            H.HealthRatio=static_cast<float>(FMath::Clamp(static_cast<double>(Current)/Maximum,0.0,1.0));
        H.HealthRisk=Unit(1.f-H.HealthRatio);
        const float Weight=S.bEnableHealthEvaluation?NonNegative(S.HealthEvaluationWeight):0.f;
        const float Risk=H.HealthRisk*Weight;
        const float Caution=FMath::Max(Unit(S.CautionHealthRatio),Unit(S.CriticalHealthRatio));
        const float Critical=FMath::Min(Unit(S.CautionHealthRatio),Unit(S.CriticalHealthRatio));
        const float Preference=Risk*Unit((Caution-H.HealthRatio)/FMath::Max(SMALL_NUMBER,Caution-Critical));
        H.ExposurePenalty=Risk*NonNegative(S.LowHealthExposurePenalty);
        H.PathExposurePenalty=Risk*NonNegative(S.LowHealthPathExposurePenalty);
        H.CoverReward=Preference*NonNegative(S.LowHealthCoverReward);
        H.RetreatReward=Preference*NonNegative(S.LowHealthRetreatReward);
        H.AggressionPenalty=Preference*NonNegative(S.LowHealthAggressionPenalty);
        H.MicroMoveScale=FMath::Lerp(1.f,Unit(S.LowHealthMicroMoveScale),Unit(Risk));
        return H;
    }
    inline bool Emergency(const FDMCombatHealthEvaluation& H, bool bEnabled, float Threshold, float SensedExposure)
    { return bEnabled && H.HealthRatio<Unit(Threshold) && SensedExposure>0; }
    inline float ExposureCost(const FDMCombatHealthEvaluation& H, float Exposure, float PathExposure)
    { return NonNegative(Exposure)*H.ExposurePenalty+NonNegative(PathExposure)*H.PathExposurePenalty; }
}
